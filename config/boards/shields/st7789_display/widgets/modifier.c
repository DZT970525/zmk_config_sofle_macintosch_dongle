/*
 * Based on ST7789V sample:
 * Copyright (c) 2019 Marc Reilly
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(modifier, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/modifiers.h>
#include "helpers/display.h"

static bool modifier_widget_running = false;
static bool modifier_widget_initialized = false;

static uint16_t modifier_font_scale = 2;
static uint16_t modifier_font_width = 11;
static uint16_t modifier_font_height = 11;
static uint16_t *scaled_bitmap_modifier_font;

SlotSide modifier_slot_side = SLOT_SIDE_NONE;
static uint16_t modifier_x = 7;
static uint16_t modifier_y = 120;

struct modifiers_state {
    uint8_t modifiers;
};

static struct modifiers_state modifier_state;

static const uint16_t cmd_bitmap[] = {
    0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1,
    1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0,
};

static const uint16_t option_bitmap[] = {
    1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
};

static const uint16_t ctrl_bitmap[] = {
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint16_t shitf_bitmap[] = {
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0,
    0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0,
};

void print_modifiers() {
    if (modifier_slot_side == SLOT_SIDE_NONE) {
        return;
    }

    if ((modifier_state.modifiers & (MOD_LGUI | MOD_RGUI)) > 0) {
        render_bitmap(scaled_bitmap_modifier_font, cmd_bitmap, modifier_x, modifier_y,
                      modifier_font_width, modifier_font_height, modifier_font_scale,
                      get_modifier_selected_color(), get_modifier_bg_color());
    } else {
        render_bitmap(scaled_bitmap_modifier_font, cmd_bitmap, modifier_x, modifier_y,
                      modifier_font_width, modifier_font_height, modifier_font_scale,
                      get_modifier_unselected_color(), get_modifier_bg_color());
    }

    if ((modifier_state.modifiers & (MOD_LALT | MOD_RALT)) > 0) {
        render_bitmap(scaled_bitmap_modifier_font, option_bitmap, modifier_x + 28, modifier_y,
                      modifier_font_width, modifier_font_height, modifier_font_scale,
                      get_modifier_selected_color(), get_modifier_bg_color());
    } else {
        render_bitmap(scaled_bitmap_modifier_font, option_bitmap, modifier_x + 28, modifier_y,
                      modifier_font_width, modifier_font_height, modifier_font_scale,
                      get_modifier_unselected_color(), get_modifier_bg_color());
    }

    if ((modifier_state.modifiers & (MOD_LCTL | MOD_RCTL)) > 0) {
        render_bitmap(scaled_bitmap_modifier_font, ctrl_bitmap, modifier_x + 56, modifier_y,
                      modifier_font_width, modifier_font_height, modifier_font_scale,
                      get_modifier_selected_color(), get_modifier_bg_color());
    } else {
        render_bitmap(scaled_bitmap_modifier_font, ctrl_bitmap, modifier_x + 56, modifier_y,
                      modifier_font_width, modifier_font_height, modifier_font_scale,
                      get_modifier_unselected_color(), get_modifier_bg_color());
    }

    if ((modifier_state.modifiers & (MOD_LSFT | MOD_RSFT)) > 0) {
        render_bitmap(scaled_bitmap_modifier_font, shitf_bitmap, modifier_x + 84, modifier_y,
                      modifier_font_width, modifier_font_height, modifier_font_scale,
                      get_modifier_selected_color(), get_modifier_bg_color());
    } else {
        render_bitmap(scaled_bitmap_modifier_font, shitf_bitmap, modifier_x + 84, modifier_y,
                      modifier_font_width, modifier_font_height, modifier_font_scale,
                      get_modifier_unselected_color(), get_modifier_bg_color());
    }
}

static struct modifiers_state modifiers_get_state(const zmk_event_t *eh) {
    return (struct modifiers_state){.modifiers = zmk_hid_get_explicit_mods()};
}

void modifiers_update_cb(struct modifiers_state state) {
    modifier_state = state;
    if (modifier_widget_initialized && modifier_widget_running) {
        print_modifiers();
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_modifiers, struct modifiers_state, modifiers_update_cb,
                            modifiers_get_state)

ZMK_SUBSCRIPTION(widget_modifiers, zmk_keycode_state_changed);

void zmk_widget_modifier_init() {
    uint16_t modifier_font_size =
        (modifier_font_width * modifier_font_scale) * (modifier_font_height * modifier_font_scale);
    scaled_bitmap_modifier_font = k_malloc(modifier_font_size * 2 * sizeof(uint16_t));

    modifier_slot_side = get_slot_to_print(INFO_SLOT_MODIFIERS);
    if (modifier_slot_side == SLOT_SIDE_RIGHT) {
        modifier_x += 120;
    }

    widget_modifiers_init();
    modifier_widget_initialized = true;
}

void start_modifier_status() { modifier_widget_running = true; }

void stop_modifier_status() { modifier_widget_running = false; }