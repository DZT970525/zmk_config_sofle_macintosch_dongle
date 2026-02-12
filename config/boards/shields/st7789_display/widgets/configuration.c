#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_configuration, LOG_LEVEL_DBG);

#include <stdlib.h>
#include "helpers/display.h"

#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

void info_slots() {
    const char *left_slot_str = CONFIG_INFO_LEFT_SLOT;
    const char *right_slot_str = CONFIG_INFO_RIGHT_SLOT;

    if (strcmp(left_slot_str, right_slot_str) == 0) {
        set_left_slot(INFO_SLOT_CONNECTIVITY);
        set_right_slot(INFO_SLOT_LAYER);
        return;
    }

    // left
    if (strcmp(left_slot_str, "modifiers") == 0) {
        set_left_slot(INFO_SLOT_MODIFIERS);
    } else if (strcmp(left_slot_str, "connectivity") == 0) {
        set_left_slot(INFO_SLOT_CONNECTIVITY);
    } else if (strcmp(left_slot_str, "layer") == 0) {
        set_left_slot(INFO_SLOT_LAYER);
    } else if (strcmp(left_slot_str, "theme") == 0) {
        set_left_slot(INFO_SLOT_THEME);
    } else if (strcmp(left_slot_str, "wpm") == 0) {
        set_left_slot(INFO_SLOT_WPM);
    } else {
        set_left_slot(INFO_SLOT_CONNECTIVITY);
    }

    // right
    if (strcmp(right_slot_str, "modifiers") == 0) {
        set_right_slot(INFO_SLOT_MODIFIERS);
    } else if (strcmp(right_slot_str, "connectivity") == 0) {
        set_right_slot(INFO_SLOT_CONNECTIVITY);
    } else if (strcmp(right_slot_str, "layer") == 0) {
        set_right_slot(INFO_SLOT_LAYER);
    } else if (strcmp(right_slot_str, "theme") == 0) {
        set_right_slot(INFO_SLOT_THEME);
    } else if (strcmp(right_slot_str, "wpm") == 0) {
        set_right_slot(INFO_SLOT_WPM);
    } else {
        set_right_slot(INFO_SLOT_LAYER);
    }

    if (get_left_slot() == get_right_slot()) {
        set_left_slot(INFO_SLOT_CONNECTIVITY);
        set_right_slot(INFO_SLOT_LAYER);
    }
}

void board_size() {
    const char *size_str = CONFIG_SNAKE_BOARD_SIZE;
    if (strcmp(size_str, "XXXL") == 0) {
        set_snake_board_width(48);
        set_snake_board_height(48);
        set_snake_pixel_size(5);
    } else if (strcmp(size_str, "XXL") == 0) {
        set_snake_board_width(24);
        set_snake_board_height(24);
        set_snake_pixel_size(10);
    } else if (strcmp(size_str, "XL") == 0) {
        set_snake_board_width(20);
        set_snake_board_height(20);
        set_snake_pixel_size(12);
    } else if (strcmp(size_str, "L") == 0) {
        set_snake_board_width(16);
        set_snake_board_height(16);
        set_snake_pixel_size(15);
    } else if (strcmp(size_str, "M") == 0) {
        set_snake_board_width(12);
        set_snake_board_height(12);
        set_snake_pixel_size(20);
    } else if (strcmp(size_str, "S") == 0) {
        set_snake_board_width(10);
        set_snake_board_height(10);
        set_snake_pixel_size(24);
    } else if (strcmp(size_str, "XS") == 0) {
        set_snake_board_width(8);
        set_snake_board_height(8);
        set_snake_pixel_size(30);
    } else {
        set_snake_board_width(24);
        set_snake_board_height(24);
        set_snake_pixel_size(10);
    }
}

void default_screen() {
    const char *default_screen_str = CONFIG_DEFAULT_SCREEN;
    if (strcmp(default_screen_str, "snake") == 0) {
        set_default_screen(SNAKE_SCREEN);
    } else if (strcmp(default_screen_str, "status") == 0) {
        set_default_screen(STATUS_SCREEN);
    } else {
        set_default_screen(SNAKE_SCREEN);
    }
}

void custom_theme() {
    uint32_t color1 = hex_string_to_uint(CONFIG_THEME_PRIMARY_COLOR);
    uint32_t color2 = hex_string_to_uint(CONFIG_THEME_SECONDARY_COLOR);
    uint32_t color3 = hex_string_to_uint(CONFIG_THEME_BG_COLOR);
    uint32_t color4 = hex_string_to_uint(CONFIG_THEME_BG_DARKER_COLOR);
    if (color1 == HEX_PARSE_ERROR || color2 == HEX_PARSE_ERROR || color3 == HEX_PARSE_ERROR ||
        color4 == HEX_PARSE_ERROR) {
        // https://lospec.com/palette-list/b4sement
        set_custom_theme_colors(0x3dff98u, 0xff4adcu, 0x222323u, 0x121313u, 0, 0);
    } else {
        set_custom_theme_colors(color1, color2, color3, color4, 0x000000u, 0x000000u);
    }
}

void configure(void) {
    info_slots();
    custom_theme();
    board_size();
    default_screen();
}
