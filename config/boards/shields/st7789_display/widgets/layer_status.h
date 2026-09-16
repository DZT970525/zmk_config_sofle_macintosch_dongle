/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

void zmk_widget_layer_init(void);
void start_layer_status(void);
void stop_layer_status(void);
void print_layer(void);
void print_layer_font_text(uint16_t *render_buffer, const char *text, uint8_t length, uint16_t x,
                           uint16_t y, uint8_t factor, uint16_t color, uint16_t bg_color);
