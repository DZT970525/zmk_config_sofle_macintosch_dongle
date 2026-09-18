/*
 * Copyright (c) 2023 ZitaoTech
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <zmk/backlight.h>
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>

#include "trackpad_led.h"

#define HID_INDICATORS_CAPS_LOCK (1 << 1)

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_HAS_CHOSEN(zmk_trackpad_led),
             "CONFIG_ZMK_TRACKPAD_LED enabled but no zmk,trackpad_led chosen node found");

static const struct device *const led_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_trackpad_led));

#define CHILD_COUNT(...) +1
#define DT_NUM_CHILD(node_id) (DT_FOREACH_CHILD(node_id, CHILD_COUNT))
#define INDICATOR_LED_NUM_LEDS (DT_NUM_CHILD(DT_CHOSEN(zmk_trackpad_led)))

#define BRT_MIN 10
#define BRT_MAX 100
#define BRT_LOW 20
#define BRT_STEP 5
#define ANIMATION_INTERVAL_MS 20
#define BOOT_BRIGHTNESS 40
#define BOOT_ON_MS 3000
#define TOUCH_ON_MS 5000

static struct k_work motion_work;
static struct k_work_delayable touch_off_work;
static struct k_work_delayable boot_off_work;
static struct k_work_delayable animation_work;
static atomic_t indicator_ready;

static bool capslock_on;
static bool boot_active;
static bool touch_active;
static bool animation_increasing = true;
static uint8_t brightness = BRT_MIN;
static uint8_t last_valid_brt = BRT_MAX;

static void set_led_brightness(uint8_t level) {
    if (!device_is_ready(led_dev)) {
        LOG_ERR("Trackpad LED device not ready");
        return;
    }

    for (int i = 0; i < INDICATOR_LED_NUM_LEDS; i++) {
        int err = led_set_brightness(led_dev, i, level);
        if (err < 0) {
            LOG_ERR("Failed to set trackpad LED[%d] brightness: %d", i, err);
        }
    }
}

uint8_t indicator_tp_get_last_valid_brightness(void) {
    /* Backlight value affects pointer sensitivity, but never triggers the LED. */
    uint8_t current_brt = zmk_backlight_get_brt();
    if (current_brt > 0) {
        last_valid_brt = MAX(BRT_MIN, current_brt);
    }
    return last_valid_brt;
}

static void boot_off_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    boot_active = false;
    if (!capslock_on && !touch_active) {
        set_led_brightness(0);
    }
}

static void touch_off_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    touch_active = false;
    if (!capslock_on && !boot_active) {
        set_led_brightness(0);
    } else if (boot_active && !capslock_on) {
        set_led_brightness(BOOT_BRIGHTNESS);
    }
}

/* Runs in the system work queue, not in the GPIO ISR. */
static void motion_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    touch_active = true;
    if (!capslock_on) {
        set_led_brightness(indicator_tp_get_last_valid_brightness());
    }
    k_work_reschedule(&touch_off_work, K_MSEC(TOUCH_ON_MS));
}

void indicator_tp_motion_triggered(void) {
    if (atomic_get(&indicator_ready)) {
        k_work_submit(&motion_work);
    }
}

static void animation_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!capslock_on) {
        return;
    }

    if (animation_increasing) {
        brightness += BRT_STEP;
        if (brightness >= BRT_MAX) {
            brightness = BRT_MAX;
            animation_increasing = false;
        }
    } else {
        brightness -= BRT_STEP;
        if (brightness <= BRT_LOW) {
            brightness = BRT_LOW;
            animation_increasing = true;
        }
    }

    set_led_brightness(brightness);
    k_work_reschedule(&animation_work, K_MSEC(ANIMATION_INTERVAL_MS));
}

static int trackpad_led_hid_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    bool new_capslock = (ev->indicators & HID_INDICATORS_CAPS_LOCK) != 0;
    if (new_capslock == capslock_on) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    capslock_on = new_capslock;
    if (capslock_on) {
        brightness = BRT_MIN;
        animation_increasing = true;
        k_work_reschedule(&animation_work, K_NO_WAIT);
    } else {
        k_work_cancel_delayable(&animation_work);
        if (touch_active) {
            set_led_brightness(indicator_tp_get_last_valid_brightness());
        } else if (boot_active) {
            set_led_brightness(BOOT_BRIGHTNESS);
        } else {
            set_led_brightness(0);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(trackpad_led_listener, trackpad_led_hid_listener);
ZMK_SUBSCRIPTION(trackpad_led_listener, zmk_hid_indicators_changed);

static int indicator_tp_init(void) {
    if (!device_is_ready(led_dev)) {
        return -ENODEV;
    }

    boot_active = true;
    indicator_tp_get_last_valid_brightness();

    k_work_init(&motion_work, motion_work_handler);
    k_work_init_delayable(&touch_off_work, touch_off_work_handler);
    k_work_init_delayable(&boot_off_work, boot_off_work_handler);
    k_work_init_delayable(&animation_work, animation_work_handler);
    set_led_brightness(BOOT_BRIGHTNESS);
    atomic_set(&indicator_ready, 1);
    k_work_reschedule(&boot_off_work, K_MSEC(BOOT_ON_MS));
    return 0;
}

SYS_INIT(indicator_tp_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
