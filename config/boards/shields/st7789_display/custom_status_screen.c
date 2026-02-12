/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "widgets/battery_status.h"
#include "widgets/output_status.h"
#include "widgets/splash.h"
#include "widgets/snake.h"
#include "widgets/helpers/display.h"
#include "widgets/action_button.h"
#include "widgets/logo.h"
#include "widgets/configuration.h"
#include "widgets/wpm.h"
#include "widgets/modifier.h"
#include <zmk/activity.h>
#include <zmk/events/activity_state_changed.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define SPLASH_DURATION_MS 50
#define SPLASH_FINAL_COUNT 15 // 约 2.5 秒（50ms × 50）

static uint8_t splash_count = 0;
static bool splash_finished = false;

/* === 阶段1：开机阶段 === */
void timer_splash(lv_timer_t *timer) {
    if (splash_finished)
        return;

    print_splash();
    splash_count++;

    if (splash_count >= SPLASH_FINAL_COUNT) {
        LOG_INF("Splash finished → show menu");
        print_background();
        initialize_battery_status();
        print_menu();

        lv_timer_pause(timer);
        splash_finished = true;
    }
}

/* === 阶段2：监听活动状态变化 === */
static int activity_listener_cb(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *event = as_zmk_activity_state_changed(eh);
    if (!event)
        return 0;

    if (event->state == ZMK_ACTIVITY_ACTIVE) {
        LOG_INF("Keyboard active → show menu");
        print_background();
        initialize_battery_status();
        print_menu();
    } else {
        LOG_INF("Keyboard idle → show splash");
        print_splash();
    }

    return 0;
}

ZMK_LISTENER(custom_status_activity_listener, activity_listener_cb);
ZMK_SUBSCRIPTION(custom_status_activity_listener, zmk_activity_state_changed);

/* === 显示初始化 === */
lv_obj_t *zmk_display_status_screen() {
    configure();
    init_display();
    theme_init();

    zmk_widget_splash_init();
    zmk_widget_output_status_init();
    zmk_widget_peripheral_battery_status_init();
    zmk_widget_layer_init();
    zmk_widget_action_button_init();
    zmk_widget_wpm_init();
    zmk_widget_modifier_init();

    // 启动开机 splash 动画
    lv_timer_create(timer_splash, SPLASH_DURATION_MS, NULL);

    return lv_obj_create(NULL);
}
