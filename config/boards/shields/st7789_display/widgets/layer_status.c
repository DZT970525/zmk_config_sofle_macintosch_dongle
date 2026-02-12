/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include "helpers/display.h"
#include <stdio.h>
#include <string.h>

static bool layer_widget_running = false;

static struct layer_status_state current_layer;
static struct layer_status_state last_printed_layer;

static uint16_t layer_font_scale = 8;
static uint16_t layer_font_width = 3;
static uint16_t layer_font_height = 5;
static uint16_t *scaled_bitmap_layer_font;

SlotSide layer_slot_side = SLOT_SIDE_NONE;
static uint16_t layer_x = 120;
static uint16_t layer_x_end = 120;
static uint16_t layer_y = 30;
static uint8_t label_limit = 100;

typedef enum {
    FONT_1,
    FONT_2,
    FONT_3,
    FONT_4,
    FONT_5,
} LAYER_FONT;

struct layer_status_state {
    uint8_t index;
    const char *label;
};

LAYER_FONT get_font(size_t label_len) {

    if (label_len <= 16) {
        return FONT_5;
    }
    if (label_len <= 8) {
        return FONT_4;
    }
    if (label_len > 8 && label_len <= 11) {
        return FONT_3;
    }
    if (label_len > 11 && label_len <= 16) {
        return FONT_2;
    }
    if (label_len > 16 && label_len <= 28) {
        return FONT_1;
    }
    return FONT_1;
}

uint16_t get_y(size_t label_len) {
    switch (get_font(label_len)) {
    case FONT_5:
        return layer_y;
    case FONT_4:
        return layer_y;
    case FONT_3:
        return layer_y + 3;
    case FONT_2:
        return layer_y + 5;
    case FONT_1:
        return layer_y + 8;
    default:
        return layer_y + 8;
    }
}

uint16_t get_scale(size_t label_len) {
    switch (get_font(label_len)) {
    case FONT_5:
        return 8;
    case FONT_4:
        return 4;
    case FONT_3:
        return 3;
    case FONT_2:
        return 2;
    case FONT_1:
        return 1;
    default:
        return 1;
    }
}

uint16_t get_gap(size_t label_len) {
    switch (get_font(label_len)) {
    case FONT_5:
        return 4;
    case FONT_4:
        return 2;
    case FONT_3:
        return 1;
    case FONT_2:
        return 1;
    case FONT_1:
        return 1;
    default:
        return 1;
    }
}

uint16_t get_x(size_t label_len) {
    uint16_t available_pixels = layer_x_end - layer_x; // 112
    uint16_t total_width =
        ((get_scale(label_len) * layer_font_width) + get_gap(label_len)) * label_len;
    uint16_t offset = (available_pixels - total_width) / 2;
    return layer_x + offset;
}

void clear_last_printed_label() {
    size_t len = strlen(last_printed_layer.label);
    uint16_t x = get_x(len);
    uint16_t y = get_y(len);
    uint16_t gap = get_gap(len);
    uint16_t scale = get_scale(len);
    print_repeat_char(scaled_bitmap_layer_font, CHAR_NONE, x, y, scale, get_layer_font_color(),
                      get_layer_font_bg_color(), FONT_SIZE_3x5, gap, len, label_limit);
}

void print_layer() {
    /*if (layer_slot_side == SLOT_SIDE_NONE) {
        return;
    } */
    clear_last_printed_label();

    size_t len = strlen(current_layer.label);
    uint16_t x = get_x(len);
    uint16_t y = get_y(len);
    uint16_t gap = get_gap(len);
    ;
    uint16_t scale = get_scale(len);
    print_char_array(scaled_bitmap_layer_font, current_layer.label, x, y, scale,
                     get_layer_font_color(), get_layer_font_bg_color(), FONT_SIZE_3x5, gap, len,
                     label_limit);

    last_printed_layer = current_layer;
}

static void layer_status_update_cb(struct layer_status_state state) {
    current_layer = state;
    if (layer_widget_running) {
        print_layer();
    }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

void zmk_widget_layer_init() {
    uint16_t layer_font_size =
        (layer_font_width * layer_font_scale) * (layer_font_height * layer_font_scale);
    scaled_bitmap_layer_font = k_malloc(layer_font_size * 2 * sizeof(uint16_t));
    last_printed_layer = (struct layer_status_state){.index = 0, .label = '\0'};

    /* layer_slot_side = get_slot_to_print(INFO_SLOT_LAYER);
     if (layer_slot_side == SLOT_SIDE_RIGHT) {
         layer_x += 120;
         layer_x_end += 120;
     }
 */
    widget_layer_status_init();
}

void start_layer_status() { layer_widget_running = true; }

void stop_layer_status() { layer_widget_running = false; }