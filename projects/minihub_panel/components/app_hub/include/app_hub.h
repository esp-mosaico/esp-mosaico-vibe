// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_HUB_PAGE_SIZE 4
#define APP_HUB_WIFI_AP_LIMIT 16
#define APP_HUB_WIFI_PAGE_SIZE APP_HUB_WIFI_AP_LIMIT
#define APP_HUB_POINT_ID_LEN 65
#define APP_HUB_NAME_LEN 97
#define APP_HUB_TYPE_LEN 25
#define APP_HUB_PARAMETER_LEN 25
#define APP_HUB_VALUE_LEN 40
#define APP_HUB_STATUS_LEN 128
#define APP_HUB_SSID_LEN 33
#define APP_HUB_WIFI_PASSWORD_LEN 65
#define APP_HUB_TOKEN_LEN 97
#define APP_HUB_URL_LEN 97
#define APP_HUB_PAIRING_CODE_LEN 64

/** 11/21-digit manual code (spaces/dashes ignored) or an MT: QR payload. */
static inline bool app_hub_matter_pairing_code_ok(const char *code)
{
    if (!code) {
        return false;
    }
    while (*code == ' ' || *code == '\t') {
        ++code;
    }
    if ((code[0] == 'M' || code[0] == 'm') &&
            (code[1] == 'T' || code[1] == 't') &&
            code[2] == ':') {
        return code[3] != '\0';
    }
    size_t digits = 0;
    for (const char *p = code; *p != '\0'; ++p) {
        if (*p == ' ' || *p == '-') {
            continue;
        }
        if (*p < '0' || *p > '9') {
            return false;
        }
        ++digits;
    }
    return digits == 11U || digits == 21U;
}
#define APP_HUB_SCENE_ID_LEN 33
#define APP_HUB_SCENE_NAME_LEN 65
#define APP_HUB_RULE_ID_LEN 33
#define APP_HUB_RULE_NAME_LEN 65
#define APP_HUB_VERSION_LEN 32
#define APP_HUB_IP_LEN 16
#define APP_HUB_HOST_LEN 64
#define APP_HUB_TOPIC_LEN 96
#define APP_HUB_CREDENTIAL_LEN 64
#define APP_HUB_ROOM_LEN 33
#define APP_HUB_SCENES_PAGE_SIZE 4
#define APP_HUB_RULES_PAGE_SIZE 4

typedef enum {
    APP_HUB_ROLE_UNSET = 0,
    APP_HUB_ROLE_HUB,
    APP_HUB_ROLE_WEB,
} app_hub_role_t;

typedef enum {
    APP_HUB_COMMISSION_IDLE = 0,
    APP_HUB_COMMISSION_RUNNING,
    APP_HUB_COMMISSION_OK,
    APP_HUB_COMMISSION_FAILED,
} app_hub_commission_state_t;

typedef enum {
    APP_HUB_COMMISSION_STAGE_NONE = 0,
    APP_HUB_COMMISSION_STAGE_DISCOVERING,
    APP_HUB_COMMISSION_STAGE_BINDING,
    APP_HUB_COMMISSION_STAGE_ACTIVATING,
} app_hub_commission_stage_t;

typedef struct {
    app_hub_commission_state_t state;
    app_hub_commission_stage_t stage;
    uint32_t node_id;
    char error[APP_HUB_STATUS_LEN];
} app_hub_commission_snapshot_t;

typedef enum {
    APP_HUB_PROTOCOL_MATTER = 0,
    APP_HUB_PROTOCOL_ZIGBEE,
    APP_HUB_PROTOCOL_BLE_MESH,
    APP_HUB_PROTOCOL_MQTT,
    APP_HUB_PROTOCOL_MODBUS_TCP,
    APP_HUB_PROTOCOL_COUNT,
} app_hub_protocol_t;

typedef enum {
    APP_HUB_ADD_IDLE = 0,
    APP_HUB_ADD_PREPARING,
    APP_HUB_ADD_WAITING,
    APP_HUB_ADD_OK,
    APP_HUB_ADD_FAILED,
    APP_HUB_ADD_CANCELLED,
} app_hub_add_state_t;

enum {
    APP_HUB_CAP_MATTER = 1U << APP_HUB_PROTOCOL_MATTER,
    APP_HUB_CAP_ZIGBEE = 1U << APP_HUB_PROTOCOL_ZIGBEE,
    APP_HUB_CAP_BLE_MESH = 1U << APP_HUB_PROTOCOL_BLE_MESH,
    APP_HUB_CAP_MQTT = 1U << APP_HUB_PROTOCOL_MQTT,
    APP_HUB_CAP_MODBUS_TCP = 1U << APP_HUB_PROTOCOL_MODBUS_TCP,
};

typedef struct {
    app_hub_protocol_t protocol;
    char name[APP_HUB_NAME_LEN];
    char room[APP_HUB_ROOM_LEN];
    char pairing_code[APP_HUB_PAIRING_CODE_LEN];
    char ieee[24];
    char install_code[40];
    char host[APP_HUB_HOST_LEN];
    uint16_t port;
    char username[APP_HUB_CREDENTIAL_LEN];
    char password[APP_HUB_CREDENTIAL_LEN];
    char point_id[APP_HUB_POINT_ID_LEN];
    char type[APP_HUB_TYPE_LEN];
    char state_topic[APP_HUB_TOPIC_LEN];
    char command_topic[APP_HUB_TOPIC_LEN];
    uint8_t unit_id;
    uint16_t address;
    char function[12];
    char datatype[8];
    float scale;
    bool writable;
} app_hub_add_request_t;

typedef struct {
    uint32_t generation;
    uint32_t capabilities;
    app_hub_protocol_t protocol;
    app_hub_add_state_t state;
    uint8_t progress;
    char point_id[APP_HUB_POINT_ID_LEN];
    char status[APP_HUB_STATUS_LEN];
} app_hub_add_snapshot_t;

typedef struct app_hub *app_hub_handle_t;

typedef enum {
    APP_HUB_OK = 0,
    APP_HUB_ERR_INVALID_ARG,
    APP_HUB_ERR_NO_MEMORY,
    APP_HUB_ERR_BUSY,
    APP_HUB_ERR_NOT_SUPPORTED,
    APP_HUB_ERR_INTERNAL,
} app_hub_result_t;

typedef struct {
    char point_id[APP_HUB_POINT_ID_LEN];
    char name[APP_HUB_NAME_LEN];
    char type[APP_HUB_TYPE_LEN];
    char value[APP_HUB_VALUE_LEN];
    bool online;
    uint8_t offline_strikes;
    bool controllable;
    bool on;
    bool commissioning;
    bool commission_failed;
    uint32_t node_id;
    bool has_brightness;
    bool has_hue;
    bool has_saturation;
    bool has_cct;
    int32_t brightness;
    int32_t hue;
    int32_t saturation;
    int32_t cct;
} app_hub_point_t;

typedef struct {
    uint32_t generation;
    size_t total_points;
    size_t online_points;
    size_t page;
    size_t page_count;
    size_t visible_points;
    bool connected;
    bool wifi_ready;
    bool wifi_needs_setup;
    bool auth_needed;
    bool role_needed;
    bool url_needed;
    app_hub_role_t role;
    char status[APP_HUB_STATUS_LEN];
    app_hub_point_t points[APP_HUB_PAGE_SIZE];
} app_hub_snapshot_t;

typedef struct {
    char ssid[APP_HUB_SSID_LEN];
    int8_t rssi;
    bool secured;
} app_hub_wifi_ap_t;

typedef struct {
    uint32_t generation;
    bool ready;
    bool scanning;
    bool connecting;
    bool needs_setup;
    char ssid[APP_HUB_SSID_LEN];
    char status[APP_HUB_STATUS_LEN];
    size_t total_aps;
    size_t page;
    size_t page_count;
    size_t visible_aps;
    app_hub_wifi_ap_t aps[APP_HUB_WIFI_PAGE_SIZE];
} app_hub_wifi_snapshot_t;

typedef struct {
    char id[APP_HUB_SCENE_ID_LEN];
    char name[APP_HUB_SCENE_NAME_LEN];
    uint8_t step_count;
    uint8_t action_count;
    char action_points[APP_HUB_PAGE_SIZE][APP_HUB_POINT_ID_LEN];
    bool action_on[APP_HUB_PAGE_SIZE];
} app_hub_scene_t;

typedef struct {
    uint32_t generation;
    size_t total;
    size_t page;
    size_t page_count;
    size_t visible;
    bool ready;
    bool pending;
    char status[APP_HUB_STATUS_LEN];
    app_hub_scene_t scenes[APP_HUB_SCENES_PAGE_SIZE];
} app_hub_scenes_snapshot_t;

typedef struct {
    char id[APP_HUB_RULE_ID_LEN];
    char name[APP_HUB_RULE_NAME_LEN];
    bool enabled;
} app_hub_rule_t;

typedef struct {
    uint32_t generation;
    size_t total;
    size_t page;
    size_t page_count;
    size_t visible;
    bool ready;
    bool pending;
    char status[APP_HUB_STATUS_LEN];
    app_hub_rule_t rules[APP_HUB_RULES_PAGE_SIZE];
} app_hub_rules_snapshot_t;

typedef struct {
    uint32_t generation;
    bool ready;
    bool hub_online;
    char version[APP_HUB_VERSION_LEN];
    char ip[APP_HUB_IP_LEN];
    uint32_t uptime_s;
    uint32_t internal_free_kb;
    uint32_t psram_free_kb;
    uint32_t storage_free_kb;
    char status[APP_HUB_STATUS_LEN];
} app_hub_system_snapshot_t;

/** Initialize the shared TCP/IP stack without starting the Wi-Fi radio. */
app_hub_result_t app_hub_network_init(void);

/**
 * Start the Wi-Fi-owned MiniHub client and its worker task.
 *
 * The returned handle owns networking state, its worker, and synchronization
 * objects. It remains valid until reboot; no shutdown path is currently
 * exposed because the application has the same lifetime.
 */
app_hub_result_t app_hub_start(app_hub_handle_t *out_handle);

/** Copy one page of the current model under a bounded mutex wait. */
app_hub_result_t app_hub_snapshot(app_hub_handle_t handle, size_t page,
                                  app_hub_snapshot_t *out_snapshot);

/** Queue a power toggle for a controllable point; never performs I/O inline. */
app_hub_result_t app_hub_toggle(app_hub_handle_t handle, const char *point_id);

/** Queue one integer-valued Point parameter update. */
app_hub_result_t app_hub_set_parameter(app_hub_handle_t handle,
                                       const char *point_id,
                                       const char *parameter, int32_t value);

/** Wake the worker for an immediate full Point refresh. */
app_hub_result_t app_hub_refresh(app_hub_handle_t handle);

/** Copy one page of scanned access points and the current Wi-Fi state. */
app_hub_result_t app_hub_wifi_snapshot(app_hub_handle_t handle, size_t page,
                                       app_hub_wifi_snapshot_t *out_snapshot);

/** Queue a station scan. The UI must not wait for radio I/O. */
app_hub_result_t app_hub_wifi_scan(app_hub_handle_t handle);

/** Queue a station connect and persist the profile after it is accepted. */
app_hub_result_t app_hub_wifi_connect(app_hub_handle_t handle, const char *ssid,
                                      const char *password);

/** Persist a MiniHub bearer token and wake the worker to retry Point sync. */
app_hub_result_t app_hub_set_token(app_hub_handle_t handle, const char *token);

/** Persist Hub vs Web role. Hub always uses http://127.0.0.1. */
app_hub_result_t app_hub_set_role(app_hub_handle_t handle, app_hub_role_t role,
                                  const char *url);

/** Persist a remote MiniHub base URL (Web role) and wake the worker. */
app_hub_result_t app_hub_set_url(app_hub_handle_t handle, const char *url);

/** Queue a Matter device commissioning request by pairing code / QR payload. */
app_hub_result_t app_hub_matter_commission(app_hub_handle_t handle,
                                           const char *pairing_code);

/** Copy the latest Matter commissioning progress snapshot. */
app_hub_result_t app_hub_matter_commission_snapshot(
    app_hub_handle_t handle, app_hub_commission_snapshot_t *out_snapshot);

/** Queue a Matter commissioning cancel request for a node (0 = active node). */
app_hub_result_t app_hub_matter_cancel(app_hub_handle_t handle,
                                       uint32_t node_id);

/** Return compiled/remote protocol capabilities and the current add session. */
app_hub_result_t app_hub_add_snapshot(app_hub_handle_t handle,
                                      app_hub_add_snapshot_t *out_snapshot);

/** Queue one complete protocol-specific device add request. */
app_hub_result_t app_hub_add_device(app_hub_handle_t handle,
                                    const app_hub_add_request_t *request);

/** Cancel the active radio provisioning/commissioning window. */
app_hub_result_t app_hub_add_cancel(app_hub_handle_t handle);

/** Queue a device deletion request (and cancel if commissioning). */
app_hub_result_t app_hub_delete_point(app_hub_handle_t handle,
                                      const char *point_id, uint32_t node_id);

/** Ask MiniHub to re-arm this point's subscription and prove reachability. */
app_hub_result_t app_hub_reconnect(app_hub_handle_t handle, const char *point_id);

/** Copy one page of local or remote MiniHub scenes. */
app_hub_result_t app_hub_scenes_snapshot(
    app_hub_handle_t handle, size_t page,
    app_hub_scenes_snapshot_t *out_snapshot);

/** Queue a scene invocation. HTTP is performed by the worker task. */
app_hub_result_t app_hub_scene_invoke(app_hub_handle_t handle,
                                      const char *scene_id);

/** Create a scene from selected point power targets. */
app_hub_result_t app_hub_scene_create(app_hub_handle_t handle,
                                      const char *name,
                                      const char *const *point_ids,
                                      const bool *on_values, size_t count);

/** Remove a scene from the Hub document and reload. */
app_hub_result_t app_hub_scene_delete(app_hub_handle_t handle,
                                      const char *scene_id);

/** Copy one page of automation rules. */
app_hub_result_t app_hub_rules_snapshot(
    app_hub_handle_t handle, size_t page,
    app_hub_rules_snapshot_t *out_snapshot);

/** Queue a rule enabled-state update using the complete rules document. */
app_hub_result_t app_hub_rule_set_enabled(app_hub_handle_t handle,
                                          const char *rule_id, bool enabled);

/** Remove a rule from the Hub document and reload. */
app_hub_result_t app_hub_rule_delete(app_hub_handle_t handle,
                                     const char *rule_id);

/** Create a simple equality-triggered point-to-point power automation. */
app_hub_result_t app_hub_rule_create(app_hub_handle_t handle, const char *name,
                                     const char *source_point,
                                     const char *source_value,
                                     const char *target_point, bool target_on);

/** Copy the latest lightweight system status. */
app_hub_result_t app_hub_system_snapshot(
    app_hub_handle_t handle, app_hub_system_snapshot_t *out_snapshot);

/** Queue a confirmed MiniHub restart. */
app_hub_result_t app_hub_system_reboot(app_hub_handle_t handle);

#ifdef __cplusplus
}
#endif
