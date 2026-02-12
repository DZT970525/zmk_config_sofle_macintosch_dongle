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

void print_frames() {
    uint16_t thickness = 3;
    // logo frame
    print_container(buf_frame, 0, 240, 0, 101, thickness);

    // status frames
    print_container(buf_frame, 0, 120, 100, 161, thickness);

    // theme frames
    print_container(buf_frame, 120, 240, 100, 161, thickness);

// battery frames
#ifdef CONFIG_SHOW_SINGLE_BATTERY
    print_container(buf_frame, 0, 240, 160, 240, thickness);
#else
    print_container(buf_frame, 0, 120, 160, 240, thickness);
    print_container(buf_frame, 120, 240, 160, 240, thickness);
#endif
}

void print_menu() {
    clear_screen();
    start_animation();
    print_frames();
    start_battery_status();
    start_output_status();
    start_wpm_status();
    start_modifier_status();
    start_layer_status();
    set_status_symbol();
    set_battery_symbol();
    print_layer();
    print_themes();
    print_wpm();
    print_modifiers();
}

void toggle_menu() {
#ifdef CONFIG_USE_BUZZER
#ifdef CONFIG_USE_MENU_SOUND
    play_notification_song();
#endif
#endif
    if (menu_on) {
        stop_wpm_status();
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

    buf_frame = (uint8_t *)k_malloc(320 * 2 * sizeof(uint8_t));
}

void start_action_button(bool is_menu_on) {
    menu_on = is_menu_on;
    action_button_initialized = true;
}