#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FACTORY_NETWORK_SSID_BYTES 33U
#define FACTORY_NETWORK_PASSWORD_BYTES 65U
#define FACTORY_NETWORK_IP_BYTES 16U
#define FACTORY_NETWORK_HOSTNAME_BYTES 48U
#define FACTORY_NETWORK_MAX_RESULTS 16U

typedef enum {
    FACTORY_NETWORK_STOPPED = 0,
    FACTORY_NETWORK_NO_CREDENTIALS,
    FACTORY_NETWORK_CONNECTING,
    FACTORY_NETWORK_CONNECTED,
    FACTORY_NETWORK_FAILED,
} factory_network_state_t;

typedef struct {
    char ssid[FACTORY_NETWORK_SSID_BYTES];
    int8_t rssi;
    uint8_t authmode;
} factory_network_ap_t;

typedef struct {
    factory_network_state_t state;
    bool started;
    bool scanning;
    bool credentials_saved;
    char ssid[FACTORY_NETWORK_SSID_BYTES];
    char ip[FACTORY_NETWORK_IP_BYTES];
    char hostname[FACTORY_NETWORK_HOSTNAME_BYTES];
    esp_err_t last_error;
    uint32_t scan_generation;
    size_t ap_count;
    factory_network_ap_t aps[FACTORY_NETWORK_MAX_RESULTS];
} factory_network_snapshot_t;

esp_err_t factory_network_start(void);
esp_err_t factory_network_request_scan(void);
esp_err_t factory_network_connect(const char *ssid, const char *password);
esp_err_t factory_network_forget(void);
esp_err_t factory_network_get_snapshot(factory_network_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif
