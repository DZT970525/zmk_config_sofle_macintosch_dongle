/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
 
 #pragma once

#include <zephyr/kernel.h>
#include <lvgl.h>

void initialize_snake_game(void);
void start_snake(void);
void stop_snake(void);
void display_setup(void);
void zmk_widget_snake_init(void);

void apply_theme_snake(void);
void set_snake_board_width(uint8_t width);
void set_snake_board_height(uint8_t height);
void set_snake_pixel_size(uint8_t pixel_size);