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

struct layer_status_state {
    uint8_t index;
    const char *label;
};

static bool layer_widget_running = false;
static struct layer_status_state current_layer;
static uint16_t *scaled_bitmap_layer_font;
/* Shared scratch space avoids placing a 1.2 KB glyph buffer on the display thread stack. */
static uint16_t layer_glyph_bitmap[(7 * 3 + 1) * (9 * 3 + 1)];
/* -1 highlights the up button, +1 highlights the down button. */
static int8_t layer_scroll_direction = 0;
static struct k_work_delayable layer_scroll_reset_work;

static const uint16_t scroll_arrow_up[] = {
    0,0,0,0,1,0,0,0,0,
    0,0,0,1,1,1,0,0,0,
    0,0,1,1,1,1,1,0,0,
    0,1,1,1,1,1,1,1,0,
    0,0,0,1,1,1,0,0,0,
    0,0,0,1,1,1,0,0,0,
    0,0,0,1,1,1,0,0,0,
};

static const uint16_t scroll_arrow_down[] = {
    0,0,0,1,1,1,0,0,0,
    0,0,0,1,1,1,0,0,0,
    0,0,0,1,1,1,0,0,0,
    0,1,1,1,1,1,1,1,0,
    0,0,1,1,1,1,1,0,0,
    0,0,0,1,1,1,0,0,0,
    0,0,0,0,1,0,0,0,0,
};

/* Complete A-Z clean sans-serif font for every possible English layer name. */
static const char mac_layer_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static const uint8_t mac_layer_rows[][9] = {
    {0x1C, 0x22, 0x41, 0x41, 0x7F, 0x41, 0x41, 0x41, 0x41}, /* A */
    {0x7E, 0x41, 0x41, 0x41, 0x7E, 0x41, 0x41, 0x41, 0x7E}, /* B */
    {0x3F, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x3F}, /* C */
    {0x7C, 0x42, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42, 0x7C}, /* D */
    {0x7F, 0x40, 0x40, 0x40, 0x7E, 0x40, 0x40, 0x40, 0x7F}, /* E */
    {0x7F, 0x40, 0x40, 0x40, 0x7E, 0x40, 0x40, 0x40, 0x40}, /* F */
    {0x3E, 0x41, 0x40, 0x40, 0x4F, 0x41, 0x41, 0x41, 0x3E}, /* G */
    {0x41, 0x41, 0x41, 0x41, 0x7F, 0x41, 0x41, 0x41, 0x41}, /* H */
    {0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x7F}, /* I */
    {0x0F, 0x02, 0x02, 0x02, 0x02, 0x02, 0x42, 0x42, 0x3C}, /* J */
    {0x41, 0x42, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x41}, /* K */
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7F}, /* L */
    {0x41, 0x63, 0x55, 0x49, 0x49, 0x41, 0x41, 0x41, 0x41}, /* M */
    {0x41, 0x61, 0x51, 0x49, 0x45, 0x43, 0x41, 0x41, 0x41}, /* N */
    {0x3E, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3E}, /* O */
    {0x7E, 0x41, 0x41, 0x41, 0x7E, 0x40, 0x40, 0x40, 0x40}, /* P */
    {0x3E, 0x41, 0x41, 0x41, 0x41, 0x41, 0x45, 0x42, 0x3D}, /* Q */
    {0x7E, 0x41, 0x41, 0x41, 0x7E, 0x48, 0x44, 0x42, 0x41}, /* R */
    {0x3F, 0x40, 0x40, 0x40, 0x3E, 0x01, 0x01, 0x01, 0x7E}, /* S */
    {0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, /* T */
    {0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3E}, /* U */
    {0x41, 0x41, 0x41, 0x41, 0x41, 0x22, 0x22, 0x14, 0x08}, /* V */
    {0x41, 0x41, 0x41, 0x41, 0x49, 0x49, 0x55, 0x55, 0x22}, /* W */
    {0x41, 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x41}, /* X */
    {0x41, 0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x08}, /* Y */
    {0x7F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7F}, /* Z */
    {0x3E, 0x41, 0x43, 0x45, 0x49, 0x51, 0x61, 0x41, 0x3E}, /* 0 */
    {0x08, 0x18, 0x28, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3E}, /* 1 */
    {0x3E, 0x41, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x7F}, /* 2 */
    {0x3E, 0x41, 0x01, 0x0E, 0x01, 0x01, 0x01, 0x41, 0x3E}, /* 3 */
    {0x06, 0x0A, 0x12, 0x22, 0x42, 0x7F, 0x02, 0x02, 0x02}, /* 4 */
    {0x7F, 0x40, 0x40, 0x7E, 0x01, 0x01, 0x01, 0x41, 0x3E}, /* 5 */
    {0x3E, 0x40, 0x40, 0x7E, 0x41, 0x41, 0x41, 0x41, 0x3E}, /* 6 */
    {0x7F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x10}, /* 7 */
    {0x3E, 0x41, 0x41, 0x41, 0x3E, 0x41, 0x41, 0x41, 0x3E}, /* 8 */
    {0x3E, 0x41, 0x41, 0x41, 0x3F, 0x01, 0x01, 0x01, 0x3E}, /* 9 */
};

static bool make_mac_layer_glyph(char c, uint8_t factor, uint16_t *bitmap) {
    if (c >= 'a' && c <= 'z') {
        c -= 'a' - 'A';
    }

    for (uint8_t glyph = 0; glyph < sizeof(mac_layer_chars) - 1; glyph++) {
        if (mac_layer_chars[glyph] != c) {
            continue;
        }
        uint8_t width = (7 * factor) + 1;
        uint8_t height = (9 * factor) + 1;
        memset(bitmap, 0, width * height * sizeof(uint16_t));

        for (uint8_t source_y = 0; source_y < 9; source_y++) {
            for (uint8_t source_x = 0; source_x < 7; source_x++) {
                if (((mac_layer_rows[glyph][source_y] >> (6 - source_x)) & 1) == 0) {
                    continue;
                }
                /* One extra output pixel makes the strokes subtly heavier. */
                for (uint8_t dy = 0; dy <= factor; dy++) {
                    for (uint8_t dx = 0; dx <= factor; dx++) {
                        bitmap[((source_y * factor + dy) * width) +
                               (source_x * factor + dx)] = 1;
                    }
                }
            }
        }
        return true;
    }
    return false;
}

void print_layer_font_text(uint16_t *render_buffer, const char *text, uint8_t length, uint16_t x,
                           uint16_t y, uint8_t factor, uint16_t color, uint16_t bg_color) {
    factor = CLAMP(factor, 1, 3);
    uint16_t glyph_width = (7 * factor) + 1;
    uint16_t glyph_height = (9 * factor) + 1;
    uint16_t gap = factor + 1;
    for (uint8_t i = 0; i < length; i++) {
        if (make_mac_layer_glyph(text[i], factor, layer_glyph_bitmap)) {
            render_bitmap(render_buffer, layer_glyph_bitmap, x + i * (glyph_width + gap), y,
                          glyph_width, glyph_height, 1, color, bg_color);
        }
    }
}

static void print_scroll_button(uint16_t x, uint16_t y, const uint16_t arrow[], bool selected) {
    const uint16_t black = get_layer_font_color();
    const uint16_t white = get_layer_font_bg_color();
    const uint16_t foreground = selected ? white : black;
    const uint16_t background = selected ? black : white;

    print_filled_screen_area(x, y, 14, 20, black);
    print_filled_screen_area(x + 1, y + 1, 12, 18, background);
    render_bitmap(scaled_bitmap_layer_font, (uint16_t *)arrow, x + 2, y + 6, 9, 7, 1,
                  foreground, background);
}

static void print_layer_scrollbar_at(uint16_t x) {
    const uint16_t black = get_layer_font_color();
    const uint16_t white = get_layer_font_bg_color();

    print_scroll_button(x, 32, scroll_arrow_up, layer_scroll_direction < 0);

    /* Recessed blank track between the two classic square arrow buttons. */
    print_filled_screen_area(x, 52, 14, 31, black);
    print_filled_screen_area(x + 1, 53, 12, 29, white);

    print_scroll_button(x, 83, scroll_arrow_down, layer_scroll_direction > 0);
}

static void print_layer_scrollbars(void) {
    print_layer_scrollbar_at(12);
    print_layer_scrollbar_at(212);
}

static void print_layer_scroll_buttons(void) {
    print_scroll_button(12, 32, scroll_arrow_up, layer_scroll_direction < 0);
    print_scroll_button(12, 83, scroll_arrow_down, layer_scroll_direction > 0);
    print_scroll_button(212, 32, scroll_arrow_up, layer_scroll_direction < 0);
    print_scroll_button(212, 83, scroll_arrow_down, layer_scroll_direction > 0);
}

static void layer_scroll_reset_handler(struct k_work *work) {
    ARG_UNUSED(work);
    layer_scroll_direction = 0;
    if (layer_widget_running) {
        print_layer_scroll_buttons();
    }
}

static void print_layer_label(void) {
    if (current_layer.label == NULL) {
        return;
    }

    const uint16_t pane_x = 11;
    const uint16_t pane_y = 31;
    const uint16_t pane_width = 216;
    const uint16_t pane_height = 73;
    /* Keep the label centered in the complete pane, not in the space left of the scrollbar. */
    const uint16_t max_text_width = 166;
    size_t len = strlen(current_layer.label);
    uint8_t factor = len <= 8 ? 3 : (len <= 13 ? 2 : 1);
    uint16_t glyph_width = (7 * factor) + 1;
    uint16_t glyph_height = (9 * factor) + 1;
    uint16_t gap = factor + 1;
    uint16_t total_width = len > 0 ? (len * glyph_width) + ((len - 1) * gap) : 0;

    while (total_width > max_text_width && factor > 1) {
        factor--;
        glyph_width = (7 * factor) + 1;
        glyph_height = (9 * factor) + 1;
        gap = factor + 1;
        total_width = (len * glyph_width) + ((len - 1) * gap);
    }

    /* Clear only the label area; both scrollbars and their tracks stay untouched. */
    print_filled_screen_area(27, pane_y, 185, pane_height, get_layer_font_bg_color());

    uint16_t x = pane_x + (pane_width - MIN(total_width, pane_width)) / 2;
    uint16_t y = pane_y + (pane_height - glyph_height) / 2;
    for (size_t i = 0; i < len && x + glyph_width <= 212; i++) {
        if (make_mac_layer_glyph(current_layer.label[i], factor, layer_glyph_bitmap)) {
            render_bitmap(scaled_bitmap_layer_font, layer_glyph_bitmap, x, y, glyph_width,
                          glyph_height, 1,
                          get_layer_font_color(), get_layer_font_bg_color());
        }
        x += glyph_width + gap;
    }

}

void print_layer() {
    if (current_layer.label == NULL) {
        return;
    }

    /* Full draw is reserved for initial screen creation and theme changes. */
    print_filled_screen_area(11, 31, 216, 73, get_layer_font_bg_color());
    print_layer_label();
    print_layer_scrollbars();
}

static void layer_status_update_cb(struct layer_status_state state) {
    bool direction_changed = false;
    if (state.index > current_layer.index) {
        layer_scroll_direction = 1;
        direction_changed = true;
    } else if (state.index < current_layer.index) {
        layer_scroll_direction = -1;
        direction_changed = true;
    }
    current_layer = state;
    if (layer_widget_running) {
        print_layer_label();
        if (direction_changed) {
            print_layer_scroll_buttons();
        }
        if (direction_changed) {
            k_work_reschedule(&layer_scroll_reset_work, K_MSEC(1000));
        }
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
    scaled_bitmap_layer_font = k_malloc((7 * 3 + 1) * (9 * 3 + 1) * sizeof(uint16_t));
    k_work_init_delayable(&layer_scroll_reset_work, layer_scroll_reset_handler);
    widget_layer_status_init();
}

void start_layer_status() {
    layer_scroll_direction = 0;
    layer_widget_running = true;
}

void stop_layer_status() {
    layer_widget_running = false;
    layer_scroll_direction = 0;
    k_work_cancel_delayable(&layer_scroll_reset_work);
}
