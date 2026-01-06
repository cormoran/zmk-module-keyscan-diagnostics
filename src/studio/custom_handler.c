/**
 * Keyscan diagnostics - Custom Studio RPC Handler
 *
 * Collects key state transitions, detects chattering, and exposes
 * per-key/line statistics over the custom Studio RPC endpoint.
 */

#include <pb_decode.h>
#include <pb_encode.h>

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <zephyr/sys/util.h>

#include <zmk/events/position_state_changed.h>
#include <zmk/matrix.h>
#include <zmk/matrix_transform.h>
#include <zmk/physical_layouts.h>
#include <zmk/studio/custom.h>

#include <zmk/keyscan/diagnostics.pb.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define KEYSCAN_DIAG_SUBSYSTEM zmk__keyscan_diag
#define KEYSCAN_DIAG_UI_URL "http://localhost:5173"

#define CHATTER_WINDOW_MS CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTER_WINDOW_MS
#define CHATTER_BURST CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTER_BURST

#ifndef ZMK_MATRIX_NODE_ID
#define ZMK_MATRIX_NODE_ID DT_CHOSEN(zmk_kscan)
#endif

#if DT_NODE_HAS_COMPAT(ZMK_MATRIX_NODE_ID, zmk_kscan_gpio_charlieplex)
#define KEYSCAN_DIAG_HAS_CHARLIEPLEX 1
#define KEYSCAN_DIAG_LINE_COUNT DT_PROP_LEN(ZMK_MATRIX_NODE_ID, gpios)
#define KEYSCAN_DIAG_GPIO_SPEC(idx, _) GPIO_DT_SPEC_GET_BY_IDX(ZMK_MATRIX_NODE_ID, gpios, idx)
static const struct gpio_dt_spec keyscan_diag_lines[KEYSCAN_DIAG_LINE_COUNT] = {
    LISTIFY(KEYSCAN_DIAG_LINE_COUNT, KEYSCAN_DIAG_GPIO_SPEC, (, ), 0)};
static uint32_t keyscan_diag_line_activity[KEYSCAN_DIAG_LINE_COUNT];
#else
#define KEYSCAN_DIAG_HAS_CHARLIEPLEX 0
#define KEYSCAN_DIAG_LINE_COUNT 0
#endif

static zmk_matrix_transform_t diag_transform;

#if KEYSCAN_DIAG_HAS_CHARLIEPLEX
#define KEYSCAN_DIAG_ROWS KEYSCAN_DIAG_LINE_COUNT
#define KEYSCAN_DIAG_COLS KEYSCAN_DIAG_LINE_COUNT
#elif ZMK_MATRIX_HAS_TRANSFORM
#define KEYSCAN_DIAG_ROWS DT_PROP(ZMK_KEYMAP_TRANSFORM_NODE, rows)
#define KEYSCAN_DIAG_COLS DT_PROP(ZMK_KEYMAP_TRANSFORM_NODE, columns)
#else
#define KEYSCAN_DIAG_ROWS ZMK_MATRIX_ROWS
#define KEYSCAN_DIAG_COLS ZMK_MATRIX_COLS
#endif

#define KEYSCAN_DIAG_KEY_COUNT ZMK_KEYMAP_LEN

struct keyscan_diag_stat {
    uint32_t press_count;
    uint32_t release_count;
    uint32_t chatter_count;
    uint8_t burst;
    int64_t burst_start;
    int64_t last_change;
    bool pressed;
    bool seen;
};

struct keyscan_line_ref {
    uint8_t drive;
    uint8_t sense;
    bool valid;
};

static struct keyscan_diag_stat keyscan_stats[KEYSCAN_DIAG_KEY_COUNT];
static struct keyscan_line_ref keyscan_line_map[KEYSCAN_DIAG_KEY_COUNT];

static void keyscan_diag_build_line_map(void) {
    if (!diag_transform) {
        return;
    }

    for (int row = 0; row < KEYSCAN_DIAG_ROWS; row++) {
        for (int col = 0; col < KEYSCAN_DIAG_COLS; col++) {
#if KEYSCAN_DIAG_HAS_CHARLIEPLEX
            if (row == col) {
                continue;
            }
#endif
            int32_t position = zmk_matrix_transform_row_column_to_position(diag_transform, row, col);
            if (position < 0 || position >= KEYSCAN_DIAG_KEY_COUNT) {
                continue;
            }

            keyscan_line_map[position] =
                (struct keyscan_line_ref){.drive = row, .sense = col, .valid = true};
        }
    }
}

static void keyscan_diag_reset_counters(void) {
    memset(keyscan_stats, 0, sizeof(keyscan_stats));
#if KEYSCAN_DIAG_HAS_CHARLIEPLEX
    memset(keyscan_diag_line_activity, 0, sizeof(keyscan_diag_line_activity));
#endif
}

static void keyscan_diag_update_chatter(struct keyscan_diag_stat *stat, int64_t timestamp,
                                        uint32_t position) {
    if (stat->burst_start == 0 || (timestamp - stat->burst_start) > CHATTER_WINDOW_MS) {
        stat->burst_start = timestamp;
        stat->burst = 0;
    }

    stat->burst++;

    if (stat->burst >= CHATTER_BURST) {
        stat->chatter_count++;
        LOG_WRN("Chatter detected on position %u", position);
        stat->burst = 0;
        stat->burst_start = timestamp;
    }
}

static int keyscan_diag_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->position >= KEYSCAN_DIAG_KEY_COUNT) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct keyscan_diag_stat *stat = &keyscan_stats[ev->position];

    stat->seen = true;
    stat->pressed = ev->state;
    stat->last_change = ev->timestamp;
    if (ev->state) {
        stat->press_count++;
    } else {
        stat->release_count++;
    }

    keyscan_diag_update_chatter(stat, ev->timestamp, ev->position);

#if KEYSCAN_DIAG_HAS_CHARLIEPLEX
    if (keyscan_line_map[ev->position].valid) {
        const struct keyscan_line_ref *ref = &keyscan_line_map[ev->position];
        keyscan_diag_line_activity[ref->drive]++;
        keyscan_diag_line_activity[ref->sense]++;
    }
#endif

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(keyscan_diagnostics, keyscan_diag_listener);
ZMK_SUBSCRIPTION(keyscan_diagnostics, zmk_position_state_changed);

static const struct zmk_physical_layout *keyscan_diag_active_layout(void) {
    const struct zmk_physical_layout *const *layouts;
    size_t layouts_len = zmk_physical_layouts_get_list(&layouts);
    if (!layouts_len) {
        return NULL;
    }

    int selected = zmk_physical_layouts_get_selected();
    if (selected < 0 || selected >= layouts_len) {
        return layouts[0];
    }

    return layouts[selected];
}

static int keyscan_diag_fill_snapshot(const zmk_keyscan_SnapshotRequest *req,
                                      zmk_keyscan_Response *resp) {
    zmk_keyscan_SnapshotResponse *out = &resp->response_type.snapshot;
    out->keys_count = 0;
    out->lines_count = 0;
    out->chatter_burst_threshold = CHATTER_BURST;
    out->chatter_window_ms = CHATTER_WINDOW_MS;

    const struct zmk_physical_layout *layout = keyscan_diag_active_layout();

#if KEYSCAN_DIAG_HAS_CHARLIEPLEX
    struct {
        uint32_t involved;
        uint32_t chatter_keys;
        uint32_t missing;
    } line_summary[KEYSCAN_DIAG_LINE_COUNT] = {0};
#endif

    for (uint32_t i = 0; i < KEYSCAN_DIAG_KEY_COUNT && out->keys_count < ARRAY_SIZE(out->keys);
         i++) {
        const struct keyscan_diag_stat *stat = &keyscan_stats[i];
        zmk_keyscan_KeyStatus *dest = &out->keys[out->keys_count++];

        *dest = (zmk_keyscan_KeyStatus){
            .position = i,
            .pressed = stat->pressed,
            .press_count = stat->press_count,
            .release_count = stat->release_count,
            .chatter_count = stat->chatter_count,
            .last_change_ms = stat->last_change,
            .never_seen = !stat->seen,
        };

        if (keyscan_line_map[i].valid) {
            dest->line_drive = keyscan_line_map[i].drive;
            dest->line_sense = keyscan_line_map[i].sense;
        } else {
            dest->line_drive = UINT32_MAX;
            dest->line_sense = UINT32_MAX;
        }

        if (layout && i < layout->keys_len) {
            const struct zmk_key_physical_attrs *attrs = &layout->keys[i];
            dest->shape = (zmk_keyscan_PhysicalPosition){
                .x = attrs->x,
                .y = attrs->y,
                .width = attrs->width,
                .height = attrs->height,
            };
        }

#if KEYSCAN_DIAG_HAS_CHARLIEPLEX
        if (keyscan_line_map[i].valid) {
            struct keyscan_line_ref ref = keyscan_line_map[i];
            line_summary[ref.drive].involved++;
            line_summary[ref.sense].involved++;
            if (stat->chatter_count > 0) {
                line_summary[ref.drive].chatter_keys++;
                line_summary[ref.sense].chatter_keys++;
            }
            if (!stat->seen) {
                line_summary[ref.drive].missing++;
                line_summary[ref.sense].missing++;
            }
        }
#endif
    }

#if KEYSCAN_DIAG_HAS_CHARLIEPLEX
    for (uint32_t i = 0; i < KEYSCAN_DIAG_LINE_COUNT && out->lines_count < ARRAY_SIZE(out->lines);
         i++) {
        zmk_keyscan_LineStatus *line = &out->lines[out->lines_count++];
        const struct gpio_dt_spec *spec = &keyscan_diag_lines[i];
        *line = (zmk_keyscan_LineStatus){
            .index = i,
            .pin = spec->pin,
            .activity = keyscan_diag_line_activity[i],
            .involved_keys = line_summary[i].involved,
            .chatter_keys = line_summary[i].chatter_keys,
        };
        if (spec->port && spec->port->name) {
            snprintf(line->port, sizeof(line->port), "%s", spec->port->name);
        }
        line->suspected_fault =
            (line->involved_keys > 0 &&
             (line->activity == 0 || line->chatter_keys > 0 || line_summary[i].missing > 0));
    }
#endif

    if (req && req->reset_counters) {
        keyscan_diag_reset_counters();
    }

    resp->which_response_type = zmk_keyscan_Response_snapshot_tag;
    return 0;
}

static bool encode_const_string(pb_ostream_t *stream, const pb_field_t *field,
                                void *const *arg) {
    const char *str = (const char *)(*arg);
    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }
    return pb_encode_string(stream, (const uint8_t *)str, strlen(str));
}

ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(KEYSCAN_DIAG_SUBSYSTEM, zmk_keyscan_Response);

static bool keyscan_diag_handle_request(const zmk_custom_CallRequest *raw_request,
                                        pb_callback_t *encode_response) {
    zmk_keyscan_Response *resp =
        ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(KEYSCAN_DIAG_SUBSYSTEM,
                                                          encode_response);

    zmk_keyscan_Request req = zmk_keyscan_Request_init_zero;

    pb_istream_t req_stream = pb_istream_from_buffer(raw_request->payload.bytes,
                                                     raw_request->payload.size);
    if (!pb_decode(&req_stream, zmk_keyscan_Request_fields, &req)) {
        LOG_WRN("Failed to decode diagnostics request: %s", PB_GET_ERROR(&req_stream));
        zmk_keyscan_ErrorResponse err = zmk_keyscan_ErrorResponse_init_zero;
        static const char decode_msg[] = "Failed to decode request";
        err.message.funcs.encode = encode_const_string;
        err.message.arg = (void *)decode_msg;
        resp->which_response_type = zmk_keyscan_Response_error_tag;
        resp->response_type.error = err;
        return true;
    }

    int rc = 0;
    switch (req.which_request_type) {
    case zmk_keyscan_Request_snapshot_tag:
        rc = keyscan_diag_fill_snapshot(&req.request_type.snapshot, resp);
        break;
    default:
        LOG_WRN("Unsupported diagnostics request type: %d", req.which_request_type);
        rc = -ENOTSUP;
    }

    if (rc != 0) {
        zmk_keyscan_ErrorResponse err = zmk_keyscan_ErrorResponse_init_zero;
        static const char process_msg[] = "Failed to process request";
        err.message.funcs.encode = encode_const_string;
        err.message.arg = (void *)process_msg;
        resp->which_response_type = zmk_keyscan_Response_error_tag;
        resp->response_type.error = err;
    }

    return true;
}

static struct zmk_rpc_custom_subsystem_meta keyscan_diag_meta = {
    ZMK_RPC_CUSTOM_SUBSYSTEM_UI_URLS(KEYSCAN_DIAG_UI_URL),
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

ZMK_RPC_CUSTOM_SUBSYSTEM(KEYSCAN_DIAG_SUBSYSTEM, &keyscan_diag_meta, keyscan_diag_handle_request);

static int keyscan_diag_init(void) {
    keyscan_diag_build_line_map();
    return 0;
}

SYS_INIT(keyscan_diag_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
