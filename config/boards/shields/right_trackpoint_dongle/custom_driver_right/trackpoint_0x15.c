/*
 * TrackPoint HID over I2C Driver (Zephyr Input Subsystem)
 * Accumulate + timer-driven 125 Hz report version
 *
 * Based on the original ZitaoTech driver and the accumulate/timer architecture
 * used by expertamateur.
 *
 * Copyright (c) 2025 ZitaoTech
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_trackpoint

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/hid.h>

#include "custom_led.h"

LOG_MODULE_REGISTER(trackpoint, LOG_LEVEL_DBG);

/* ========================================================================= */
/* Dedicated TrackPoint work queue                                            */
/* ========================================================================= */

#define TP_WORKQ_STACK_SIZE 2048
#define TP_WORKQ_PRIORITY 5

K_THREAD_STACK_DEFINE(tp_workq_stack, TP_WORKQ_STACK_SIZE);
static struct k_work_q tp_workq;

/*
 * I2C access is protected because other code may share the same I2C controller.
 * read_work itself is serialized with report_work by tp_workq.
 */
static struct k_mutex trackpoint_i2c_mutex;

/* ========================================================================= */
/* Configurable mouse / scroll parameters                                     */
/* ========================================================================= */

#define SCROLL_X_DIR (-CONFIG_TRACKPOINT_SCROLL_X_DIR)
#define SCROLL_Y_DIR CONFIG_TRACKPOINT_SCROLL_Y_DIR

#define SCROLL_DEADZONE CONFIG_TRACKPOINT_SCROLL_DEADZONE
#define SCROLL_INPUT_MAX CONFIG_TRACKPOINT_SCROLL_INPUT_MAX
#define SCROLL_DIVISOR_SLOW CONFIG_TRACKPOINT_SCROLL_DIVISOR_SLOW
#define SCROLL_DIVISOR_FAST CONFIG_TRACKPOINT_SCROLL_DIVISOR_FAST

#define ARROW_DEADZONE CONFIG_TRACKPOINT_SCROLL_DEADZONE
#define ARROW_INPUT_MAX 256
#define ARROW_DIVISOR_SLOW CONFIG_TRACKPOINT_SCROLL_DIVISOR_SLOW
#define ARROW_DIVISOR_FAST CONFIG_TRACKPOINT_SCROLL_DIVISOR_FAST

#define DOMINANT_NUMERATOR CONFIG_TRACKPOINT_DOMINANT_NUMERATOR
#define DOMINANT_DENOMINATOR CONFIG_TRACKPOINT_DOMINANT_DENOMINATOR

#define MOUSE_BASE_SPEED (CONFIG_TRACKPOINT_MOUSE_BASE_SPEED_PERCENT / 100.0f)
#define MOUSE_SENS_BASE (CONFIG_TRACKPOINT_MOUSE_SENS_BASE_PERCENT / 100.0f)
#define MOUSE_SENS_STEP (CONFIG_TRACKPOINT_MOUSE_SENS_STEP_PERCENT / 100.0f)

#define SLOW_KEY_MULTIPLIER 0.5f

/* ========================================================================= */
/* TrackPoint hardware / timing constants                                     */
/* ========================================================================= */

#define MOTION_GPIO_NODE DT_NODELABEL(gpio0)
#define MOTION_GPIO_PIN 14
#define MOTION_GPIO_FLAGS (GPIO_ACTIVE_LOW | GPIO_PULL_UP)

#define TRACKPOINT_PACKET_LEN 7
#define TRACKPOINT_MAGIC_BYTE0 0x50

/*
 * One TrackPoint packet may arrive every few milliseconds during fast motion.
 * A single read_work drains up to this many packets while MOTION stays active.
 */
#define MAX_PACKETS_PER_WORK 32

/*
 * 8 ms period = 125 reports per second.
 * No HID report is sent when no movement has accumulated.
 */
#define REPORT_INTERVAL_MS 8

#define TRACKPOINT_WDT_TIMEOUT 200

/* ========================================================================= */
/* Global key / indicator state                                               */
/* ========================================================================= */

static bool scroll_key_pressed;
static bool arrow_key_pressed;
static bool slow_key_pressed;

static zmk_hid_indicators_t current_indicators;
#define HID_INDICATORS_CAPS_LOCK (1 << 1)

static int hid_indicators_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);

    if (ev != NULL) {
        current_indicators = ev->indicators;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(trackpoint_hid_listener, hid_indicators_listener);
ZMK_SUBSCRIPTION(trackpoint_hid_listener, zmk_hid_indicators_changed);

static int special_key_listener_cb(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* Arrow mode key */
    if (ev->position == 34) {
        arrow_key_pressed = ev->state;
        LOG_INF("arrow key position=34 %s", arrow_key_pressed ? "PRESSED" : "RELEASED");
    }

    /* Scroll mode key */
    if (ev->position == 61) {
        scroll_key_pressed = ev->state;
        LOG_INF("scroll key position=61 %s", scroll_key_pressed ? "PRESSED" : "RELEASED");
    }

    /* Slow movement key */
    if (ev->position == 36) {
        slow_key_pressed = ev->state;
        LOG_INF("slow key position=36 %s", slow_key_pressed ? "PRESSED" : "RELEASED");
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(trackpoint_special_key_listener, special_key_listener_cb);
ZMK_SUBSCRIPTION(trackpoint_special_key_listener, zmk_position_state_changed);

/* ========================================================================= */
/* Driver data structures                                                     */
/* ========================================================================= */

struct trackpoint_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec motion_gpio;
};

struct trackpoint_snapshot {
    int32_t sum_dx;
    int32_t sum_dy;
    uint32_t packet_count;
    uint32_t first_ts;
    uint32_t last_ts;
};

struct trackpoint_data {
    const struct device *dev;

    /* GPIO-triggered I2C reader and timer-triggered HID reporter. */
    struct k_work read_work;
    struct k_work report_work;
    struct k_timer report_timer;

    struct gpio_callback motion_cb_data;
    struct k_work_delayable enable_irq_work;

    /*
     * read_work and report_work both execute on tp_workq, so snapshot access is
     * serialized and needs no additional lock.
     */
    struct trackpoint_snapshot snapshot;

    uint32_t last_activity_time;
    uint32_t last_report_time;

    int32_t scroll_residue_x;
    int32_t scroll_residue_y;
    int32_t arrow_residue_x;
    int32_t arrow_residue_y;

    bool last_scroll_mode;
    bool last_arrow_mode;
};

/* ========================================================================= */
/* Acceleration                                                               */
/* ========================================================================= */

#ifdef CONFIG_TRACKPOINT_EXPONENTIAL
#define TP_MAX_MULT 2.0f

static inline float trackpoint_exponential_factor(int32_t dx, int32_t dy, uint32_t delta_ms) {
    if (delta_ms == 0U) {
        delta_ms = 1U;
    }

    int32_t dist = abs(dx) + abs(dy);

    if (dist < 1) {
        return 1.0f;
    }

    float speed = (float)dist / (float)delta_ms;
    float mult = expf(speed * 1.307357f);

    return MIN(mult, TP_MAX_MULT);
}
#endif

/* ========================================================================= */
/* Read one TrackPoint packet                                                 */
/* ========================================================================= */

static int trackpoint_read_packet(const struct device *dev, int8_t *dx, int8_t *dy) {
    const struct trackpoint_config *cfg = dev->config;
    uint8_t buf[TRACKPOINT_PACKET_LEN] = {0};

    /*
     * This function runs in work-queue thread context, so waiting for the mutex
     * is safe. K_NO_WAIT must not be used unless its return value is checked.
     */
    int ret = k_mutex_lock(&trackpoint_i2c_mutex, K_FOREVER);

    if (ret != 0) {
        return ret;
    }

    ret = i2c_read_dt(&cfg->i2c, buf, sizeof(buf));

    k_mutex_unlock(&trackpoint_i2c_mutex);

    if (ret < 0) {
        return ret;
    }

    if (buf[0] != TRACKPOINT_MAGIC_BYTE0) {
        LOG_WRN("Invalid TrackPoint packet header: 0x%02x", buf[0]);
        return -EIO;
    }

    *dx = (int8_t)buf[2];
    *dy = (int8_t)buf[3];

    return 0;
}

/* ========================================================================= */
/* Scroll / arrow helpers                                                     */
/* ========================================================================= */

static inline void apply_dominant_axis_lock(int32_t *dx, int32_t *dy) {
    int32_t abs_dx = abs(*dx);
    int32_t abs_dy = abs(*dy);

    if (abs_dy * DOMINANT_DENOMINATOR > abs_dx * DOMINANT_NUMERATOR) {
        *dx = 0;
    } else if (abs_dx * DOMINANT_DENOMINATOR > abs_dy * DOMINANT_NUMERATOR) {
        *dy = 0;
    } else {
        *dx = 0;
        *dy = 0;
    }
}

static inline void process_scroll_axis(const struct device *dev, int32_t delta, int32_t *residue,
                                       uint16_t input_code, int8_t dir_mult) {
    int32_t abs_delta = abs(delta);

    if (abs_delta <= SCROLL_DEADZONE) {
        return;
    }

    int32_t curve_delta = MIN(abs_delta, SCROLL_INPUT_MAX);

    /*
     * Preserve the nonlinear curve from the user's original driver.
     * Larger motion gives a smaller divisor and therefore faster scrolling.
     */
    float t = (float)curve_delta / (float)SCROLL_INPUT_MAX;
    t *= t;

    float f_div = SCROLL_DIVISOR_SLOW - (SCROLL_DIVISOR_SLOW - SCROLL_DIVISOR_FAST) * t;

    int32_t divisor = MAX((int32_t)f_div, 1);

    *residue += delta * dir_mult;

    int32_t scroll_ticks = *residue / divisor;

    if (scroll_ticks != 0) {
        input_report_rel(dev, input_code, scroll_ticks, true, K_NO_WAIT);
        *residue %= divisor;
    }

    /* Preserve the damping behavior from the original driver. */
    *residue = (*residue * 3) / 4;
}

static inline void process_arrow_axis(const struct device *dev, int32_t delta, int32_t *residue,
                                      uint16_t key_neg, uint16_t key_pos) {
    int32_t abs_delta = abs(delta);

    if (abs_delta <= ARROW_DEADZONE) {
        return;
    }

    int32_t curve_delta = MIN(abs_delta, ARROW_INPUT_MAX);

    float t = (float)curve_delta / (float)ARROW_INPUT_MAX;
    t *= t;

    float f_div = ARROW_DIVISOR_SLOW - (ARROW_DIVISOR_SLOW - ARROW_DIVISOR_FAST) * t;

    int32_t divisor = MAX((int32_t)f_div, 1);

    *residue += delta;

    int32_t arrow_ticks = *residue / divisor;

    if (arrow_ticks != 0) {
        uint16_t key = (arrow_ticks > 0) ? key_pos : key_neg;
        int32_t pulse_count = MIN(abs(arrow_ticks), 8);

        /*
         * Limit each 8 ms report to eight key pulses, preventing a very large
         * accumulated movement from monopolizing the work queue.
         */
        for (int32_t i = 0; i < pulse_count; i++) {
            input_report_key(dev, key, 1, true, K_NO_WAIT);
            input_report_key(dev, key, 0, true, K_NO_WAIT);
        }

        *residue %= divisor;
    }

    *residue = (*residue * 3) / 4;
}

/* ========================================================================= */
/* GPIO-triggered reader                                                      */
/* ========================================================================= */

static void trackpoint_read_work_cb(struct k_work *work) {
    struct trackpoint_data *data = CONTAINER_OF(work, struct trackpoint_data, read_work);
    const struct device *dev = data->dev;
    const struct trackpoint_config *cfg = dev->config;

    int32_t local_sum_dx = 0;
    int32_t local_sum_dy = 0;
    uint32_t packets = 0;
    uint32_t first_ts = 0;
    uint32_t last_ts = 0;

    /*
     * gpio_pin_get_dt() returns the logical value. Because the pin is declared
     * GPIO_ACTIVE_LOW, an asserted physical-low MOTION signal reads as 1.
     */
    while (packets < MAX_PACKETS_PER_WORK && gpio_pin_get_dt(&cfg->motion_gpio) > 0) {
        int8_t dx = 0;
        int8_t dy = 0;

        int ret = trackpoint_read_packet(dev, &dx, &dy);

        if (ret != 0) {
            LOG_WRN("TrackPoint I2C read stopped: %d", ret);
            break;
        }

        uint32_t now = k_uptime_get_32();

        if (packets == 0U) {
            first_ts = now;
        }

        last_ts = now;
        local_sum_dx += dx;
        local_sum_dy += dy;
        packets++;
    }

    if (packets == 0U) {
        return;
    }

    if (data->snapshot.packet_count == 0U) {
        data->snapshot.first_ts = first_ts;
    }

    data->snapshot.sum_dx += local_sum_dx;
    data->snapshot.sum_dy += local_sum_dy;
    data->snapshot.packet_count += packets;
    data->snapshot.last_ts = last_ts;
    data->last_activity_time = last_ts;

    /*
     * If MAX_PACKETS_PER_WORK was reached and MOTION is still asserted,
     * resubmit read_work so remaining hardware packets are not stranded.
     */
    if (packets == MAX_PACKETS_PER_WORK && gpio_pin_get_dt(&cfg->motion_gpio) > 0) {
        k_work_submit_to_queue(&tp_workq, &data->read_work);
    }
}

/* ========================================================================= */
/* Fixed-rate reporter                                                        */
/* ========================================================================= */

static void trackpoint_report_work_cb(struct k_work *work) {
    struct trackpoint_data *data = CONTAINER_OF(work, struct trackpoint_data, report_work);
    const struct device *dev = data->dev;

    /*
     * Snapshot read-and-clear is safe without a lock because read_work and
     * report_work execute serially on the same dedicated work queue.
     */
    int32_t sum_dx = data->snapshot.sum_dx;
    int32_t sum_dy = data->snapshot.sum_dy;
    uint32_t packet_count = data->snapshot.packet_count;
    uint32_t first_ts = data->snapshot.first_ts;
    uint32_t last_ts = data->snapshot.last_ts;

    data->snapshot.sum_dx = 0;
    data->snapshot.sum_dy = 0;
    data->snapshot.packet_count = 0;
    data->snapshot.first_ts = 0;
    data->snapshot.last_ts = 0;

    if (packet_count == 0U || (sum_dx == 0 && sum_dy == 0)) {
        return;
    }

    uint32_t now = k_uptime_get_32();

    if (now - data->last_activity_time > TRACKPOINT_WDT_TIMEOUT) {
        LOG_WRN("TrackPoint watchdog recovery");

        data->scroll_residue_x = 0;
        data->scroll_residue_y = 0;
        data->arrow_residue_x = 0;
        data->arrow_residue_y = 0;
        data->last_scroll_mode = false;
        data->last_arrow_mode = false;
        return;
    }

    bool capslock = (current_indicators & HID_INDICATORS_CAPS_LOCK) != 0;
    bool scroll_mode = scroll_key_pressed || capslock;
    bool arrow_mode = arrow_key_pressed;

    bool just_enter_scroll = scroll_mode && !data->last_scroll_mode;
    bool just_enter_arrow = arrow_mode && !data->last_arrow_mode;

    if (arrow_mode) {
        if (just_enter_arrow) {
            data->arrow_residue_x = sum_dx;
            data->arrow_residue_y = sum_dy;
        }

        apply_dominant_axis_lock(&sum_dx, &sum_dy);

        process_arrow_axis(dev, sum_dx, &data->arrow_residue_x, INPUT_BTN_0, INPUT_BTN_1);
        process_arrow_axis(dev, sum_dy, &data->arrow_residue_y, INPUT_BTN_2, INPUT_BTN_3);

    } else if (scroll_mode) {
        if (just_enter_scroll) {
            data->scroll_residue_x = sum_dx * SCROLL_X_DIR;
            data->scroll_residue_y = sum_dy * SCROLL_Y_DIR;
        }

        apply_dominant_axis_lock(&sum_dx, &sum_dy);

        process_scroll_axis(dev, sum_dx, &data->scroll_residue_x, INPUT_REL_HWHEEL, SCROLL_X_DIR);
        process_scroll_axis(dev, sum_dy, &data->scroll_residue_y, INPUT_REL_WHEEL, SCROLL_Y_DIR);

    } else {
        uint8_t tp_led_brt = custom_led_get_last_valid_brightness();
        float tp_factor = MOUSE_SENS_BASE + MOUSE_SENS_STEP * tp_led_brt;

#ifdef CONFIG_TRACKPOINT_EXPONENTIAL
        uint32_t delta_ms = last_ts - first_ts;

        /*
         * A one-packet batch has no meaningful first-to-last interval.
         * Use time since the previous report as a stable fallback.
         */
        if (delta_ms == 0U) {
            delta_ms = now - data->last_report_time;
        }
        if (delta_ms == 0U) {
            delta_ms = 1U;
        }

        float exp_mult = trackpoint_exponential_factor(sum_dx, sum_dy, delta_ms);
#else
        float exp_mult = 1.0f;
#endif

        float slow_mult = slow_key_pressed ? SLOW_KEY_MULTIPLIER : 1.0f;

        float fx = sum_dx * MOUSE_BASE_SPEED * tp_factor * exp_mult * slow_mult;
        float fy = sum_dy * MOUSE_BASE_SPEED * tp_factor * exp_mult * slow_mult;

        input_report_rel(dev, INPUT_REL_X, -(int32_t)fx, false, K_NO_WAIT);
        input_report_rel(dev, INPUT_REL_Y, -(int32_t)fy, true, K_NO_WAIT);
    }

    data->last_scroll_mode = scroll_mode;
    data->last_arrow_mode = arrow_mode;
    data->last_report_time = now;
}

/*
 * k_timer callback runs in interrupt context. It only submits work and never
 * touches the accumulators directly.
 */
static void trackpoint_report_timer_cb(struct k_timer *timer) {
    struct trackpoint_data *data = CONTAINER_OF(timer, struct trackpoint_data, report_timer);

    k_work_submit_to_queue(&tp_workq, &data->report_work);
}

/* ========================================================================= */
/* GPIO interrupt                                                             */
/* ========================================================================= */

static void motion_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins) {
    ARG_UNUSED(port);
    ARG_UNUSED(pins);

    struct trackpoint_data *data = CONTAINER_OF(cb, struct trackpoint_data, motion_cb_data);

    k_work_submit_to_queue(&tp_workq, &data->read_work);
}

static void trackpoint_enable_irq_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct trackpoint_data *data = CONTAINER_OF(dwork, struct trackpoint_data, enable_irq_work);
    const struct trackpoint_config *cfg = data->dev->config;

    int ret = gpio_pin_interrupt_configure_dt(&cfg->motion_gpio, GPIO_INT_EDGE_TO_ACTIVE);

    if (ret != 0) {
        LOG_ERR("Failed to enable TrackPoint IRQ: %d", ret);
        return;
    }

    LOG_INF("TrackPoint IRQ enabled");
}

/* ========================================================================= */
/* Initialization                                                             */
/* ========================================================================= */

static int trackpoint_init(const struct device *dev) {
    const struct trackpoint_config *cfg = dev->config;
    struct trackpoint_data *data = dev->data;
    int ret;

    if (!i2c_is_ready_dt(&cfg->i2c)) {
        LOG_ERR("TrackPoint I2C bus is not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&cfg->motion_gpio)) {
        LOG_ERR("TrackPoint MOTION GPIO is not ready");
        return -ENODEV;
    }

    k_mutex_init(&trackpoint_i2c_mutex);

    data->dev = dev;
    data->snapshot = (struct trackpoint_snapshot){0};
    data->scroll_residue_x = 0;
    data->scroll_residue_y = 0;
    data->arrow_residue_x = 0;
    data->arrow_residue_y = 0;
    data->last_scroll_mode = false;
    data->last_arrow_mode = false;
    data->last_activity_time = k_uptime_get_32();
    data->last_report_time = data->last_activity_time;

    /*
     * Both work items use the same single-threaded queue. This is what makes
     * snapshot access deterministic without a second mutex/spinlock.
     */
    k_work_queue_start(&tp_workq, tp_workq_stack, K_THREAD_STACK_SIZEOF(tp_workq_stack),
                       TP_WORKQ_PRIORITY, NULL);

    k_work_init(&data->read_work, trackpoint_read_work_cb);
    k_work_init(&data->report_work, trackpoint_report_work_cb);

    /*
     * Fixed 8 ms timer:
     * 1000 ms / 8 ms = 125 Hz.
     */
    k_timer_init(&data->report_timer, trackpoint_report_timer_cb, NULL);
    k_timer_start(&data->report_timer, K_MSEC(REPORT_INTERVAL_MS), K_MSEC(REPORT_INTERVAL_MS));

    ret = gpio_pin_configure_dt(&cfg->motion_gpio, GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Failed to configure TrackPoint MOTION GPIO: %d", ret);
        k_timer_stop(&data->report_timer);
        return ret;
    }

    gpio_init_callback(&data->motion_cb_data, motion_isr, BIT(cfg->motion_gpio.pin));

    ret = gpio_add_callback(cfg->motion_gpio.port, &data->motion_cb_data);
    if (ret != 0) {
        LOG_ERR("Failed to add TrackPoint GPIO callback: %d", ret);
        k_timer_stop(&data->report_timer);
        return ret;
    }

    k_work_init_delayable(&data->enable_irq_work, trackpoint_enable_irq_work_cb);
    k_work_schedule(&data->enable_irq_work, K_MSEC(200));

    LOG_INF("TrackPoint initialized: accumulate + fixed 125 Hz report");

    return 0;
}

#define TRACKPOINT_DEFINE(inst)                                                                    \
    static struct trackpoint_data trackpoint_data_##inst;                                          \
    static const struct trackpoint_config trackpoint_config_##inst = {                             \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                                                         \
        .motion_gpio =                                                                             \
            {                                                                                      \
                .port = DEVICE_DT_GET(MOTION_GPIO_NODE),                                           \
                .pin = MOTION_GPIO_PIN,                                                            \
                .dt_flags = MOTION_GPIO_FLAGS,                                                     \
            },                                                                                     \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(inst, trackpoint_init, NULL, &trackpoint_data_##inst,                    \
                          &trackpoint_config_##inst, POST_KERNEL, 70, NULL);

DT_INST_FOREACH_STATUS_OKAY(TRACKPOINT_DEFINE)
