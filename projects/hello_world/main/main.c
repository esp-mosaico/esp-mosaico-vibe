// SPDX-License-Identifier: Apache-2.0

#include "bsp/esp_mosaico.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iris_ota_support.h"
#include "lvgl.h"
#include "nvs_flash.h"

#define HELLO_LOG_PERIOD_MS 5000

static const char *TAG = "hello_world";

static esp_err_t hello_world_ui_start(void)
{
    lv_display_t *display = bsp_display_start();
    ESP_RETURN_ON_FALSE(display, ESP_FAIL, TAG, "start display");
    ESP_RETURN_ON_FALSE(bsp_display_lock(-1), ESP_FAIL, TAG, "lock display");

    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF6F6F3), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Hello World!");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_center(label);

    bsp_display_unlock();
    ESP_LOGI(TAG, "Hello World UI ready at %dx%d", BSP_LCD_H_RES,
             BSP_LCD_V_RES);
    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(hello_world_ui_start());

    /* Start ESP-Iris and expose the enter-Recovery RPC before the main loop. */
    iris_ota_support_start();

    while (true) {
        ESP_LOGI(TAG, "Hello World!");
        vTaskDelay(pdMS_TO_TICKS(HELLO_LOG_PERIOD_MS));
    }
}
