/*
 * keyboard_backlight_peripheral.c - Peripheral keyboard backlight
 * (activity-timer based, OUT_TOG & RGB_TOG safe)
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>

#include <zmk/events/position_state_changed.h>
#include <zmk/rgb_underglow.h>
#include <zmk/backlight.h>

LOG_MODULE_REGISTER(keyboard_backlight, CONFIG_ZMK_LOG_LEVEL);

/* ==== Device ==== */
#define KEYBOARD_BACKLIGHT_NODE DT_NODELABEL(keyboard_backlight)
#if !DT_NODE_HAS_STATUS(KEYBOARD_BACKLIGHT_NODE, okay)
#error "Missing DT node: keyboard_backlight"
#endif

static const struct device *const backlight_dev = DEVICE_DT_GET(KEYBOARD_BACKLIGHT_NODE);

/* ==== Config ==== */
#define MAX_BRT 20
#define MIN_BRT 0
#define FADE_STEP 2
#define FADE_INTERVAL_MS 60

#define BOOT_FADE_DELAY_MS 3000
#define IDLE_CHECK_INTERVAL_MS 500

#define AUTO_OFF_MIN_MS 1000
#define AUTO_OFF_MAX_MS 3000

/* ==== WPM ==== */
#define CHARS_PER_WORD 5.0
#define WPM_UPDATE_INTERVAL_S 1
#define WPM_RESET_INTERVAL_S 5

/* ==== State ==== */
enum bl_state {
    BL_OFF,
    BL_FADING_UP,
    BL_ON,
    BL_FADING_DOWN,
};

static enum bl_state bl_state = BL_OFF;
static int current_brt;

/* Activity */
static int64_t last_activity_ms;

/* RGB edge detect */
static bool last_rgb_on;

/* WPM */
static uint32_t key_pressed_count;
static uint8_t wpm_state;
static uint8_t wpm_tick;

/* ==== Works ==== */
static struct k_work_delayable bl_work;
static struct k_work_delayable idle_work;
static struct k_work_delayable wpm_work;
static struct k_work_delayable boot_work;

/* ==== Helpers ==== */
static bool backlight_enabled(void) {
    bool rgb_on = true;

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
    if (zmk_rgb_underglow_get_state(&rgb_on) < 0) {
        rgb_on = true;
    }
#endif

    return rgb_on;
}

static void set_brightness(int brt) {
    if (!device_is_ready(backlight_dev))
        return;

    brt = CLAMP(brt, MIN_BRT, MAX_BRT);
    led_set_brightness(backlight_dev, 0, brt);
    current_brt = brt;
}

static void force_off(void) {
    bl_state = BL_OFF;
    set_brightness(MIN_BRT);
}

/* ==== Backlight FSM ==== */
static void bl_work_handler(struct k_work *work) {
    if (!backlight_enabled()) {
        force_off();
        return;
    }

    switch (bl_state) {
    case BL_FADING_UP:
        current_brt += FADE_STEP;
        if (current_brt >= MAX_BRT) {
            current_brt = MAX_BRT;
            bl_state = BL_ON;
        }
        set_brightness(current_brt);
        k_work_schedule(&bl_work, K_MSEC(FADE_INTERVAL_MS));
        break;

    case BL_FADING_DOWN:
        current_brt -= FADE_STEP;
        if (current_brt <= MIN_BRT) {
            current_brt = MIN_BRT;
            bl_state = BL_OFF;
        }
        set_brightness(current_brt);
        k_work_schedule(&bl_work, K_MSEC(FADE_INTERVAL_MS));
        break;

    default:
        break;
    }
}

/* ==== Idle Watchdog + RGB edge detect ==== */
static void idle_work_handler(struct k_work *work) {
    bool rgb_on = backlight_enabled();
    int64_t now = k_uptime_get();

    /* ---- RGB OFF -> ON edge ---- */
    if (rgb_on && !last_rgb_on) {
        last_activity_ms = now;

        if (bl_state == BL_OFF || bl_state == BL_FADING_DOWN) {
            bl_state = BL_FADING_UP;
            k_work_schedule(&bl_work, K_NO_WAIT);
        }

        /* 关键修复点：重新安排 boot fade */
        k_work_schedule(&boot_work, K_MSEC(BOOT_FADE_DELAY_MS));
    }

    last_rgb_on = rgb_on;

    if (!rgb_on) {
        force_off();
        k_work_schedule(&idle_work, K_MSEC(IDLE_CHECK_INTERVAL_MS));
        return;
    }

    int idle_timeout_ms = AUTO_OFF_MIN_MS + ((AUTO_OFF_MAX_MS - AUTO_OFF_MIN_MS) *
                                             (wpm_state > 100 ? 100 : wpm_state) / 100);

    if (bl_state == BL_ON && now - last_activity_ms > idle_timeout_ms) {

        bl_state = BL_FADING_DOWN;
        k_work_schedule(&bl_work, K_NO_WAIT);
    }

    k_work_schedule(&idle_work, K_MSEC(IDLE_CHECK_INTERVAL_MS));
}

/* ==== Boot fade ==== */
static void boot_work_handler(struct k_work *work) {
    if (bl_state == BL_ON) {
        bl_state = BL_FADING_DOWN;
        k_work_schedule(&bl_work, K_NO_WAIT);
    }
}

/* ==== WPM ==== */
static void wpm_work_handler(struct k_work *work) {
    wpm_tick++;

    wpm_state = (key_pressed_count / CHARS_PER_WORD) / (wpm_tick * WPM_UPDATE_INTERVAL_S / 60.0);

    if (wpm_tick >= WPM_RESET_INTERVAL_S) {
        wpm_tick = 0;
        key_pressed_count = 0;
    }

    k_work_schedule(&wpm_work, K_SECONDS(WPM_UPDATE_INTERVAL_S));
}

/* ==== Key Event ==== */
static int kb_listener_cb(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (!ev || ev->source != ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL)
        return 0;

    if (!backlight_enabled())
        return 0;

    if (ev->state) {
        last_activity_ms = k_uptime_get();
        key_pressed_count++;

        if (bl_state == BL_OFF || bl_state == BL_FADING_DOWN) {
            bl_state = BL_FADING_UP;
            k_work_schedule(&bl_work, K_NO_WAIT);
        }
    }

    return 0;
}

ZMK_LISTENER(keyboard_backlight_listener, kb_listener_cb);
ZMK_SUBSCRIPTION(keyboard_backlight_listener, zmk_position_state_changed);

/* ==== Init ==== */
static int keyboard_backlight_init(void) {
    if (!device_is_ready(backlight_dev)) {
        LOG_ERR("Backlight device not ready");
        return -ENODEV;
    }

    current_brt = MIN_BRT;
    set_brightness(current_brt);

    last_activity_ms = k_uptime_get();
    last_rgb_on = backlight_enabled();

    k_work_init_delayable(&bl_work, bl_work_handler);
    k_work_init_delayable(&idle_work, idle_work_handler);
    k_work_init_delayable(&wpm_work, wpm_work_handler);
    k_work_init_delayable(&boot_work, boot_work_handler);

    k_work_schedule(&idle_work, K_MSEC(IDLE_CHECK_INTERVAL_MS));
    k_work_schedule(&wpm_work, K_SECONDS(WPM_UPDATE_INTERVAL_S));

    if (last_rgb_on) {
        bl_state = BL_FADING_UP;
        k_work_schedule(&bl_work, K_NO_WAIT);
        k_work_schedule(&boot_work, K_MSEC(BOOT_FADE_DELAY_MS));
    }

    LOG_INF("Peripheral keyboard backlight initialized");
    return 0;
}

SYS_INIT(keyboard_backlight_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
