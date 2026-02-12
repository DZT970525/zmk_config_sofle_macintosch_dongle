/*
 *
 * Copyright (c) 2021 Darryl deHaan
 * SPDX-License-Identifier: MIT
 *
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/split_peripheral_status_changed.h>

#ifdef CONFIG_USE_BUZZER
#include "helpers/buzzer.h"
#endif

static bool peripheral_status_initialized = true;

struct peripheral_status_state {
    bool connected;
};

static struct peripheral_status_state get_peripheral_state(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev = as_zmk_split_peripheral_status_changed(eh);
    return (struct peripheral_status_state){.connected = ev->connected};
}

static void alarm_peripheral_status(struct peripheral_status_state state) {
    // do we need this ?
}

static void peripheral_status_update_cb(struct peripheral_status_state state) {
    if (peripheral_status_initialized) {
        alarm_peripheral_status(state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state, peripheral_status_update_cb, get_peripheral_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

void zmk_widget_peripheral_status_init() {
    widget_peripheral_status_init();
    peripheral_status_initialized = true;
}