/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
 
 #pragma once

#include <zephyr/kernel.h>
#include <lvgl.h>

extern uint32_t themes_colors[][6];

void set_next_theme(void);
void print_themes(void);
void theme_init(void);