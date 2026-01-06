/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Key event structure for diagnostics
 */
struct keyscan_diag_event {
    uint32_t row;
    uint32_t col;
    bool pressed;
    uint64_t timestamp_ms;
};

/**
 * Chattering statistics for a key
 */
struct keyscan_diag_chatter_stats {
    uint32_t row;
    uint32_t col;
    uint32_t event_count;
    uint32_t chatter_count;
    uint64_t last_event_ms;
    uint32_t min_interval_ms;
};

/**
 * GPIO pin information
 */
struct keyscan_diag_gpio_pin {
    uint32_t pin;
    const char *port_name;
};

/**
 * Initialize keyscan diagnostics module
 * @return 0 on success, negative errno on failure
 */
int keyscan_diagnostics_init(void);

/**
 * Start monitoring keyscan events
 * @param chatter_threshold_ms Threshold for chattering detection in milliseconds
 * @return 0 on success, negative errno on failure
 */
int keyscan_diagnostics_start(uint32_t chatter_threshold_ms);

/**
 * Stop monitoring keyscan events
 * @return 0 on success, negative errno on failure
 */
int keyscan_diagnostics_stop(void);

/**
 * Check if monitoring is active
 * @return true if monitoring, false otherwise
 */
bool keyscan_diagnostics_is_monitoring(void);

/**
 * Get total number of events recorded
 * @return Total event count
 */
uint32_t keyscan_diagnostics_get_total_events(void);

/**
 * Get recent events
 * @param events Output buffer for events
 * @param max_count Maximum number of events to retrieve
 * @return Number of events copied
 */
int keyscan_diagnostics_get_recent_events(struct keyscan_diag_event *events, 
                                          uint32_t max_count);

/**
 * Get chattering statistics
 * @param stats Output buffer for statistics
 * @param max_count Maximum number of stats entries to retrieve
 * @return Number of stats entries copied
 */
int keyscan_diagnostics_get_chatter_stats(struct keyscan_diag_chatter_stats *stats,
                                          uint32_t max_count);

/**
 * Get GPIO pin configuration
 * @param pins Output buffer for GPIO pins
 * @param max_count Maximum number of pins to retrieve
 * @return Number of pins copied
 */
int keyscan_diagnostics_get_gpio_pins(struct keyscan_diag_gpio_pin *pins,
                                      uint32_t max_count);

/**
 * Get matrix dimensions
 * @param rows Output for number of rows
 * @param cols Output for number of columns
 * @return 0 on success, negative errno on failure
 */
int keyscan_diagnostics_get_matrix_size(uint32_t *rows, uint32_t *cols);

/**
 * Clear all diagnostic data
 * @return 0 on success, negative errno on failure
 */
int keyscan_diagnostics_clear(void);

/**
 * Callback for key events (called by kscan driver wrapper)
 * Internal use only
 */
void keyscan_diagnostics_event_callback(uint32_t row, uint32_t col, 
                                       bool pressed, uint64_t timestamp_ms);

#ifdef __cplusplus
}
#endif
