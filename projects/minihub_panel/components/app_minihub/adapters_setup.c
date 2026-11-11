// SPDX-License-Identifier: Apache-2.0

#include "adapters_setup.h"

#include "esp_log.h"
#include "minihub_adapter.h"
#include "sdkconfig.h"

#if CONFIG_MINIHUB_MQTT_ADAPTER
extern const minihub_adapter_t mqtt_adapter;
#endif
#if CONFIG_MINIHUB_NET_WIFI_STA
extern const minihub_adapter_t wifi_adapter;
#endif
#if CONFIG_MINIHUB_ZIGBEE_SDK
extern const minihub_adapter_t zigbee_adapter;
#endif
#if CONFIG_MINIHUB_BLE_MESH_SDK
extern const minihub_adapter_t ble_mesh_adapter;
#endif
#if CONFIG_MINIHUB_MODBUS
extern const minihub_adapter_t modbus_adapter;
#endif
#if CONFIG_MINIHUB_MATTER_ADAPTER
extern const minihub_adapter_t matter_adapter;
#endif
#if CONFIG_MINIHUB_THREAD_ADAPTER
extern const minihub_adapter_t thread_adapter;
#endif

static const char *TAG = "adapters_setup";

esp_err_t minihub_protocol_adapters_register_all(void)
{
    esp_err_t err = ESP_OK;

#if CONFIG_MINIHUB_MQTT_ADAPTER
    err = minihub_adapter_register(&mqtt_adapter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register mqtt adapter failed: %s", esp_err_to_name(err));
        return err;
    }
#endif
#if CONFIG_MINIHUB_NET_WIFI_STA
    err = minihub_adapter_register(&wifi_adapter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register wifi adapter failed: %s", esp_err_to_name(err));
        return err;
    }
#endif
#if CONFIG_MINIHUB_ZIGBEE_SDK
    err = minihub_adapter_register(&zigbee_adapter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register zigbee adapter failed: %s", esp_err_to_name(err));
        return err;
    }
#endif
#if CONFIG_MINIHUB_BLE_MESH_SDK
    err = minihub_adapter_register(&ble_mesh_adapter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register ble_mesh adapter failed: %s", esp_err_to_name(err));
        return err;
    }
#endif
#if CONFIG_MINIHUB_MODBUS
    err = minihub_adapter_register(&modbus_adapter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register modbus adapter failed: %s", esp_err_to_name(err));
        return err;
    }
#endif
#if CONFIG_MINIHUB_MATTER_ADAPTER
    err = minihub_adapter_register(&matter_adapter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register matter adapter failed: %s", esp_err_to_name(err));
        return err;
    }
#endif
#if CONFIG_MINIHUB_THREAD_ADAPTER
    err = minihub_adapter_register(&thread_adapter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register thread adapter failed: %s", esp_err_to_name(err));
        return err;
    }
#endif
    (void)err;
    return ESP_OK;
}
