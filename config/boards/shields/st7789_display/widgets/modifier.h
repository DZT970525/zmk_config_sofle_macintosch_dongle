/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

void print_modifier();
void zmk_widget_modifier_init();
void start_modifier_status();
void stop_modifier_status();