/*
 * Based on ST7789V sample:
 * Copyright (c) 2019 Marc Reilly
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wpm, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>
#include "helpers/display.h"

static bool wpm_widget_running = false;
static bool wpm_widget_initialized = false;

static uint16_t wpm_font_scale = 4;
static uint16_t wpm_font_width = 3;
static uint16_t wpm_font_height = 5;
static uint16_t *scaled_bitmap_wpm_font;

SlotSide wpm_slot_side = SLOT_SIDE_NONE;
static uint16_t wpm_x = 14;
static uint16_t wpm_y = 121;

struct wpm_speed_state {
    uint8_t wpm;
};

static struct wpm_speed_state wpm_speed;

void print_wpm() {
    if (wpm_slot_side == SLOT_SIDE_NONE) {
        return;
    }
    uint8_t wpm_string_size = 3;
    uint8_t gap = 2;

    uint8_t speed = wpm_speed.wpm;

    uint16_t num_1 = speed / 100;
    uint16_t num_2 = (speed % 100) / 10;
    uint16_t num_3 = speed % 10;

    uint8_t char_len = 14;

    print_bitmap(scaled_bitmap_wpm_font, CHAR_W, wpm_x, wpm_y, wpm_font_scale, get_wpm_font_color(),
                 get_wpm_font_bg_color(), FONT_SIZE_3x5);
    print_bitmap(scaled_bitmap_wpm_font, CHAR_P, wpm_x + (char_len * 1), wpm_y, wpm_font_scale,
                 get_wpm_font_color(), get_wpm_font_bg_color(), FONT_SIZE_3x5);
    print_bitmap(scaled_bitmap_wpm_font, CHAR_M, wpm_x + (char_len * 2), wpm_y, wpm_font_scale,
                 get_wpm_font_color(), get_wpm_font_bg_color(), FONT_SIZE_3x5);
    print_bitmap(scaled_bitmap_wpm_font, CHAR_COLON, wpm_x + (char_len * 3), wpm_y, wpm_font_scale,
                 get_wpm_font_color(), get_wpm_font_bg_color(), FONT_SIZE_3x5);

    print_bitmap(scaled_bitmap_wpm_font, num_1, wpm_x + (char_len * 4) - 2, wpm_y, wpm_font_scale,
                 get_wpm_font_1_color(), get_wpm_font_bg_color(), FONT_SIZE_3x5);
    print_bitmap(scaled_bitmap_wpm_font, num_2, wpm_x + (char_len * 5) - 2, wpm_y, wpm_font_scale,
                 get_wpm_font_1_color(), get_wpm_font_bg_color(), FONT_SIZE_3x5);
    print_bitmap(scaled_bitmap_wpm_font, num_3, wpm_x + (char_len * 6) - 2, wpm_y, wpm_font_scale,
                 get_wpm_font_1_color(), get_wpm_font_bg_color(), FONT_SIZE_3x5);
}

struct wpm_speed_state wpm_speed_get_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct wpm_speed_state){.wpm = ev->state};
}

void wpm_speed_update_cb(struct wpm_speed_state state) {
    wpm_speed = state;
    if (wpm_widget_initialized && wpm_widget_running) {
        print_wpm();
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm, struct wpm_speed_state, wpm_speed_update_cb,
                            wpm_speed_get_state)

ZMK_SUBSCRIPTION(widget_wpm, zmk_wpm_state_changed);

void zmk_widget_wpm_init() {
    uint16_t wpm_font_size = (wpm_font_width * wpm_font_scale) * (wpm_font_height * wpm_font_scale);
    scaled_bitmap_wpm_font = k_malloc(wpm_font_size * 2 * sizeof(uint16_t));

    wpm_slot_side = get_slot_to_print(INFO_SLOT_WPM);
    if (wpm_slot_side == SLOT_SIDE_RIGHT) {
        wpm_x += 120;
    }

    widget_wpm_init();
    wpm_widget_initialized = true;
}

void start_wpm_status() { wpm_widget_running = true; }

void stop_wpm_status() { wpm_widget_running = false; }