

#define DT_DRV_COMPAT jdi_lpm009m360a

#include "lpm009m360a.h"

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/display.h>

#include <zephyr/sys/util.h>

#include <string.h>

#define LPM009M360A_RESET_TIME      K_MSEC(1)
#define LPM009M360A_EXIT_SLEEP_TIME K_MSEC(1)

struct lpm009m360a_data {
    uint8_t buf[9 * 144];
};

struct lpm009m360a_config {
    struct spi_dt_spec bus;
    uint16_t height;
    uint16_t width;
    int rotation;
    int reverse;
    uint8_t color_mode;
};

/* 性能优化核心：分散聚集 SPI 传输 (Scatter-Gather SPI) */
static inline int lpm009m360a_transmit_hold(const struct device *dev, uint8_t cmd, uint8_t arg,
                                            const uint8_t *tx_data, size_t tx_count) {
    const struct lpm009m360a_config *config = dev->config;
    struct spi_buf tx_bufs[3];
    struct spi_buf_set tx_set = { .buffers = tx_bufs };

    tx_bufs[0].buf = &cmd;
    tx_bufs[0].len = 1;
    tx_bufs[1].buf = &arg;
    tx_bufs[1].len = 1;

    if (tx_data != NULL && tx_count > 0) {
        tx_bufs[2].buf = (void *)tx_data;
        tx_bufs[2].len = tx_count;
        tx_set.count = 3;
    } else {
        tx_set.count = 2;
    }

    return spi_write_dt(&config->bus, &tx_set);
}

static inline int lpm009m360a_transmit(const struct device *dev, uint8_t cmd, uint8_t arg,
                                       const uint8_t *tx_data, size_t tx_count) {
    const struct lpm009m360a_config *config = dev->config;
    int ret = lpm009m360a_transmit_hold(dev, cmd, arg, tx_data, tx_count);
    spi_release_dt(&config->bus);
    return ret;
}

static int lpm009m360a_exit_sleep(const struct device *dev) {
    ARG_UNUSED(dev);
    k_sleep(LPM009M360A_EXIT_SLEEP_TIME);
    return 0;
}

static int lpm009m360a_sleep(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

static int lpm009m360a_reset_display(const struct device *dev) {
    int ret = lpm009m360a_transmit(dev, LPM009M360A_CMD_ALL_CLEAR, 0, NULL, 0);
    k_sleep(LPM009M360A_RESET_TIME);
    return ret;
}

static int lpm009m360a_blanking_on(const struct device *dev) { return lpm009m360a_sleep(dev); }
static int lpm009m360a_blanking_off(const struct device *dev) { return lpm009m360a_exit_sleep(dev); }

static int lpm009m360a_read(const struct device *dev, const uint16_t x, const uint16_t y,
                            const struct display_buffer_descriptor *desc, void *buf) {
    ARG_UNUSED(dev); ARG_UNUSED(x); ARG_UNUSED(y); ARG_UNUSED(desc); ARG_UNUSED(buf);
    return -ENOTSUP;
}

static int lpm009m360a_write(const struct device *dev, const uint16_t x, const uint16_t y,
                             const struct display_buffer_descriptor *desc, const void *buf) {
    const uint8_t *source_buf8 = (const uint8_t *)buf;
    const struct lpm009m360a_config *config = dev->config;
    struct lpm009m360a_data *data = dev->data;
    int ret = 0;
    
    uint8_t cmd = LPM009M360A_CMD_UPDATE | (config->color_mode << 2);
    size_t len = (config->color_mode == 0x02) ? 9 : 36;
    
    /* 性能优化：位运算代替除法 (Bitwise shift instead of division) */
    uint16_t x_bytes = x >> 3; 
    uint16_t y_bytes = y >> 3;

    if (config->rotation == 0) {
        uint8_t cnt = desc->height;
        uint16_t copy_bytes = (config->color_mode == 0x02) ? (desc->width >> 3) : (desc->width >> 1);
        if (copy_bytes == 0) copy_bytes = desc->width;

        /* 性能优化：内存块拷贝 (Memory Copy) 消除逐字节 for 循环 */
        for (uint8_t i = 0; i < cnt; i++) {
            memcpy(&data->buf[(y + i) * len + x_bytes], &source_buf8[i * copy_bytes], copy_bytes);
        }
        for (uint8_t i = 0; i < cnt; i++) {
            ret = lpm009m360a_transmit_hold(dev, cmd, i + y + 1,
                                            &data->buf[(y + i) * len], len);
            if (ret < 0) break;
        }
    } else if (config->rotation == 1) {
        uint8_t cnt = desc->width;
        uint16_t h_bytes = desc->height >> 3;
        
        for (uint8_t i = 0; i < h_bytes; i++) {
            uint16_t src_offset = i * desc->width;
            uint16_t dest_col = y_bytes + i;
            for (uint8_t j = 0; j < desc->width; j++) {
                data->buf[(143 - x - j) * len + dest_col] = source_buf8[src_offset + j];
            }
        }
        for (uint8_t i = 0; i < cnt; i++) {
            uint8_t line = 143 - x - i;
            ret = lpm009m360a_transmit_hold(dev, cmd, line + 1,
                                            &data->buf[line * len], len);
            if (ret < 0) break;
        }
    }

    /* 保证状态机安全结束 */
    if (ret >= 0) {
        lpm009m360a_transmit_hold(dev, LPM009M360A_CMD_NO_UPDATE, 0, NULL, 0);
        lpm009m360a_transmit_hold(dev, LPM009M360A_CMD_NO_UPDATE, 0, NULL, 0);
    }
    
    spi_release_dt(&config->bus);
    return ret;
}

static void *lpm009m360a_get_framebuffer(const struct device *dev) { 
    ARG_UNUSED(dev); 
    return NULL; 
}

static int lpm009m360a_set_brightness(const struct device *dev, const uint8_t brightness) {
    ARG_UNUSED(dev); ARG_UNUSED(brightness);
    return -ENOTSUP;
}

static int lpm009m360a_set_contrast(const struct device *dev, const uint8_t contrast) {
    ARG_UNUSED(dev); ARG_UNUSED(contrast);
    return -ENOTSUP;
}

static void lpm009m360a_get_capabilities(const struct device *dev,
                                         struct display_capabilities *capabilities) {
    const struct lpm009m360a_config *config = dev->config;

    memset(capabilities, 0, sizeof(struct display_capabilities));
    capabilities->x_resolution = config->width;
    capabilities->y_resolution = config->height;
    capabilities->supported_pixel_formats = PIXEL_FORMAT_MONO01 | PIXEL_FORMAT_MONO10;
    capabilities->current_pixel_format = config->reverse ? PIXEL_FORMAT_MONO01 : PIXEL_FORMAT_MONO10;
    
    if (config->rotation == 0) {
        capabilities->screen_info = SCREEN_INFO_X_ALIGNMENT_WIDTH | SCREEN_INFO_MONO_MSB_FIRST;
    } else if (config->rotation == 1) {
        capabilities->screen_info = SCREEN_INFO_MONO_VTILED | SCREEN_INFO_MONO_MSB_FIRST;
    }
    capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int lpm009m360a_set_pixel_format(const struct device *dev,
                                        const enum display_pixel_format pixel_format) {
    ARG_UNUSED(dev); ARG_UNUSED(pixel_format);
    return -ENOTSUP;
}

static int lpm009m360a_set_orientation(const struct device *dev,
                                       const enum display_orientation orientation) {
    ARG_UNUSED(dev);
    if (orientation == DISPLAY_ORIENTATION_NORMAL) {
        return 0;
    }
    return -ENOTSUP;
}

static int lpm009m360a_init(const struct device *dev) {
    return lpm009m360a_reset_display(dev);
}

#ifdef CONFIG_PM_DEVICE
static int lpm009m360a_pm_action(const struct device *dev, enum pm_device_action action) {
    int ret = 0;
    switch (action) {
    case PM_DEVICE_ACTION_RESUME:
        lpm009m360a_exit_sleep(dev);
        break;
    case PM_DEVICE_ACTION_SUSPEND:
        lpm009m360a_sleep(dev);
        break;
    case PM_DEVICE_ACTION_TURN_ON:
        lpm009m360a_init(dev);
        break;
    case PM_DEVICE_ACTION_TURN_OFF:
        break;
    default:
        ret = -ENOTSUP;
        break;
    }
    return ret;
}
#endif /* CONFIG_PM_DEVICE */

static const struct display_driver_api lpm009m360a_api = {
    .blanking_on = lpm009m360a_blanking_on,
    .blanking_off = lpm009m360a_blanking_off,
    .write = lpm009m360a_write,
    .read = lpm009m360a_read,
    .get_framebuffer = lpm009m360a_get_framebuffer,
    .set_brightness = lpm009m360a_set_brightness,
    .set_contrast = lpm009m360a_set_contrast,
    .get_capabilities = lpm009m360a_get_capabilities,
    .set_pixel_format = lpm009m360a_set_pixel_format,
    .set_orientation = lpm009m360a_set_orientation,
};

#define LPM009M360A_INIT(inst)                                                                     \
    static const struct lpm009m360a_config lpm009m360a_config_##inst = {                           \
        .bus = SPI_DT_SPEC_INST_GET(                                                               \
            inst, SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_HOLD_ON_CS | SPI_LOCK_ON, 0),         \
        .width = DT_INST_PROP(inst, width),                                                        \
        .height = DT_INST_PROP(inst, height),                                                      \
        .color_mode = DT_INST_PROP(inst, color_mode),                                              \
        .rotation = DT_INST_PROP(inst, rotation),                                                  \
        .reverse = DT_INST_PROP(inst, reverse),                                                    \
    };                                                                                             \
    static struct lpm009m360a_data lpm009m360a_data_##inst = {0};                                  \
                                                                                                   \
    PM_DEVICE_DT_INST_DEFINE(inst, lpm009m360a_pm_action);                                         \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(inst, lpm009m360a_init, PM_DEVICE_DT_INST_GET(inst),                     \
                          &lpm009m360a_data_##inst, &lpm009m360a_config_##inst, POST_KERNEL,       \
                          CONFIG_DISPLAY_INIT_PRIORITY, &lpm009m360a_api);

DT_INST_FOREACH_STATUS_OKAY(LPM009M360A_INIT)