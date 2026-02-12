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