/*
 * bbtrackball_input_handler.c
 * BB Trackball FULL interrupt-driven version
 *
 * + Dedicated workqueue version (NO system workqueue)
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_bbtrackball

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>

#include <math.h>
#include <stdlib.h>

#include <zmk/hid.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/position_state_changed.h>

LOG_MODULE_REGISTER(bbtrackball_input_handler, LOG_LEVEL_INF);

/* =========================================================
 * Workqueue config  ⭐⭐⭐新增
 * ========================================================= */

#define BBTRACKBALL_WORKQ_STACK_SIZE 2048
#define BBTRACKBALL_WORKQ_PRIORITY 5

K_THREAD_STACK_DEFINE(bbtrackball_workq_stack, BBTRACKBALL_WORKQ_STACK_SIZE);
static struct k_work_q bbtrackball_work_q;

/* =========================================================
 * GPIO Pins
 * ========================================================= */

#define DOWN_GPIO_PIN 9
#define LEFT_GPIO_PIN 12
#define UP_GPIO_PIN 5
#define RIGHT_GPIO_PIN 27

#define GPIO0_DEV DT_NODELABEL(gpio0)
#define GPIO1_DEV DT_NODELABEL(gpio1)

/* =========================================================
 * Config
 * ========================================================= */

#define BASE_MOVE_PIXELS 3
#define EXPONENTIAL_BASE 1.12f
#define SPEED_SCALE 60.0f

#define MOVE_IDLE_TIMEOUT 30

#define ARROW_TRIGGER_THRESHOLD 4
#define ARROW_REPEAT_MS 35

/* =========================================================
 * Runtime State
 * ========================================================= */

static bool moved = false;
static bool space_pressed = false;
static bool arrow_key_pressed = false;

static int dx_acc = 0;
static int dy_acc = 0;

static uint32_t last_move_time = 0;
static uint32_t last_arrow_trigger = 0;

/* =========================================================
 * HID indicators
 * ========================================================= */

static zmk_hid_indicators_t current_indicators;

#define HID_INDICATORS_CAPS_LOCK (1 << 1)

/* =========================================================
 * GPIO Input Description
 * ========================================================= */

typedef struct {
    const struct device *gpio_dev;
    int pin;
    int last_state;
    uint32_t last_time;
    int sign;
} DirInput;

static DirInput dir_inputs[] = {
    {DEVICE_DT_GET(GPIO0_DEV), LEFT_GPIO_PIN, 1, 0, -1},
    {DEVICE_DT_GET(GPIO0_DEV), RIGHT_GPIO_PIN, 1, 0, +1},
    {DEVICE_DT_GET(GPIO0_DEV), UP_GPIO_PIN, 1, 0, -1},
    {DEVICE_DT_GET(GPIO1_DEV), DOWN_GPIO_PIN, 1, 0, +1},
};

/* ========================================================= */

struct bbtrackball_dev_config {
    uint16_t x_input_code;
    uint16_t y_input_code;
};

struct bbtrackball_data;

/* ========================================================= */

struct bb_gpio_cb {
    struct gpio_callback cb;
    struct bbtrackball_data *parent;
};

struct bbtrackball_data {
    const struct device *dev;
    struct k_work work;
    struct bb_gpio_cb gpio_cbs[ARRAY_SIZE(dir_inputs)];
};

/* =========================================================
 * HID indicator listener
 * ========================================================= */

static int hid_indicators_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    if (ev) {
        current_indicators = ev->indicators;
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(a320_hid_listener, hid_indicators_listener);
ZMK_SUBSCRIPTION(a320_hid_listener, zmk_hid_indicators_changed);

/* ========================================================= */

bool trackball_is_active(void) { return (k_uptime_get_32() - last_move_time) < 40; }

/* =========================================================
 * Position listener
 * ========================================================= */

static int space_listener_cb(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (!ev)
        return 0;

    if (ev->position == 32) {
        arrow_key_pressed = ev->state;
    }

    if (ev->position == 61) {
        space_pressed = ev->state;
    }

    return 0;
}

ZMK_LISTENER(space_listener, space_listener_cb);
ZMK_SUBSCRIPTION(space_listener, zmk_position_state_changed);

/* =========================================================
 * GPIO interrupt callback
 * ========================================================= */

static void dir_edge_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {

    struct bb_gpio_cb *wrapper = CONTAINER_OF(cb, struct bb_gpio_cb, cb);
    struct bbtrackball_data *data = wrapper->parent;

    for (size_t i = 0; i < ARRAY_SIZE(dir_inputs); i++) {

        DirInput *d = &dir_inputs[i];

        if ((dev == d->gpio_dev) && (pins & BIT(d->pin))) {

            int val = gpio_pin_get(dev, d->pin);

            if (val != d->last_state) {

                uint32_t now = k_uptime_get_32();
                uint32_t delta = now - d->last_time;
                if (delta == 0)
                    delta = 1;

                float speed_factor = SPEED_SCALE / (float)delta;
                float mult = powf(EXPONENTIAL_BASE, speed_factor);
                int delta_px = (int)roundf(BASE_MOVE_PIXELS * mult);

                if (i < 2)
                    dx_acc += d->sign * delta_px;
                else
                    dy_acc += d->sign * delta_px;

                d->last_state = val;
                d->last_time = now;

                if (!k_work_is_pending(&data->work)) {
                    k_work_submit_to_queue(&bbtrackball_work_q, &data->work); // ⭐修改点
                }
            }
        }
    }
}

/* =========================================================
 * Work Handler
 * ========================================================= */

static void bbtrackball_work_handler(struct k_work *work) {

    struct bbtrackball_data *data = CONTAINER_OF(work, struct bbtrackball_data, work);
    const struct device *dev = data->dev;

    uint32_t now = k_uptime_get_32();

    int dx = dx_acc;
    int dy = dy_acc;

    dx_acc = 0;
    dy_acc = 0;

    if (dx == 0 && dy == 0) {
        if (now - last_move_time > MOVE_IDLE_TIMEOUT) {
            moved = false;
        }
        return;
    }

    last_move_time = now;
    moved = true;

    bool capslock = current_indicators & HID_INDICATORS_CAPS_LOCK;

    if (arrow_key_pressed) {

        int abs_dx = abs(dx);
        int abs_dy = abs(dy);

        if (abs_dx < ARROW_TRIGGER_THRESHOLD && abs_dy < ARROW_TRIGGER_THRESHOLD) {
            return;
        }

        if (now - last_arrow_trigger < ARROW_REPEAT_MS) {
            return;
        }

        last_arrow_trigger = now;

        uint16_t key = 0;

        if (abs_dx > abs_dy)
            key = (dx > 0) ? INPUT_BTN_1 : INPUT_BTN_0;
        else
            key = (dy > 0) ? INPUT_BTN_3 : INPUT_BTN_2;

        input_report_key(dev, key, 1, false, K_NO_WAIT);
        input_report_key(dev, key, 0, true, K_NO_WAIT);

        return;
    }

    if (space_pressed || capslock) {
        input_report_rel(dev, INPUT_REL_HWHEEL, -dx, false, K_NO_WAIT);
        input_report_rel(dev, INPUT_REL_WHEEL, dy, true, K_NO_WAIT);
        return;
    }

    input_report_rel(dev, INPUT_REL_X, -dx, false, K_NO_WAIT);
    input_report_rel(dev, INPUT_REL_Y, -dy, true, K_NO_WAIT);
}

/* =========================================================
 * Init
 * ========================================================= */

static int bbtrackball_init(const struct device *dev) {

    struct bbtrackball_data *data = dev->data;

    LOG_INF("Initializing BBtrackball");

    data->dev = dev;

    /* ⭐ 启动独立 workqueue */
    k_work_queue_start(&bbtrackball_work_q, bbtrackball_workq_stack,
                       K_THREAD_STACK_SIZEOF(bbtrackball_workq_stack), BBTRACKBALL_WORKQ_PRIORITY,
                       NULL);

    k_work_init(&data->work, bbtrackball_work_handler);

    for (size_t i = 0; i < ARRAY_SIZE(dir_inputs); i++) {

        DirInput *d = &dir_inputs[i];

        gpio_pin_configure(d->gpio_dev, d->pin, GPIO_INPUT | GPIO_PULL_UP);

        d->last_state = gpio_pin_get(d->gpio_dev, d->pin);
        d->last_time = k_uptime_get_32();

        data->gpio_cbs[i].parent = data;

        gpio_init_callback(&data->gpio_cbs[i].cb, dir_edge_cb, BIT(d->pin));
        gpio_add_callback(d->gpio_dev, &data->gpio_cbs[i].cb);

        gpio_pin_interrupt_configure(d->gpio_dev, d->pin, GPIO_INT_EDGE_BOTH);
    }

    return 0;
}

/* ========================================================= */

#define BBTRACKBALL_INIT_PRIORITY CONFIG_INPUT_INIT_PRIORITY

#define BBTRACKBALL_DEFINE(inst)                                                                   \
    static struct bbtrackball_data bbtrackball_data_##inst;                                        \
                                                                                                   \
    static const struct bbtrackball_dev_config bbtrackball_config_##inst = {                       \
        .x_input_code = DT_PROP_OR(DT_DRV_INST(inst), x_input_code, INPUT_REL_X),                  \
        .y_input_code = DT_PROP_OR(DT_DRV_INST(inst), y_input_code, INPUT_REL_Y),                  \
    };                                                                                             \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(inst, bbtrackball_init, NULL, &bbtrackball_data_##inst,                  \
                          &bbtrackball_config_##inst, POST_KERNEL, BBTRACKBALL_INIT_PRIORITY,      \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(BBTRACKBALL_DEFINE);
