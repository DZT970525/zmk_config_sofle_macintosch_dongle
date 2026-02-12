/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

#include "battery_status.h"
#include "helpers/display.h"

static bool battery_widget_initialized = false;
static bool battery_widget_running = false;
static struct peripheral_battery_state battery_state_0;
static struct peripheral_battery_state battery_state_1;
static uint16_t *scaled_bitmap_1;

static uint8_t previous_battery_level_0 = 0;
static uint8_t previous_battery_level_1 = 0;

#ifdef CONFIG_SHOW_SINGLE_BATTERY
static const uint16_t font_offset = 6;
static const uint16_t single_battery_offset = 60;
#else
static const uint16_t font_offset = 2;
#endif

#ifdef CONFIG_USE_BATTERY_FONT_3X5
static const uint16_t scale = 10;
static const uint16_t font_width = 3;
static const uint16_t font_height = 5;
#else
static const uint16_t scale = 6;
static const uint16_t font_width = 5;
static const uint16_t font_height = 8;
#endif

static const uint16_t start_x_peripheral_1 = 12;
static const uint16_t start_x_peripheral_2 = 132;
static const uint16_t start_y = 176;

struct peripheral_battery_state {
    uint8_t source;
    uint8_t level;
};

uint16_t x_position_scaled(uint16_t x, uint16_t index) {
    uint16_t width = index * scale * font_width;
    uint16_t offset = index * font_offset;
    return x + width + offset;
}

void print_percentage(uint8_t digit, uint16_t x, uint16_t y, uint16_t scale, uint16_t num_color,
                      uint16_t bg_color, uint16_t percentage_color) {
    uint16_t first_x = x_position_scaled(x, 0);
    uint16_t second_x = x_position_scaled(x, 1);
    uint16_t third_x = x_position_scaled(x, 2);
    if (digit == 0) {
#ifdef CONFIG_USE_BATTERY_FONT_3X5
        print_bitmap(scaled_bitmap_1, CHAR_DASH, first_x, y, scale, num_color, bg_color,
                     FONT_SIZE_3x5);
        print_bitmap(scaled_bitmap_1, CHAR_DASH, second_x, y, scale, num_color, bg_color,
                     FONT_SIZE_3x5);
        print_bitmap(scaled_bitmap_1, CHAR_PERCENTAGE, third_x + 2, y, scale, percentage_color,
                     bg_color, FONT_SIZE_3x5);
#else
        print_bitmap(scaled_bitmap_1, CHAR_DASH, first_x, y, scale, num_color, bg_color,
                     FONT_SIZE_5x8);
        print_bitmap(scaled_bitmap_1, CHAR_DASH, second_x, y, scale, num_color, bg_color,
                     FONT_SIZE_5x8);
        print_bitmap(scaled_bitmap_1, CHAR_PERCENTAGE, third_x + 2, y, scale, percentage_color,
                     bg_color, FONT_SIZE_5x8);
#endif
        return;
    }

    if (digit > 99) {

#ifdef CONFIG_USE_BATTERY_FONT_3X5
        print_bitmap(scaled_bitmap_1, 1, first_x, y, scale, num_color, bg_color, FONT_SIZE_3x5);
        print_bitmap(scaled_bitmap_1, 0, second_x, y, scale, num_color, bg_color, FONT_SIZE_3x5);
        print_bitmap(scaled_bitmap_1, 0, third_x + 2, y, scale, num_color, bg_color, FONT_SIZE_3x5);
#else
        print_bitmap(scaled_bitmap_1, 1, first_x, y, scale, num_color, bg_color, FONT_SIZE_5x8);
        print_bitmap(scaled_bitmap_1, 0, second_x, y, scale, num_color, bg_color, FONT_SIZE_5x8);
        print_bitmap(scaled_bitmap_1, 0, third_x + 2, y, scale, num_color, bg_color, FONT_SIZE_5x8);
#endif
        return;
    }

    uint16_t first_num = digit / 10;
    uint16_t second_num = digit % 10;

#ifdef CONFIG_USE_BATTERY_FONT_3X5
    print_bitmap(scaled_bitmap_1, first_num, first_x, y, scale, num_color, bg_color, FONT_SIZE_3x5);
    print_bitmap(scaled_bitmap_1, second_num, second_x, y, scale, num_color, bg_color,
                 FONT_SIZE_3x5);
    print_bitmap(scaled_bitmap_1, CHAR_PERCENTAGE, third_x + 2, y, scale, percentage_color,
                 bg_color, FONT_SIZE_3x5);
#else
    print_bitmap(scaled_bitmap_1, first_num, first_x, y, scale, num_color, bg_color, FONT_SIZE_5x8);
    print_bitmap(scaled_bitmap_1, second_num, second_x, y, scale, num_color, bg_color,
                 FONT_SIZE_5x8);
    print_bitmap(scaled_bitmap_1, CHAR_PERCENTAGE, third_x + 2, y, scale, percentage_color,
                 bg_color, FONT_SIZE_5x8);
#endif
}

void set_battery_symbol() {
#ifdef CONFIG_SHOW_SINGLE_BATTERY
    print_percentage(battery_state_0.level, start_x_peripheral_1 + single_battery_offset, start_y,
                     scale, get_battery_num_color(), get_battery_bg_color(),
                     get_battery_percentage_color());
#else
    print_percentage(battery_state_0.level, start_x_peripheral_1, start_y, scale,
                     get_battery_num_color(), get_battery_bg_color(),
                     get_battery_percentage_color());
    print_percentage(battery_state_1.level, start_x_peripheral_2, start_y, scale,
                     get_battery_num_color_1(), get_battery_bg_color_1(),
                     get_battery_percentage_color_1());
#endif
}

void battery_status_update_cb(struct peripheral_battery_state state) {
    if (state.source == 0) {
        battery_state_0 = state;
    } else {
        battery_state_1 = state;
    }
    if (battery_widget_initialized) {
    }
    if (battery_widget_running) {
        set_battery_symbol();
    }
}

static struct peripheral_battery_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    return (struct peripheral_battery_state){
        .source = ev->source,
        .level = ev->state_of_charge,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct peripheral_battery_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_peripheral_battery_state_changed);

void print_empty_batteries() {
#ifdef CONFIG_SHOW_SINGLE_BATTERY
    print_percentage(0, start_x_peripheral_1 + single_battery_offset, start_y, scale,
                     get_battery_num_color(), get_battery_bg_color(),
                     get_battery_percentage_color());
#else
    print_percentage(0, start_x_peripheral_1, start_y, scale, get_battery_num_color(),
                     get_battery_bg_color(), get_battery_percentage_color());
    print_percentage(0, start_x_peripheral_2, start_y, scale, get_battery_num_color_1(),
                     get_battery_bg_color_1(), get_battery_percentage_color_1());
#endif
}

void zmk_widget_peripheral_battery_status_init() {
    uint16_t bitmap_size = (font_width * scale) * (font_height * scale);

    scaled_bitmap_1 = k_malloc(bitmap_size * 2 * sizeof(uint16_t));

    widget_battery_status_init();
}

void initialize_battery_status() { battery_widget_initialized = true; }

void start_battery_status() {
    print_empty_batteries();
    battery_widget_running = true;
}

void stop_battery_status(void) { battery_widget_running = false; }