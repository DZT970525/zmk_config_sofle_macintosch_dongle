#include <zephyr/logging/log.h>
#include <lvgl.h>
#include "helpers/display.h"
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static bool second_cycle = false;
static bool animation_running = false;
static bool animation_initialized = false;

static uint8_t logo_animation_width = 15;
static uint8_t logo_animation_height = 6;
static uint8_t logo_animation_scale = 2;

static uint16_t logo_animation_x = 24;
static uint16_t logo_animation_y = 15;

static uint16_t animation_sections_total;
static uint16_t animation_cycle_count = 0;

static uint16_t snake_logo_x = 44;
static uint16_t snake_logo_y = 38;
static uint16_t snake_logo_font_width = 3;
static uint16_t snake_logo_font_height = 5;
static uint16_t snake_logo_font_scale = 8;

static uint16_t *snake_animation_buf;
static uint16_t *snake_logo_buf;

typedef struct section {
    uint16_t x;
    uint16_t y;
    uint8_t num;
} Section;

const static uint16_t animation_snake_head[4][36] = {
    {
        1, 1, 1, 1, 1, 0,
        1, 1, 0, 0, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 0, 0, 1, 1,
        1, 1, 1, 1, 1, 0,
    }, {
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 0, 1, 1, 0, 1,
        1, 0, 1, 1, 0, 1,
        1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 0,
    }, {
        0, 1, 1, 1, 1, 1,
        1, 1, 0, 0, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 0, 0, 1, 1,
        0, 1, 1, 1, 1, 1,
    }, {
        0, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1,
        1, 0, 1, 1, 0, 1,
        1, 0, 1, 1, 0, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
    }
};

const static uint16_t animation_snake_tail[4][36] = {
    {
        0, 0, 0, 1, 1, 1,
        0, 0, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1,
        0, 0, 1, 1, 1, 1,
        0, 0, 0, 1, 1, 1,
    }, {
        0, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 0, 0,
        0, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
    }, {
        1, 1, 1, 0, 0, 0,
        1, 1, 1, 1, 0, 0,
        1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 0, 0,
        1, 1, 1, 0, 0, 0,
    }, {
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 0,
        0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0,
    },
};

const static uint16_t animation_snake_food[] = {
    0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 0, 0,
    0, 1, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 0,
    0, 0, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0,
};

const static uint16_t animation_snake_body[] = {
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
};

const static uint16_t animation_snake_body_corners[4][36] = {
    {
        1, 1, 1, 1, 0, 0,
        1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
    },
    {
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 0, 0,
    },
    {
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1,
        0, 0, 1, 1, 1, 1,
    },
    {
        0, 0, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
    },
};

const static uint16_t animation_space[] = {
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};

uint8_t get_section_num_on_corners(Section s) {
    uint8_t num = s.num;
    if(s.x == 0 && s.y == 0) {
        num = 3;
    }
    if (s.x == logo_animation_width && s.y == 0) {
        num = 0;
    }
    if (s.x == logo_animation_width && s.y == logo_animation_height) {
        num = 1;
    }
    if (s.x == 0 && s.y == logo_animation_height) {
        num = 2;
    }
    return num;
}

void print_animation(Section s, uint16_t animation_sprite[]) {
    uint16_t x = (s.x * 6 * logo_animation_scale) + logo_animation_x;
    uint16_t y = (s.y * 6 * logo_animation_scale) + logo_animation_y;
    render_bitmap(snake_animation_buf, animation_sprite, x, y, 6, 6, logo_animation_scale, get_logo_snake_color(), get_logo_bg_color());
}

void print_animation_body(Section s) {
    uint8_t num = get_section_num_on_corners(s);
    if (num != s.num) {
        print_animation(s, animation_snake_body_corners[num]);
        return;
    }
    print_animation(s, animation_snake_body);
}

void print_animation_space(Section s) {
    print_animation(s, animation_space);
}

void print_animation_tail(Section s) {
    print_animation(s, animation_snake_tail[s.num]);
}

void print_animation_head(Section s) {
    uint8_t num = get_section_num_on_corners(s);
    print_animation(s, animation_snake_head[num]);
}

void print_animation_food(Section s) {
    print_animation(s, animation_snake_food);
}

struct section get_section(uint16_t cc) {
    cc = cc % animation_sections_total;
    Section s;
    if (cc < logo_animation_width) {
        s.x = cc;
        s.y = 0;
        s.num = 0;
        return s;
    }
    if (cc >= logo_animation_width && cc < logo_animation_width + logo_animation_height) {
        s.x = logo_animation_width;
        s.y = cc - logo_animation_width;
        s.num = 1;
        return s;
    }
    if (cc >= logo_animation_width + logo_animation_height && cc < (logo_animation_width * 2) + logo_animation_height) {
        s.x = (logo_animation_width * 2) + logo_animation_height - cc;
        s.y = logo_animation_height;
        s.num = 2;
        return s;
    }
    if (cc >= (logo_animation_width * 2) + logo_animation_height) {
        s.x = 0;
        s.y = animation_sections_total - cc;
        s.num = 3;
        return s;
    }
}

void print_initial_animation() {
    Character logo_chars[] = {
        CHAR_S,
        CHAR_N,
        CHAR_A,
        CHAR_K,
        CHAR_E,
    };
    uint16_t char_gap_pixels = snake_logo_font_scale;
    uint8_t logo_chars_len = 5;
    print_string(snake_logo_buf, logo_chars, snake_logo_x, snake_logo_y, snake_logo_font_scale, get_logo_font_color(), get_logo_bg_color(), FONT_SIZE_3x5, char_gap_pixels, logo_chars_len);
    
    for (uint16_t i; i < animation_sections_total; i++) {
        Section s = get_section(i);
        if (i == 5 || i == 6 || i == 7) {
            print_animation_space(s);
        } else {
            print_animation_body(s);
        }
    }
}

void next_animation_count() {
    if (!second_cycle) {
        second_cycle = true;
        return;
    }
    if (animation_cycle_count + 1 >= animation_sections_total) {
        animation_cycle_count = 0;
    } else {
        animation_cycle_count++;
    }
    second_cycle = false;
}

void reset_animation_count() {
    animation_cycle_count = 5;
}

void logo_animation_timer(lv_timer_t * timer) {
    if (!animation_running) {
        return;
    }

    if (!animation_initialized) {
        print_initial_animation();
        animation_initialized = true;
    }

    // first snake
    if (second_cycle) {
        if (animation_cycle_count == 0) {
            print_animation_body(get_section(animation_sections_total - 1));
        } else {
            print_animation_body(get_section(animation_cycle_count - 1));
        }
        print_animation_body(get_section(animation_cycle_count));
        print_animation_head(get_section(animation_cycle_count + 1));
    } else {
        print_animation_space(get_section(animation_cycle_count + 2));
        print_animation_space(get_section(animation_cycle_count + 3));
        print_animation_tail(get_section(animation_cycle_count + 4));
    }

    // second snake
    uint16_t offset = (animation_sections_total / 2) + animation_cycle_count;
    if (second_cycle) {
        if (animation_cycle_count == 0) {
            print_animation_body(get_section(animation_sections_total - 1));
        } else {
            print_animation_body(get_section(offset - 1));
        }
        print_animation_body(get_section(offset));
        print_animation_head(get_section(offset + 1));
    } else {
        print_animation_space(get_section(offset + 2));
        print_animation_space(get_section(offset + 3));
        print_animation_tail(get_section(offset + 4));
    }

    next_animation_count();
}

void stop_animation() {
    animation_running = false;
}

void start_animation() {
    print_initial_animation();
    reset_animation_count();
    animation_initialized = false;
    animation_running = true;
}

void logo_animation_init() {
    animation_sections_total = (logo_animation_width * 2) + (logo_animation_height * 2);

    snake_animation_buf = k_malloc(36 * 2 * sizeof(uint16_t));

    snake_logo_buf = k_malloc((snake_logo_font_width * snake_logo_font_scale) * (snake_logo_font_height * snake_logo_font_scale) * 2u);
}
