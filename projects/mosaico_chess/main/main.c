// SPDX-License-Identifier: Apache-2.0

#include "board_display.h"
#define GSP_BUNDLE_ENABLE_LEGACY_NAMES
#include "bundle_gsp.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "game_ui.h"
#include "iris_ota_support.h"
#include "nvs_flash.h"

#define GAME_IMAGE_CACHE_LARGE_BYTES (6U * 1024U * 1024U)
#define GAME_IMAGE_CACHE_SMALL_BYTES (4U * 1024U * 1024U)
#define GAME_IMAGE_CACHE_LARGE_HEAP_THRESHOLD (16U * 1024U * 1024U)

static const char *TAG = "mosaico_chess";

void app_main(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init());
    /* Recovery must remain reachable even if display or UI startup fails. */
    iris_ota_support_start();

    esp_display_present_target_config_t display;
    ESP_ERROR_CHECK(board_display_init(&display));

    esp_lcd_touch_handle_t touch = NULL;
    ESP_ERROR_CHECK(board_touch_init(&touch));

    esp_gsp_config_t app_config = gsp_bundle_config();
    app_config.disable_swipe = true;
    const size_t free_spiram =
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    app_config.image_cache_bytes =
        free_spiram >= GAME_IMAGE_CACHE_LARGE_HEAP_THRESHOLD ?
            GAME_IMAGE_CACHE_LARGE_BYTES :
            GAME_IMAGE_CACHE_SMALL_BYTES;
    ESP_LOGI(
        TAG, "GSP image cache: %u bytes (free SPIRAM before UI: %u)",
        (unsigned)app_config.image_cache_bytes, (unsigned)free_spiram);
    esp_gsp_esp_lcd_config_t lcd = ESP_GSP_ESP_LCD_CONFIG_INIT();
    lcd.display = display;
    lcd.touch = touch;

    esp_gsp_handle_t ui = NULL;
    ESP_ERROR_CHECK(esp_gsp_esp_lcd_start(&app_config, &lcd, &ui));
    ESP_ERROR_CHECK(game_ui_init(ui));

    ESP_LOGI(TAG, "mosaico_chess S00-S30 ready");
}
