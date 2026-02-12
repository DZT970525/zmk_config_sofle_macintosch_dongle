/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <lvgl.h>

void zmk_widget_action_button_init(void);
void start_action_button(bool is_menu_on);
void set_theme_threshold(uint16_t term_ms);
void set_mute_threshold(uint16_t term_ms);