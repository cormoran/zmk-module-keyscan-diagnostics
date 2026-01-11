/**
 * Keyscan Diagnostics - Custom Studio RPC Handler
 *
 * This module provides diagnostics for keyscan issues including:
 * - GPIO pin configuration retrieval
 * - Real-time key event monitoring
 * - Chattering detection
 * - Key matrix health status
 *
 * Currently supports charlieplex matrix. Designed to be extensible
 * to other kscan types (matrix, direct) in the future.
 */

#include <pb_decode.h>
#include <pb_encode.h>
#include <zmk/studio/custom.h>
#include <zmk/keyscan_diagnostics/diagnostics.pb.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

#define MAX_EVENTS CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_MAX_EVENTS
#define DEFAULT_CHATTERING_WINDOW_MS CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTERING_WINDOW_MS
#define DEFAULT_CHATTERING_THRESHOLD CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTERING_THRESHOLD

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/** Buffered key event for monitoring */
struct buffered_event {
    uint32_t row;
    uint32_t col;
    bool pressed;
    int64_t timestamp_ms;
};

/** Per-key statistics */
struct key_stats {
    uint32_t press_count;
    uint32_t release_count;
    bool current_state;
    int64_t last_event_time;
};

/** Chattering alert information */
struct chattering_alert_info {
    uint32_t row;
    uint32_t col;
    uint32_t event_count;
    int64_t first_event_ms;
    int64_t last_event_ms;
};

/** Module state */
static struct {
    bool monitoring_active;
    struct buffered_event events[MAX_EVENTS];
    uint32_t event_head;
    uint32_t event_count;
    uint32_t total_events;
    bool buffer_overflow;
    
    /* Chattering detection */
    uint32_t chattering_window_ms;
    uint32_t chattering_threshold;
    struct chattering_alert_info chattering_alerts[32];
    uint32_t chattering_alert_count;
    
    /* Key statistics - dynamically indexed by (row * max_cols + col) */
    struct key_stats key_stats[120]; /* Max 120 keys */
    uint32_t max_rows;
    uint32_t max_cols;
} state = {
    .monitoring_active = false,
    .event_head = 0,
    .event_count = 0,
    .total_events = 0,
    .buffer_overflow = false,
    .chattering_window_ms = DEFAULT_CHATTERING_WINDOW_MS,
    .chattering_threshold = DEFAULT_CHATTERING_THRESHOLD,
    .chattering_alert_count = 0,
    .max_rows = 0,
    .max_cols = 0,
};

/* ============================================================================
 * Charlieplex Configuration Detection
 * ============================================================================ */

#define CHARLIEPLEX_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zmk_kscan_gpio_charlieplex)

#if DT_NODE_EXISTS(CHARLIEPLEX_NODE)
#define HAS_CHARLIEPLEX 1
#define CHARLIEPLEX_GPIO_COUNT DT_PROP_LEN(CHARLIEPLEX_NODE, gpios)

static const struct gpio_dt_spec charlieplex_gpios[] = {
    DT_FOREACH_PROP_ELEM_SEP(CHARLIEPLEX_NODE, gpios, GPIO_DT_SPEC_GET_BY_IDX, (,))
};

static uint32_t get_charlieplex_debounce_press_ms(void) {
#if DT_NODE_HAS_PROP(CHARLIEPLEX_NODE, debounce_press_ms)
    return DT_PROP(CHARLIEPLEX_NODE, debounce_press_ms);
#elif DT_NODE_HAS_PROP(CHARLIEPLEX_NODE, debounce_period)
    return DT_PROP(CHARLIEPLEX_NODE, debounce_period);
#else
    return 5; /* Default */
#endif
}

static uint32_t get_charlieplex_debounce_release_ms(void) {
#if DT_NODE_HAS_PROP(CHARLIEPLEX_NODE, debounce_release_ms)
    return DT_PROP(CHARLIEPLEX_NODE, debounce_release_ms);
#elif DT_NODE_HAS_PROP(CHARLIEPLEX_NODE, debounce_period)
    return DT_PROP(CHARLIEPLEX_NODE, debounce_period);
#else
    return 5; /* Default */
#endif
}

static uint32_t get_charlieplex_debounce_scan_period_ms(void) {
#if DT_NODE_HAS_PROP(CHARLIEPLEX_NODE, debounce_scan_period_ms)
    return DT_PROP(CHARLIEPLEX_NODE, debounce_scan_period_ms);
#else
    return 1; /* Default */
#endif
}

static uint32_t get_charlieplex_poll_period_ms(void) {
#if DT_NODE_HAS_PROP(CHARLIEPLEX_NODE, poll_period_ms)
    return DT_PROP(CHARLIEPLEX_NODE, poll_period_ms);
#else
    return 10; /* Default */
#endif
}

static bool get_charlieplex_use_interrupt(void) {
#if DT_NODE_HAS_PROP(CHARLIEPLEX_NODE, interrupt_gpios)
    return true;
#else
    return false;
#endif
}

#else
#define HAS_CHARLIEPLEX 0
#define CHARLIEPLEX_GPIO_COUNT 0
#endif

/* ============================================================================
 * Keyscan Event Listener
 * ============================================================================ */

static const struct device *kscan_dev = NULL;

static void record_key_event(uint32_t row, uint32_t col, bool pressed) {
    if (!state.monitoring_active) {
        return;
    }
    
    int64_t now = k_uptime_get();
    
    /* Record event in buffer */
    if (state.event_count < MAX_EVENTS) {
        uint32_t idx = (state.event_head + state.event_count) % MAX_EVENTS;
        state.events[idx].row = row;
        state.events[idx].col = col;
        state.events[idx].pressed = pressed;
        state.events[idx].timestamp_ms = now;
        state.event_count++;
    } else {
        /* Buffer full - overwrite oldest */
        state.events[state.event_head].row = row;
        state.events[state.event_head].col = col;
        state.events[state.event_head].pressed = pressed;
        state.events[state.event_head].timestamp_ms = now;
        state.event_head = (state.event_head + 1) % MAX_EVENTS;
        state.buffer_overflow = true;
    }
    state.total_events++;
    
    /* Update key statistics */
    if (row < state.max_rows && col < state.max_cols) {
        uint32_t key_idx = row * state.max_cols + col;
        if (key_idx < ARRAY_SIZE(state.key_stats)) {
            struct key_stats *ks = &state.key_stats[key_idx];
            if (pressed) {
                ks->press_count++;
            } else {
                ks->release_count++;
            }
            ks->current_state = pressed;
            
            /* Check for chattering */
            if (ks->last_event_time > 0) {
                int64_t delta = now - ks->last_event_time;
                if (delta < state.chattering_window_ms) {
                    /* Potential chattering - check if we need to create/update alert */
                    bool found = false;
                    for (uint32_t i = 0; i < state.chattering_alert_count; i++) {
                        if (state.chattering_alerts[i].row == row &&
                            state.chattering_alerts[i].col == col) {
                            state.chattering_alerts[i].event_count++;
                            state.chattering_alerts[i].last_event_ms = now;
                            found = true;
                            break;
                        }
                    }
                    if (!found && state.chattering_alert_count < ARRAY_SIZE(state.chattering_alerts)) {
                        struct chattering_alert_info *alert = 
                            &state.chattering_alerts[state.chattering_alert_count++];
                        alert->row = row;
                        alert->col = col;
                        alert->event_count = 2; /* This event + previous */
                        alert->first_event_ms = ks->last_event_time;
                        alert->last_event_ms = now;
                    }
                }
            }
            ks->last_event_time = now;
        }
    }
    
    LOG_DBG("Key event: row=%d, col=%d, pressed=%d, timestamp=%lld", 
            row, col, pressed, now);
}

static void kscan_callback(const struct device *dev, uint32_t row, uint32_t col, bool pressed) {
    record_key_event(row, col, pressed);
}

/* ============================================================================
 * RPC Handler Registration
 * ============================================================================ */

static struct zmk_rpc_custom_subsystem_meta keyscan_diagnostics_meta = {
    ZMK_RPC_CUSTOM_SUBSYSTEM_UI_URLS("http://localhost:5173"),
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

ZMK_RPC_CUSTOM_SUBSYSTEM(zmk__keyscan_diagnostics, &keyscan_diagnostics_meta,
                         keyscan_diagnostics_handle_request);

ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(zmk__keyscan_diagnostics, 
                                          zmk_keyscan_diagnostics_Response);

/* ============================================================================
 * Request Handlers
 * ============================================================================ */

static int handle_get_kscan_config(zmk_keyscan_diagnostics_Response *resp) {
    resp->which_response_type = zmk_keyscan_diagnostics_Response_kscan_config_tag;
    zmk_keyscan_diagnostics_KscanConfig *config = &resp->response_type.kscan_config;
    
#if HAS_CHARLIEPLEX
    config->type = zmk_keyscan_diagnostics_KscanType_KSCAN_TYPE_CHARLIEPLEX;
    config->which_config = zmk_keyscan_diagnostics_KscanConfig_charlieplex_tag;
    
    zmk_keyscan_diagnostics_CharlieplexConfig *cp = &config->config.charlieplex;
    cp->gpios_count = CHARLIEPLEX_GPIO_COUNT;
    
    for (int i = 0; i < CHARLIEPLEX_GPIO_COUNT && i < ARRAY_SIZE(cp->gpios); i++) {
        cp->gpios[i].port = 0; /* Port number - typically 0 for gpio0 */
        cp->gpios[i].pin = charlieplex_gpios[i].pin;
        
        /* Get port name */
        const char *port_name = charlieplex_gpios[i].port->name;
        strncpy(cp->gpios[i].port_name, port_name, 
                sizeof(cp->gpios[i].port_name) - 1);
        cp->gpios[i].port_name[sizeof(cp->gpios[i].port_name) - 1] = '\0';
        
        cp->gpios[i].active_low = (charlieplex_gpios[i].dt_flags & GPIO_ACTIVE_LOW) != 0;
    }
    
    cp->debounce_press_ms = get_charlieplex_debounce_press_ms();
    cp->debounce_release_ms = get_charlieplex_debounce_release_ms();
    cp->debounce_scan_period_ms = get_charlieplex_debounce_scan_period_ms();
    cp->poll_period_ms = get_charlieplex_poll_period_ms();
    cp->use_interrupt = get_charlieplex_use_interrupt();
    
    /* Initialize state dimensions for charlieplex */
    state.max_rows = CHARLIEPLEX_GPIO_COUNT;
    state.max_cols = CHARLIEPLEX_GPIO_COUNT;
#else
    config->type = zmk_keyscan_diagnostics_KscanType_KSCAN_TYPE_UNKNOWN;
#endif
    
    return 0;
}

static int handle_get_key_matrix(zmk_keyscan_diagnostics_Response *resp) {
    resp->which_response_type = zmk_keyscan_diagnostics_Response_key_matrix_tag;
    zmk_keyscan_diagnostics_GetKeyMatrixResponse *matrix = &resp->response_type.key_matrix;
    
#if HAS_CHARLIEPLEX
    matrix->type = zmk_keyscan_diagnostics_KscanType_KSCAN_TYPE_CHARLIEPLEX;
    matrix->rows = CHARLIEPLEX_GPIO_COUNT;
    matrix->cols = CHARLIEPLEX_GPIO_COUNT;
    
    uint32_t key_count = 0;
    for (uint32_t row = 0; row < CHARLIEPLEX_GPIO_COUNT; row++) {
        for (uint32_t col = 0; col < CHARLIEPLEX_GPIO_COUNT; col++) {
            if (row == col) continue; /* Charlieplex can't scan same pin */
            if (key_count >= ARRAY_SIZE(matrix->keys)) break;
            
            zmk_keyscan_diagnostics_KeyInfo *key = &matrix->keys[key_count];
            key->row = row;
            key->col = col;
            key->gpio_out_index = row;
            key->gpio_in_index = col;
            
            uint32_t key_idx = row * CHARLIEPLEX_GPIO_COUNT + col;
            if (key_idx < ARRAY_SIZE(state.key_stats)) {
                key->current_state = state.key_stats[key_idx].current_state;
                key->press_count = state.key_stats[key_idx].press_count;
                key->release_count = state.key_stats[key_idx].release_count;
            }
            
            key_count++;
        }
    }
    matrix->keys_count = key_count;
#else
    matrix->type = zmk_keyscan_diagnostics_KscanType_KSCAN_TYPE_UNKNOWN;
    matrix->rows = 0;
    matrix->cols = 0;
    matrix->keys_count = 0;
#endif
    
    return 0;
}

static int handle_start_monitoring(const zmk_keyscan_diagnostics_StartMonitoringRequest *req,
                                   zmk_keyscan_diagnostics_Response *resp) {
    resp->which_response_type = zmk_keyscan_diagnostics_Response_start_monitoring_tag;
    zmk_keyscan_diagnostics_StartMonitoringResponse *start_resp = 
        &resp->response_type.start_monitoring;
    
    /* Reset state */
    state.monitoring_active = true;
    state.event_head = 0;
    state.event_count = 0;
    state.total_events = 0;
    state.buffer_overflow = false;
    state.chattering_alert_count = 0;
    
    /* Reset key statistics */
    memset(state.key_stats, 0, sizeof(state.key_stats));
    
#if HAS_CHARLIEPLEX
    state.max_rows = CHARLIEPLEX_GPIO_COUNT;
    state.max_cols = CHARLIEPLEX_GPIO_COUNT;
#endif
    
    /* Try to configure the kscan callback */
    if (kscan_dev == NULL) {
#if HAS_CHARLIEPLEX
        kscan_dev = DEVICE_DT_GET(CHARLIEPLEX_NODE);
        if (device_is_ready(kscan_dev)) {
            /* Note: We can't actually replace the callback in ZMK as it's 
             * configured at init time. This is a limitation we need to work around.
             * For now, we'll rely on the event system integration. */
            start_resp->success = true;
            snprintf(start_resp->message, sizeof(start_resp->message),
                     "Monitoring started (charlieplex)");
        } else {
            start_resp->success = false;
            snprintf(start_resp->message, sizeof(start_resp->message),
                     "Kscan device not ready");
        }
#else
        start_resp->success = false;
        snprintf(start_resp->message, sizeof(start_resp->message),
                 "No supported kscan found");
#endif
    } else {
        start_resp->success = true;
        snprintf(start_resp->message, sizeof(start_resp->message),
                 "Monitoring restarted");
    }
    
    LOG_INF("Monitoring started: success=%d", start_resp->success);
    return 0;
}

static int handle_stop_monitoring(zmk_keyscan_diagnostics_Response *resp) {
    resp->which_response_type = zmk_keyscan_diagnostics_Response_stop_monitoring_tag;
    resp->response_type.stop_monitoring.success = true;
    state.monitoring_active = false;
    
    LOG_INF("Monitoring stopped");
    return 0;
}

static int handle_get_events(const zmk_keyscan_diagnostics_GetEventsRequest *req,
                             zmk_keyscan_diagnostics_Response *resp) {
    resp->which_response_type = zmk_keyscan_diagnostics_Response_events_tag;
    zmk_keyscan_diagnostics_GetEventsResponse *events_resp = &resp->response_type.events;
    
    events_resp->buffer_overflow = state.buffer_overflow;
    events_resp->total_events = state.total_events;
    
    /* Copy events to response */
    uint32_t copy_count = MIN(state.event_count, ARRAY_SIZE(events_resp->events));
    for (uint32_t i = 0; i < copy_count; i++) {
        uint32_t idx = (state.event_head + i) % MAX_EVENTS;
        events_resp->events[i].row = state.events[idx].row;
        events_resp->events[i].col = state.events[idx].col;
        events_resp->events[i].pressed = state.events[idx].pressed;
        events_resp->events[i].timestamp_ms = state.events[idx].timestamp_ms;
    }
    events_resp->events_count = copy_count;
    
    /* Clear buffer if requested */
    if (req->clear_buffer) {
        state.event_head = 0;
        state.event_count = 0;
        state.buffer_overflow = false;
    }
    
    return 0;
}

static int handle_configure_chattering(
    const zmk_keyscan_diagnostics_ConfigureChatteringRequest *req,
    zmk_keyscan_diagnostics_Response *resp) {
    
    resp->which_response_type = zmk_keyscan_diagnostics_Response_configure_chattering_tag;
    
    if (req->config.window_ms > 0) {
        state.chattering_window_ms = req->config.window_ms;
    }
    if (req->config.threshold_count > 0) {
        state.chattering_threshold = req->config.threshold_count;
    }
    
    resp->response_type.configure_chattering.success = true;
    
    LOG_INF("Chattering config updated: window=%d ms, threshold=%d",
            state.chattering_window_ms, state.chattering_threshold);
    return 0;
}

static int handle_get_chattering_alerts(
    const zmk_keyscan_diagnostics_GetChatteringAlertsRequest *req,
    zmk_keyscan_diagnostics_Response *resp) {
    
    resp->which_response_type = zmk_keyscan_diagnostics_Response_chattering_alerts_tag;
    zmk_keyscan_diagnostics_GetChatteringAlertsResponse *alerts_resp = 
        &resp->response_type.chattering_alerts;
    
    /* Only include alerts that meet the threshold */
    uint32_t alert_count = 0;
    for (uint32_t i = 0; i < state.chattering_alert_count; i++) {
        if (state.chattering_alerts[i].event_count >= state.chattering_threshold) {
            if (alert_count < ARRAY_SIZE(alerts_resp->alerts)) {
                alerts_resp->alerts[alert_count].row = state.chattering_alerts[i].row;
                alerts_resp->alerts[alert_count].col = state.chattering_alerts[i].col;
                alerts_resp->alerts[alert_count].event_count = state.chattering_alerts[i].event_count;
                alerts_resp->alerts[alert_count].first_event_ms = state.chattering_alerts[i].first_event_ms;
                alerts_resp->alerts[alert_count].last_event_ms = state.chattering_alerts[i].last_event_ms;
                alert_count++;
            }
        }
    }
    alerts_resp->alerts_count = alert_count;
    
    /* Clear alerts if requested */
    if (req->clear_alerts) {
        state.chattering_alert_count = 0;
    }
    
    return 0;
}

static int handle_test_gpio_pin(const zmk_keyscan_diagnostics_TestGpioPinRequest *req,
                                zmk_keyscan_diagnostics_Response *resp) {
    resp->which_response_type = zmk_keyscan_diagnostics_Response_test_gpio_pin_tag;
    zmk_keyscan_diagnostics_TestGpioPinResponse *test_resp = &resp->response_type.test_gpio_pin;
    
    test_resp->gpio_index = req->gpio_index;
    
#if HAS_CHARLIEPLEX
    if (req->gpio_index >= CHARLIEPLEX_GPIO_COUNT) {
        test_resp->success = false;
        snprintf(test_resp->error_message, sizeof(test_resp->error_message),
                 "Invalid GPIO index: %d", req->gpio_index);
        return 0;
    }
    
    const struct gpio_dt_spec *gpio = &charlieplex_gpios[req->gpio_index];
    if (!device_is_ready(gpio->port)) {
        test_resp->success = false;
        snprintf(test_resp->error_message, sizeof(test_resp->error_message),
                 "GPIO port not ready");
        return 0;
    }
    
    /* Read current pin state */
    int val = gpio_pin_get_dt(gpio);
    if (val < 0) {
        test_resp->success = false;
        snprintf(test_resp->error_message, sizeof(test_resp->error_message),
                 "GPIO read error: %d", val);
        return 0;
    }
    
    test_resp->success = true;
    test_resp->pin_state = (val != 0);
#else
    test_resp->success = false;
    snprintf(test_resp->error_message, sizeof(test_resp->error_message),
             "No kscan configured");
#endif
    
    return 0;
}

/* ============================================================================
 * Main Request Handler
 * ============================================================================ */

static bool keyscan_diagnostics_handle_request(const zmk_custom_CallRequest *raw_request,
                                               pb_callback_t *encode_response) {
    zmk_keyscan_diagnostics_Response *resp =
        ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(zmk__keyscan_diagnostics,
                                                          encode_response);
    
    zmk_keyscan_diagnostics_Request req = zmk_keyscan_diagnostics_Request_init_zero;
    
    /* Decode the incoming request */
    pb_istream_t req_stream = pb_istream_from_buffer(raw_request->payload.bytes,
                                                     raw_request->payload.size);
    if (!pb_decode(&req_stream, zmk_keyscan_diagnostics_Request_fields, &req)) {
        LOG_WRN("Failed to decode keyscan diagnostics request: %s", 
                PB_GET_ERROR(&req_stream));
        zmk_keyscan_diagnostics_ErrorResponse err = 
            zmk_keyscan_diagnostics_ErrorResponse_init_zero;
        snprintf(err.message, sizeof(err.message), "Failed to decode request");
        resp->which_response_type = zmk_keyscan_diagnostics_Response_error_tag;
        resp->response_type.error = err;
        return true;
    }
    
    int rc = 0;
    switch (req.which_request_type) {
    case zmk_keyscan_diagnostics_Request_get_kscan_config_tag:
        rc = handle_get_kscan_config(resp);
        break;
    case zmk_keyscan_diagnostics_Request_get_key_matrix_tag:
        rc = handle_get_key_matrix(resp);
        break;
    case zmk_keyscan_diagnostics_Request_start_monitoring_tag:
        rc = handle_start_monitoring(&req.request_type.start_monitoring, resp);
        break;
    case zmk_keyscan_diagnostics_Request_stop_monitoring_tag:
        rc = handle_stop_monitoring(resp);
        break;
    case zmk_keyscan_diagnostics_Request_get_events_tag:
        rc = handle_get_events(&req.request_type.get_events, resp);
        break;
    case zmk_keyscan_diagnostics_Request_configure_chattering_tag:
        rc = handle_configure_chattering(&req.request_type.configure_chattering, resp);
        break;
    case zmk_keyscan_diagnostics_Request_get_chattering_alerts_tag:
        rc = handle_get_chattering_alerts(&req.request_type.get_chattering_alerts, resp);
        break;
    case zmk_keyscan_diagnostics_Request_test_gpio_pin_tag:
        rc = handle_test_gpio_pin(&req.request_type.test_gpio_pin, resp);
        break;
    default:
        LOG_WRN("Unsupported keyscan diagnostics request type: %d", 
                req.which_request_type);
        rc = -1;
    }
    
    if (rc != 0) {
        zmk_keyscan_diagnostics_ErrorResponse err = 
            zmk_keyscan_diagnostics_ErrorResponse_init_zero;
        snprintf(err.message, sizeof(err.message), "Failed to process request");
        resp->which_response_type = zmk_keyscan_diagnostics_Response_error_tag;
        resp->response_type.error = err;
    }
    
    return true;
}

/* ============================================================================
 * ZMK Event Listener Integration
 * ============================================================================ */

/* 
 * To capture key events from ZMK's event system, we need to listen to
 * position_state_changed events. This allows us to monitor key presses
 * without modifying the kscan driver callback.
 */
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

static int keyscan_diagnostics_position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    /* Convert position to row/col based on matrix dimensions */
#if HAS_CHARLIEPLEX
    uint32_t pos = ev->position;
    uint32_t cols = CHARLIEPLEX_GPIO_COUNT;
    
    /* For charlieplex, we need to calculate the actual row/col 
     * considering that diagonal entries (row == col) are skipped */
    uint32_t row = 0;
    uint32_t col = 0;
    uint32_t current_pos = 0;
    
    for (row = 0; row < CHARLIEPLEX_GPIO_COUNT; row++) {
        for (col = 0; col < CHARLIEPLEX_GPIO_COUNT; col++) {
            if (row == col) continue;
            if (current_pos == pos) {
                record_key_event(row, col, ev->state);
                return ZMK_EV_EVENT_BUBBLE;
            }
            current_pos++;
        }
    }
#endif
    
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(keyscan_diagnostics, keyscan_diagnostics_position_listener);
ZMK_SUBSCRIPTION(keyscan_diagnostics, zmk_position_state_changed);
