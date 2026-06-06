#pragma once

#include <stdint.h>
#include <stdbool.h>

void zmk_widget_bagua_init(void);
void start_bagua(void);
void stop_bagua(void);
void bagua_set_active_profile(uint8_t profile);
void bagua_set_usb_connected(bool connected);
void bagua_set_num_lock(bool on);
void bagua_set_profile_state(bool connected, bool bonded);
void bagua_set_layer(uint8_t layer);
