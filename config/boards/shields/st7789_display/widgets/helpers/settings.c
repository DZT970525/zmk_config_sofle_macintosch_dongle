#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>
#include "settings.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Internal stored settings
static settings_t current_settings = {
    .current_theme = 0,
    .mute = false,
};

// SETTINGS HANDLER CALLBACKS
static int snake_settings_set(const char *key, size_t len_rd, settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(key, "settings") == 0) {
        if (len_rd != sizeof(current_settings)) {
            LOG_ERR("Invalid size for settings struct: %zu", len_rd);
            return -EINVAL;
        }

        ssize_t rc = read_cb(cb_arg, &current_settings, sizeof(current_settings));
        if (rc < 0) {
            LOG_ERR("Failed to read struct: %d", rc);
            return rc;
        }

        LOG_INF("Loaded theme: %u", current_settings.current_theme);

        return 0;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(snake_settings_handler, "snake", NULL, snake_settings_set, NULL, NULL);

int snake_settings_save() {
    int rc = settings_save_one("snake/settings", &current_settings, sizeof(current_settings));
    if (rc) {
        LOG_ERR("Failed to save settings: %d", rc);
        return rc;
    }

    LOG_INF("Saved theme: %u, mute: %d", current_settings.current_theme, current_settings.mute);

    return 0;
}

int snake_settings_save_mute(bool is_muted) {
    current_settings.mute = is_muted;
    return snake_settings_save();
}

int snake_settings_toggle_mute(void) {
    return snake_settings_save_mute(!current_settings.mute);
}

bool snake_settings_get_mute(void) {
    return current_settings.mute;
}

int snake_settings_save_current_theme(uint8_t current_theme) {
    uint8_t previous_theme = current_settings.current_theme;
    current_settings.current_theme = current_theme;
    int rc = snake_settings_save();
    if (rc) {
        current_settings.current_theme = previous_theme;
    }
    return rc;
}

uint8_t snake_settings_get_current_theme(void) {
    return current_settings.current_theme;
}
