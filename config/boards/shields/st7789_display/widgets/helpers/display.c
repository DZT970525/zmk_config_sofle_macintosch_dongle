#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include "display.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const struct device *display_dev;
static uint8_t *buf_screen_area;

static uint8_t screen_width;
static uint8_t screen_height;
static size_t buf_screen_size;

static uint16_t splash_logo_color;
static uint16_t splash_created_by_color;
static uint16_t splash_bg_color;

static uint16_t snake_font_color;
static uint16_t snake_num_color;
static uint16_t snake_bg_color;

static uint16_t snake_default_color;
static uint16_t snake_board_color;
static uint16_t snake_board_1_color;
static uint16_t food_color;

static uint16_t snake_color_0;
static uint16_t snake_color_1;
static uint16_t snake_color_2;
static uint16_t snake_color_3;
static uint16_t snake_color_4;
static uint16_t snake_color_5;
static uint16_t snake_color_6;

static uint16_t battery_num_color_1;
static uint16_t battery_percentage_color_1;
static uint16_t battery_bg_color_1;
static uint16_t battery_num_color;
static uint16_t battery_percentage_color;
static uint16_t battery_bg_color;

static uint16_t symbol_selected_color;
static uint16_t symbol_unselected_color;
static uint16_t symbol_bg_color;
static uint16_t modifier_selected_color;
static uint16_t modifier_unselected_color;
static uint16_t modifier_bg_color;
static uint16_t bt_num_color;
static uint16_t bt_bg_color;
static uint16_t bt_status_ok_color;
static uint16_t bt_status_not_ok_color;
static uint16_t bt_status_open_color;
static uint16_t bt_status_bg_color;

static uint16_t layer_font_bg_color;
static uint16_t layer_font_color;
static uint16_t theme_font_bg_color;
static uint16_t theme_font_color;
static uint16_t theme_font_color_1;

static uint16_t logo_bg_color;
static uint16_t logo_font_color;
static uint16_t logo_snake_color;

static uint16_t frame_color;
static uint16_t frame_color_1;
static uint16_t menu_bg_color;
static uint16_t wpm_font_color;
static uint16_t wpm_font_1_color;
static uint16_t wpm_font_bg_color;

static DefaultScreen default_screen = SNAKE_SCREEN;

static InfoSlot left_slot;
static InfoSlot right_slot;

#define COLORS_PER_THEME 6

static uint8_t themes_colors_len = 11;
static uint32_t themes_colors[][COLORS_PER_THEME] = {
    // primary  secondary  back1      back2
    {0x3dff98u, 0xff4adcu, 0x222323u, 0x121313u, 0,
     0}, // C  - custom https://lospec.com/palette-list/b4sement
    {0xd0d058u, 0xa0a840u, 0x708028u, 0x405010u, 0,
     0}, // 01 - https://lospec.com/palette-list/nostalgia
    {0x3dff98u, 0xff4adcu, 0x222323u, 0x121313u, 0,
     0}, // 02 - https://lospec.com/palette-list/b4sement
    {0x94e344u, 0x46878fu, 0x332c50u, 0x231c40u, 0,
     0}, // 03 - https://lospec.com/palette-list/kirokaze-gameboy
    {0x5fc75du, 0x36868fu, 0x203671u, 0x0f052du, 0,
     0}, // 04 - https://lospec.com/palette-list/moonlight-gb
    {0xff4d6du, 0xfcdeeau, 0x265935u, 0x012824u, 0,
     0}, // 05 - https://lospec.com/palette-list/cherrymelon
    {0xc56981u, 0xa3a29au, 0x545c7eu, 0x282328u, 0,
     0}, // 06 - https://lospec.com/palette-list/bittersweet
    {0xff8e80u, 0xc53a9du, 0x4a2480u, 0x051f39u, 0,
     0}, // 07 - https://lospec.com/palette-list/lava-gb
    {0xecfffbu, 0x858f97u, 0x576373u, 0x323859u, 0,
     0}, // 08 - https://lospec.com/gallery/dogmaster/cave
    {0xa3da58u, 0xf7ba2bu, 0x615aa8u, 0x592661u, 0, 0}, // 09 - Eva 01 neon genesis evangelion
    {0xff3b94u, 0xa6fd29u, 0x55ffe1u, 0xaf3dffu, 0, 0}, // 10 - neon colors
};

void set_complete_colors_theme() {
    uint32_t splash_logo_color = hex_string_to_uint(CONFIG_SPLASH_LOGO_COLOR);
    uint32_t splash_created_by_color = hex_string_to_uint(CONFIG_SPLASH_CREATED_BY_COLOR);
    uint32_t splash_bg_color = hex_string_to_uint(CONFIG_SPLASH_BG_COLOR);
    uint32_t snake_default_color = hex_string_to_uint(CONFIG_SNAKE_DEFAULT_COLOR);
    uint32_t snake_board_color = hex_string_to_uint(CONFIG_SNAKE_BOARD_COLOR);
    uint32_t snake_board_1_color = hex_string_to_uint(CONFIG_SNAKE_BOARD_1_COLOR);
    uint32_t food_color = hex_string_to_uint(CONFIG_FOOD_COLOR);
    uint32_t snake_color_0 = hex_string_to_uint(CONFIG_SNAKE_COLOR_0);
    uint32_t snake_color_1 = hex_string_to_uint(CONFIG_SNAKE_COLOR_1);
    uint32_t snake_color_2 = hex_string_to_uint(CONFIG_SNAKE_COLOR_2);
    uint32_t snake_color_3 = hex_string_to_uint(CONFIG_SNAKE_COLOR_3);
    uint32_t snake_color_4 = hex_string_to_uint(CONFIG_SNAKE_COLOR_4);
    uint32_t snake_color_5 = hex_string_to_uint(CONFIG_SNAKE_COLOR_5);
    uint32_t snake_color_6 = hex_string_to_uint(CONFIG_SNAKE_COLOR_6);
    uint32_t battery_num_color = hex_string_to_uint(CONFIG_BATTERY_NUM_COLOR);
    uint32_t battery_percentage_color = hex_string_to_uint(CONFIG_BATTERY_PERCENTAGE_COLOR);
    uint32_t battery_bg_color = hex_string_to_uint(CONFIG_BATTERY_BG_COLOR);
    uint32_t battery_num_color_1 = hex_string_to_uint(CONFIG_BATTERY_NUM_COLOR_1);
    uint32_t battery_percentage_color_1 = hex_string_to_uint(CONFIG_BATTERY_PERCENTAGE_COLOR_1);
    uint32_t battery_bg_color_1 = hex_string_to_uint(CONFIG_BATTERY_BG_COLOR_1);
    uint32_t symbol_selected_color = hex_string_to_uint(CONFIG_SYMBOL_SELECTED_COLOR);
    uint32_t symbol_unselected_color = hex_string_to_uint(CONFIG_SYMBOL_UNSELECTED_COLOR);
    uint32_t symbol_bg_color = hex_string_to_uint(CONFIG_SYMBOL_BG_COLOR);
    uint32_t modifier_selected_color = hex_string_to_uint(CONFIG_MODIFIER_SELECTED_COLOR);
    uint32_t modifier_unselected_color = hex_string_to_uint(CONFIG_MODIFIER_UNSELECTED_COLOR);
    uint32_t modifier_bg_color = hex_string_to_uint(CONFIG_MODIFIER_BG_COLOR);
    uint32_t bt_num_color = hex_string_to_uint(CONFIG_BT_NUM_COLOR);
    uint32_t bt_bg_color = hex_string_to_uint(CONFIG_BT_BG_COLOR);
    uint32_t bt_status_ok_color = hex_string_to_uint(CONFIG_BT_STATUS_OK_COLOR);
    uint32_t bt_status_not_ok_color = hex_string_to_uint(CONFIG_BT_STATUS_NOT_OK_COLOR);
    uint32_t bt_status_open_color = hex_string_to_uint(CONFIG_BT_STATUS_OPEN_COLOR);
    uint32_t bt_status_bg_color = hex_string_to_uint(CONFIG_BT_STATUS_BG_COLOR);
    uint32_t theme_font_color = hex_string_to_uint(CONFIG_THEME_FONT_COLOR);
    uint32_t theme_font_color_1 = hex_string_to_uint(CONFIG_THEME_FONT_COLOR_1);
    uint32_t theme_font_bg_color = hex_string_to_uint(CONFIG_THEME_FONT_BG_COLOR);
    uint32_t layer_font_color = hex_string_to_uint(CONFIG_LAYER_FONT_COLOR);
    uint32_t layer_font_bg_color = hex_string_to_uint(CONFIG_LAYER_FONT_BG_COLOR);
    uint32_t logo_font_color = hex_string_to_uint(CONFIG_LOGO_FONT_COLOR);
    uint32_t logo_snake_color = hex_string_to_uint(CONFIG_LOGO_SNAKE_COLOR);
    uint32_t logo_bg_color = hex_string_to_uint(CONFIG_LOGO_BG_COLOR);
    uint32_t frame_color = hex_string_to_uint(CONFIG_FRAME_COLOR);
    uint32_t frame_color_1 = hex_string_to_uint(CONFIG_FRAME_COLOR_1);
    uint32_t menu_bg_color = hex_string_to_uint(CONFIG_MENU_BG_COLOR);
    uint32_t wpm_font_color = hex_string_to_uint(CONFIG_WPM_FONT_COLOR);
    uint32_t wpm_font_1_color = hex_string_to_uint(CONFIG_WPM_FONT_1_COLOR);
    uint32_t wpm_font_bg_color = hex_string_to_uint(CONFIG_WPM_FONT_BG_COLOR);

    if (splash_logo_color == HEX_PARSE_ERROR) {
        splash_logo_color = 0xFFFFFF;
    }

    if (splash_created_by_color == HEX_PARSE_ERROR) {
        splash_created_by_color = 0xFFFFFF;
    }

    if (splash_bg_color == HEX_PARSE_ERROR) {
        splash_bg_color = 0xFFFFFF;
    }

    if (snake_default_color == HEX_PARSE_ERROR) {
        snake_default_color = 0xFFFFFF;
    }

    if (snake_board_color == HEX_PARSE_ERROR) {
        snake_board_color = 0xFFFFFF;
    }

    if (snake_board_1_color == HEX_PARSE_ERROR) {
        snake_board_1_color = 0xFFFFFF;
    }

    if (food_color == HEX_PARSE_ERROR) {
        food_color = 0xFFFFFF;
    }

    if (snake_color_0 == HEX_PARSE_ERROR) {
        snake_color_0 = 0xFFFFFF;
    }

    if (snake_color_1 == HEX_PARSE_ERROR) {
        snake_color_1 = 0xFFFFFF;
    }

    if (snake_color_2 == HEX_PARSE_ERROR) {
        snake_color_2 = 0xFFFFFF;
    }

    if (snake_color_3 == HEX_PARSE_ERROR) {
        snake_color_3 = 0xFFFFFF;
    }

    if (snake_color_4 == HEX_PARSE_ERROR) {
        snake_color_4 = 0xFFFFFF;
    }

    if (snake_color_5 == HEX_PARSE_ERROR) {
        snake_color_5 = 0xFFFFFF;
    }

    if (snake_color_6 == HEX_PARSE_ERROR) {
        snake_color_6 = 0xFFFFFF;
    }

    if (battery_num_color == HEX_PARSE_ERROR) {
        battery_num_color = 0xFFFFFF;
    }

    if (battery_percentage_color == HEX_PARSE_ERROR) {
        battery_percentage_color = 0xFFFFFF;
    }

    if (battery_bg_color == HEX_PARSE_ERROR) {
        battery_bg_color = 0xFFFFFF;
    }

    if (battery_num_color_1 == HEX_PARSE_ERROR) {
        battery_num_color_1 = 0xFFFFFF;
    }

    if (battery_percentage_color_1 == HEX_PARSE_ERROR) {
        battery_percentage_color_1 = 0xFFFFFF;
    }

    if (battery_bg_color_1 == HEX_PARSE_ERROR) {
        battery_bg_color_1 = 0xFFFFFF;
    }

    if (modifier_selected_color == HEX_PARSE_ERROR) {
        modifier_selected_color = 0xFFFFFF;
    }

    if (modifier_unselected_color == HEX_PARSE_ERROR) {
        modifier_unselected_color = 0xFFFFFF;
    }

    if (modifier_bg_color == HEX_PARSE_ERROR) {
        modifier_bg_color = 0xFFFFFF;
    }

    if (symbol_selected_color == HEX_PARSE_ERROR) {
        symbol_selected_color = 0xFFFFFF;
    }

    if (symbol_unselected_color == HEX_PARSE_ERROR) {
        symbol_unselected_color = 0xFFFFFF;
    }

    if (symbol_bg_color == HEX_PARSE_ERROR) {
        symbol_bg_color = 0xFFFFFF;
    }

    if (bt_num_color == HEX_PARSE_ERROR) {
        bt_num_color = 0xFFFFFF;
    }

    if (bt_bg_color == HEX_PARSE_ERROR) {
        bt_bg_color = 0xFFFFFF;
    }

    if (bt_status_ok_color == HEX_PARSE_ERROR) {
        bt_status_ok_color = 0xFFFFFF;
    }

    if (bt_status_not_ok_color == HEX_PARSE_ERROR) {
        bt_status_not_ok_color = 0xFFFFFF;
    }

    if (bt_status_open_color == HEX_PARSE_ERROR) {
        bt_status_open_color = 0xFFFFFF;
    }

    if (bt_status_bg_color == HEX_PARSE_ERROR) {
        bt_status_bg_color = 0xFFFFFF;
    }

    if (theme_font_color == HEX_PARSE_ERROR) {
        theme_font_color = 0xFFFFFF;
    }

    if (theme_font_color_1 == HEX_PARSE_ERROR) {
        theme_font_color_1 = 0xFFFFFF;
    }

    if (theme_font_bg_color == HEX_PARSE_ERROR) {
        theme_font_bg_color = 0xFFFFFF;
    }

    if (layer_font_bg_color == HEX_PARSE_ERROR) {
        layer_font_bg_color = 0xFFFFFF;
    }

    if (layer_font_color == HEX_PARSE_ERROR) {
        layer_font_color = 0xFFFFFF;
    }

    if (logo_font_color == HEX_PARSE_ERROR) {
        logo_font_color = 0xFFFFFF;
    }

    if (logo_snake_color == HEX_PARSE_ERROR) {
        logo_snake_color = 0xFFFFFF;
    }

    if (logo_bg_color == HEX_PARSE_ERROR) {
        logo_bg_color = 0xFFFFFF;
    }

    if (frame_color == HEX_PARSE_ERROR) {
        frame_color = 0xFFFFFF;
    }

    if (frame_color_1 == HEX_PARSE_ERROR) {
        frame_color_1 = 0xFFFFFF;
    }

    if (menu_bg_color == HEX_PARSE_ERROR) {
        menu_bg_color = 0xFFFFFF;
    }

    if (wpm_font_color == HEX_PARSE_ERROR) {
        wpm_font_color = 0xFFFFFF;
    }

    if (wpm_font_1_color == HEX_PARSE_ERROR) {
        wpm_font_1_color = 0xFFFFFF;
    }

    if (wpm_font_bg_color == HEX_PARSE_ERROR) {
        wpm_font_bg_color = 0xFFFFFF;
    }

    set_all_colors(splash_logo_color, splash_created_by_color, splash_bg_color, snake_default_color,
                   snake_board_color, snake_board_1_color, food_color, snake_color_0, snake_color_1,
                   snake_color_2, snake_color_3, snake_color_4, snake_color_5, snake_color_6,
                   battery_num_color, battery_percentage_color, battery_bg_color,
                   battery_num_color_1, battery_percentage_color_1, battery_bg_color_1,
                   modifier_selected_color, modifier_unselected_color, modifier_bg_color,
                   symbol_selected_color, symbol_unselected_color, symbol_bg_color, bt_num_color,
                   bt_bg_color, bt_status_ok_color, bt_status_not_ok_color, bt_status_open_color,
                   bt_status_bg_color, theme_font_color, theme_font_color_1, theme_font_bg_color,
                   layer_font_color, layer_font_bg_color, logo_font_color, logo_snake_color,
                   logo_bg_color, frame_color, frame_color_1, menu_bg_color, wpm_font_color,
                   wpm_font_1_color, wpm_font_bg_color);
}

static const uint16_t empty_bitmap_5x7[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const uint16_t none_bitmap_5x7[] = {
    0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
};
static const uint16_t b_bitmap_5x7[] = {
    1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1,
    1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0,
};
static const uint16_t e_bitmap_5x7[] = {
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1,
    1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1,
};
static const uint16_t h_bitmap_5x7[] = {
    1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,
};
static const uint16_t l_bitmap_5x7[] = {
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1,
};
static const uint16_t m_bitmap_5x7[] = {
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1,
    0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1,
};
static const uint16_t n_bitmap_5x7[] = {
    1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1,
    0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1,
};
static const uint16_t p_bitmap_5x7[] = {
    1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1,
    1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,
};
static const uint16_t s_bitmap_5x7[] = {
    0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0,
};
static const uint16_t t_bitmap_5x7[] = {
    1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0,
};
static const uint16_t w_bitmap_5x7[] = {
    1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1,
    0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
};
static const uint16_t colon_bitmap_5x7[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const uint16_t num_bitmaps_5x7[10][35] = {
    {// zero
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0,
     0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {// one
     0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0},
    {// two
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
     1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1},
    {// three
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
     1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {// four
     0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0,
     1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0},
    {// five
     1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0,
     0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {// six
     0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1,
     1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {
        // seven
        1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0,
        1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    },
    {// eight
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1,
     1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {// nine
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1,
     1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0},
};

static const uint16_t none_bitmap_5x8[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t dash_bitmap_5x8[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const uint16_t f_bitmap_5x8[] = {
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,
};
static const uint16_t u_bitmap_5x8[] = {
    1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,
    1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1,
};
static const uint16_t l_bitmap_5x8[] = {
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1,
};
static const uint16_t percentage_bitmap_5x8[] = {
    1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0,
    0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1,
};

static const uint16_t num_bitmaps_3x5[10][15] = {
    {
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
    },
    {
        0,
        1,
        0,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
    },
    {
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
    },
    {
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
    },
    {
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
    },
};

static const uint16_t num_bitmaps_3x6[10][18] = {
    {
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
    },
    {
        0,
        1,
        0,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
    },
    {
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
    },
    {
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
    },
    {
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
    },
    {
        1,
        1,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
    },
};

static const uint16_t num_bitmaps_5x8[10][40] = {
    {// zero
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,
     1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {// one
     0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0,
     0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0},
    {// two
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0,
     0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1},
    {// three
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {// four
     0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0,
     1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0},
    {// five
     1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {// six
     0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0,
     1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {// seven
     1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
     0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0},
    {// eight
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0,
     1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {// nine
     0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,
     0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0},
};

const uint16_t none_letter_3x5[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
const uint16_t percentage_letter_3x5[] = {
    1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1,
};
const uint16_t colon_letter_3x5[] = {
    0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
};
const uint16_t dash_letter_3x5[] = {
    0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0,
};
const uint16_t underline_letter_3x5[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
};
const uint16_t pipe_letter_3x5[] = {
    0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
};
const uint16_t plus_letter_3x5[] = {
    0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0,
};
const uint16_t a_letter_3x5[] = {
    1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1,
};
const uint16_t b_letter_3x5[] = {
    1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1,
};
const uint16_t c_letter_3x5[] = {
    1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1,
};
const uint16_t d_letter_3x5[] = {
    1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0,
};
const uint16_t e_letter_3x5[] = {
    1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1,
};
const uint16_t f_letter_3x5[] = {
    1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0,
};
const uint16_t g_letter_3x5[] = {
    1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1,
};
const uint16_t h_letter_3x5[] = {
    1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1,
};
const uint16_t i_letter_3x5[] = {
    1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1,
};
const uint16_t j_letter_3x5[] = {
    0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1,
};
const uint16_t k_letter_3x5[] = {
    1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1,
};
const uint16_t l_letter_3x5[] = {
    1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1,
};
const uint16_t m_letter_3x5[] = {
    1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
};
const uint16_t n_letter_3x5[] = {
    1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
};
const uint16_t o_letter_3x5[] = {
    1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
};
const uint16_t p_letter_3x5[] = {
    1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0,
};
const uint16_t q_letter_3x5[] = {
    1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1,
};
const uint16_t r_letter_3x5[] = {
    1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1,
};
const uint16_t s_letter_3x5[] = {
    1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1,
};
const uint16_t t_letter_3x5[] = {
    1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
};
const uint16_t u_letter_3x5[] = {
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
};
const uint16_t v_letter_3x5[] = {
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0,
};
const uint16_t w_letter_3x5[] = {
    1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1,
};
const uint16_t x_letter_3x5[] = {
    1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1,
};
const uint16_t y_letter_3x5[] = {
    1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1,
};
const uint16_t z_letter_3x5[] = {
    1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1,
};

// #######################################

const uint16_t none_letter_3x6[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
const uint16_t s_letter_3x6[] = {
    1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1,
};
const uint16_t n_letter_3x6[] = {
    1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
};
const uint16_t a_letter_3x6[] = {
    1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
};
const uint16_t k_letter_3x6[] = {
    1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1,
};
const uint16_t e_letter_3x6[] = {
    1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1,
};
const uint16_t i_letter_3x6[] = {
    1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1,
};

// ###############################################################

uint8_t get_themes_colors_len() { return themes_colors_len; }

void set_custom_theme_colors(uint32_t color1, uint32_t color2, uint32_t color3, uint32_t color4,
                             uint32_t color5, uint32_t color6) {
    themes_colors[0][0] = color1;
    themes_colors[0][1] = color2;
    themes_colors[0][2] = color3;
    themes_colors[0][3] = color4;
    themes_colors[0][4] = color5;
    themes_colors[0][5] = color6;
}

void apply_current_theme(uint8_t current_theme) {
#ifdef CONFIG_USE_COMPLETE_CUSTOM_THEME
    if (current_theme == 0) {
        set_complete_colors_theme();
    } else {
        set_colorscheme(themes_colors[current_theme][0], themes_colors[current_theme][1],
                        themes_colors[current_theme][2], themes_colors[current_theme][3],
                        themes_colors[current_theme][4], themes_colors[current_theme][5]);
    }
#else
    set_colorscheme(themes_colors[current_theme][0], themes_colors[current_theme][1],
                    themes_colors[current_theme][2], themes_colors[current_theme][3],
                    themes_colors[current_theme][4], themes_colors[current_theme][5]);
#endif
}

uint16_t rgb888_to_rgb565(uint32_t color) {
    uint16_t red = (((color & 0xff0000) / 0x10000) * 31 / 255);
    uint16_t green = (((color & 0x00ff00) / 0x100) * 63 / 255);
    uint16_t blue = (((color & 0x0000ff) / 0x1) * 31 / 255);

    // Shift the red value to the left by 11 bits.
    uint16_t red_shifted = red << 11;
    // Shift the green value to the left by 5 bits.
    uint16_t green_shifted = green << 5;

    // Combine the red, green, and blue values.
    return red_shifted | green_shifted | blue;
}

void set_left_slot(InfoSlot slot) { left_slot = slot; }

InfoSlot get_left_slot() { return left_slot; }

void set_right_slot(InfoSlot slot) { right_slot = slot; }

InfoSlot get_right_slot() { return right_slot; }

void set_default_screen(DefaultScreen screen) { default_screen = screen; }

void set_splash_logo_color(uint32_t color) { splash_logo_color = rgb888_to_rgb565(color); }

void set_splash_created_by_color(uint32_t color) {
    splash_created_by_color = rgb888_to_rgb565(color);
}

void set_splash_bg_color(uint32_t color) { splash_bg_color = rgb888_to_rgb565(color); }

void set_snake_font_color(uint32_t color) { snake_font_color = rgb888_to_rgb565(color); }

void set_snake_num_color(uint32_t color) { snake_num_color = rgb888_to_rgb565(color); }

void set_snake_bg_color(uint32_t color) { snake_bg_color = rgb888_to_rgb565(color); }

void set_snake_default_color(uint32_t color) { snake_default_color = rgb888_to_rgb565(color); }

void set_snake_board_color(uint32_t color) { snake_board_color = rgb888_to_rgb565(color); }

void set_snake_board_1_color(uint32_t color) { snake_board_1_color = rgb888_to_rgb565(color); }

void set_food_color(uint32_t color) { food_color = rgb888_to_rgb565(color); }

void set_snake_color_0(uint32_t color) { snake_color_0 = rgb888_to_rgb565(color); }

void set_snake_color_1(uint32_t color) { snake_color_1 = rgb888_to_rgb565(color); }

void set_snake_color_2(uint32_t color) { snake_color_2 = rgb888_to_rgb565(color); }

void set_snake_color_3(uint32_t color) { snake_color_3 = rgb888_to_rgb565(color); }

void set_snake_color_4(uint32_t color) { snake_color_4 = rgb888_to_rgb565(color); }

void set_snake_color_5(uint32_t color) { snake_color_5 = rgb888_to_rgb565(color); }

void set_snake_color_6(uint32_t color) { snake_color_6 = rgb888_to_rgb565(color); }

void set_battery_num_color(uint32_t color) { battery_num_color = rgb888_to_rgb565(color); }

void set_battery_percentage_color(uint32_t color) {
    battery_percentage_color = rgb888_to_rgb565(color);
}

void set_battery_bg_color(uint32_t color) { battery_bg_color = rgb888_to_rgb565(color); }

void set_battery_num_color_1(uint32_t color) { battery_num_color_1 = rgb888_to_rgb565(color); }

void set_battery_percentage_color_1(uint32_t color) {
    battery_percentage_color_1 = rgb888_to_rgb565(color);
}

void set_battery_bg_color_1(uint32_t color) { battery_bg_color_1 = rgb888_to_rgb565(color); }

void set_frame_color(uint32_t color) { frame_color = rgb888_to_rgb565(color); }

void set_frame_color_1(uint32_t color) { frame_color_1 = rgb888_to_rgb565(color); }

void set_wpm_font_color(uint32_t color) { wpm_font_color = rgb888_to_rgb565(color); }

void set_wpm_font_1_color(uint32_t color) { wpm_font_1_color = rgb888_to_rgb565(color); }

void set_wpm_font_bg_color(uint32_t color) { wpm_font_bg_color = rgb888_to_rgb565(color); }

void set_menu_bg_color(uint32_t color) { menu_bg_color = rgb888_to_rgb565(color); }

void set_modifier_selected_color(uint32_t color) {
    modifier_selected_color = rgb888_to_rgb565(color);
}

void set_modifier_unselected_color(uint32_t color) {
    modifier_unselected_color = rgb888_to_rgb565(color);
}

void set_modifier_bg_color(uint32_t color) { modifier_bg_color = rgb888_to_rgb565(color); }

void set_symbol_selected_color(uint32_t color) { symbol_selected_color = rgb888_to_rgb565(color); }

void set_symbol_unselected_color(uint32_t color) {
    symbol_unselected_color = rgb888_to_rgb565(color);
}

void set_symbol_bg_color(uint32_t color) { symbol_bg_color = rgb888_to_rgb565(color); }

void set_logo_bg_color(uint32_t color) { logo_bg_color = rgb888_to_rgb565(color); }

void set_logo_font_color(uint32_t color) { logo_font_color = rgb888_to_rgb565(color); }

void set_logo_snake_color(uint32_t color) { logo_snake_color = rgb888_to_rgb565(color); }

void set_theme_font_bg_color(uint32_t color) { theme_font_bg_color = rgb888_to_rgb565(color); }

void set_theme_font_color(uint32_t color) { theme_font_color = rgb888_to_rgb565(color); }

void set_layer_font_bg_color(uint32_t color) { layer_font_bg_color = rgb888_to_rgb565(color); }

void set_layer_font_color(uint32_t color) { layer_font_color = rgb888_to_rgb565(color); }

void set_theme_font_color_1(uint32_t color) { theme_font_color_1 = rgb888_to_rgb565(color); }

void set_bt_num_color(uint32_t color) { bt_num_color = rgb888_to_rgb565(color); }

void set_bt_status_ok_color(uint32_t color) { bt_status_ok_color = rgb888_to_rgb565(color); }

void set_bt_status_not_ok_color(uint32_t color) {
    bt_status_not_ok_color = rgb888_to_rgb565(color);
}

void set_bt_status_open_color(uint32_t color) { bt_status_open_color = rgb888_to_rgb565(color); }

void set_bt_status_bg_color(uint32_t color) { bt_status_bg_color = rgb888_to_rgb565(color); }

void set_bt_bg_color(uint32_t color) { bt_bg_color = rgb888_to_rgb565(color); }

DefaultScreen get_default_screen() { return default_screen; }

uint16_t get_splash_created_by_color() { return splash_created_by_color; }

uint16_t get_splash_logo_color() { return splash_logo_color; }

uint16_t get_splash_bg_color() { return splash_bg_color; }

uint16_t get_snake_font_color() { return snake_font_color; }

uint16_t get_snake_num_color() { return snake_num_color; }

uint16_t get_snake_bg_color() { return snake_bg_color; }

uint16_t get_snake_default_color() { return snake_default_color; }

uint16_t get_snake_board_color() { return snake_board_color; }

uint16_t get_snake_board_1_color() { return snake_board_1_color; }

uint16_t get_food_color() { return food_color; }

uint16_t get_snake_color_0() { return snake_color_0; }

uint16_t get_snake_color_1() { return snake_color_1; }

uint16_t get_snake_color_2() { return snake_color_2; }

uint16_t get_snake_color_3() { return snake_color_3; }

uint16_t get_snake_color_4() { return snake_color_4; }

uint16_t get_snake_color_5() { return snake_color_5; }

uint16_t get_snake_color_6() { return snake_color_6; }

uint16_t get_battery_num_color() { return battery_num_color; }

uint16_t get_battery_percentage_color() { return battery_percentage_color; }

uint16_t get_battery_bg_color() { return battery_bg_color; }

uint16_t get_battery_num_color_1() { return battery_num_color_1; }

uint16_t get_battery_percentage_color_1() { return battery_percentage_color_1; }

uint16_t get_battery_bg_color_1() { return battery_bg_color_1; }

uint16_t get_symbol_selected_color() { return symbol_selected_color; }

uint16_t get_symbol_unselected_color() { return symbol_unselected_color; }

uint16_t get_symbol_bg_color() { return symbol_bg_color; }

uint16_t get_modifier_selected_color() { return modifier_selected_color; }

uint16_t get_modifier_unselected_color() { return modifier_unselected_color; }

uint16_t get_modifier_bg_color() { return modifier_bg_color; }

uint16_t get_theme_font_bg_color() { return theme_font_bg_color; }

uint16_t get_layer_font_bg_color() { return layer_font_bg_color; }

uint16_t get_layer_font_color() { return layer_font_color; }

uint16_t get_theme_font_color() { return theme_font_color; }

uint16_t get_theme_font_color_1() { return theme_font_color_1; }

uint16_t get_logo_bg_color() { return logo_bg_color; }

uint16_t get_logo_font_color() { return logo_font_color; }

uint16_t get_logo_snake_color() { return logo_snake_color; }

uint16_t get_bt_num_color() { return bt_num_color; }

uint16_t get_bt_status_ok_color() { return bt_status_ok_color; }

uint16_t get_bt_status_not_ok_color() { return bt_status_not_ok_color; }

uint16_t get_bt_status_open_color() { return bt_status_open_color; }

uint16_t get_bt_status_bg_color() { return bt_status_bg_color; }

uint16_t get_bt_bg_color() { return bt_bg_color; }

uint16_t get_frame_color() { return frame_color; }

uint16_t get_frame_color_1() { return frame_color_1; }

uint16_t get_wpm_font_color() { return wpm_font_color; }

uint16_t get_wpm_font_1_color() { return wpm_font_1_color; }

uint16_t get_wpm_font_bg_color() { return wpm_font_bg_color; }

uint16_t get_menu_bg_color() { return menu_bg_color; }

// Clamp function to ensure values stay within 0-255
int clamp(int value) {
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return value;
}

// Function to darken RGB color
uint32_t darken_color(uint32_t rgb, float percentage) {
    if (percentage < 0.0f)
        percentage = 0.0f;
    if (percentage > 1.0f)
        percentage = 1.0f;

    // Extract red, green, and blue components
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8) & 0xFF;
    uint32_t b = rgb & 0xFF;

    // Darken each component
    r = clamp((uint32_t)(r * (1.0f - percentage)));
    g = clamp((uint32_t)(g * (1.0f - percentage)));
    b = clamp((uint32_t)(b * (1.0f - percentage)));

    // Recombine into a single int
    return (r << 16) | (g << 8) | b;
}

void display_write_wrapper(uint16_t x, uint16_t y, struct display_buffer_descriptor *buf_desc,
                           uint8_t *buf) {
    display_write(display_dev, x, y, buf_desc, buf);
}

void init_display(void) {
    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Device %s not found. Aborting sample.", display_dev->name);
        return;
    }

    screen_width = 20;
    screen_height = 20;
    buf_screen_size = screen_width * screen_height * 2u;
    buf_screen_area = k_malloc(buf_screen_size);
}

uint32_t hex_string_to_uint(const char *hex_str) {
    if (!hex_str) {
        return HEX_PARSE_ERROR;
    }

    uint32_t result = 0;
    uint8_t i = 0;

    // Optional "0x" or "0X" prefix
    if (hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
        i = 2;
    }

    if (hex_str[0] == '#') {
        i = 1;
    }

    if (hex_str[i + 6] != '\0') {
        // Not rgb hex
        return HEX_PARSE_ERROR;
    }

    if (hex_str[i] == '\0') {
        // Empty string after "0x"
        return HEX_PARSE_ERROR;
    }

    for (; hex_str[i] != '\0'; ++i) {
        char c = hex_str[i];
        uint32_t digit;

        if (isdigit(c)) {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digit = 10 + (c - 'A');
        } else {
            // Invalid character for hex
            return HEX_PARSE_ERROR;
        }

        result = (result << 4) | digit; // Multiply result by 16 and add digit
    }

    return result;
}

void fill_buffer_color(uint8_t *buf, size_t buf_size, uint32_t color) {
    for (size_t idx = 0; idx < buf_size; idx += 2) {
        *(buf + idx + 0) = (color >> 8) & 0xFFu;
        *(buf + idx + 1) = (color >> 0) & 0xFFu;
    }
}

uint16_t swap_16_bit_color(uint16_t color) { return (color >> 8) | (color << 8); }

void render_bitmap(uint16_t *scaled_bitmap, uint16_t bitmap[], uint16_t x, uint16_t y,
                   uint16_t width, uint16_t height, uint16_t scale, uint16_t num_color,
                   uint16_t bg_color) {
    struct display_buffer_descriptor buf_font_desc;

    uint16_t color;
    uint16_t pixel;
    uint16_t font_width_scaled = width * scale;
    uint16_t font_height_scaled = height * scale;
    uint16_t font_buf_size_scaled = font_width_scaled * font_height_scaled;
    uint16_t index = 0;
    for (uint16_t line = 0; line < height; line++) {
        for (uint16_t i = 0; i < scale; i++) {
            for (uint16_t column = 0; column < width; column++) {
                for (uint16_t j = 0; j < scale; j++) {
                    pixel = bitmap[(line * width) + column];
                    if (pixel == 1) {
                        color = num_color;
                    } else {
                        color = bg_color;
                    }
                    *(scaled_bitmap + index) = swap_16_bit_color(color);
                    index++;
                }
            }
        }
    }
    buf_font_desc.buf_size = font_buf_size_scaled;
    buf_font_desc.pitch = font_width_scaled;
    buf_font_desc.width = font_width_scaled;
    buf_font_desc.height = font_height_scaled;
    display_write(display_dev, x, y, &buf_font_desc, scaled_bitmap);
}

void print_bitmap_5x8(uint16_t *scaled_bitmap, Character c, uint16_t x, uint16_t y, uint16_t scale,
                      uint16_t color, uint16_t bg_color) {
    uint8_t font_width = 5;
    uint8_t font_height = 8;

    if (c >= 0 && c < 10) {
        render_bitmap(scaled_bitmap, num_bitmaps_5x8[c], x, y, font_width, font_height, scale,
                      color, bg_color);
        return;
    }
    switch (c) {
    case CHAR_F:
        render_bitmap(scaled_bitmap, f_bitmap_5x8, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_U:
        render_bitmap(scaled_bitmap, u_bitmap_5x8, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_L:
        render_bitmap(scaled_bitmap, l_bitmap_5x8, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_DASH:
        render_bitmap(scaled_bitmap, dash_bitmap_5x8, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_PERCENTAGE:
        render_bitmap(scaled_bitmap, percentage_bitmap_5x8, x, y, font_width, font_height, scale,
                      color, bg_color);
        break;
    default:
        render_bitmap(scaled_bitmap, none_bitmap_5x8, x, y, font_width, font_height, scale, color,
                      bg_color);
    }
}

void print_bitmap_5x7(uint16_t *scaled_bitmap, Character c, uint16_t x, uint16_t y, uint16_t scale,
                      uint16_t color, uint16_t bg_color) {
    uint8_t font_width = 5;
    uint8_t font_height = 7;

    if (c >= 0 && c < 10) {
        render_bitmap(scaled_bitmap, num_bitmaps_5x7[c], x, y, font_width, font_height, scale,
                      color, bg_color);
        return;
    }
    switch (c) {
    case CHAR_B:
        render_bitmap(scaled_bitmap, b_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_E:
        render_bitmap(scaled_bitmap, e_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_H:
        render_bitmap(scaled_bitmap, h_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_L:
        render_bitmap(scaled_bitmap, l_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_M:
        render_bitmap(scaled_bitmap, m_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_N:
        render_bitmap(scaled_bitmap, n_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_P:
        render_bitmap(scaled_bitmap, p_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_S:
        render_bitmap(scaled_bitmap, s_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_T:
        render_bitmap(scaled_bitmap, t_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_W:
        render_bitmap(scaled_bitmap, w_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_COLON:
        render_bitmap(scaled_bitmap, colon_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    case CHAR_EMPTY:
        render_bitmap(scaled_bitmap, empty_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
        break;
    default:
        render_bitmap(scaled_bitmap, none_bitmap_5x7, x, y, font_width, font_height, scale, color,
                      bg_color);
    }
}

void print_bitmap_3x5(uint16_t *scaled_bitmap, Character c, uint16_t x, uint16_t y, uint16_t scale,
                      uint16_t color, uint16_t bg_color) {
    if (c >= 0 && c < 10) {
        render_bitmap(scaled_bitmap, num_bitmaps_3x5[c], x, y, 3, 5, scale, color, bg_color);
        return;
    }
    switch (c) {
    case CHAR_A:
        render_bitmap(scaled_bitmap, a_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_B:
        render_bitmap(scaled_bitmap, b_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_C:
        render_bitmap(scaled_bitmap, c_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_D:
        render_bitmap(scaled_bitmap, d_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_E:
        render_bitmap(scaled_bitmap, e_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_F:
        render_bitmap(scaled_bitmap, f_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_G:
        render_bitmap(scaled_bitmap, g_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_H:
        render_bitmap(scaled_bitmap, h_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_I:
        render_bitmap(scaled_bitmap, i_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_J:
        render_bitmap(scaled_bitmap, j_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_K:
        render_bitmap(scaled_bitmap, k_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_L:
        render_bitmap(scaled_bitmap, l_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_M:
        render_bitmap(scaled_bitmap, m_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_N:
        render_bitmap(scaled_bitmap, n_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_O:
        render_bitmap(scaled_bitmap, o_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_P:
        render_bitmap(scaled_bitmap, p_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_Q:
        render_bitmap(scaled_bitmap, q_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_R:
        render_bitmap(scaled_bitmap, r_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_S:
        render_bitmap(scaled_bitmap, s_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_T:
        render_bitmap(scaled_bitmap, t_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_U:
        render_bitmap(scaled_bitmap, u_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_V:
        render_bitmap(scaled_bitmap, v_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_W:
        render_bitmap(scaled_bitmap, w_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_X:
        render_bitmap(scaled_bitmap, x_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_Y:
        render_bitmap(scaled_bitmap, y_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_Z:
        render_bitmap(scaled_bitmap, z_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_COLON:
        render_bitmap(scaled_bitmap, colon_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_DASH:
        render_bitmap(scaled_bitmap, dash_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_UNDERLINE:
        render_bitmap(scaled_bitmap, underline_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_PIPE:
        render_bitmap(scaled_bitmap, pipe_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_PLUS:
        render_bitmap(scaled_bitmap, plus_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    case CHAR_PERCENTAGE:
        render_bitmap(scaled_bitmap, percentage_letter_3x5, x, y, 3, 5, scale, color, bg_color);
        break;
    default:
        render_bitmap(scaled_bitmap, none_letter_3x5, x, y, 3, 5, scale, color, bg_color);
    }
}

void print_bitmap_3x6(uint16_t *scaled_bitmap, Character c, uint16_t x, uint16_t y, uint16_t scale,
                      uint16_t color, uint16_t bg_color) {
    if (c >= 0 && c < 10) {
        render_bitmap(scaled_bitmap, num_bitmaps_3x6[c], x, y, 3, 6, scale, color, bg_color);
        return;
    }
    switch (c) {
    case CHAR_S:
        render_bitmap(scaled_bitmap, s_letter_3x6, x, y, 3, 6, scale, color, bg_color);
        break;
    case CHAR_N:
        render_bitmap(scaled_bitmap, n_letter_3x6, x, y, 3, 6, scale, color, bg_color);
        break;
    case CHAR_A:
        render_bitmap(scaled_bitmap, a_letter_3x6, x, y, 3, 6, scale, color, bg_color);
        break;
    case CHAR_K:
        render_bitmap(scaled_bitmap, k_letter_3x6, x, y, 3, 6, scale, color, bg_color);
        break;
    case CHAR_E:
        render_bitmap(scaled_bitmap, e_letter_3x6, x, y, 3, 6, scale, color, bg_color);
        break;
    case CHAR_I:
        render_bitmap(scaled_bitmap, i_letter_3x6, x, y, 3, 6, scale, color, bg_color);
        break;
    default:
        render_bitmap(scaled_bitmap, none_letter_3x6, x, y, 3, 6, scale, color, bg_color);
    }
}

void print_bitmap(uint16_t *scaled_bitmap, Character c, uint16_t x, uint16_t y, uint16_t scale,
                  uint16_t color, uint16_t bg_color, FontSize font_size) {
    switch (font_size) {
    case FONT_SIZE_3x6:
        print_bitmap_3x6(scaled_bitmap, c, x, y, scale, color, bg_color);
        break;
    case FONT_SIZE_3x5:
        print_bitmap_3x5(scaled_bitmap, c, x, y, scale, color, bg_color);
        break;
    case FONT_SIZE_5x8:
        print_bitmap_5x8(scaled_bitmap, c, x, y, scale, color, bg_color);
        break;
    case FONT_SIZE_5x7:
        print_bitmap_5x7(scaled_bitmap, c, x, y, scale, color, bg_color);
        break;
    }
}

void print_line_horizontal(uint8_t *buf_frame, uint16_t start_x, uint16_t end_x, uint16_t start_y,
                           uint16_t end_y, uint16_t scale, uint16_t color) {
    struct display_buffer_descriptor horizontal_line_desc;

    uint16_t horizontal_line_len = end_x - start_x + scale;

    horizontal_line_desc.buf_size = horizontal_line_len * scale;
    horizontal_line_desc.pitch = horizontal_line_len;
    horizontal_line_desc.width = horizontal_line_len;
    horizontal_line_desc.height = scale;

    fill_buffer_color(buf_frame, horizontal_line_desc.buf_size * 2u, color);
    display_write(display_dev, start_x, start_y, &horizontal_line_desc, buf_frame);
    display_write(display_dev, start_x, end_y, &horizontal_line_desc, buf_frame);
}

void print_line_vertical(uint8_t *buf_frame, uint16_t start_x, uint16_t end_x, uint16_t start_y,
                         uint16_t end_y, uint16_t scale, uint16_t color) {
    struct display_buffer_descriptor vertical_line_desc;

    uint16_t vertical_line_len = end_y - start_y + scale;

    vertical_line_desc.buf_size = vertical_line_len * scale;
    vertical_line_desc.pitch = scale;
    vertical_line_desc.width = scale;
    vertical_line_desc.height = vertical_line_len;

    fill_buffer_color(buf_frame, vertical_line_desc.buf_size * 2u, color);
    display_write(display_dev, start_x, start_y, &vertical_line_desc, buf_frame);
    display_write(display_dev, end_x, start_y, &vertical_line_desc, buf_frame);
}

void print_rectangle(uint8_t *buf_frame, uint16_t start_x, uint16_t end_x, uint16_t start_y,
                     uint16_t end_y, uint16_t color, uint16_t scale) {
    print_line_horizontal(buf_frame, start_x, end_x, start_y, end_y, scale, color);
    print_line_vertical(buf_frame, start_x, end_x, start_y, end_y, scale, color);
}

void render_filled_rectangle(uint8_t *buf_area, uint8_t x, uint8_t y, uint8_t width,
                             uint8_t height) {
    struct display_buffer_descriptor buf_desc_area;
    buf_desc_area.pitch = width;
    buf_desc_area.width = width;
    buf_desc_area.height = height;
    display_write_wrapper(x, y, &buf_desc_area, buf_area);
}

void clear_screen() {
    fill_buffer_color(buf_screen_area, buf_screen_size, get_menu_bg_color());
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 12; j++) {
            render_filled_rectangle(buf_screen_area, i * screen_width, j * screen_height,
                                    screen_width, screen_height);
        }
    }
}

void set_all_colors(
    uint32_t splash_logo_color, uint32_t splash_created_by_color, uint32_t splash_bg_color,
    uint32_t snake_default_color, uint32_t snake_board_color, uint32_t snake_board_1_color,
    uint32_t food_color, uint32_t snake_color_0, uint32_t snake_color_1, uint32_t snake_color_2,
    uint32_t snake_color_3, uint32_t snake_color_4, uint32_t snake_color_5, uint32_t snake_color_6,
    uint32_t battery_num_color, uint32_t battery_percentage_color, uint32_t battery_bg_color,
    uint32_t battery_num_color_1, uint32_t battery_percentage_color_1, uint32_t battery_bg_color_1,
    uint32_t modifier_selected_color, uint32_t modifier_unselected_color,
    uint32_t modifier_bg_color, uint32_t symbol_selected_color, uint32_t symbol_unselected_color,
    uint32_t symbol_bg_color, uint32_t bt_num_color, uint32_t bt_bg_color,
    uint32_t bt_status_ok_color, uint32_t bt_status_not_ok_color, uint32_t bt_status_open_color,
    uint32_t bt_status_bg_color, uint32_t theme_font_color, uint32_t theme_font_color_1,
    uint32_t theme_font_bg_color, uint32_t layer_font_color, uint32_t layer_font_bg_color,
    uint32_t logo_font_color, uint32_t logo_snake_color, uint32_t logo_bg_color,
    uint32_t frame_color, uint32_t frame_color_1, uint32_t menu_bg_color, uint32_t wpm_font_color,
    uint32_t wpm_font_1_color, uint32_t wpm_font_bg_color) {
    set_splash_logo_color(splash_logo_color);
    set_splash_created_by_color(splash_created_by_color);
    set_splash_bg_color(splash_bg_color);

    set_snake_default_color(snake_default_color);
    set_snake_board_color(snake_board_color);
    set_snake_board_1_color(snake_board_1_color);

    set_food_color(food_color);
    set_snake_color_0(snake_color_0);
    set_snake_color_1(snake_color_1);
    set_snake_color_2(snake_color_2);
    set_snake_color_3(snake_color_3);
    set_snake_color_4(snake_color_4);
    set_snake_color_5(snake_color_5);
    set_snake_color_6(snake_color_6);

    set_battery_num_color(battery_num_color);
    set_battery_percentage_color(battery_percentage_color);
    set_battery_bg_color(battery_bg_color);
    set_battery_num_color_1(battery_num_color_1);
    set_battery_percentage_color_1(battery_percentage_color_1);
    set_battery_bg_color_1(battery_bg_color_1);

    set_modifier_selected_color(modifier_selected_color);
    set_modifier_unselected_color(modifier_unselected_color);
    set_modifier_bg_color(modifier_bg_color);
    set_symbol_selected_color(symbol_selected_color);
    set_symbol_unselected_color(symbol_unselected_color);
    set_symbol_bg_color(symbol_bg_color);
    set_bt_num_color(bt_num_color);
    set_bt_bg_color(bt_bg_color);
    set_bt_status_ok_color(bt_status_ok_color);
    set_bt_status_not_ok_color(bt_status_not_ok_color);
    set_bt_status_open_color(bt_status_open_color);
    set_bt_status_bg_color(bt_status_bg_color);

    set_theme_font_color(theme_font_color);
    set_theme_font_color_1(theme_font_color_1);
    set_theme_font_bg_color(theme_font_bg_color);

    set_layer_font_color(layer_font_color);
    set_layer_font_bg_color(layer_font_bg_color);

    set_logo_font_color(logo_font_color);
    set_logo_snake_color(logo_snake_color);
    set_logo_bg_color(logo_bg_color);

    set_frame_color(frame_color);
    set_frame_color_1(frame_color_1);
    set_menu_bg_color(menu_bg_color);

    set_wpm_font_color(wpm_font_color);
    set_wpm_font_1_color(wpm_font_1_color);
    set_wpm_font_bg_color(wpm_font_bg_color);
}

void set_colorscheme(uint32_t primary, uint32_t secondary, uint32_t background1,
                     uint32_t background2, uint32_t color5, uint32_t color6) {
    set_splash_logo_color(primary);
    set_splash_created_by_color(background1);
    set_splash_bg_color(background2);

    set_snake_default_color(secondary);
    set_snake_board_color(background2);
#ifdef CONFIG_CHECKERED_BOARD
    set_snake_board_1_color(background1);
#else
    set_snake_board_1_color(background2);
#endif

    set_food_color(primary);
    set_snake_color_0(primary);
    set_snake_color_1(secondary);
    set_snake_color_2(background1);
    set_snake_color_3(secondary);
    set_snake_color_4(primary);
    set_snake_color_5(secondary);
    set_snake_color_6(background1);

    set_battery_num_color(primary);
    set_battery_percentage_color(background1);
    set_battery_bg_color(background2);
    set_battery_num_color_1(primary);
    set_battery_percentage_color_1(background1);
    set_battery_bg_color_1(background2);

    set_modifier_selected_color(primary);
    set_modifier_unselected_color(background1);
    set_modifier_bg_color(background2);
    set_symbol_selected_color(primary);
    set_symbol_unselected_color(background1);
    set_symbol_bg_color(background2);
    set_bt_num_color(secondary);
    set_bt_bg_color(background2);
    set_bt_status_ok_color(background1);
    set_bt_status_not_ok_color(background1);
    set_bt_status_open_color(background1);
    set_bt_status_bg_color(background2);

    set_theme_font_color(primary);
    set_theme_font_color_1(primary);
    set_theme_font_bg_color(background2);

    set_layer_font_color(primary);
    set_layer_font_bg_color(background2);

    set_logo_font_color(primary);
    set_logo_snake_color(secondary);
    set_logo_bg_color(background2);

    set_frame_color(background1);
    set_frame_color_1(darken_color(background1, 0.2));
    set_menu_bg_color(background2);

    set_wpm_font_color(primary);
    set_wpm_font_1_color(primary);
    set_wpm_font_bg_color(background2);
}

void print_string(uint16_t *scaled_bitmap, Character str[], uint16_t x, uint16_t y, uint16_t scale,
                  uint16_t color, uint16_t bg_color, FontSize font_size, uint16_t gap_pixels,
                  uint8_t str_len) {
    uint16_t string_font_width_scaled = 0;
    if (font_size == FONT_SIZE_3x6 || font_size == FONT_SIZE_3x5) {
        string_font_width_scaled = 3 * scale;
    }
    if (font_size == FONT_SIZE_5x7 || font_size == FONT_SIZE_5x8) {
        string_font_width_scaled = 5 * scale;
    }
    if (string_font_width_scaled == 0) {
        return;
    }

    for (uint8_t i = 0; i < str_len; i++) {
        Character c = str[i];
        uint16_t actual_x = x + (string_font_width_scaled * i) + (gap_pixels * i);
        print_bitmap(scaled_bitmap, c, actual_x, y, scale, color, bg_color, font_size);
    }
}

static Character char_to_enum(char ch) {
    if (ch >= '0' && ch <= '9') {
        return (Character)(CHAR_0 + (ch - '0'));
    } else if (ch >= 'A' && ch <= 'Z') {
        return (Character)(CHAR_A + (ch - 'A'));
    } else if (ch >= 'a' && ch <= 'z') {
        return (Character)(CHAR_A + (ch - 'a')); // convert lowercase to enum
    } else if (ch == ':') {
        return CHAR_COLON;
    } else if (ch == '-') {
        return CHAR_DASH;
    } else if (ch == '%') {
        return CHAR_PERCENTAGE;
    } else {
        return CHAR_NONE; // fallback for unsupported characters
    }
}

void print_char_array(uint16_t *scaled_bitmap, char *str, uint16_t x, uint16_t y, uint16_t scale,
                      uint16_t color, uint16_t bg_color, FontSize font_size, uint16_t gap_pixels,
                      uint8_t str_len, uint8_t limit) {
    uint16_t string_font_width_scaled = 0;
    if (font_size == FONT_SIZE_3x6 || font_size == FONT_SIZE_3x5) {
        string_font_width_scaled = 3 * scale;
    }
    if (font_size == FONT_SIZE_5x7 || font_size == FONT_SIZE_5x8) {
        string_font_width_scaled = 5 * scale;
    }
    if (string_font_width_scaled == 0) {
        return;
    }

    for (uint8_t i = 0; i < str_len && i < limit; i++) {
        Character c = char_to_enum(str[i]);
        uint16_t actual_x = x + (string_font_width_scaled * i) + (gap_pixels * i);
        print_bitmap(scaled_bitmap, c, actual_x, y, scale, color, bg_color, font_size);
    }
}

void print_repeat_char(uint16_t *scaled_bitmap, Character c, uint16_t x, uint16_t y, uint16_t scale,
                       uint16_t color, uint16_t bg_color, FontSize font_size, uint16_t gap_pixels,
                       uint8_t str_len, uint8_t limit) {
    uint16_t string_font_width_scaled = 0;
    if (font_size == FONT_SIZE_3x6 || font_size == FONT_SIZE_3x5) {
        string_font_width_scaled = 3 * scale;
    }
    if (font_size == FONT_SIZE_5x7 || font_size == FONT_SIZE_5x8) {
        string_font_width_scaled = 5 * scale;
    }
    if (string_font_width_scaled == 0) {
        return;
    }

    for (uint8_t i = 0; i < str_len && i < limit; i++) {
        uint16_t actual_x = x + (string_font_width_scaled * i) + (gap_pixels * i);
        print_bitmap(scaled_bitmap, c, actual_x, y, scale, color, bg_color, font_size);
    }
}

SlotSide get_slot_to_print(InfoSlot slot) {
    if (slot == left_slot) {
        return SLOT_SIDE_LEFT;
    }
    if (slot == right_slot) {
        return SLOT_SIDE_RIGHT;
    }
    return SLOT_SIDE_NONE;
}