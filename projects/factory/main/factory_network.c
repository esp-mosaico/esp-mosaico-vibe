#include "factory_network.h"

#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "factory_system_metadata.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mdns.h"
#include "nvs.h"

#define FACTORY_WIFI_NAMESPACE "wifi"
#define FACTORY_WIFI_SSID_KEY "ssid"
#define FACTORY_WIFI_PASSWORD_KEY "password"

typedef struct {
    SemaphoreHandle_t lock;
    esp_netif_t *netif;
    esp_event_handler_instance_t wifi_events;
    esp_event_handler_instance_t ip_events;
    factory_network_snapshot_t snapshot;
    char pending_ssid[FACTORY_NETWORK_SSID_BYTES];
    char pending_password[FACTORY_NETWORK_PASSWORD_BYTES];
    int64_t connect_started_us;
    bool pending_credentials;
    bool mdns_started;
} factory_network_context_t;

static const char *TAG = "factory_network";
static factory_network_context_t s_network;

static void snapshot_set_error(esp_err_t error,
                               factory_network_state_t state)
{
    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    s_network.snapshot.last_error = error;
    s_network.snapshot.state = state;
    xSemaphoreGive(s_network.lock);
}

static void scan_set_error(esp_err_t error)
{
    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    s_network.snapshot.scanning = false;
    s_network.snapshot.last_error = error;
    xSemaphoreGive(s_network.lock);
}

static esp_err_t credentials_load(char ssid[FACTORY_NETWORK_SSID_BYTES],
                                  char password[FACTORY_NETWORK_PASSWORD_BYTES])
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        FACTORY_SYSTEM_METADATA_PARTITION, FACTORY_WIFI_NAMESPACE,
        NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    size_t ssid_size = FACTORY_NETWORK_SSID_BYTES;
    size_t password_size = FACTORY_NETWORK_PASSWORD_BYTES;
    err = nvs_get_str(handle, FACTORY_WIFI_SSID_KEY, ssid, &ssid_size);
    if (err == ESP_OK) {
        err = nvs_get_str(handle, FACTORY_WIFI_PASSWORD_KEY, password,
                          &password_size);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t credentials_save(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open_from_partition(
                            FACTORY_SYSTEM_METADATA_PARTITION,
                            FACTORY_WIFI_NAMESPACE, NVS_READWRITE, &handle),
                        TAG, "open system metadata Wi-Fi namespace");
    esp_err_t err = nvs_set_str(handle, FACTORY_WIFI_SSID_KEY, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, FACTORY_WIFI_PASSWORD_KEY, password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t credentials_erase(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        FACTORY_SYSTEM_METADATA_PARTITION, FACTORY_WIFI_NAMESPACE,
        NVS_READWRITE, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "open factory Wi-Fi NVS");
    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t apply_station_config(const char *ssid, const char *password)
{
    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, password, sizeof(config.sta.password));
    config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &config), TAG,
                        "set station configuration");

    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    strlcpy(s_network.snapshot.ssid, ssid,
            sizeof(s_network.snapshot.ssid));
    s_network.snapshot.state = FACTORY_NETWORK_CONNECTING;
    s_network.snapshot.last_error = ESP_OK;
    s_network.snapshot.ip[0] = '\0';
    s_network.connect_started_us = esp_timer_get_time();
    xSemaphoreGive(s_network.lock);
    return esp_wifi_connect();
}

static void mdns_start(void)
{
    if (s_network.mdns_started) {
        return;
    }

    char device_id[33] = {0};
    char suffix[7] = {0};
    if (esp_iris_format_device_id(device_id) == ESP_OK) {
        memcpy(suffix, device_id + sizeof(device_id) - sizeof(suffix),
               sizeof(suffix) - 1);
    } else {
        uint8_t mac[6];
        if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
            return;
        }
        snprintf(suffix, sizeof(suffix), "%02x%02x%02x", mac[3], mac[4],
                 mac[5]);
    }

    char hostname[FACTORY_NETWORK_HOSTNAME_BYTES];
    char instance[64];
    snprintf(hostname, sizeof(hostname), "%s-%s",
             CONFIG_IRIS_FACTORY_MDNS_PREFIX, suffix);
    snprintf(instance, sizeof(instance), "ESP-Mosaico %s", suffix);

    esp_err_t err = mdns_init();
    if (err == ESP_OK) {
        err = mdns_hostname_set(hostname);
    }
    if (err == ESP_OK) {
        err = mdns_instance_name_set(instance);
    }
    char port[8];
    snprintf(port, sizeof(port), "%u", CONFIG_ESP_IRIS_TCP_PORT);
    mdns_txt_item_t txt[] = {
        {.key = "device_id", .value = device_id},
        {.key = "mode", .value = "recovery"},
        {.key = "protocol", .value = "1"},
        {.key = "pairing", .value = "hmac"},
        {.key = "port", .value = port},
    };
    if (err == ESP_OK) {
        err = mdns_service_add(instance, "_esp-iris", "_tcp",
                               CONFIG_ESP_IRIS_TCP_PORT, txt,
                               sizeof(txt) / sizeof(txt[0]));
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS setup failed: %s", esp_err_to_name(err));
        mdns_free();
        return;
    }

    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    strlcpy(s_network.snapshot.hostname, hostname,
            sizeof(s_network.snapshot.hostname));
    s_network.mdns_started = true;
    xSemaphoreGive(s_network.lock);
    ESP_LOGI(TAG, "mDNS service %s.local:%u", hostname,
             CONFIG_ESP_IRIS_TCP_PORT);
}

static void scan_results_update(void)
{
    uint16_t available = 0;
    if (esp_wifi_scan_get_ap_num(&available) != ESP_OK) {
        scan_set_error(ESP_FAIL);
        return;
    }
    uint16_t count = available;
    if (count > CONFIG_IRIS_FACTORY_WIFI_MAX_NETWORKS) {
        count = CONFIG_IRIS_FACTORY_WIFI_MAX_NETWORKS;
    }
    if (count > FACTORY_NETWORK_MAX_RESULTS) {
        count = FACTORY_NETWORK_MAX_RESULTS;
    }
    wifi_ap_record_t records[FACTORY_NETWORK_MAX_RESULTS] = {0};
    esp_err_t err = count > 0
        ? esp_wifi_scan_get_ap_records(&count, records) : ESP_OK;
    if (err != ESP_OK) {
        scan_set_error(err);
        return;
    }

    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    s_network.snapshot.ap_count = 0;
    for (uint16_t i = 0; i < count; ++i) {
        if (records[i].ssid[0] == '\0') {
            continue;
        }
        bool duplicate = false;
        for (size_t j = 0; j < s_network.snapshot.ap_count; ++j) {
            if (strncmp(s_network.snapshot.aps[j].ssid,
                        (const char *)records[i].ssid,
                        FACTORY_NETWORK_SSID_BYTES) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        factory_network_ap_t *ap =
            &s_network.snapshot.aps[s_network.snapshot.ap_count++];
        strlcpy(ap->ssid, (const char *)records[i].ssid, sizeof(ap->ssid));
        ap->rssi = records[i].rssi;
        ap->authmode = (uint8_t)records[i].authmode;
    }
    s_network.snapshot.scanning = false;
    s_network.snapshot.scan_generation++;
    xSemaphoreGive(s_network.lock);
}

static void network_event(void *arg, esp_event_base_t base, int32_t id,
                          void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        scan_results_update();
        return;
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = data;
        const int64_t now = esp_timer_get_time();
        bool retry = false;
        xSemaphoreTake(s_network.lock, portMAX_DELAY);
        if (s_network.snapshot.state == FACTORY_NETWORK_CONNECTED) {
            s_network.connect_started_us = now;
        }
        const int64_t elapsed_ms =
            (now - s_network.connect_started_us) / 1000;
        s_network.snapshot.ip[0] = '\0';
        s_network.snapshot.last_error = event != NULL
            ? (esp_err_t)(ESP_ERR_WIFI_BASE + event->reason) : ESP_FAIL;
        retry = s_network.snapshot.credentials_saved ||
                s_network.pending_credentials;
        if (retry && elapsed_ms < CONFIG_IRIS_FACTORY_WIFI_CONNECT_TIMEOUT_MS) {
            s_network.snapshot.state = FACTORY_NETWORK_CONNECTING;
        } else {
            s_network.snapshot.state = FACTORY_NETWORK_FAILED;
            retry = false;
        }
        xSemaphoreGive(s_network.lock);
        if (retry) {
            (void)esp_wifi_connect();
        }
        return;
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        char ip[FACTORY_NETWORK_IP_BYTES] = {0};
        esp_ip4addr_ntoa(&event->ip_info.ip, ip, sizeof(ip));

        bool save = false;
        char ssid[FACTORY_NETWORK_SSID_BYTES] = {0};
        char password[FACTORY_NETWORK_PASSWORD_BYTES] = {0};
        xSemaphoreTake(s_network.lock, portMAX_DELAY);
        strlcpy(s_network.snapshot.ip, ip, sizeof(s_network.snapshot.ip));
        s_network.snapshot.state = FACTORY_NETWORK_CONNECTED;
        s_network.snapshot.last_error = ESP_OK;
        if (s_network.pending_credentials) {
            strlcpy(ssid, s_network.pending_ssid, sizeof(ssid));
            strlcpy(password, s_network.pending_password, sizeof(password));
            s_network.pending_credentials = false;
            save = true;
        }
        xSemaphoreGive(s_network.lock);

        if (save) {
            const esp_err_t save_err = credentials_save(ssid, password);
            xSemaphoreTake(s_network.lock, portMAX_DELAY);
            s_network.snapshot.credentials_saved = save_err == ESP_OK;
            if (save_err != ESP_OK) {
                s_network.snapshot.last_error = save_err;
            }
            xSemaphoreGive(s_network.lock);
        }
        mdns_start();
        char connected_ssid[FACTORY_NETWORK_SSID_BYTES];
        xSemaphoreTake(s_network.lock, portMAX_DELAY);
        strlcpy(connected_ssid, s_network.snapshot.ssid,
                sizeof(connected_ssid));
        xSemaphoreGive(s_network.lock);
        ESP_LOGI(TAG, "factory Wi-Fi connected: SSID=%s IP=%s",
                 connected_ssid, ip);
    }
}

esp_err_t factory_network_start(void)
{
    if (s_network.lock != NULL) {
        return ESP_OK;
    }
    s_network.lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_network.lock, ESP_ERR_NO_MEM, TAG,
                        "create network mutex");
    s_network.snapshot.state = FACTORY_NETWORK_STOPPED;

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    s_network.netif = esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_FALSE(s_network.netif, ESP_ERR_NO_MEM, TAG,
                        "create station netif");
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&config), TAG, "initialize Wi-Fi");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                            WIFI_EVENT, ESP_EVENT_ANY_ID, network_event, NULL,
                            &s_network.wifi_events),
                        TAG, "register Wi-Fi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                            IP_EVENT, IP_EVENT_STA_GOT_IP, network_event, NULL,
                            &s_network.ip_events),
                        TAG, "register IP events");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG,
                        "keep factory credentials out of default NVS");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG,
                        "set station mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start Wi-Fi");

    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    s_network.snapshot.started = true;
    s_network.snapshot.state = FACTORY_NETWORK_NO_CREDENTIALS;
    xSemaphoreGive(s_network.lock);

    char ssid[FACTORY_NETWORK_SSID_BYTES] = {0};
    char password[FACTORY_NETWORK_PASSWORD_BYTES] = {0};
    err = credentials_load(ssid, password);
    if (err == ESP_OK && ssid[0] != '\0') {
        xSemaphoreTake(s_network.lock, portMAX_DELAY);
        s_network.snapshot.credentials_saved = true;
        xSemaphoreGive(s_network.lock);
        ESP_RETURN_ON_ERROR(apply_station_config(ssid, password), TAG,
                            "connect saved factory network");
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        snapshot_set_error(err, FACTORY_NETWORK_FAILED);
    }
    (void)factory_network_request_scan();
    return ESP_OK;
}

esp_err_t factory_network_request_scan(void)
{
    ESP_RETURN_ON_FALSE(s_network.lock, ESP_ERR_INVALID_STATE, TAG,
                        "factory network is not started");
    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    if (s_network.snapshot.scanning) {
        xSemaphoreGive(s_network.lock);
        return ESP_OK;
    }
    s_network.snapshot.scanning = true;
    xSemaphoreGive(s_network.lock);
    const esp_err_t err = esp_wifi_scan_start(NULL, false);
    if (err != ESP_OK) {
        xSemaphoreTake(s_network.lock, portMAX_DELAY);
        s_network.snapshot.scanning = false;
        s_network.snapshot.last_error = err;
        xSemaphoreGive(s_network.lock);
    }
    return err;
}

esp_err_t factory_network_connect(const char *ssid, const char *password)
{
    ESP_RETURN_ON_FALSE(s_network.lock && ssid && password,
                        ESP_ERR_INVALID_ARG, TAG, "invalid connection request");
    const size_t ssid_size = strnlen(ssid, FACTORY_NETWORK_SSID_BYTES);
    const size_t password_size =
        strnlen(password, FACTORY_NETWORK_PASSWORD_BYTES);
    bool password_valid = password_size == 0 ||
        (password_size >= 8 && password_size <= 63);
    if (password_size == 64) {
        password_valid = true;
        for (size_t i = 0; i < password_size; ++i) {
            const char character = password[i];
            if (!((character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f') ||
                  (character >= 'A' && character <= 'F'))) {
                password_valid = false;
                break;
            }
        }
    }
    ESP_RETURN_ON_FALSE(ssid_size > 0 && ssid_size < FACTORY_NETWORK_SSID_BYTES,
                        ESP_ERR_INVALID_SIZE, TAG, "invalid SSID length");
    ESP_RETURN_ON_FALSE(password_valid,
                        ESP_ERR_INVALID_SIZE, TAG, "invalid password length");

    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    strlcpy(s_network.pending_ssid, ssid, sizeof(s_network.pending_ssid));
    strlcpy(s_network.pending_password, password,
            sizeof(s_network.pending_password));
    s_network.pending_credentials = true;
    xSemaphoreGive(s_network.lock);
    (void)esp_wifi_disconnect();
    return apply_station_config(ssid, password);
}

esp_err_t factory_network_forget(void)
{
    ESP_RETURN_ON_FALSE(s_network.lock, ESP_ERR_INVALID_STATE, TAG,
                        "factory network is not started");
    ESP_RETURN_ON_ERROR(credentials_erase(), TAG, "erase factory credentials");
    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    s_network.pending_credentials = false;
    memset(s_network.pending_ssid, 0, sizeof(s_network.pending_ssid));
    memset(s_network.pending_password, 0, sizeof(s_network.pending_password));
    s_network.snapshot.credentials_saved = false;
    s_network.snapshot.ssid[0] = '\0';
    s_network.snapshot.ip[0] = '\0';
    s_network.snapshot.state = FACTORY_NETWORK_NO_CREDENTIALS;
    s_network.snapshot.last_error = ESP_OK;
    xSemaphoreGive(s_network.lock);
    (void)esp_wifi_disconnect();
    return factory_network_request_scan();
}

esp_err_t factory_network_get_snapshot(factory_network_snapshot_t *snapshot)
{
    ESP_RETURN_ON_FALSE(snapshot, ESP_ERR_INVALID_ARG, TAG,
                        "snapshot is null");
    ESP_RETURN_ON_FALSE(s_network.lock, ESP_ERR_INVALID_STATE, TAG,
                        "factory network is not started");
    xSemaphoreTake(s_network.lock, portMAX_DELAY);
    *snapshot = s_network.snapshot;
    xSemaphoreGive(s_network.lock);
    return ESP_OK;
}
