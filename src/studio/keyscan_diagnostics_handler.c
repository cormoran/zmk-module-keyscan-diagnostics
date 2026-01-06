/**
 * Keyscan Diagnostics - Custom Studio RPC Handler
 *
 * This file implements the RPC subsystem for keyscan diagnostics.
 * It handles requests from the web UI to start/stop monitoring,
 * retrieve diagnostics data, and manage chattering detection.
 */

#include <pb_decode.h>
#include <pb_encode.h>
#include <zmk/studio/custom.h>
#include <zmk/keyscan_diagnostics/custom.pb.h>
#include <zmk/keyscan_diagnostics.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/**
 * Metadata for the keyscan diagnostics subsystem.
 */
static struct zmk_rpc_custom_subsystem_meta keyscan_diagnostics_meta = {
    ZMK_RPC_CUSTOM_SUBSYSTEM_UI_URLS("http://localhost:5173"),
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

/**
 * Register the custom RPC subsystem.
 */
ZMK_RPC_CUSTOM_SUBSYSTEM(zmk__keyscan_diagnostics, &keyscan_diagnostics_meta,
                         keyscan_diagnostics_rpc_handle_request);

ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(zmk__keyscan_diagnostics, zmk_keyscan_diagnostics_Response);

// Forward declarations
static int handle_start_monitoring(const zmk_keyscan_diagnostics_StartMonitoringRequest *req,
                                   zmk_keyscan_diagnostics_Response *resp);
static int handle_stop_monitoring(const zmk_keyscan_diagnostics_StopMonitoringRequest *req,
                                  zmk_keyscan_diagnostics_Response *resp);
static int handle_get_state(const zmk_keyscan_diagnostics_GetStateRequest *req,
                           zmk_keyscan_diagnostics_Response *resp);
static int handle_clear_data(const zmk_keyscan_diagnostics_ClearDataRequest *req,
                            zmk_keyscan_diagnostics_Response *resp);
static int handle_get_gpio_config(const zmk_keyscan_diagnostics_GetGPIOConfigRequest *req,
                                  zmk_keyscan_diagnostics_Response *resp);

/**
 * Main request handler for the keyscan diagnostics RPC subsystem.
 */
static bool
keyscan_diagnostics_rpc_handle_request(const zmk_custom_CallRequest *raw_request,
                                       pb_callback_t *encode_response) {
    zmk_keyscan_diagnostics_Response *resp =
        ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(zmk__keyscan_diagnostics,
                                                          encode_response);

    zmk_keyscan_diagnostics_Request req = zmk_keyscan_diagnostics_Request_init_zero;

    // Decode the incoming request
    pb_istream_t req_stream = pb_istream_from_buffer(raw_request->payload.bytes,
                                                     raw_request->payload.size);
    if (!pb_decode(&req_stream, zmk_keyscan_diagnostics_Request_fields, &req)) {
        LOG_WRN("Failed to decode keyscan diagnostics request: %s", PB_GET_ERROR(&req_stream));
        zmk_keyscan_diagnostics_ErrorResponse err = zmk_keyscan_diagnostics_ErrorResponse_init_zero;
        snprintf(err.message, sizeof(err.message), "Failed to decode request");
        resp->which_response_type = zmk_keyscan_diagnostics_Response_error_tag;
        resp->response_type.error = err;
        return true;
    }

    int rc = 0;
    switch (req.which_request_type) {
    case zmk_keyscan_diagnostics_Request_start_monitoring_tag:
        rc = handle_start_monitoring(&req.request_type.start_monitoring, resp);
        break;
    case zmk_keyscan_diagnostics_Request_stop_monitoring_tag:
        rc = handle_stop_monitoring(&req.request_type.stop_monitoring, resp);
        break;
    case zmk_keyscan_diagnostics_Request_get_state_tag:
        rc = handle_get_state(&req.request_type.get_state, resp);
        break;
    case zmk_keyscan_diagnostics_Request_clear_data_tag:
        rc = handle_clear_data(&req.request_type.clear_data, resp);
        break;
    case zmk_keyscan_diagnostics_Request_get_gpio_config_tag:
        rc = handle_get_gpio_config(&req.request_type.get_gpio_config, resp);
        break;
    default:
        LOG_WRN("Unsupported keyscan diagnostics request type: %d", req.which_request_type);
        rc = -1;
    }

    if (rc != 0) {
        zmk_keyscan_diagnostics_ErrorResponse err = zmk_keyscan_diagnostics_ErrorResponse_init_zero;
        snprintf(err.message, sizeof(err.message), "Failed to process request (rc=%d)", rc);
        resp->which_response_type = zmk_keyscan_diagnostics_Response_error_tag;
        resp->response_type.error = err;
    }
    return true;
}

/**
 * Handle start monitoring request
 */
static int handle_start_monitoring(const zmk_keyscan_diagnostics_StartMonitoringRequest *req,
                                   zmk_keyscan_diagnostics_Response *resp) {
    uint32_t chatter_threshold = req->chatter_threshold_ms;
    if (chatter_threshold == 0) {
        chatter_threshold = CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTER_THRESHOLD_MS;
    }

    LOG_INF("Start monitoring request: chatter_threshold=%u ms", chatter_threshold);

    int rc = keyscan_diagnostics_start(chatter_threshold);
    
    zmk_keyscan_diagnostics_StartMonitoringResponse result = 
        zmk_keyscan_diagnostics_StartMonitoringResponse_init_zero;
    result.success = (rc == 0);
    
    // Get GPIO count
    struct keyscan_diag_gpio_pin pins[16];
    int gpio_count = keyscan_diagnostics_get_gpio_pins(pins, 16);
    result.gpio_count = (gpio_count > 0) ? gpio_count : 0;

    resp->which_response_type = zmk_keyscan_diagnostics_Response_start_monitoring_tag;
    resp->response_type.start_monitoring = result;
    return rc;
}

/**
 * Handle stop monitoring request
 */
static int handle_stop_monitoring(const zmk_keyscan_diagnostics_StopMonitoringRequest *req,
                                  zmk_keyscan_diagnostics_Response *resp) {
    LOG_INF("Stop monitoring request");

    int rc = keyscan_diagnostics_stop();
    
    zmk_keyscan_diagnostics_StopMonitoringResponse result = 
        zmk_keyscan_diagnostics_StopMonitoringResponse_init_zero;
    result.success = (rc == 0);

    resp->which_response_type = zmk_keyscan_diagnostics_Response_stop_monitoring_tag;
    resp->response_type.stop_monitoring = result;
    return rc;
}

/**
 * Handle get state request
 */
static int handle_get_state(const zmk_keyscan_diagnostics_GetStateRequest *req,
                           zmk_keyscan_diagnostics_Response *resp) {
    LOG_DBG("Get state request");

    zmk_keyscan_diagnostics_GetStateResponse result = 
        zmk_keyscan_diagnostics_GetStateResponse_init_zero;
    
    result.monitoring_active = keyscan_diagnostics_is_monitoring();
    result.total_events = keyscan_diagnostics_get_total_events();

    // Get recent events
    struct keyscan_diag_event events[20];
    int event_count = keyscan_diagnostics_get_recent_events(events, 20);
    result.recent_events_count = event_count;
    for (int i = 0; i < event_count; i++) {
        result.recent_events[i].row = events[i].row;
        result.recent_events[i].col = events[i].col;
        result.recent_events[i].pressed = events[i].pressed;
        result.recent_events[i].timestamp_ms = events[i].timestamp_ms;
    }

    // Get chattering statistics
    struct keyscan_diag_chatter_stats stats[32];
    int stats_count = keyscan_diagnostics_get_chatter_stats(stats, 32);
    result.chatter_stats_count = stats_count;
    for (int i = 0; i < stats_count; i++) {
        result.chatter_stats[i].row = stats[i].row;
        result.chatter_stats[i].col = stats[i].col;
        result.chatter_stats[i].event_count = stats[i].event_count;
        result.chatter_stats[i].chatter_count = stats[i].chatter_count;
        result.chatter_stats[i].last_event_ms = stats[i].last_event_ms;
        result.chatter_stats[i].min_interval_ms = stats[i].min_interval_ms;
    }

    // Get GPIO pins
    struct keyscan_diag_gpio_pin pins[16];
    int gpio_count = keyscan_diagnostics_get_gpio_pins(pins, 16);
    result.gpio_pins_count = gpio_count;
    for (int i = 0; i < gpio_count; i++) {
        result.gpio_pins[i].pin = pins[i].pin;
        snprintf(result.gpio_pins[i].port_name, sizeof(result.gpio_pins[i].port_name),
                "%s", pins[i].port_name ? pins[i].port_name : "unknown");
    }

    resp->which_response_type = zmk_keyscan_diagnostics_Response_get_state_tag;
    resp->response_type.get_state = result;
    return 0;
}

/**
 * Handle clear data request
 */
static int handle_clear_data(const zmk_keyscan_diagnostics_ClearDataRequest *req,
                            zmk_keyscan_diagnostics_Response *resp) {
    LOG_INF("Clear data request");

    int rc = keyscan_diagnostics_clear();
    
    zmk_keyscan_diagnostics_ClearDataResponse result = 
        zmk_keyscan_diagnostics_ClearDataResponse_init_zero;
    result.success = (rc == 0);

    resp->which_response_type = zmk_keyscan_diagnostics_Response_clear_data_tag;
    resp->response_type.clear_data = result;
    return rc;
}

/**
 * Handle get GPIO configuration request
 */
static int handle_get_gpio_config(const zmk_keyscan_diagnostics_GetGPIOConfigRequest *req,
                                  zmk_keyscan_diagnostics_Response *resp) {
    LOG_DBG("Get GPIO config request");

    zmk_keyscan_diagnostics_GetGPIOConfigResponse result = 
        zmk_keyscan_diagnostics_GetGPIOConfigResponse_init_zero;

    // Get GPIO pins
    struct keyscan_diag_gpio_pin pins[16];
    int gpio_count = keyscan_diagnostics_get_gpio_pins(pins, 16);
    result.gpio_pins_count = gpio_count;
    for (int i = 0; i < gpio_count; i++) {
        result.gpio_pins[i].pin = pins[i].pin;
        snprintf(result.gpio_pins[i].port_name, sizeof(result.gpio_pins[i].port_name),
                "%s", pins[i].port_name ? pins[i].port_name : "unknown");
    }

    // Get matrix dimensions
    uint32_t rows, cols;
    if (keyscan_diagnostics_get_matrix_size(&rows, &cols) == 0) {
        result.matrix_rows = rows;
        result.matrix_cols = cols;
    }

    resp->which_response_type = zmk_keyscan_diagnostics_Response_get_gpio_config_tag;
    resp->response_type.get_gpio_config = result;
    return 0;
}
