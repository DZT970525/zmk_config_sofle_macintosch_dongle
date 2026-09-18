/*
 * A320 trackpad HID over I2C Driver (Zephyr Input Subsystem)
 * Interrupt-driven version (minimal modification)
 * Copyright (c) 2025 ZitaoTech
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT avago_a320

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <stdlib.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zmk/hid.h>

#include "trackpad_led.h"
#include "a320.h"

LOG_MODULE_REGISTER(a320, CONFIG_A320_LOG_LEVEL);

/* ========= ⭐ A320 专用 Work Queue ========= */
#define A320_WORKQ_STACK_SIZE 2048
#define A320_WORKQ_PRIORITY 5

/* ========= ⭐ NEW: I2C Mutex ========= */
static struct k_mutex a320_i2c_mutex;

K_THREAD_STACK_DEFINE(a320_workq_stack, A320_WORKQ_STACK_SIZE);
static struct k_work_q a320_workq;

/* ========================================================================= */
/* 鼠标与滚轮可调参数 (已映射至 Kconfig，用户可在 .conf 中配置)                 */
/* ========================================================================= */

// --- 滚轮方向配置 ---
#define SCROLL_X_DIR (-CONFIG_A320_SCROLL_X_DIR)
#define SCROLL_Y_DIR CONFIG_A320_SCROLL_Y_DIR

// --- 滚轮灵敏度与粒度配置 ---
#define SCROLL_INPUT_MAX CONFIG_A320_SCROLL_INPUT_MAX
#define SCROLL_DIVISOR_SLOW CONFIG_A320_SCROLL_DIVISOR_SLOW
#define SCROLL_DIVISOR_FAST CONFIG_A320_SCROLL_DIVISOR_FAST

// --- Arrow key threshold / divisor ---
#define ARROW_DEADZONE CONFIG_A320_SCROLL_DEADZONE
#define ARROW_INPUT_MAX 128
#define ARROW_DIVISOR_SLOW CONFIG_A320_SCROLL_DIVISOR_SLOW
#define ARROW_DIVISOR_FAST CONFIG_A320_SCROLL_DIVISOR_FAST

// --- 防误触锁定比例配置 ---
#define DOMINANT_NUMERATOR CONFIG_A320_DOMINANT_NUMERATOR
#define DOMINANT_DENOMINATOR CONFIG_A320_DOMINANT_DENOMINATOR

// --- 鼠标指针基础配置 (Kconfig 为整数百分比，这里除以 100 转为浮点数) ---
#define MOUSE_BASE_SPEED (CONFIG_A320_MOUSE_BASE_SPEED_PERCENT / 100.0f)
#define MOUSE_SENS_BASE (CONFIG_A320_MOUSE_SENS_BASE_PERCENT / 100.0f)
#define MOUSE_SENS_STEP (CONFIG_A320_MOUSE_SENS_STEP_PERCENT / 100.0f)

/* ========= Motion GPIO ========= */

#define MOTION_GPIO_NODE DT_NODELABEL(gpio0)
#define MOTION_GPIO_PIN 5
#define MOTION_GPIO_FLAGS (GPIO_ACTIVE_LOW | GPIO_PULL_UP)

/* ========= A320 常量 ========= */
#define A320_I2C_ADDR_3B 0x3B
#define A320_I2C_ADDR_37 0x37
#define A320_DEFAULT_I2C_ADDR A320_I2C_ADDR_37

#define SLOW_KEY_MULTIPLIER 0.5f
#define TOUCH_IDLE_TIMEOUT 50 // 30~80ms 看手感
/* ========= Watch Dog ========= */
static uint32_t last_activity_time = 0;
#define A320_WDT_TIMEOUT 200
/* ========= 全局状态 ========= */
static bool scroll_key_pressed = false;
static bool arrow_key_pressed = false;
static bool slow_key_pressed = false;
static bool last_arrow_key_pressed = false;
uint32_t last_packet_time = 0;
static bool touched = false;

/* ==== HID indicators ==== */
static zmk_hid_indicators_t current_indicators;
#define HID_INDICATORS_CAPS_LOCK (1 << 1)
/* =========================
 *   HID indicator listener
 * ========================= */
static int hid_indicators_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    if (ev) {
        current_indicators = ev->indicators;
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(a320_hid_listener, hid_indicators_listener);
ZMK_SUBSCRIPTION(a320_hid_listener, zmk_hid_indicators_changed);

/* ========= Space + Slow 按键监听 ========= */
static int special_key_listener_cb(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (!ev)
        return 0;
    if (ev->position == 35) {
        arrow_key_pressed = ev->state;
        LOG_INF("Arrow position=49 %s", arrow_key_pressed ? "PRESSED" : "RELEASED");
    }

    // Scroll key (Space)
    if (ev->position == 62) {
        scroll_key_pressed = ev->state;
        LOG_INF("space position=49 %s", scroll_key_pressed ? "PRESSED" : "RELEASED");
    }

    // ★ NEW: Slow key
    if (ev->position == 37) {
        slow_key_pressed = ev->state;
        LOG_INF("slow_key position=37 %s", slow_key_pressed ? "PRESSED" : "RELEASED");
    }

    return 0;
}
ZMK_LISTENER(a320_special_key_listener, special_key_listener_cb);
ZMK_SUBSCRIPTION(a320_special_key_listener, zmk_position_state_changed);

struct a320_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec motion_gpio;
};

typedef int (*a320_read_packet_fn_t)(const struct device *dev, int8_t *dx, int8_t *dy);

struct a320_data {
    const struct device *dev;
    struct i2c_dt_spec i2c;
    a320_read_packet_fn_t read_packet;
    uint8_t detected_i2c_addr;
    struct k_work work;
    struct gpio_callback motion_cb_data;
    struct k_work_delayable enable_irq_work; // ⭐ 新增
    uint32_t last_packet_time;
    uint32_t last_scroll_time;
    bool last_scroll_mode;
    float scroll_residue_x;
    float scroll_residue_y;
    int16_t arrow_residue_x;
    int16_t arrow_residue_y;
};

/* Read the selected variant's registers, always releasing the I2C mutex. */
static int a320_read_registers(const struct device *dev, uint8_t reg, uint8_t *buf, size_t len) {
    struct a320_data *data = dev->data;
    k_mutex_lock(&a320_i2c_mutex, K_FOREVER);
    int ret = i2c_write_dt(&data->i2c, &reg, 1);
    if (ret < 0)
        goto out;
    ret = i2c_burst_read_dt(&data->i2c, reg, buf, len);
out:
    k_mutex_unlock(&a320_i2c_mutex);
    return ret;
}

/* Keep the original 0x3B dx/dy orientation. */
static int a320_read_packet_3b(const struct device *dev, int8_t *dx, int8_t *dy) {
    uint8_t buf[3] = {0};
    int ret = a320_read_registers(dev, 0x82, buf, sizeof(buf));
    if (ret < 0)
        return ret;
    *dx = (int8_t)buf[1];
    *dy = -(int8_t)buf[2];
    return 0;
}

/* The 0x37 variant has a different packet layout. */
static int a320_read_packet_37(const struct device *dev, int8_t *dx, int8_t *dy) {
    uint8_t buf[7] = {0};
    int ret = a320_read_registers(dev, 0x0A, buf, sizeof(buf));
    if (ret < 0)
        return ret;
    *dy = -(int8_t)buf[1];
    *dx = -(int8_t)buf[3];
    return 0;
}

static void a320_detect_variant(const struct device *dev) {
    struct a320_data *data = dev->data;
    const uint8_t candidates[] = {A320_I2C_ADDR_3B, A320_I2C_ADDR_37};
    uint8_t test_byte = 0;
    uint8_t found_addr = 0;

    for (size_t i = 0; i < ARRAY_SIZE(candidates); i++) {
        data->i2c.addr = candidates[i];
        if (i2c_read_dt(&data->i2c, &test_byte, 1) == 0) {
            found_addr = candidates[i];
            break;
        }
    }

    if (found_addr == 0) {
        found_addr = A320_DEFAULT_I2C_ADDR;
        LOG_WRN("Trackpad I2C not detected, fallback to 0x%02X", found_addr);
    } else {
        LOG_INF("Trackpad detected at I2C address 0x%02X", found_addr);
    }

    data->i2c.addr = found_addr;
    data->detected_i2c_addr = found_addr;
    data->read_packet =
        (found_addr == A320_I2C_ADDR_3B) ? a320_read_packet_3b : a320_read_packet_37;
}

/* Same speed scaling and fractional accumulation as the Q20 CM5 driver. */
static inline void process_cm5_scroll(const struct device *dev, struct a320_data *data,
                                      int16_t dx, int16_t dy, uint32_t now) {
    if (now - data->last_scroll_time > 60) {
        data->scroll_residue_x = 0.0f;
        data->scroll_residue_y = 0.0f;
    }
    data->last_scroll_time = now;

    float speed = sqrtf((float)dx * dx + (float)dy * dy);
    float scale;
    if (speed > 80.0f)
        scale = 0.05f;
    else if (speed > 40.0f)
        scale = 0.04f;
    else if (speed > 20.0f)
        scale = 0.03f;
    else if (speed > 5.0f)
        scale = 0.02f;
    else
        scale = 0.015f;

    data->scroll_residue_x += dx * scale;
    data->scroll_residue_y += dy * scale;
    int16_t out_x = (int16_t)data->scroll_residue_x;
    int16_t out_y = (int16_t)data->scroll_residue_y;
    data->scroll_residue_x -= out_x;
    data->scroll_residue_y -= out_y;
    if (out_x || out_y) {
        input_report_rel(dev, INPUT_REL_HWHEEL, -out_x, false, K_FOREVER);
        input_report_rel(dev, INPUT_REL_WHEEL, -out_y, true, K_FOREVER);
    }
}

static inline void process_arrow_axis(const struct device *dev, int16_t delta, int16_t *residue,
                                      uint16_t key_neg, uint16_t key_pos) {

    int abs_delta = abs(delta);

    if (abs_delta <= ARROW_DEADZONE) {
        return;
    }

    if (abs_delta > ARROW_INPUT_MAX) {
        abs_delta = ARROW_INPUT_MAX;
    }

    // ★ 非线性 divisor（更丝滑）
    float t = (float)abs_delta / SCROLL_INPUT_MAX;
    t = t * t;

    float f_div = SCROLL_DIVISOR_SLOW - (SCROLL_DIVISOR_SLOW - SCROLL_DIVISOR_FAST) * t;

    int divisor = (int)f_div;
    if (divisor < 1)
        divisor = 1;

    *residue += delta; // 替换掉 dir_mult
    int16_t arrow_ticks = *residue / divisor;
    if (arrow_ticks != 0) {
        uint16_t key = (arrow_ticks > 0) ? key_pos : key_neg;

        // 触发 key press + release（脉冲）
        input_report_key(dev, key, 1, true, K_FOREVER);
        input_report_key(dev, key, 0, true, K_FOREVER);

        *residue %= divisor;
    }

    // 阻尼（防止漂移）
    *residue = (*residue * 3) / 4;
}

static void a320_work_cb(struct k_work *work) {
    struct a320_data *data = CONTAINER_OF(work, struct a320_data, work);
    const struct device *dev = data->dev;

    uint32_t now = k_uptime_get_32();

    /* ========= WATCHDOG ========= */
    if (now - last_activity_time > A320_WDT_TIMEOUT) {
        LOG_WRN("A320 watchdog recovery");

        data->scroll_residue_x = 0.0f;
        data->scroll_residue_y = 0.0f;
        data->last_scroll_time = 0;
        data->last_scroll_mode = false;
        data->arrow_residue_x = 0;
        data->arrow_residue_y = 0;

        last_arrow_key_pressed = arrow_key_pressed;

        touched = false;
        return;
    }

    int8_t packet_dx = 0, packet_dy = 0;

    /* ========= ⭐ NEW: DRAIN MODE ========= */
    int16_t total_dx = 0;
    int16_t total_dy = 0;
    bool got_data = false;

    while (1) {
        int ret = data->read_packet(dev, &packet_dx, &packet_dy);

        if (ret != 0) {
            break;
        }

        /* 防止异常空包 */
        if (packet_dx == 0 && packet_dy == 0) {
            break;
        }

        total_dx += packet_dx;
        total_dy += packet_dy;
        got_data = true;
    }

    /* ========= ⭐ TOUCH TIME TRACK ========= */
    static uint32_t last_touch_time = 0;

    if (got_data) {
        last_touch_time = now;
        touched = true;
    }

    /* ========= ⭐ TOUCH RELEASE 判定（关键修复） ========= */
    if (!got_data) {
        if (now - last_touch_time > TOUCH_IDLE_TIMEOUT) { // 30~80ms 可调
            touched = false;
        }
        return;
    }

    int16_t dx = total_dx;
    int16_t dy = total_dy;

    /* ========= scroll / arrow mode 切换检测 ========= */
    bool just_enter_arrow = arrow_key_pressed && !last_arrow_key_pressed;
    bool capslock = current_indicators & HID_INDICATORS_CAPS_LOCK;
    bool scroll_mode = scroll_key_pressed || capslock;

    if (scroll_mode && !data->last_scroll_mode) {
        data->scroll_residue_x = 0.0f;
        data->scroll_residue_y = 0.0f;
        data->last_scroll_time = 0;
    }

    if (arrow_key_pressed) {

        if (just_enter_arrow) {
            data->arrow_residue_x = dx;
            data->arrow_residue_y = dy;
        }

        int abs_dx = abs(dx);
        int abs_dy = abs(dy);

        if (abs_dy * DOMINANT_DENOMINATOR > abs_dx * DOMINANT_NUMERATOR) {
            dx = 0;
        } else if (abs_dx * DOMINANT_DENOMINATOR > abs_dy * DOMINANT_NUMERATOR) {
            dy = 0;
        } else {
            dx = 0;
            dy = 0;
        }

        process_arrow_axis(dev, dx, &data->arrow_residue_x, INPUT_BTN_1, INPUT_BTN_0);

        process_arrow_axis(dev, dy, &data->arrow_residue_y, INPUT_BTN_3, INPUT_BTN_2);
    } else if (scroll_mode) {
        /* Keep this board's original X/Y axes and scroll directions. */
        int16_t scroll_x = dx * SCROLL_X_DIR;
        int16_t scroll_y = dy * SCROLL_Y_DIR;
        process_cm5_scroll(dev, data, scroll_x, scroll_y, now);
    } else if (!capslock) {

        uint8_t a320_led_brt = indicator_tp_get_last_valid_brightness();
        float a320_factor = 0.4f + 0.01f * a320_led_brt;

        float slow_mult = slow_key_pressed ? SLOW_KEY_MULTIPLIER : 1.0f;

        float fx = dx * 3 / 4 * a320_factor * slow_mult;
        float fy = dy * 3 / 4 * a320_factor * slow_mult;

        input_report_rel(dev, INPUT_REL_X, (int)fx, false, K_NO_WAIT);
        input_report_rel(dev, INPUT_REL_Y, (int)fy, true, K_NO_WAIT);
    } else {
        touched = false;
    }

    data->last_scroll_mode = scroll_mode && !arrow_key_pressed;
    last_arrow_key_pressed = arrow_key_pressed;
    touched = false;
    data->last_packet_time = now;
}

/* ========= GPIO ISR ========= */
static void motion_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins) {
    struct a320_data *data = CONTAINER_OF(cb, struct a320_data, motion_cb_data);

    last_activity_time = k_uptime_get_32();
    indicator_tp_motion_triggered();

    /* ⭐ 防止 work 堆积 */
    k_work_submit_to_queue(&a320_workq, &data->work);
}

/* An edge interrupt will not fire when MOTION is already active at startup. */
static void a320_process_pending_motion(struct a320_data *data) {
    const struct a320_config *cfg = data->dev->config;
    int active = gpio_pin_get_dt(&cfg->motion_gpio);

    if (active < 0) {
        LOG_WRN("Failed to read A320 MOTION pin: %d", active);
        return;
    }

    if (active) {
        last_activity_time = k_uptime_get_32();
        indicator_tp_motion_triggered();
        k_work_submit_to_queue(&a320_workq, &data->work);
    }
}

bool tp_is_touched(void) { return touched; }

static void a320_enable_irq_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct a320_data *data = CONTAINER_OF(dwork, struct a320_data, enable_irq_work);
    const struct device *dev = data->dev;
    const struct a320_config *cfg = dev->config;

    gpio_pin_interrupt_configure_dt(&cfg->motion_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    a320_process_pending_motion(data);

    LOG_INF("A320 IRQ enabled (delayed)");
}
/* ========= 初始化 ========= */
static int a320_init(const struct device *dev) {
    const struct a320_config *cfg = dev->config;
    struct a320_data *data = dev->data;

    if (!i2c_is_ready_dt(&cfg->i2c))
        return -ENODEV;
    if (!gpio_is_ready_dt(&cfg->motion_gpio))
        return -ENODEV;

    /* ⭐ 初始化 mutex */
    k_mutex_init(&a320_i2c_mutex);

    data->dev = dev;
    data->i2c = cfg->i2c;
    a320_detect_variant(dev);

    k_work_init(&data->work, a320_work_cb);

    /* ⭐ 启动 workqueue */
    k_work_queue_start(&a320_workq, a320_workq_stack, K_THREAD_STACK_SIZEOF(a320_workq_stack),
                       A320_WORKQ_PRIORITY, NULL);

    gpio_pin_configure_dt(&cfg->motion_gpio, GPIO_INPUT);

    gpio_init_callback(&data->motion_cb_data, motion_isr, BIT(cfg->motion_gpio.pin));
    gpio_add_callback(cfg->motion_gpio.port, &data->motion_cb_data);

    gpio_pin_interrupt_configure_dt(&cfg->motion_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    a320_process_pending_motion(data);

    k_work_init_delayable(&data->enable_irq_work, a320_enable_irq_work_cb);
    k_work_schedule(&data->enable_irq_work, K_MSEC(200));

    LOG_INF("A320 Driver Initialized (addr=0x%02X, I2C mutex enabled)",
            data->detected_i2c_addr);
    return 0;
}

#define A320_DEFINE(inst)                                                                          \
    static struct a320_data a320_data_##inst;                                                      \
    static const struct a320_config a320_config_##inst = {                                         \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                                                         \
        .motion_gpio = {.port = DEVICE_DT_GET(MOTION_GPIO_NODE),                                   \
                        .pin = MOTION_GPIO_PIN,                                                    \
                        .dt_flags = MOTION_GPIO_FLAGS},                                            \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(inst, a320_init, NULL, &a320_data_##inst, &a320_config_##inst,           \
                          POST_KERNEL, 70, NULL);

DT_INST_FOREACH_STATUS_OKAY(A320_DEFINE);
