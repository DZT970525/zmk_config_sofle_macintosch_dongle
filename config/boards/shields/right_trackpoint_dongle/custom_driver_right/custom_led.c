/*
 * custom_led_backlight_follow.c
 * LED immediately follows backlight brightness (never 0, min 10),
 * but fades out 3s after boot or brightness change (timeout allows 0),
 * and fades in when brightness becomes nonzero again.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>

#include <zmk/backlight.h>

#include "custom_led.h" // ★ 新增

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_HAS_CHOSEN(zmk_custom_led),
             "Custom LED enabled but no zmk,custom_led chosen node found");

static const struct device *const led_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_custom_led));

#define CHILD_COUNT(...) +1
#define DT_NUM_CHILD(node_id) (DT_FOREACH_CHILD(node_id, CHILD_COUNT))
#define LED_NUM (DT_NUM_CHILD(DT_CHOSEN(zmk_custom_led)))

#define BRT_MIN 10
#define OFF_DELAY_MS 3000

/* Fade configs */
#define FADE_STEP_MS 20
#define FADE_STEPS 20

static struct k_work_delayable auto_off_work;
static struct k_work_delayable poll_work;
static struct k_work_delayable fade_work;

static uint8_t last_brt = 255;
static uint8_t current_brt = 0;
static uint8_t target_brt = 0;
static int fade_step = -1;

/* ★ 对外状态：最近一次有效亮度 */
static uint8_t last_valid_brt = BRT_MIN;

/* === Immediate apply LED === */
static void apply_led(uint8_t brightness) {
    if (!device_is_ready(led_dev))
        return;

    for (int i = 0; i < LED_NUM; i++) {
        led_set_brightness(led_dev, i, brightness);
    }
    current_brt = brightness;
}

/* === Fade animation handler === */
static void fade_handler(struct k_work *work) {
    if (fade_step < 0)
        return;

    float ratio = (float)fade_step / FADE_STEPS;
    int new_level = current_brt + (int)((target_brt - current_brt) * ratio);

    apply_led(new_level);

    fade_step++;
    if (fade_step > FADE_STEPS) {
        apply_led(target_brt);
        fade_step = -1;
        return;
    }

    k_work_reschedule(&fade_work, K_MSEC(FADE_STEP_MS));
}

/* === Start fade === */
static void fade_to(uint8_t new_brt) {
    target_brt = new_brt;
    fade_step = 0;
    k_work_reschedule(&fade_work, K_MSEC(FADE_STEP_MS));
}

/* === Auto-off timeout → fade-out === */
static void auto_off_handler(struct k_work *work) {
    LOG_INF("Auto-off → fade-out to 0");
    fade_to(0);
}

/* === Poll backlight changes every 10ms === */
static void poll_handler(struct k_work *work) {
    uint8_t brt = zmk_backlight_get_brt();

    if (brt != last_brt) {
        last_brt = brt;

        uint8_t led_level = (brt == 0) ? 0 : MAX(BRT_MIN, brt);

        if (led_level > 0) {
            /* ★ 只在有效亮度时更新 */
            last_valid_brt = led_level;
        }

        if (led_level == 0) {
            k_work_cancel_delayable(&auto_off_work);
            k_work_reschedule(&auto_off_work, K_MSEC(OFF_DELAY_MS));
        } else {
            if (current_brt == 0) {
                LOG_INF("Fade-in from dark → %d", led_level);
                fade_to(led_level);
            } else {
                apply_led(led_level);
            }

            k_work_cancel_delayable(&auto_off_work);
            k_work_reschedule(&auto_off_work, K_MSEC(OFF_DELAY_MS));
        }
    }

    k_work_reschedule(&poll_work, K_MSEC(10));
}

/* === Public API === */
uint8_t custom_led_get_last_valid_brightness(void) { return last_valid_brt; }

/* === Init === */
static int init_led_follow(void) {
    if (!device_is_ready(led_dev))
        return -ENODEV;

    k_work_init_delayable(&auto_off_work, auto_off_handler);
    k_work_init_delayable(&poll_work, poll_handler);
    k_work_init_delayable(&fade_work, fade_handler);

    uint8_t boot = zmk_backlight_get_brt();
    uint8_t led_level = (boot == 0) ? 0 : MAX(BRT_MIN, boot);

    if (led_level > 0) {
        last_valid_brt = led_level; // ★ 初始化
    }

    apply_led(led_level);

    k_work_reschedule(&auto_off_work, K_MSEC(OFF_DELAY_MS));
    k_work_reschedule(&poll_work, K_NO_WAIT);

    LOG_INF("LED backlight follow + fade driver initialized");
    return 0;
}

SYS_INIT(init_led_follow, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
