// SPDX-License-Identifier: Apache-2.0

#include "board_display.h"
#include "bsp/esp_mosaico.h"
#include "bundle_gsp.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_log.h"
#include "iris_ota_support.h"
#include "nvs_flash.h"

static const char *TAG = "gsp_hello";

static void feed_load(esp_gsp_handle_t ui, void *user_ctx)
{
    static int32_t load;
    (void)user_ctx;
    load = (load + 5) % 101;
    ESP_ERROR_CHECK(gsp_hello_load_set_value(ui, load));
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_display_present_target_config_t display;
    ESP_ERROR_CHECK(board_display_init(&display));

    esp_lcd_touch_handle_t touch = NULL;
    ESP_ERROR_CHECK(board_touch_init(&touch));

    esp_gsp_config_t app_config = gsp_bundle_config();
    esp_gsp_esp_lcd_config_t lcd = ESP_GSP_ESP_LCD_CONFIG_INIT();
    lcd.display = display;
    lcd.touch = touch;

    esp_gsp_handle_t ui;
    ESP_ERROR_CHECK(esp_gsp_esp_lcd_start(&app_config, &lcd, &ui));

    void *load_timer = esp_gsp_timer_create(ui, 250, feed_load, NULL);
    ESP_ERROR_CHECK(load_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    /* Start ESP-Iris and expose the enter-Recovery RPC. */
    iris_ota_support_start();
    ESP_LOGI(TAG, "GSP Hello World ready at %dx%d RGB565",
             BSP_LCD_H_RES, BSP_LCD_V_RES);
}
