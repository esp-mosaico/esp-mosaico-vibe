// SPDX-License-Identifier: Apache-2.0

#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include IRIS_ACCEPTANCE_WIFI_HEADER

_Static_assert(sizeof(IRIS_ACCEPTANCE_WIFI_SSID) > 1 &&
               sizeof(IRIS_ACCEPTANCE_WIFI_SSID) <= 33,
               "private Wi-Fi SSID must contain 1..32 bytes");
_Static_assert(sizeof(IRIS_ACCEPTANCE_WIFI_PASSWORD) <= 65,
               "private Wi-Fi password must contain at most 64 bytes");

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT &&
        (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) {
        (void)esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        ESP_LOGI("iris_acceptance", "ACCEPTANCE_WIFI_READY ip=" IPSTR,
                 IP2STR(&event->ip_info.ip));
    }
}

void iris_acceptance_wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta() != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, IRIS_ACCEPTANCE_WIFI_SSID, sizeof(IRIS_ACCEPTANCE_WIFI_SSID) - 1);
    memcpy(config.sta.password, IRIS_ACCEPTANCE_WIFI_PASSWORD, sizeof(IRIS_ACCEPTANCE_WIFI_PASSWORD) - 1);
    config.sta.threshold.authmode = sizeof(IRIS_ACCEPTANCE_WIFI_PASSWORD) > 1
        ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
