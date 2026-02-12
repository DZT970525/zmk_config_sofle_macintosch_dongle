/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

void print_wpm();
void zmk_widget_wpm_init();
void start_wpm_status();
void stop_wpm_status();