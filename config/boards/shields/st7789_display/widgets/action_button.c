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
#include <drivers/behavior.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zmk/display.h>
#include <zmk/display/widgets/layer_status.h>
// #include <zmk_dongle_events/dongle_action_event.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/display.h>

#include "action_button.h"
#include "splash.h"
#include "snake.h"
#include "output_status.h"
#include "battery_status.h"
#include "layer_status.h"
#include "helpers/display.h"
#include "helpers/buzzer.h"
#include "helpers/settings.h"
#include "theme.h"
#include "wpm.h"
// #include "snake_image.h"
#include "logo.h"
#include <stdint.h>

static uint8_t *buf_frame;
static uint16_t *title_font_buf;
static uint16_t menu_threshold = 0;
static uint16_t theme_threshold = 300;
static uint16_t mute_threshold = 600;
static int64_t pressed_timestamp = 0;
static int64_t released_timestamp = 0;

static bool action_button_initialized = false;
static struct layer_status_state ls_state;

static bool menu_on = false;
static bool dongle_lock = false;

static const uint8_t base_layer = 0;
static const uint8_t menu_layer = 4;
static const uint8_t theme_layer = 5;
static const uint8_t mute_layer = 6;

struct layer_status_state {
    uint8_t index;
    const char *label;
};

void set_theme_threshold(uint16_t term_ms) { theme_threshold = term_ms; }

void set_mute_threshold(uint16_t term_ms) { mute_threshold = term_ms; }

void print_container(uint8_t *buf_frame, uint16_t start_x, uint16_t end_x, uint16_t start_y,
                     uint16_t end_y, uint16_t scale) {
    print_rectangle(buf_frame, start_x, end_x - scale, start_y, end_y - scale, get_frame_color(),
                    scale);
    print_rectangle(buf_frame, start_x + scale, end_x - (scale * 2), start_y + scale,
                    end_y - (scale * 2), get_frame_color_1(), scale);
}

static void apply_macintosh_status_colors(void) {
    const uint32_t black = 0x000000;
    const uint32_t white = 0xFFFFFF;
    const uint32_t gray = 0x808080;

    set_menu_bg_color(white);
    set_frame_color(black);
    set_frame_color_1(gray);
    set_layer_font_color(black);
    set_layer_font_bg_color(white);
    set_wpm_font_color(black);
    set_wpm_font_1_color(black);
    set_wpm_font_bg_color(white);
    set_symbol_selected_color(black);
    set_symbol_unselected_color(gray);
    set_symbol_bg_color(white);
    set_bt_num_color(black);
    set_bt_bg_color(white);
    set_bt_status_ok_color(black);
    set_bt_status_not_ok_color(black);
    set_bt_status_open_color(gray);
    set_bt_status_bg_color(white);
    set_battery_num_color(0x008FCB);
    set_battery_percentage_color(0xFF8C00);
    set_battery_bg_color(white);
    set_battery_num_color_1(0x008FCB);
    set_battery_percentage_color_1(0xFF8C00);
    set_battery_bg_color_1(white);
    set_modifier_selected_color(black);
    set_modifier_unselected_color(gray);
    set_modifier_bg_color(white);
    set_theme_font_color(black);
    set_theme_font_color_1(black);
    set_theme_font_bg_color(white);
}

/* Bold 7 x 9 Chicago-style glyphs for the Macintosh title bar. */
static const uint16_t mac_title_s[] = {
    0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1,
    1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0,
};
static const uint16_t mac_title_o[] = {
    0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0,
    1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0,
};
static const uint16_t mac_title_f[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0,
};
static const uint16_t mac_title_l[] = {
    1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0,
    0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t mac_title_e[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

static void print_macintosh_title(void) {
    const uint16_t *glyphs[] = {
        mac_title_s, mac_title_o, mac_title_f, mac_title_l, mac_title_e,
    };

    /* Scale 7 x 9 to 10 x 13: midway between the previous 7 x 9 and 14 x 18 titles. */
    uint16_t scaled_glyph[10 * 13];

    for (uint8_t i = 0; i < ARRAY_SIZE(glyphs); i++) {
        for (uint8_t y = 0; y < 13; y++) {
            for (uint8_t x = 0; x < 10; x++) {
                scaled_glyph[(y * 10) + x] = glyphs[i][((y * 9) / 13) * 7 + ((x * 7) / 10)];
            }
        }

        render_bitmap(title_font_buf, scaled_glyph, 91 + (i * 12), 14, 10, 13, 1, get_frame_color(),
                      get_menu_bg_color());
    }
}

void print_frames() {
    const uint16_t black = get_frame_color();
    const uint16_t white = get_menu_bg_color();
    const uint16_t gray = get_frame_color_1();

    /* 240 x 240 classic Macintosh dialog, with all coordinates kept in bounds. */
    print_filled_screen_area(6, 8, 226, 224, white);
    print_rectangle(buf_frame, 6, 230, 8, 230, black, 2);
    print_rectangle(buf_frame, 9, 227, 11, 227, black, 1);

    /* Five uniform stripes, inset from both the top border and lower divider. */
    for (uint16_t y = 14; y <= 26; y += 3) {
        print_filled_screen_area(12, y, 8, 2, gray);
        print_filled_screen_area(40, y, 47, 2, gray);
        print_filled_screen_area(153, y, 75, 2, gray);
    }
    print_filled_screen_area(89, 12, 62, 18, white);

    print_macintosh_title();
    /* Classic Mac title-bar close box, with a clear gap on either side. */
    print_filled_screen_area(23, 14, 14, 14, black);
    print_filled_screen_area(25, 16, 10, 10, white);

    /* Information panes: layer, output selector, and the two keyboard halves. */
    print_filled_screen_area(10, 30, 218, 1, black);
    print_filled_screen_area(10, 104, 218, 1, black);
    print_filled_screen_area(10, 160, 218, 1, black);
    print_filled_screen_area(10, 199, 218, 1, black);
#ifndef CONFIG_SHOW_SINGLE_BATTERY
    print_filled_screen_area(119, 160, 1, 39, black);
#endif
}

void print_menu() {
    invalidate_splash();
    apply_macintosh_status_colors();
    print_checkerboard_screen(get_menu_bg_color(), get_frame_color());
    stop_animation();
    print_frames();
    start_battery_status();
    start_output_status();
    start_modifier_status();
    start_layer_status();
    set_battery_symbol();
    print_layer();
    print_themes();
    print_modifiers();
}

void toggle_menu() {
#ifdef CONFIG_USE_BUZZER
#ifdef CONFIG_USE_MENU_SOUND
    play_notification_song();
#endif
#endif
    if (menu_on) {
        stop_modifier_status();
        stop_output_status();
        stop_battery_status();
        stop_animation();
        stop_layer_status();
        start_snake();
        menu_on = false;
    } else {
        stop_snake();
        print_menu();
        menu_on = true;
    }
}

void change_theme() {
    set_next_theme();
#ifdef CONFIG_USE_BUZZER
#ifdef CONFIG_USE_THEME_SOUND
    play_startup_song();
#endif
#endif
    if (menu_on) {
        print_menu();
        apply_theme_snake();
    } else {
        stop_snake();
        apply_theme_snake();
        start_snake();
    }
}

void set_layer_symbol() {
    if (dongle_lock) {
        return;
    }
    dongle_lock = true;
    if (ls_state.index == menu_layer) {
        toggle_menu();
    }
    if (ls_state.index == theme_layer) {
        change_theme();
    }
    if (ls_state.index == mute_layer) {
#ifdef CONFIG_USE_BUZZER
        snake_settings_toggle_mute();
        if (!snake_settings_get_mute()) {
            play_once(coin);
        }
#endif
    }
    dongle_lock = false;
}

void zmk_widget_action_button_init() {
    // dongle_action_init();

    /* A 240 px wide, 3 px thick RGB565 line is the largest frame write. */
    buf_frame = k_malloc(240 * 3 * 2);
    title_font_buf = k_malloc(7 * 2 * 9 * 2 * sizeof(uint16_t));
}

void start_action_button(bool is_menu_on) {
    menu_on = is_menu_on;
    action_button_initialized = true;
}
