/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_trackball_led

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/hid_indicators.h>
#include <zmk/rgb_underglow.h>

#include "bbtrackball_input_handler.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ==== 配置 ==== */
#define BRT_MIN 10
#define BRT_MAX CONFIG_BBtrackball_max_brightness
#define BRT_STEP 5
#define ANIMATION_INTERVAL 50
#define AUTO_OFF_DELAY 1500
#define POLL_INTERVAL 50
#define FADE_STEP 2
#define FADE_INTERVAL 10

/* ==== LED 数量 ==== */
#define CHILD_COUNT(...) +1
#define DT_NUM_CHILD(node_id) (DT_FOREACH_CHILD(node_id, CHILD_COUNT))
#define LED_NUM (DT_NUM_CHILD(DT_CHOSEN(zmk_trackball_led)))

static const struct device *const led_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_trackball_led));

/* ==== 工作队列 ==== */
static struct k_work_delayable anim_work;
static struct k_work_delayable poll_work;
static struct k_work_delayable off_work;
static struct k_work_delayable fade_in_work;
static struct k_work_delayable fade_out_work;

/* ==== 状态变量 ==== */
static bool caps_on = false;
static bool last_move_state = false;

static uint8_t ug_last_brt = 0;

static bool fade_in_active = false;
static bool fade_out_active = false;
static uint8_t current_brt = 0;
static uint8_t target_brt = 0;

static uint8_t last_valid_brt = BRT_MIN;

/* ==== 设置亮度 ==== */
static void set_led_brightness(uint8_t level) {
    for (int i = 0; i < LED_NUM; i++) {
        led_set_brightness(led_dev, i, level);
    }
    if (level > 0) {
        last_valid_brt = level;
    }
}

/* ============================================================================================
 *  渐亮：fade_in
 * ============================================================================================ */
static void fade_in_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!fade_in_active || caps_on || fade_out_active)
        return;

    if (current_brt < target_brt) {
        current_brt = MIN(current_brt + FADE_STEP, target_brt);
        set_led_brightness(current_brt);
        k_work_reschedule(&fade_in_work, K_MSEC(FADE_INTERVAL));
    } else {
        fade_in_active = false;
        LOG_DBG("Fade-in done (%d)", current_brt);
    }
}

/* ============================================================================================
 *  渐暗：fade_out —— 🔥 修复灯灭不掉的核心逻辑
 *  fade_out_active=true 时，poll_handler 绝对不会亮灯
 * ============================================================================================ */
static void fade_out_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!fade_out_active || caps_on)
        return;

    if (current_brt > 0) {
        current_brt = (current_brt > FADE_STEP) ? current_brt - FADE_STEP : 0;
        set_led_brightness(current_brt);
        k_work_reschedule(&fade_out_work, K_MSEC(FADE_INTERVAL));
    } else {
        fade_out_active = false;
        LOG_DBG("Fade-out complete -> LED OFF");
    }
}

/* ============================================================================================
 * 自动熄灭计时器触发 fade-out
 * ============================================================================================ */
static void off_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (caps_on || trackball_is_moving())
        return;

    fade_in_active = false;
    fade_out_active = true;

    k_work_schedule(&fade_out_work, K_NO_WAIT);

    LOG_DBG("Auto-off -> start fade-out");
}

/* ============================================================================================
 * CapsLock 呼吸动画（保持不变）
 * ============================================================================================ */
static bool anim_up = true;
static uint8_t anim_brt = BRT_MIN;

static void anim_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!caps_on)
        return;

    set_led_brightness(anim_brt);

    if (anim_up) {
        anim_brt += BRT_STEP;
        if (anim_brt >= BRT_MAX) {
            anim_brt = BRT_MAX;
            anim_up = false;
        }
    } else {
        anim_brt -= BRT_STEP;
        if (anim_brt <= BRT_MIN) {
            anim_brt = BRT_MIN;
            anim_up = true;
        }
    }

    k_work_reschedule(&anim_work, K_MSEC(ANIMATION_INTERVAL));
}

/* ============================================================================================
 * poll_handler —— 🟢 修复核心：fade_out_active 时禁止任何亮灯操作
 * ============================================================================================ */
static void poll_handler(struct k_work *work) {
    ARG_UNUSED(work);

    bool moving = trackball_is_moving();
    uint8_t ug_brt = zmk_rgb_underglow_calc_brt(0).b;

    /* fade_out 中禁止任何点亮 */
    if (fade_out_active || caps_on) {
        last_move_state = moving;
        k_work_reschedule(&poll_work, K_MSEC(POLL_INTERVAL));
        return;
    }

    /* 轨迹球移动触发渐亮 */
    /* 轨迹球移动触发渐亮（仅在 LED 当前 = 0 时执行）*/
    if (moving) {
        if (current_brt > 0) {
            /* 💡 LED 已亮，不重复 fade-in */
            last_move_state = moving;
            k_work_reschedule(&poll_work, K_MSEC(POLL_INTERVAL));
            return;
        }

        if (!last_move_state) {
            /* 仅首次点亮时触发 fade-in */
            fade_in_active = true;
            fade_out_active = false;

            current_brt = 0;
            target_brt = ug_last_brt > 0 ? MAX(BRT_MIN, ug_brt) : BRT_MAX;

            k_work_cancel_delayable(&fade_out_work);
            k_work_schedule(&fade_in_work, K_NO_WAIT);

            LOG_DBG("Trackball move -> start fade-in to %d", target_brt);
        }

        k_work_cancel_delayable(&off_work);
    }

    /* 轨迹球停止 -> 启动 fade-out 计时 */
    if (!moving && last_move_state && current_brt > 0) {
        k_work_reschedule(&off_work, K_MSEC(AUTO_OFF_DELAY));
        LOG_DBG("Trackball stop -> schedule fade-out");
    }

    /* UnderGlow 亮度触发渐亮 */
    if (ug_brt != ug_last_brt) {
        ug_last_brt = ug_brt;

        if (ug_brt > 0) {
            fade_in_active = true;
            fade_out_active = false;

            current_brt = 0;
            target_brt = MAX(BRT_MIN, ug_brt);

            k_work_cancel_delayable(&fade_out_work);
            k_work_schedule(&fade_in_work, K_NO_WAIT);

            k_work_reschedule(&off_work, K_MSEC(AUTO_OFF_DELAY));

            LOG_DBG("Underglow changed -> Fade-in to %d", target_brt);
        }
    }

    last_move_state = moving;
    k_work_reschedule(&poll_work, K_MSEC(POLL_INTERVAL));
}

/* ============================================================================================
 * HID 监听 CapsLock
 * ============================================================================================ */
static int hid_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    if (!ev)
        return ZMK_EV_EVENT_BUBBLE;

    bool new_caps = ev->indicators & (1 << 1);

    if (new_caps != caps_on) {
        caps_on = new_caps;

        fade_in_active = false;
        fade_out_active = false;

        if (caps_on) {
            current_brt = 0;
            anim_brt = BRT_MIN;
            anim_up = true;
            k_work_schedule(&anim_work, K_NO_WAIT);
        } else {
            // 取消 CapsLock -> 立即熄灭 LED
            k_work_cancel_delayable(&anim_work);
            k_work_cancel_delayable(&fade_in_work);
            k_work_cancel_delayable(&fade_out_work);
            current_brt = 0;
            set_led_brightness(0);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

/* ============================================================================================
 * 初始化
 * ============================================================================================ */
static int trackball_led_init(void) {
    if (!device_is_ready(led_dev)) {
        LOG_ERR("LED device not ready");
        return -ENODEV;
    }

    k_work_init_delayable(&anim_work, anim_handler);
    k_work_init_delayable(&poll_work, poll_handler);
    k_work_init_delayable(&off_work, off_handler);
    k_work_init_delayable(&fade_in_work, fade_in_handler);
    k_work_init_delayable(&fade_out_work, fade_out_handler);

    set_led_brightness(0);
    current_brt = 0;

    k_work_schedule(&poll_work, K_NO_WAIT);

    LOG_INF("Trackball LED driver ready (%d LEDs)", LED_NUM);
    return 0;
}

SYS_INIT(trackball_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
ZMK_LISTENER(trackball_led_listener, hid_listener);
ZMK_SUBSCRIPTION(trackball_led_listener, zmk_hid_indicators_changed);

/* API */
uint8_t trackball_led_get_last_valid_brightness(void) { return last_valid_brt; }
