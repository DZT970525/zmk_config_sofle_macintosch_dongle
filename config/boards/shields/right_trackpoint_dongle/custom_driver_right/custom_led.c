/*
 * custom_led.c
 * 优化说明 (v5 - 完全独立亮度控制版):
 * 1. 彻底去除了对 ZMK Backlight 核心模块的依赖，解耦全局背光。
 * 2. 自己维护目标亮度、最小/最大值和加减步长。
 * 3. 响应更快，无需再等待 ZMK 核心处理。
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/math_extras.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

#include "custom_led.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_HAS_CHOSEN(zmk_custom_led),
             "Custom LED enabled but no zmk,custom_led chosen node found");

static const struct device *const led_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_custom_led));

#define CHILD_COUNT(...) +1
#define DT_NUM_CHILD(node_id) (DT_FOREACH_CHILD(node_id, CHILD_COUNT))
#define LED_NUM (DT_NUM_CHILD(DT_CHOSEN(zmk_custom_led)))

#define BRT_MIN 10
#define OFF_DELAY_MS 3000
#define FADE_STEP_MS 20
#define FADE_STEPS 20

static struct k_work_delayable auto_off_work;
static struct k_work_delayable fade_work;

static uint8_t current_brt = 0;
static uint8_t start_brt = 0;
static uint8_t target_brt = 0;
static int fade_step = -1;

/* 独立维护的亮度状态 */
static uint8_t manual_brt = CONFIG_CUSTOM_LED_BRT_DEFAULT;
/* 对外状态：最近一次有效亮度，这确保了灯灭后小红点灵敏度不变 */
static uint8_t last_valid_brt = CONFIG_CUSTOM_LED_BRT_DEFAULT;

#define BRT_KEY_UP_POS CONFIG_CUSTOM_LED_BRT_UP_POS
#define BRT_KEY_DOWN_POS CONFIG_CUSTOM_LED_BRT_DOWN_POS
#define BRT_STEP CONFIG_CUSTOM_LED_BRT_STEP

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
    if (fade_step < 0) return;

    int new_level = start_brt + ((target_brt - start_brt) * fade_step) / FADE_STEPS;
    apply_led((uint8_t)new_level);

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
    start_brt = current_brt;
    fade_step = 0;
    k_work_reschedule(&fade_work, K_MSEC(FADE_STEP_MS));
}

/* === Auto-off timeout → fade out === */
static void auto_off_handler(struct k_work *work) {
    if (current_brt > 0) {
        LOG_INF("Idle timeout -> fade out to 0");
        fade_to(0);
    }
}

/* === 核心亮度更新逻辑（被按键触发） === */
static void trigger_brightness_update(void) {
    if (manual_brt > 0) {
        last_valid_brt = manual_brt;
    }

    if (manual_brt == 0) {
        k_work_reschedule(&auto_off_work, K_MSEC(OFF_DELAY_MS));
        fade_to(0);
    } else {
        if (current_brt == 0) {
            LOG_INF("Fade-in from dark → %d", manual_brt);
            fade_to(manual_brt);
        } else {
            // 如果已经在亮，平滑过渡到新亮度
            fade_to(manual_brt);
        }
        // 刷新 3 秒闲置熄灭计时
        k_work_reschedule(&auto_off_work, K_MSEC(OFF_DELAY_MS));
    }
}

/* === ZMK Event Listener: 直接修改内部亮度 === */
static int led_activity_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (!ev) return 0;

    // 仅在按下时触发
    if (ev->state) {
        bool changed = false;
        
        if (ev->position == BRT_KEY_UP_POS) {
            int new_target = manual_brt + BRT_STEP;
            manual_brt = MIN(new_target, 255);
            changed = true;
        } else if (ev->position == BRT_KEY_DOWN_POS) {
            int new_target = manual_brt - BRT_STEP;
            // 确保不会低于最低可见亮度限制，除非你想让它完全能被按键关掉
            manual_brt = MAX(new_target, BRT_MIN);
            changed = true;
        }

        if (changed) {
            trigger_brightness_update();
        }
    }
    return 0;
}
ZMK_LISTENER(custom_led_activity_listener, led_activity_listener);
ZMK_SUBSCRIPTION(custom_led_activity_listener, zmk_position_state_changed);

/* === Public API === */
uint8_t custom_led_get_last_valid_brightness(void) { return last_valid_brt; }

/* === Init === */
static int init_led_follow(void) {
    if (!device_is_ready(led_dev)) return -ENODEV;

    k_work_init_delayable(&auto_off_work, auto_off_handler);
    k_work_init_delayable(&fade_work, fade_handler);

    // 启动时套用默认亮度，并开始 3 秒倒计时
    last_valid_brt = MAX(BRT_MIN, manual_brt);
    apply_led(manual_brt);

    k_work_reschedule(&auto_off_work, K_MSEC(OFF_DELAY_MS));

    LOG_INF("Independent LED driver initialized");
    return 0;
}

SYS_INIT(init_led_follow, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);