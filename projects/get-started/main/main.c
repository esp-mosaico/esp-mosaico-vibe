// SPDX-License-Identifier: Apache-2.0

#include "esp_err.h"
#include "esp_log.h"
#include "bsp/esp_mosaico.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iris_ota_support.h"
#include "nvs_flash.h"

static const char *TAG = "get_started";

static esp_err_t show_mosaico_title(void)
{
    lv_display_t *display = bsp_display_start();
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to start the ESP-Mosaico display");
        return ESP_FAIL;
    }

    if (!bsp_display_lock(-1)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return ESP_FAIL;
    }

    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "ESP-MOSAICO");
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_color(title, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(title);

    bsp_display_unlock();
    ESP_LOGI(TAG, "Displaying ESP-MOSAICO at %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    return ESP_OK;
}

static void hello_world_task(void *arg)
{
    (void)arg;
    TickType_t next_wake = xTaskGetTickCount();

    while (true) {
        ESP_LOGI(TAG, "HELLO WORLD");
        vTaskDelayUntil(&next_wake, pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    iris_ota_support_start();
    ESP_ERROR_CHECK(show_mosaico_title());

    ESP_ERROR_CHECK(xTaskCreate(hello_world_task, "hello_world", 2048, NULL, 4,
                                NULL) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
}
