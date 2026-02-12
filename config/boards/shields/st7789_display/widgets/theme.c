/*
 * Copyright (c) 2019 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * Based on ST7789V sample:
 * Copyright (c) 2019 Marc Reilly
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zmk/display.h>
#include "helpers/display.h"
#include "helpers/settings.h"

static uint8_t current_theme = 0;

static uint16_t theme_font_scale = 4;
static uint16_t theme_font_width = 3;
static uint16_t theme_font_height = 6;
static uint16_t *scaled_bitmap_theme_font;

SlotSide theme_slot_side = SLOT_SIDE_NONE;
static uint16_t theme_x = 17;
static uint16_t theme_y = 127;

static Character int_to_num_char(uint8_t i) {
    switch (i) {
        case 0: return CHAR_0;
        case 1: return CHAR_1;
        case 2: return CHAR_2;
        case 3: return CHAR_3;
        case 4: return CHAR_4;
        case 5: return CHAR_5;
        case 6: return CHAR_6;
        case 7: return CHAR_7;
        case 8: return CHAR_8;
        case 9: return CHAR_9;
    }
    return CHAR_NONE;
}

void print_themes() {
    if (theme_slot_side == SLOT_SIDE_NONE) {
        return;
    }
    Character theme_template[] = {
        CHAR_S,
        CHAR_K,
        CHAR_I,
        CHAR_N,
    };

    uint8_t num = current_theme;
    uint16_t first_num = current_theme / 10;
    uint16_t second_num = current_theme % 10;
    uint16_t theme_x_custom = theme_x + 6;


    uint16_t char_gap_pixels = 2;
    if (num == 0) {
        print_string(scaled_bitmap_theme_font, theme_template, theme_x_custom, theme_y, theme_font_scale, get_theme_font_color(), get_theme_font_bg_color(), FONT_SIZE_3x5, char_gap_pixels, 4);
        print_bitmap(scaled_bitmap_theme_font, CHAR_C, theme_x_custom + 62, theme_y, theme_font_scale, get_theme_font_color_1(), get_theme_font_bg_color(), FONT_SIZE_3x5);
        print_bitmap(scaled_bitmap_theme_font, CHAR_NONE, theme_x_custom + 76, theme_y, theme_font_scale, get_theme_font_color_1(), get_theme_font_bg_color(), FONT_SIZE_3x5);
        return;
    }
    print_string(scaled_bitmap_theme_font, theme_template, theme_x, theme_y, theme_font_scale, get_theme_font_color(), get_theme_font_bg_color(), FONT_SIZE_3x5, char_gap_pixels, 4);
    print_bitmap(scaled_bitmap_theme_font, int_to_num_char(first_num), theme_x + 62, theme_y, theme_font_scale, get_theme_font_color_1(), get_theme_font_bg_color(), FONT_SIZE_3x5);
    print_bitmap(scaled_bitmap_theme_font, int_to_num_char(second_num), theme_x + 76, theme_y, theme_font_scale, get_theme_font_color_1(), get_theme_font_bg_color(), FONT_SIZE_3x5);
}

void set_next_theme_number() {
    current_theme++;
    if (current_theme >= get_themes_colors_len()) {
        current_theme = 0;
    }
}

void set_previous_theme_number() {
    current_theme--;
    if (current_theme < 0) {
        current_theme = get_themes_colors_len() - 1;
    }
}

void set_next_theme() {
    set_next_theme_number();
    int rc = snake_settings_save_current_theme(current_theme);
    if (rc) {
        set_previous_theme_number();
        return;
    }
    apply_current_theme(current_theme);
}

void theme_init() {
    uint16_t theme_font_size = (theme_font_width * theme_font_scale) * (theme_font_height * theme_font_scale);

    scaled_bitmap_theme_font = k_malloc(theme_font_size * 2 * sizeof(uint16_t));

    current_theme = snake_settings_get_current_theme();
    apply_current_theme(current_theme);
    
    theme_slot_side = get_slot_to_print(INFO_SLOT_THEME);
    if (theme_slot_side == SLOT_SIDE_RIGHT) {
        theme_x += 120;
    }
}