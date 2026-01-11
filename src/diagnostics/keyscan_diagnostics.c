/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zmk/keyscan_diagnostics.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define EVENT_BUFFER_SIZE CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_EVENT_BUFFER_SIZE
#define MAX_CHATTER_STATS 32
#define MAX_GPIO_PINS 16

// Circular buffer for events
static struct keyscan_diag_event event_buffer[EVENT_BUFFER_SIZE];
static uint32_t event_buffer_head = 0;
static uint32_t event_buffer_count = 0;
static uint32_t total_event_count = 0;

// Chattering statistics
static struct keyscan_diag_chatter_stats chatter_stats[MAX_CHATTER_STATS];
static uint32_t chatter_stats_count = 0;

// GPIO pins (populated from device tree)
static struct keyscan_diag_gpio_pin gpio_pins[MAX_GPIO_PINS];
static uint32_t gpio_pins_count = 0;

// Matrix dimensions
static uint32_t matrix_rows = 0;
static uint32_t matrix_cols = 0;

// State
static bool monitoring_active = false;
static uint32_t chatter_threshold_ms = CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTER_THRESHOLD_MS;

// Mutex for thread safety
K_MUTEX_DEFINE(diag_mutex);

/**
 * Find or create chattering stats entry for a key
 */
static struct keyscan_diag_chatter_stats *get_chatter_stats(uint32_t row, uint32_t col) {
    // Look for existing entry
    for (uint32_t i = 0; i < chatter_stats_count; i++) {
        if (chatter_stats[i].row == row && chatter_stats[i].col == col) {
            return &chatter_stats[i];
        }
    }

    // Create new entry if space available
    if (chatter_stats_count < MAX_CHATTER_STATS) {
        struct keyscan_diag_chatter_stats *stats = &chatter_stats[chatter_stats_count++];
        stats->row = row;
        stats->col = col;
        stats->event_count = 0;
        stats->chatter_count = 0;
        stats->last_event_ms = 0;
        stats->min_interval_ms = UINT32_MAX;
        return stats;
    }

    return NULL;
}

/**
 * Add event to circular buffer
 */
static void add_event(uint32_t row, uint32_t col, bool pressed, uint64_t timestamp_ms) {
    event_buffer[event_buffer_head].row = row;
    event_buffer[event_buffer_head].col = col;
    event_buffer[event_buffer_head].pressed = pressed;
    event_buffer[event_buffer_head].timestamp_ms = timestamp_ms;

    event_buffer_head = (event_buffer_head + 1) % EVENT_BUFFER_SIZE;
    if (event_buffer_count < EVENT_BUFFER_SIZE) {
        event_buffer_count++;
    }
    total_event_count++;
}

/**
 * Update chattering statistics
 */
static void update_chatter_stats(uint32_t row, uint32_t col, uint64_t timestamp_ms) {
    struct keyscan_diag_chatter_stats *stats = get_chatter_stats(row, col);
    if (!stats) {
        return;
    }

    stats->event_count++;

    if (stats->last_event_ms > 0) {
        uint32_t interval_ms = (uint32_t)(timestamp_ms - stats->last_event_ms);
        
        // Update minimum interval
        if (interval_ms < stats->min_interval_ms) {
            stats->min_interval_ms = interval_ms;
        }

        // Check for chattering
        if (interval_ms < chatter_threshold_ms) {
            stats->chatter_count++;
            LOG_DBG("Chattering detected at row=%u, col=%u, interval=%u ms", 
                   row, col, interval_ms);
        }
    }

    stats->last_event_ms = timestamp_ms;
}

int keyscan_diagnostics_init(void) {
    k_mutex_lock(&diag_mutex, K_FOREVER);
    
    // Clear all data
    event_buffer_head = 0;
    event_buffer_count = 0;
    total_event_count = 0;
    chatter_stats_count = 0;
    gpio_pins_count = 0;
    monitoring_active = false;

    // Try to get GPIO configuration from device tree
    // This is a simplified version - in a real implementation, we would
    // parse the actual kscan device tree node
    
    // For charlieplex, we'll try to find the kscan device
#if DT_HAS_COMPAT_STATUS_OKAY(zmk_kscan_gpio_charlieplex)
    #define KSCAN_NODE DT_INST(0, zmk_kscan_gpio_charlieplex)
    #if DT_NODE_EXISTS(KSCAN_NODE)
        // Get GPIO count from the gpios property
        gpio_pins_count = DT_PROP_LEN(KSCAN_NODE, gpios);
        if (gpio_pins_count > MAX_GPIO_PINS) {
            gpio_pins_count = MAX_GPIO_PINS;
        }

        // For charlieplex, rows = cols = number of GPIOs
        matrix_rows = gpio_pins_count;
        matrix_cols = gpio_pins_count;

        // Populate GPIO pin information
        #define GET_GPIO_INFO(idx) \
            do { \
                if (idx < gpio_pins_count) { \
                    gpio_pins[idx].pin = DT_GPIO_PIN_BY_IDX(KSCAN_NODE, gpios, idx); \
                    gpio_pins[idx].port_name = DT_LABEL(DT_GPIO_CTLR_BY_IDX(KSCAN_NODE, gpios, idx)); \
                } \
            } while(0)

        // Manually unroll for up to 16 GPIOs (charlieplex typical max)
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 0
        GET_GPIO_INFO(0);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 1
        GET_GPIO_INFO(1);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 2
        GET_GPIO_INFO(2);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 3
        GET_GPIO_INFO(3);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 4
        GET_GPIO_INFO(4);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 5
        GET_GPIO_INFO(5);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 6
        GET_GPIO_INFO(6);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 7
        GET_GPIO_INFO(7);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 8
        GET_GPIO_INFO(8);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 9
        GET_GPIO_INFO(9);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 10
        GET_GPIO_INFO(10);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 11
        GET_GPIO_INFO(11);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 12
        GET_GPIO_INFO(12);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 13
        GET_GPIO_INFO(13);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 14
        GET_GPIO_INFO(14);
        #endif
        #if DT_PROP_LEN(KSCAN_NODE, gpios) > 15
        GET_GPIO_INFO(15);
        #endif

        LOG_INF("Keyscan diagnostics initialized: %u GPIOs, %ux%u matrix", 
               gpio_pins_count, matrix_rows, matrix_cols);
    #endif
#endif

    k_mutex_unlock(&diag_mutex);
    return 0;
}

int keyscan_diagnostics_start(uint32_t chatter_threshold) {
    k_mutex_lock(&diag_mutex, K_FOREVER);
    
    if (chatter_threshold > 0) {
        chatter_threshold_ms = chatter_threshold;
    }
    
    monitoring_active = true;
    LOG_INF("Keyscan diagnostics monitoring started (chatter threshold: %u ms)", 
           chatter_threshold_ms);
    
    k_mutex_unlock(&diag_mutex);
    return 0;
}

int keyscan_diagnostics_stop(void) {
    k_mutex_lock(&diag_mutex, K_FOREVER);
    monitoring_active = false;
    LOG_INF("Keyscan diagnostics monitoring stopped");
    k_mutex_unlock(&diag_mutex);
    return 0;
}

bool keyscan_diagnostics_is_monitoring(void) {
    bool result;
    k_mutex_lock(&diag_mutex, K_FOREVER);
    result = monitoring_active;
    k_mutex_unlock(&diag_mutex);
    return result;
}

uint32_t keyscan_diagnostics_get_total_events(void) {
    uint32_t result;
    k_mutex_lock(&diag_mutex, K_FOREVER);
    result = total_event_count;
    k_mutex_unlock(&diag_mutex);
    return result;
}

int keyscan_diagnostics_get_recent_events(struct keyscan_diag_event *events, 
                                          uint32_t max_count) {
    if (!events || max_count == 0) {
        return 0;
    }

    k_mutex_lock(&diag_mutex, K_FOREVER);
    
    uint32_t count = MIN(event_buffer_count, max_count);
    uint32_t start_idx = (event_buffer_head + EVENT_BUFFER_SIZE - count) % EVENT_BUFFER_SIZE;
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start_idx + i) % EVENT_BUFFER_SIZE;
        events[i] = event_buffer[idx];
    }
    
    k_mutex_unlock(&diag_mutex);
    return count;
}

int keyscan_diagnostics_get_chatter_stats(struct keyscan_diag_chatter_stats *stats,
                                          uint32_t max_count) {
    if (!stats || max_count == 0) {
        return 0;
    }

    k_mutex_lock(&diag_mutex, K_FOREVER);
    
    uint32_t count = MIN(chatter_stats_count, max_count);
    for (uint32_t i = 0; i < count; i++) {
        stats[i] = chatter_stats[i];
    }
    
    k_mutex_unlock(&diag_mutex);
    return count;
}

int keyscan_diagnostics_get_gpio_pins(struct keyscan_diag_gpio_pin *pins,
                                      uint32_t max_count) {
    if (!pins || max_count == 0) {
        return 0;
    }

    k_mutex_lock(&diag_mutex, K_FOREVER);
    
    uint32_t count = MIN(gpio_pins_count, max_count);
    for (uint32_t i = 0; i < count; i++) {
        pins[i] = gpio_pins[i];
    }
    
    k_mutex_unlock(&diag_mutex);
    return count;
}

int keyscan_diagnostics_get_matrix_size(uint32_t *rows, uint32_t *cols) {
    if (!rows || !cols) {
        return -EINVAL;
    }

    k_mutex_lock(&diag_mutex, K_FOREVER);
    *rows = matrix_rows;
    *cols = matrix_cols;
    k_mutex_unlock(&diag_mutex);
    return 0;
}

int keyscan_diagnostics_clear(void) {
    k_mutex_lock(&diag_mutex, K_FOREVER);
    
    event_buffer_head = 0;
    event_buffer_count = 0;
    total_event_count = 0;
    chatter_stats_count = 0;
    
    LOG_INF("Keyscan diagnostics data cleared");
    
    k_mutex_unlock(&diag_mutex);
    return 0;
}

void keyscan_diagnostics_event_callback(uint32_t row, uint32_t col, 
                                       bool pressed, uint64_t timestamp_ms) {
    if (!monitoring_active) {
        return;
    }

    k_mutex_lock(&diag_mutex, K_FOREVER);
    
    add_event(row, col, pressed, timestamp_ms);
    update_chatter_stats(row, col, timestamp_ms);
    
    k_mutex_unlock(&diag_mutex);
}

// Event listener for keycode state changes
static int keyscan_diagnostics_keycode_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (monitoring_active) {
        uint64_t timestamp = k_uptime_get();
        // Note: We don't have direct row/col from keycode events
        // This is a simplified approach - real implementation would need
        // to hook into the kscan driver directly
        LOG_DBG("Key event: state=%d, timestamp=%llu", ev->state, timestamp);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(keyscan_diagnostics, keyscan_diagnostics_keycode_listener);
ZMK_SUBSCRIPTION(keyscan_diagnostics, zmk_keycode_state_changed);

// Initialize on startup
SYS_INIT(keyscan_diagnostics_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
