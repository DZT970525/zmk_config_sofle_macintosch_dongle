/*
 * custom_led_boot_fade.c
 * Boot: LED on for 3s, then fade out to 0.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_HAS_CHOSEN(zmk_custom_led), "No zmk,custom_led chosen node found");

static const struct device *const led_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_custom_led));

#define CHILD_COUNT(...) +1
#define DT_NUM_CHILD(node_id) (DT_FOREACH_CHILD(node_id, CHILD_COUNT))
#define LED_NUM (DT_NUM_CHILD(DT_CHOSEN(zmk_custom_led)))

#define BOOT_BRIGHTNESS 40
#define OFF_DELAY_MS 3000

#define FADE_STEP_MS 20
#define FADE_STEPS 20

static struct k_work_delayable fade_work;
static struct k_work_delayable auto_off_work;

static uint8_t current_brt = BOOT_BRIGHTNESS;
static int fade_step = 0;

/* === Apply brightness === */
static void apply_led(uint8_t brightness) {
    if (!device_is_ready(led_dev))
        return;

    for (int i = 0; i < LED_NUM; i++) {
        led_set_brightness(led_dev, i, brightness);
    }

    current_brt = brightness;
}

/* === Fade-out handler === */
static void fade_handler(struct k_work *work) {
    int new_level = BOOT_BRIGHTNESS - ((BOOT_BRIGHTNESS * fade_step) / FADE_STEPS);

    if (new_level < 0)
        new_level = 0;

    apply_led(new_level);

    fade_step++;

    if (fade_step <= FADE_STEPS) {
        k_work_reschedule(&fade_work, K_MSEC(FADE_STEP_MS));
    }
}

/* === Delay timeout → start fade === */
static void auto_off_handler(struct k_work *work) {
    fade_step = 0;
    k_work_reschedule(&fade_work, K_NO_WAIT);
}

/* === Init === */
static int init_led_boot_effect(void) {
    if (!device_is_ready(led_dev))
        return -ENODEV;

    k_work_init_delayable(&fade_work, fade_handler);
    k_work_init_delayable(&auto_off_work, auto_off_handler);

    /* Boot light on */
    apply_led(BOOT_BRIGHTNESS);

    /* 3s later fade out */
    k_work_reschedule(&auto_off_work, K_MSEC(OFF_DELAY_MS));

    return 0;
}

SYS_INIT(init_led_boot_effect, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);