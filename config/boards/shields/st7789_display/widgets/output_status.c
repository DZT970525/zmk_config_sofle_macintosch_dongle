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
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>

#include "output_status.h"
#include "layer_status.h"
#include "helpers/display.h"

static bool status_widget_initialized = false;
static struct output_status_state status_state;
static uint16_t *scaled_bitmap_status;
static uint16_t *scaled_bitmap_symbol;
static uint16_t *scaled_bitmap_bt_num;
static uint16_t logo_bitmap[34 * 42];

static uint16_t to_display_color(uint16_t color) { return (color >> 8) | (color << 8); }

static void write_logo_bitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    struct display_buffer_descriptor descriptor = {
        .buf_size = width * height,
        .pitch = width,
        .width = width,
        .height = height,
    };
    display_write_wrapper(x, y, &descriptor, (uint8_t *)logo_bitmap);
}

static const uint16_t status_height = 9;
static const uint16_t status_width = 9;
static const uint16_t status_scale = 3;

static const uint16_t symbol_scale = 2;
static const uint16_t symbol_width = 9;
static const uint16_t symbol_height = 15;

static const uint16_t bt_num_scale = 4;
static const uint16_t bt_num_width = 5;
static const uint16_t bt_num_height = 7;

static const uint16_t usb_ready_bitmap[] = {
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
    0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1,
    1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t usb_not_ready_bitmap[] = {
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
    0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t bluetooth_bitmap[] = {
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
    0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0,
    0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0,
    0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
};
static const uint16_t none_bitmap[] = {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
};

static const uint16_t open[] = {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
};
static const uint16_t not_ok[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t ok[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1,
    1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t none[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool usb_is_hid_ready;
};

void print_bitmap_transport(uint16_t *scaled_bitmap, Transport t, bool is_ready, uint16_t x,
                            uint16_t y, uint16_t scale, uint16_t color, uint16_t bg_color) {
    switch (t) {
    case TRANSPORT_USB:
        if (is_ready) {
            render_bitmap(scaled_bitmap, usb_ready_bitmap, x, y, symbol_width, symbol_height, scale,
                          color, bg_color);
        } else {
            render_bitmap(scaled_bitmap, usb_not_ready_bitmap, x, y, symbol_width, symbol_height,
                          scale, color, bg_color);
        }
        break;
    case TRANSPORT_BLUETOOTH:
        render_bitmap(scaled_bitmap, bluetooth_bitmap, x, y, symbol_width, symbol_height, scale,
                      color, bg_color);
        break;
    default:
        render_bitmap(scaled_bitmap, none_bitmap, x, y, symbol_width, symbol_height, scale, color,
                      bg_color);
    }
}

void print_bitmap_status(uint16_t *scaled_bitmap, Status s, uint16_t x, uint16_t y, uint16_t scale,
                         uint16_t color, uint16_t bg_color) {
    switch (s) {
    case STATUS_OPEN:
        render_bitmap(scaled_bitmap, open, x, y, status_width, status_height, scale, color,
                      bg_color);
        break;
    case STATUS_OK:
        render_bitmap(scaled_bitmap, ok, x, y, status_width, status_height, scale, color, bg_color);
        break;
    case STATUS_NOT_OK:
        render_bitmap(scaled_bitmap, not_ok, x, y, status_width, status_height, scale, color,
                      bg_color);
        break;
    default:
        render_bitmap(scaled_bitmap, none, x, y, 4, 6, scale, color, bg_color);
    }
}

static struct output_status_state get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){.selected_endpoint = zmk_endpoints_selected(),
                                        .active_profile_index = zmk_ble_active_profile_index(),
                                        .active_profile_connected =
                                            zmk_ble_active_profile_is_connected(),
                                        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
                                        .usb_is_hid_ready = zmk_usb_is_hid_ready()};
}

void print_bluetooth_status(uint16_t x, uint16_t y, struct output_status_state state) {
    if (state.active_profile_bonded) {
        if (state.active_profile_connected) {
            print_bitmap_status(scaled_bitmap_status, STATUS_OK, x, y, status_scale,
                                get_bt_status_ok_color(), get_bt_status_bg_color());
        } else {
            print_bitmap_status(scaled_bitmap_status, STATUS_NOT_OK, x, y, status_scale,
                                get_bt_status_not_ok_color(), get_bt_status_bg_color());
        }
    } else {
        print_bitmap_status(scaled_bitmap_status, STATUS_OPEN, x, y, status_scale,
                            get_bt_status_open_color(), get_bt_status_bg_color());
    }
}

void print_bluetooth_profile(uint16_t x, uint16_t y, int active_profile) {
    if (active_profile < 0 || active_profile > 4) {
        print_bitmap(scaled_bitmap_bt_num, CHAR_NONE, x, y, bt_num_scale, get_bt_num_color(),
                     get_bt_bg_color(), FONT_SIZE_5x7);
        return;
    }
    print_bitmap(scaled_bitmap_bt_num, active_profile + 1, x, y, bt_num_scale, get_bt_num_color(),
                 get_bt_bg_color(), FONT_SIZE_5x7);
}

void print_bluetooth_profiles(uint16_t x, uint16_t y, struct output_status_state state) {
    print_bluetooth_profile(x, y, state.active_profile_index);
}

void print_symbols(uint16_t usb_x, uint16_t ble_x, uint16_t y, struct output_status_state state) {
    const uint16_t usb_active = rgb888_to_rgb565(0xFFD700);
    const uint16_t ble_active = rgb888_to_rgb565(0x008FD5);
    const uint16_t inactive = rgb888_to_rgb565(0x808080);

    switch (state.selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_USB, state.usb_is_hid_ready, usb_x,
                               y, symbol_scale, usb_active, get_symbol_bg_color());
        print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_BLUETOOTH, true, ble_x, y,
                               symbol_scale, inactive, get_symbol_bg_color());
        break;
    case ZMK_TRANSPORT_BLE:
        print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_USB, state.usb_is_hid_ready, usb_x,
                               y, symbol_scale, inactive, get_symbol_bg_color());
        print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_BLUETOOTH, true, ble_x, y,
                               symbol_scale, ble_active, get_symbol_bg_color());
        break;
    }
}

static void print_output_button(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                const char text[], uint8_t text_len, bool selected,
                                uint16_t selected_background) {
    const uint16_t black = get_frame_color();
    const uint16_t white = get_menu_bg_color();
    const uint16_t foreground = selected ? white : black;
    const uint16_t background = selected ? selected_background : white;
    const uint8_t factor = 2;
    const uint16_t character_width = (7 * factor) + 1;
    const uint16_t character_height = (9 * factor) + 1;
    const uint16_t gap = factor + 1;
    const uint16_t text_width =
        (text_len * character_width) + ((text_len > 0 ? text_len - 1 : 0) * gap);
    const uint16_t text_x = x + (width - text_width) / 2;
    const uint16_t text_y = y + (height - character_height) / 2;

    print_filled_screen_area(x, y, width, height, black);
    print_filled_screen_area(x + 1, y + 1, width - 2, height - 2, background);
    print_layer_font_text(scaled_bitmap_bt_num, text, text_len, text_x, text_y, factor, foreground,
                          background);
}

static void print_transport_icons(struct output_status_state state) {
    /* Original transport glyphs: active USB is yellow, active BLE is sky blue,
     * and whichever transport is inactive is neutral grey. */
    print_symbols(14, 36, 117, state);
}

static void print_profile_button(uint8_t profile, bool selected) {
    static const uint32_t profile_colors[] = {
        0x73B55B, /* green  */
        0xFFD700, /* same yellow as the active USB icon */
        0xC83B3F, /* red: darker for white-number contrast    */
        0x773B8F, /* purple: darker for white-number contrast */
        0x4798C8, /* blue   */
    };

    if (profile > 4) {
        return;
    }
    char number[] = {(char)('1' + profile)};
    /* USB, Bluetooth and all five profile slots share one centered horizontal row. */
    print_output_button(58 + (profile * 25), 118, 24, 28, number, ARRAY_SIZE(number), selected,
                        rgb888_to_rgb565(profile_colors[profile]));
}

static void print_classic_macintosh_icon(void) {
    const uint16_t black = get_frame_color();
    static const char *const rows[] = {
        "..#############################..", ".###############################.",
        "###...........................###", "##.............................##",
        "##...#######################...##", "##..#########################..##",
        "##..##.....................##..##", "##..##.....................##..##",
        "##..##.....................##..##", "##..##....##....##...##....##..##",
        "##..##....##....##...##....##..##", "##..##....##....##...##....##..##",
        "##..##..........##.........##..##", "##..##..........##.........##..##",
        "##..##........####.........##..##", "##..##........####.........##..##",
        "##..##.....................##..##", "##..##......##....##.......##..##",
        "##..##......########.......##..##", "##..##.......######........##..##",
        "##..##.....................##..##", "##..##.....................##..##",
        "##..#########################..##", "##...#######################...##",
        "##.............................##", "##.............................##",
        "##.............................##", "##.............................##",
        "##....................####.....##", "##..###............#########...##",
        "##..###.............########...##", "##.............................##",
        "##.............................##", "##.............................##",
        "##.............................##", "#################################",
        ".###############################.", ".##...........................##.",
        ".##...........................##.", ".##...........................##.",
        ".###############################.", ".###############################.",
    };

    /* Compose the 33 x 42 monochrome sampling in RAM, then send it in one transfer. */
    uint16_t white = to_display_color(get_menu_bg_color());
    uint16_t black_pixel = to_display_color(black);
    for (uint16_t i = 0; i < 33 * 42; i++) {
        logo_bitmap[i] = white;
    }
    for (uint8_t y = 0; y < ARRAY_SIZE(rows); y++) {
        for (uint8_t x = 0; x < 33; x++) {
            if (rows[y][x] == '#') {
                logo_bitmap[(y * 33) + x] = black_pixel;
            }
        }
    }
    write_logo_bitmap(187, 111, 33, 42);
}

void set_status_symbol() {
    /* One centered black-and-white output panel spanning the full middle row. */
    print_filled_screen_area(11, 105, 216, 55, get_menu_bg_color());
    print_classic_macintosh_icon();
    print_transport_icons(status_state);

    for (uint8_t profile = 0; profile < 5; profile++) {
        print_profile_button(profile, status_state.active_profile_index == profile);
    }
}

void output_status_update_cb(struct output_status_state state) {
    struct output_status_state previous = status_state;
    status_state = state;
    if (status_widget_initialized) {
        if (previous.selected_endpoint.transport != state.selected_endpoint.transport ||
            previous.usb_is_hid_ready != state.usb_is_hid_ready) {
            print_transport_icons(state);
        }

        if (previous.active_profile_index != state.active_profile_index) {
            if (previous.active_profile_index >= 0 && previous.active_profile_index <= 4) {
                print_profile_button(previous.active_profile_index, false);
            }
            if (state.active_profile_index >= 0 && state.active_profile_index <= 4) {
                print_profile_button(state.active_profile_index, true);
            }
        }
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);

void zmk_widget_output_status_init() {
    /* Two original 9 x 15 transport glyphs rendered at 2x. */
    scaled_bitmap_symbol =
        k_malloc((symbol_width * symbol_scale) * (symbol_height * symbol_scale) * sizeof(uint16_t));

    /* One 7 x 9 layer-style glyph at factor 2 expands to 15 x 19 pixels. */
    scaled_bitmap_bt_num = k_malloc((7 * 2 + 1) * (9 * 2 + 1) * sizeof(uint16_t));

    widget_output_status_init();
}

void start_output_status() {
    set_status_symbol();
    status_widget_initialized = true;
}

void stop_output_status(void) { status_widget_initialized = false; }
