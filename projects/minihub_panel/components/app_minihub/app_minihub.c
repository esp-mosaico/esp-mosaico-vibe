// SPDX-License-Identifier: Apache-2.0

#include "app_minihub.h"

#include "adapters_setup.h"
#include "web_seed.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_random.h"
#include "minihub_adapter.h"
#include "minihub_auth.h"
#include "minihub_config.h"
#include "minihub_core.h"
#include "minihub_js.h"
#include "minihub_module.h"
#include "minihub_net.h"
#include "minihub_pack.h"
#include "minihub_provision.h"
#include "minihub_rules.h"
#include "minihub_scenes.h"
#include "minihub_web.h"
#include "nvs.h"
#include "sdkconfig.h"

#if CONFIG_MINIHUB_MATTER_ADAPTER
#include "minihub_matter.h"
#endif
#if CONFIG_MINIHUB_THREAD_ADAPTER
#include "minihub_thread.h"
#endif

static const char *TAG = "app_minihub";
static bool s_started;

#define LOCAL_TOKEN_BYTES 32
#define LOCAL_TOKEN_HEX_LEN (LOCAL_TOKEN_BYTES * 2)

esp_err_t app_minihub_prepare_local_token(char *out_token, size_t out_size)
{
    if (!out_token || out_size <= LOCAL_TOKEN_HEX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    out_token[0] = '\0';

    nvs_handle_t nvs;
    esp_err_t err = nvs_open("minihub", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }
    size_t length = out_size;
    err = nvs_get_str(nvs, "api_token", out_token, &length);
    if (err == ESP_OK && out_token[0] != '\0') {
        nvs_close(nvs);
        return ESP_OK;
    }
    if (err == ESP_OK) {
        err = ESP_ERR_NVS_NOT_FOUND;
    }
    if (err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs);
        return err;
    }

    uint8_t random[LOCAL_TOKEN_BYTES];
    esp_fill_random(random, sizeof(random));
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(random); ++i) {
        out_token[i * 2] = hex[random[i] >> 4];
        out_token[i * 2 + 1] = hex[random[i] & 0x0f];
    }
    out_token[LOCAL_TOKEN_HEX_LEN] = '\0';
    err = nvs_set_str(nvs, "api_token", out_token);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (err != ESP_OK) {
        out_token[0] = '\0';
    }
    return err;
}

#if !CONFIG_MINIHUB_JS_PLUGINS
esp_err_t minihub_js_run_on_worker_async(esp_err_t (*fn)(void *arg), void *arg)
{
    (void)fn;
    (void)arg;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif

esp_err_t app_minihub_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "MiniHub Host starting on Mosaico");

    esp_err_t err = minihub_auth_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "auth init failed: %s", esp_err_to_name(err));
    }

    err = minihub_core_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "core init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = minihub_config_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = app_minihub_seed_web();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "web seed failed: %s", esp_err_to_name(err));
    }

#if CONFIG_MINIHUB_NET_WIFI_STA
    (void)minihub_config_ensure_adapter_slot("wifi", false);
#endif
#if CONFIG_MINIHUB_MATTER_ADAPTER
    (void)minihub_config_ensure_adapter_slot("matter", true);
#endif

    err = minihub_adapter_registry_init();
    if (err != ESP_OK) {
        return err;
    }
    err = minihub_protocol_adapters_register_all();
    if (err != ESP_OK) {
        return err;
    }
    err = minihub_module_loader_init();
    if (err != ESP_OK) {
        return err;
    }

    cJSON *adapters_cfg = minihub_config_get_adapters();
    err = minihub_module_load_all(adapters_cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = minihub_pack_init();
    if (err != ESP_OK) {
        return err;
    }
    err = minihub_pack_merge_into_adapters(adapters_cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = minihub_adapter_load_all(adapters_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = minihub_net_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "net init failed: %s (panel owns Wi-Fi)",
                 esp_err_to_name(err));
    }

#if CONFIG_MINIHUB_THREAD_ADAPTER
    err = minihub_thread_init();
    if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Thread BR init failed: %s", esp_err_to_name(err));
    }
#endif

    err = minihub_rules_init(minihub_config_get_rules());
    if (err != ESP_OK) {
        return err;
    }
    err = minihub_scenes_init(minihub_config_get_scenes());
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scenes init failed: %s", esp_err_to_name(err));
    }

    err = minihub_web_start(80);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "web start failed: %s", esp_err_to_name(err));
        return err;
    }

    err = minihub_adapter_start_all();
    if (err != ESP_OK) {
        return err;
    }

#if CONFIG_MINIHUB_THREAD_ADAPTER && CONFIG_MINIHUB_MATTER_ADAPTER
    if (minihub_matter_ready() && minihub_thread_ready()) {
        err = minihub_matter_attach_thread();
        if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "Matter Thread attach failed: %s", esp_err_to_name(err));
        }
    }
#endif

    err = minihub_js_reload();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "JS plugin reload failed: %s", esp_err_to_name(err));
    }
    err = minihub_module_load_deferred(adapters_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "deferred module load failed: %s", esp_err_to_name(err));
    }

    err = minihub_adapter_start_poll_task();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "adapter poll task failed: %s", esp_err_to_name(err));
    }

    minihub_event_bus_publish("hub.ready", NULL);
    s_started = true;
    ESP_LOGI(TAG, "MiniHub ready (web=:80 eth=0 wifi_sta=%d matter=%d)",
#if CONFIG_MINIHUB_NET_WIFI_STA
             1,
#else
             0,
#endif
#if CONFIG_MINIHUB_MATTER_ADAPTER
             1);
#else
             0);
#endif
    return ESP_OK;
}
