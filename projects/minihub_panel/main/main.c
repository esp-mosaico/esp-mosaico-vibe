// SPDX-License-Identifier: Apache-2.0
#include "app_display.h"
#include "app_hub.h"
#include "app_minihub.h"
#include "app_panel.h"
#include "cJSON.h"
#include "esp_bt.h"
#include "esp_flash_dispatcher.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "iris_ota_support.h"
#include "iris_screen_mirror.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>
#include "ui_bundle.h"

static const char *TAG = "minihub_panel";

/* The composition root retains owned handles, never controller internals. */
static struct {
    esp_gsp_handle_t ui;
    app_hub_handle_t hub;
    app_panel_handle_t panel;
} application;

static void *json_psram_malloc(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return ptr ? ptr : heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

void app_main(void)
{
    /*
     * The Web device catalog builds large cJSON trees while Wi-Fi modem sleep
     * may need internal RAM to recreate its PHY timer. cJSON hooks are global,
     * so install them once before any application component can parse JSON.
     */
    cJSON_Hooks json_hooks = {
        .malloc_fn = json_psram_malloc,
        .free_fn = free,
    };
    cJSON_InitHooks(&json_hooks);

    const esp_flash_dispatcher_config_t flash_dispatcher = {
        .task_stack_size = 4096,
        .task_priority = 10,
        .task_core_id = tskNO_AFFINITY,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(esp_flash_dispatcher_init(&flash_dispatcher));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init());

    char local_hub_token[65];
    const esp_err_t token_err = app_minihub_prepare_local_token(
        local_hub_token, sizeof(local_hub_token));
    if (token_err != ESP_OK) {
        ESP_LOGE(TAG, "Local MiniHub token unavailable: %s",
                 esp_err_to_name(token_err));
        return;
    }

    /* Keep the official MiniHub watchdog policy. Commissioning is asynchronous,
     * so its five-minute protocol deadline does not block this watchdog. */
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 30000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_err_t wdt_err = esp_task_wdt_reconfigure(&wdt_cfg);
    if (wdt_err == ESP_ERR_INVALID_STATE) {
        wdt_err = esp_task_wdt_init(&wdt_cfg);
    }
    if (wdt_err != ESP_OK) {
        ESP_LOGW(TAG, "task_wdt config: %s", esp_err_to_name(wdt_err));
    }

    /* Recovery must remain reachable even when the UI bundle cannot be loaded. */
    iris_ota_support_start();

    /* Verify and map the deployable UI before BLE starts. On ESP32-S31 the
     * hardware SHA state can read back as all zero while the controller is
     * active, and IDF deliberately aborts on that fault-injection condition. */
    esp_gsp_config_t config;
    const esp_err_t err = ui_bundle_open(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GSP UI unavailable (0x%x); Recovery RPC remains active", err);
        return;
    }

    /* Initialize and enable the BLE controller before GSP/Wi-Fi claim internal
     * DMA RAM, so its link-layer mempool always gets contiguous internal DMA. */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    esp_gsp_esp_lcd_config_t display;
    const esp_err_t display_err = app_display_init(&display);
    if (display_err != ESP_OK) {
        ESP_LOGE(TAG, "Display unavailable (0x%x); Recovery RPC remains active", display_err);
        return;
    }
    ESP_ERROR_CHECK(iris_screen_mirror_init());
    ESP_ERROR_CHECK(esp_gsp_esp_lcd_start(&config, &display, &application.ui));
    ESP_ERROR_CHECK(iris_screen_mirror_attach(application.ui));

    /* Accept the image once Recovery, the deployable UI and the display are
     * operational, before MiniHub's networking tasks consume the remaining
     * internal heap needed by the OTA health transaction. */
    const esp_err_t health_err = esp_iris_mark_healthy();
    if (health_err != ESP_OK) {
        ESP_LOGW(TAG, "Could not report healthy startup: %s",
                 esp_err_to_name(health_err));
    }

    ESP_ERROR_CHECK(app_hub_network_init() == APP_HUB_OK ?
                    ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(app_hub_start(&application.hub) == APP_HUB_OK ?
                    ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(app_hub_set_token(application.hub, local_hub_token) ==
                    APP_HUB_OK ? ESP_OK : ESP_FAIL);
    memset(local_hub_token, 0, sizeof(local_hub_token));

    const esp_err_t hub_err = app_minihub_start();
    if (hub_err != ESP_OK) {
        ESP_LOGE(TAG, "MiniHub Host unavailable (0x%x); panel stays on localhost",
                 hub_err);
    } else {
        /* IMU publish runs MiniHub rules_exec → Matter OnOff on that
         * task; shake coredumps in CHIPCryptoPALPSA. Handheld classify
         * is also not accurate enough. Re-enable via app_imu_start(). */
        ESP_LOGI(TAG, "Onboard IMU disabled");
    }

    ESP_ERROR_CHECK(app_panel_create(application.ui, application.hub,
                                     &application.panel));
    ESP_LOGI(TAG, "MiniHub panel ready (local Host)");
}
