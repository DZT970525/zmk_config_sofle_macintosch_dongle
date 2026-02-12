#pragma once

#include <stdint.h>

/**
 * @brief Get last non-zero LED brightness that followed backlight
 *
 * This value is NOT cleared when LED fades to 0.
 */
uint8_t custom_led_get_last_valid_brightness(void);
