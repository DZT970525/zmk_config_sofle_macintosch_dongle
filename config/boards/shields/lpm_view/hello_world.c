#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

static lv_obj_t *label;

int display_hello_init(void) {
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        printk("Display not ready!\n");
        return -ENODEV;
    }

    lv_obj_clean(lv_scr_act()); // 清空屏幕
    label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello, World!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    display_blanking_off(display_dev);
    printk("Display initialized successfully!\n");
    return 0;
}

SYS_INIT(display_hello_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
