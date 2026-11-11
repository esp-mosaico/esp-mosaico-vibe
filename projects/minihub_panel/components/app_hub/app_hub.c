// SPDX-License-Identifier: Apache-2.0

#include "app_hub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_websocket_client.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "minihub_provision.h"
#include "sdkconfig.h"

#ifndef CONFIG_MINIHUB_MATTER_COMMISSION_WAIT_MS
#define CONFIG_MINIHUB_MATTER_COMMISSION_WAIT_MS 300000
#endif

#define HUB_MAX_POINTS 64
#define HUB_MAX_SCENES 8
#define HUB_MAX_RULES 16
#define HUB_HTTP_BODY_CAPACITY (64 * 1024)
#define HUB_URL_CAPACITY 256
#define HUB_COOKIE_CAPACITY 160
#define HUB_COMMAND_QUEUE_LENGTH 8
#define HUB_CONNECTED_BIT BIT0
#define HUB_STA_STARTED_BIT BIT1
#define HUB_SCAN_DONE_BIT BIT2
#define HUB_STA_DISCONNECTED_BIT BIT3
#define HUB_WIFI_SCAN_TIMEOUT_MS 15000
#define HUB_WIFI_DISCONNECT_TIMEOUT_MS 1000
#define HUB_WIFI_CONNECT_TIMEOUT_MS 20000
#define HUB_MATTER_ADD_TIMEOUT_MS CONFIG_MINIHUB_MATTER_COMMISSION_WAIT_MS
#define HUB_SYNC_INTERVAL_MS 3000
#define HUB_METADATA_SYNC_CYCLES 10
#define HUB_HTTP_TIMEOUT_MS 5000
#define HUB_MATTER_START_TIMEOUT_MS 15000
#define HUB_MUTEX_TIMEOUT_MS 100
#define HUB_OFFLINE_STRIKES 3
#define HUB_NVS_NAMESPACE "minihub_panel"
#define HUB_NVS_SSID "wifi_ssid"
#define HUB_NVS_PASSWORD "wifi_pass"
#define HUB_NVS_WIFI_VERIFIED "wifi_ok"
#define HUB_NVS_TOKEN "api_token"
#define HUB_NVS_ROLE "hub_role"
#define HUB_NVS_URL "hub_url"
#define HUB_NVS_SETUP_WIPE "setup_wipe"
#define HUB_SETUP_WIPE_TOKEN 2
#define HUB_LOCAL_URL "http://127.0.0.1"

typedef enum {
    HUB_CMD_DEVICE = 0,
    HUB_CMD_WIFI_SCAN,
    HUB_CMD_WIFI_CONNECT,
    HUB_CMD_MATTER_COMMISSION,
    HUB_CMD_MATTER_CANCEL,
    HUB_CMD_ADD_DEVICE,
    HUB_CMD_ADD_CANCEL,
    HUB_CMD_DELETE_POINT,
    HUB_CMD_SCENE_INVOKE,
    HUB_CMD_SCENE_CREATE,
    HUB_CMD_SCENE_DELETE,
    HUB_CMD_RULE_SET_ENABLED,
    HUB_CMD_RULE_CREATE,
    HUB_CMD_RULE_DELETE,
    HUB_CMD_SYSTEM_REBOOT,
    HUB_CMD_HUB_RELOAD,
} hub_cmd_kind_t;

static const char *TAG = "app_hub";

typedef struct {
    hub_cmd_kind_t kind;
    char point_id[APP_HUB_POINT_ID_LEN];
    char parameter[APP_HUB_PARAMETER_LEN];
    char wifi_password[APP_HUB_WIFI_PASSWORD_LEN];
    int32_t value;
    bool has_value;
    bool enabled;
    app_hub_add_request_t add;
    char name[APP_HUB_NAME_LEN];
    char source_value[APP_HUB_VALUE_LEN];
    char target_point[APP_HUB_POINT_ID_LEN];
    char scene_points[APP_HUB_PAGE_SIZE][APP_HUB_POINT_ID_LEN];
    bool scene_values[APP_HUB_PAGE_SIZE];
    uint8_t scene_count;
} hub_command_t;

typedef struct {
    char *body;
    size_t length;
    size_t capacity;
    char *cookie;
    size_t cookie_capacity;
} http_capture_t;

typedef struct {
    int status;
    char *body;
    char cookie[HUB_COOKIE_CAPACITY];
} http_response_t;

struct app_hub {
    SemaphoreHandle_t mutex;
    EventGroupHandle_t wifi_events;
    QueueHandle_t command_queue;
    TaskHandle_t worker;
    app_hub_point_t points[HUB_MAX_POINTS];
    size_t point_count;
    size_t online_count;
    uint32_t generation;
    bool hub_connected;
    bool wifi_ready;
    bool wifi_needs_setup;
    bool wifi_scanning;
    bool wifi_connecting;
    bool wifi_has_profile;
    bool wifi_auto_reconnect;
    bool wifi_expect_disconnect;
    volatile bool wifi_profile_save_pending;
    TickType_t wifi_connect_started_at;
    bool auth_failed;
    app_hub_role_t role;
    char wifi_ssid[APP_HUB_SSID_LEN];
    char wifi_connected_ssid[APP_HUB_SSID_LEN];
    char wifi_password[APP_HUB_WIFI_PASSWORD_LEN];
    char api_token[APP_HUB_TOKEN_LEN];
    char hub_url[HUB_URL_CAPACITY];
    size_t wifi_ap_count;
    app_hub_wifi_ap_t wifi_aps[APP_HUB_WIFI_AP_LIMIT];
    wifi_ap_record_t wifi_scan_records[APP_HUB_WIFI_AP_LIMIT];
    char status[APP_HUB_STATUS_LEN];
    char cookie[HUB_COOKIE_CAPACITY];
    esp_websocket_client_handle_t websocket;
    char websocket_url[HUB_URL_CAPACITY];
    char websocket_headers[256];
    app_hub_commission_snapshot_t commission;
    bool commission_active;
    app_hub_add_snapshot_t add;
    app_hub_add_request_t active_add;
    char *add_adapters_backup;
    size_t add_baseline_points;
    TickType_t add_deadline;
    volatile bool add_cancel_requested;
    app_hub_scene_t scenes[HUB_MAX_SCENES];
    size_t scene_count;
    uint32_t scenes_generation;
    bool scenes_ready;
    bool scene_pending;
    char scenes_status[APP_HUB_STATUS_LEN];
    char *scenes_json;
    app_hub_rule_t rules[HUB_MAX_RULES];
    size_t rule_count;
    uint32_t rules_generation;
    bool rules_ready;
    bool rule_pending;
    char rules_status[APP_HUB_STATUS_LEN];
    char *rules_json;
    app_hub_system_snapshot_t system;
    char local_ip[APP_HUB_IP_LEN];
    esp_netif_t *sta_netif;
};

static bool url_is_local(const char *url)
{
    return url != NULL && (strstr(url, "127.0.0.1") != NULL ||
                           strstr(url, "localhost") != NULL);
}

static bool hub_is_local(const struct app_hub *hub)
{
    return hub != NULL && (hub->role == APP_HUB_ROLE_HUB ||
                           url_is_local(hub->hub_url));
}

static void normalize_hub_url(const char *in, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (in == NULL) {
        return;
    }
    while (*in == ' ') {
        ++in;
    }
    if (in[0] == '\0') {
        return;
    }
    if (strncmp(in, "http://", 7) == 0 || strncmp(in, "https://", 8) == 0) {
        strlcpy(out, in, out_len);
    } else {
        snprintf(out, out_len, "http://%s", in);
    }
    size_t length = strlen(out);
    while (length > 0 && out[length - 1] == '/') {
        out[--length] = '\0';
    }
}

static bool copy_hub_url(const struct app_hub *hub, char *out, size_t out_len)
{
    if (!hub || !out || out_len == 0) {
        return false;
    }
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }
    strlcpy(out, hub->hub_url, out_len);
    xSemaphoreGive(hub->mutex);
    return out[0] != '\0';
}

static esp_err_t save_hub_identity(const struct app_hub *hub)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(HUB_NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG,
                        "nvs open");
    esp_err_t err = nvs_set_u8(nvs, HUB_NVS_ROLE, (uint8_t)hub->role);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, HUB_NVS_URL, hub->hub_url);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

static void load_hub_identity(struct app_hub *hub)
{
    nvs_handle_t nvs;
    if (nvs_open(HUB_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    uint8_t role = APP_HUB_ROLE_UNSET;
    if (nvs_get_u8(nvs, HUB_NVS_ROLE, &role) == ESP_OK &&
            (role == APP_HUB_ROLE_HUB || role == APP_HUB_ROLE_WEB)) {
        hub->role = (app_hub_role_t)role;
    }
    size_t url_len = sizeof(hub->hub_url);
    (void)nvs_get_str(nvs, HUB_NVS_URL, hub->hub_url, &url_len);
    nvs_close(nvs);
    if (hub->role == APP_HUB_ROLE_HUB && hub->hub_url[0] == '\0') {
        strlcpy(hub->hub_url, HUB_LOCAL_URL, sizeof(hub->hub_url));
    }
}

static esp_err_t save_api_token(const char *token);

static void set_status(struct app_hub *hub, bool connected, const char *status)
{
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return;
    }
    hub->hub_connected = connected;
    strlcpy(hub->status, status, sizeof(hub->status));
    ++hub->generation;
    xSemaphoreGive(hub->mutex);
}

static void set_add_status(struct app_hub *hub, app_hub_add_state_t state,
                           uint8_t progress, const char *status,
                           const char *point_id)
{
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return;
    }
    hub->add.state = state;
    hub->add.progress = progress;
    if (status) {
        strlcpy(hub->add.status, status, sizeof(hub->add.status));
    }
    if (point_id) {
        strlcpy(hub->add.point_id, point_id, sizeof(hub->add.point_id));
    }
    ++hub->add.generation;
    xSemaphoreGive(hub->mutex);
}

static esp_err_t http_event(esp_http_client_event_t *event)
{
    http_capture_t *capture = event->user_data;
    if (!capture) {
        return ESP_OK;
    }
    if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
        const size_t room = capture->capacity - capture->length - 1;
        const size_t copy = (size_t)event->data_len < room ? (size_t)event->data_len : room;
        if (copy > 0) {
            memcpy(capture->body + capture->length, event->data, copy);
            capture->length += copy;
            capture->body[capture->length] = '\0';
        }
    } else if (event->event_id == HTTP_EVENT_ON_HEADER && capture->cookie &&
               event->header_key && event->header_value &&
               strcasecmp(event->header_key, "Set-Cookie") == 0) {
        const char *end = strchr(event->header_value, ';');
        size_t length = end ? (size_t)(end - event->header_value) :
                        strlen(event->header_value);
        if (length >= capture->cookie_capacity) {
            length = capture->cookie_capacity - 1;
        }
        memcpy(capture->cookie, event->header_value, length);
        capture->cookie[length] = '\0';
    }
    return ESP_OK;
}

static void response_release(http_response_t *response)
{
    if (response) {
        free(response->body);
        response->body = NULL;
        response->status = 0;
    }
}

static esp_err_t request_with_timeout(const struct app_hub *hub,
                                      esp_http_client_method_t method,
                                      const char *endpoint,
                                      const char *json_body,
                                      int timeout_ms,
                                      http_response_t *out_response)
{
    if (!hub || !endpoint || !out_response) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_response = (http_response_t) {
        0
    };

    char base[HUB_URL_CAPACITY];
    if (!copy_hub_url(hub, base, sizeof(base))) {
        return ESP_ERR_INVALID_STATE;
    }
    char url[HUB_URL_CAPACITY];
    int written = snprintf(url, sizeof(url), "%s%s", base, endpoint);
    if (written < 0 || (size_t)written >= sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    /* 64KB belongs in SPIRAM. Falling back to internal RAM starves GSP. */
    char *body = heap_caps_calloc(1, HUB_HTTP_BODY_CAPACITY,
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!body) {
        return ESP_ERR_NO_MEM;
    }
    http_capture_t capture = {
        .body = body,
        .capacity = HUB_HTTP_BODY_CAPACITY,
        .cookie = out_response->cookie,
        .cookie_capacity = sizeof(out_response->cookie),
    };
    const esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event,
        .user_data = &capture,
        .timeout_ms = timeout_ms,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(body);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_set_method(client, method);
    char token[APP_HUB_TOKEN_LEN] = {0};
    if (CONFIG_MINIHUB_PANEL_API_TOKEN[0] != '\0') {
        strlcpy(token, CONFIG_MINIHUB_PANEL_API_TOKEN, sizeof(token));
    } else {
        strlcpy(token, hub->api_token, sizeof(token));
    }
    if (err == ESP_OK && token[0] != '\0') {
        char authorization[192];
        written = snprintf(authorization, sizeof(authorization), "Bearer %s",
                           token);
        if (written < 0 || (size_t)written >= sizeof(authorization)) {
            err = ESP_ERR_INVALID_SIZE;
        } else {
            err = esp_http_client_set_header(client, "Authorization", authorization);
        }
    } else if (err == ESP_OK && hub->cookie[0] != '\0') {
        err = esp_http_client_set_header(client, "Cookie", hub->cookie);
    }
    if (err == ESP_OK && json_body) {
        err = esp_http_client_set_header(client, "Content-Type", "application/json");
    }
    if (err == ESP_OK && json_body) {
        err = esp_http_client_set_post_field(client, json_body, strlen(json_body));
    }
    if (err == ESP_OK) {
        err = esp_http_client_perform(client);
    }
    const int status = esp_http_client_get_status_code(client);
    if (status > 0) {
        out_response->status = status;
        out_response->body = body;
        body = NULL;
        if (status == 401) {
            err = ESP_OK;
        }
    }
    esp_http_client_cleanup(client);
    free(body);
    return err;
}

static esp_err_t request(const struct app_hub *hub,
                         esp_http_client_method_t method,
                         const char *endpoint, const char *json_body,
                         http_response_t *out_response)
{
    return request_with_timeout(hub, method, endpoint, json_body,
                                HUB_HTTP_TIMEOUT_MS, out_response);
}

static bool has_api_token(const struct app_hub *hub)
{
    return CONFIG_MINIHUB_PANEL_API_TOKEN[0] != '\0' || hub->api_token[0] != '\0';
}

static esp_err_t login(struct app_hub *hub)
{
    if (has_api_token(hub) || hub->cookie[0] != '\0') {
        return ESP_OK;
    }
    if (CONFIG_MINIHUB_PANEL_HUB_PASSWORD[0] == '\0') {
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "username", CONFIG_MINIHUB_PANEL_HUB_USERNAME);
    cJSON_AddStringToObject(root, "password", CONFIG_MINIHUB_PANEL_HUB_PASSWORD);
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }

    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_POST, "/api/auth/login", payload,
                            &response);
    free(payload);
    if (err == ESP_OK && response.cookie[0] != '\0') {
        strlcpy(hub->cookie, response.cookie, sizeof(hub->cookie));
    }
    if (err == ESP_OK && response.status != 200) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    response_release(&response);
    return err;
}

static const cJSON *point_power_value(const cJSON *point)
{
    const cJSON *props = cJSON_GetObjectItemCaseSensitive(point, "props");
    const cJSON *params = cJSON_IsObject(props) ?
                          cJSON_GetObjectItemCaseSensitive(props, "params") : NULL;
    const cJSON *power = cJSON_IsObject(params) ?
                         cJSON_GetObjectItemCaseSensitive(params, "power") : NULL;
    if (cJSON_IsObject(power)) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(power, "value");
        return value ? value : power;
    }
    return power;
}

static bool point_param_int(const cJSON *point, const char *name,
                            int32_t *out_value)
{
    const cJSON *props = cJSON_GetObjectItemCaseSensitive(point, "props");
    const cJSON *params = cJSON_IsObject(props) ?
                          cJSON_GetObjectItemCaseSensitive(props, "params") : NULL;
    const cJSON *param = cJSON_IsObject(params) ?
                         cJSON_GetObjectItemCaseSensitive(params, name) : NULL;
    const cJSON *value = cJSON_IsObject(param) ?
                         cJSON_GetObjectItemCaseSensitive(param, "value") : param;
    if (cJSON_IsNumber(value)) {
        *out_value = (int32_t)value->valuedouble;
        return true;
    }
    if (cJSON_IsString(value) && value->valuestring) {
        char *end = NULL;
        const long parsed = strtol(value->valuestring, &end, 10);
        if (end != value->valuestring && *end == '\0') {
            *out_value = (int32_t)parsed;
            return true;
        }
    }
    return false;
}

static bool json_is_on(const cJSON *item)
{
    if (cJSON_IsTrue(item)) {
        return true;
    }
    if (cJSON_IsNumber(item)) {
        return item->valuedouble != 0;
    }
    if (cJSON_IsString(item) && item->valuestring) {
        return strcasecmp(item->valuestring, "on") == 0 ||
               strcasecmp(item->valuestring, "true") == 0 ||
               strcmp(item->valuestring, "1") == 0;
    }
    return false;
}

static void format_value(const cJSON *item, char *out, size_t out_size)
{
    if (!item) {
        strlcpy(out, "--", out_size);
    } else if (cJSON_IsString(item) && item->valuestring) {
        strlcpy(out, item->valuestring, out_size);
    } else if (cJSON_IsTrue(item)) {
        strlcpy(out, "ON", out_size);
    } else if (cJSON_IsFalse(item)) {
        strlcpy(out, "OFF", out_size);
    } else if (cJSON_IsNumber(item)) {
        snprintf(out, out_size, "%.2f", item->valuedouble);
    } else {
        strlcpy(out, "available", out_size);
    }
}

static bool type_is_controllable(const char *type)
{
    return strcmp(type, "light") == 0 || strcmp(type, "switch") == 0 ||
           strcmp(type, "plug") == 0 || strcmp(type, "outlet") == 0;
}

static esp_err_t parse_points(struct app_hub *hub, const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    app_hub_point_t *parsed = heap_caps_calloc(
        HUB_MAX_POINTS, sizeof(*parsed), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!parsed) {
        parsed = calloc(HUB_MAX_POINTS, sizeof(*parsed));
    }
    if (!parsed) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    size_t count = 0;
    size_t online_count = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, root) {
        if (count >= HUB_MAX_POINTS || !cJSON_IsObject(item)) {
            break;
        }
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "point_id");
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(item, "type");
        if (!cJSON_IsString(id) || !id->valuestring ||
                !cJSON_IsString(type) || !type->valuestring) {
            continue;
        }
        app_hub_point_t *point = &parsed[count];
        strlcpy(point->point_id, id->valuestring, sizeof(point->point_id));
        const cJSON *props = cJSON_GetObjectItemCaseSensitive(item, "props");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        if (!cJSON_IsString(name) && cJSON_IsObject(props)) {
            name = cJSON_GetObjectItemCaseSensitive(props, "name");
        }
        strlcpy(point->name, cJSON_IsString(name) && name->valuestring ?
                name->valuestring : id->valuestring, sizeof(point->name));
        strlcpy(point->type, type->valuestring, sizeof(point->type));
        const cJSON *online = cJSON_GetObjectItemCaseSensitive(item, "online");
        point->online = !cJSON_IsFalse(online);
        const cJSON *power = point_power_value(item);
        const cJSON *value = power ? power :
                             cJSON_GetObjectItemCaseSensitive(item, "value");
        const cJSON *no_control = cJSON_IsObject(props) ?
            cJSON_GetObjectItemCaseSensitive(props, "no_control") : NULL;
        const cJSON *commissioning = cJSON_IsObject(props) ?
            cJSON_GetObjectItemCaseSensitive(props, "commissioning") : NULL;
        const cJSON *cphase = cJSON_IsObject(props) ?
            cJSON_GetObjectItemCaseSensitive(props, "commission_phase") : NULL;
        const cJSON *nid = cJSON_IsObject(props) ?
            cJSON_GetObjectItemCaseSensitive(props, "node_id") : NULL;
        point->on = json_is_on(value);
        point->commissioning = cJSON_IsTrue(commissioning);
        point->commission_failed = cJSON_IsString(cphase) &&
                                   cphase->valuestring &&
                                   strcmp(cphase->valuestring, "failed") == 0;
        if (cJSON_IsString(nid) && nid->valuestring) {
            point->node_id = (uint32_t)strtoul(nid->valuestring, NULL, 10);
        } else if (cJSON_IsNumber(nid)) {
            point->node_id = (uint32_t)nid->valuedouble;
        }
        point->controllable = !cJSON_IsTrue(no_control) &&
                              !point->commissioning &&
                              (type_is_controllable(point->type) || power != NULL);
        if (point->commission_failed) {
            strlcpy(point->value, "FAIL", sizeof(point->value));
        } else if (point->commissioning) {
            strlcpy(point->value, "PAIR", sizeof(point->value));
        } else {
            format_value(value, point->value, sizeof(point->value));
        }
        point->has_brightness = point_param_int(item, "brightness",
                                                &point->brightness);
        point->has_hue = point_param_int(item, "hue", &point->hue);
        point->has_saturation = point_param_int(item, "saturation",
                                                &point->saturation);
        point->has_cct = point_param_int(item, "cct", &point->cct);
        ++count;
    }
    cJSON_Delete(root);

    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        free(parsed);
        return ESP_ERR_TIMEOUT;
    }
    online_count = 0;
    for (size_t i = 0; i < count; ++i) {
        uint8_t strikes = 0;
        bool was_shown_online = false;
        for (size_t j = 0; j < hub->point_count; ++j) {
            if (strcmp(hub->points[j].point_id, parsed[i].point_id) != 0) {
                continue;
            }
            strikes = hub->points[j].offline_strikes;
            was_shown_online = hub->points[j].online;
            break;
        }
        if (parsed[i].online) {
            parsed[i].offline_strikes = 0;
        } else if (was_shown_online || strikes > 0) {
            if (strikes < 255) {
                ++strikes;
            }
            parsed[i].offline_strikes = strikes;
            if (strikes < HUB_OFFLINE_STRIKES) {
                parsed[i].online = true;
            }
        } else {
            parsed[i].offline_strikes = HUB_OFFLINE_STRIKES;
        }
        if (parsed[i].online) {
            ++online_count;
        }
    }
    memcpy(hub->points, parsed, count * sizeof(*parsed));
    if (count < HUB_MAX_POINTS) {
        memset(&hub->points[count], 0,
               (HUB_MAX_POINTS - count) * sizeof(*parsed));
    }
    hub->point_count = count;
    hub->online_count = online_count;
    hub->hub_connected = true;
    hub->auth_failed = false;
    if (hub->commission_active) {
        if (hub->commission.stage == APP_HUB_COMMISSION_STAGE_BINDING) {
            snprintf(hub->status, sizeof(hub->status), "Binding...");
        } else if (hub->commission.stage == APP_HUB_COMMISSION_STAGE_ACTIVATING) {
            snprintf(hub->status, sizeof(hub->status), "Activating...");
        } else {
            snprintf(hub->status, sizeof(hub->status), "Discovering...");
        }
    } else {
        snprintf(hub->status, sizeof(hub->status), "MiniHub online");
    }
    ++hub->generation;
    xSemaphoreGive(hub->mutex);
    free(parsed);
    return ESP_OK;
}

static void websocket_event(void *arg, esp_event_base_t base, int32_t event_id,
                            void *event_data)
{
    (void)base;
    (void)event_data;
    struct app_hub *hub = arg;
    if (event_id == WEBSOCKET_EVENT_DATA && hub->worker) {
        xTaskNotifyGive(hub->worker);
    }
}

static esp_err_t websocket_start(struct app_hub *hub)
{
    if (hub->websocket) {
        return ESP_OK;
    }
    char base_url[HUB_URL_CAPACITY];
    if (!copy_hub_url(hub, base_url, sizeof(base_url))) {
        return ESP_ERR_INVALID_STATE;
    }
    const char *base = base_url;
    const char *suffix = NULL;
    const char *scheme = NULL;
    if (strncmp(base, "http://", 7) == 0) {
        scheme = "ws://";
        suffix = base + 7;
    } else if (strncmp(base, "https://", 8) == 0) {
        scheme = "wss://";
        suffix = base + 8;
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    int written = snprintf(hub->websocket_url, sizeof(hub->websocket_url),
                           "%s%s/ws/events", scheme, suffix);
    if (written < 0 || (size_t)written >= sizeof(hub->websocket_url)) {
        return ESP_ERR_INVALID_SIZE;
    }
    hub->websocket_headers[0] = '\0';
    written = 0;
    const char *token = CONFIG_MINIHUB_PANEL_API_TOKEN[0] != '\0' ?
                        CONFIG_MINIHUB_PANEL_API_TOKEN : hub->api_token;
    if (token[0] != '\0') {
        written = snprintf(hub->websocket_headers,
                           sizeof(hub->websocket_headers),
                           "Authorization: Bearer %s\r\n",
                           token);
    } else if (hub->cookie[0] != '\0') {
        written = snprintf(hub->websocket_headers,
                           sizeof(hub->websocket_headers),
                           "Cookie: %s\r\n", hub->cookie);
    }
    if (written < 0 || (size_t)written >= sizeof(hub->websocket_headers)) {
        return ESP_ERR_INVALID_SIZE;
    }

    const esp_websocket_client_config_t config = {
        .uri = hub->websocket_url,
        .headers = hub->websocket_headers[0] ? hub->websocket_headers : NULL,
        .network_timeout_ms = HUB_HTTP_TIMEOUT_MS,
        .reconnect_timeout_ms = 3000,
        .task_stack = 4096,
        .buffer_size = 1024,
    };
    hub->websocket = esp_websocket_client_init(&config);
    if (!hub->websocket) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = esp_websocket_register_events(
                        hub->websocket, WEBSOCKET_EVENT_ANY, websocket_event, hub);
    if (err == ESP_OK) {
        err = esp_websocket_client_start(hub->websocket);
    }
    if (err != ESP_OK) {
        esp_websocket_client_destroy(hub->websocket);
        hub->websocket = NULL;
    }
    return err;
}

static esp_err_t sync_points(struct app_hub *hub)
{
    esp_err_t err = login(hub);
    if (err != ESP_OK) {
        set_status(hub, false, "MiniHub login failed");
        return err;
    }
    if (!has_api_token(hub) && hub->cookie[0] == '\0' && !hub_is_local(hub)) {
        set_status(hub, false,
                   hub->auth_failed ? "Invalid token" : "Enter MiniHub token");
        return ESP_ERR_INVALID_STATE;
    }

    http_response_t response = {0};
    err = request(hub, HTTP_METHOD_GET, "/api/points", NULL, &response);
    if (err != ESP_OK) {
        set_status(hub, false, "MiniHub unreachable");
        return err;
    }
    if (response.status == 401) {
        if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            hub->cookie[0] = '\0';
            hub->api_token[0] = '\0';
            hub->auth_failed = true;
            xSemaphoreGive(hub->mutex);
        }
        (void)save_api_token("");
        set_status(hub, false, "Invalid token");
        response_release(&response);
        return ESP_ERR_INVALID_STATE;
    }
    if (response.status != 200) {
        set_status(hub, false, "MiniHub returned an error");
        response_release(&response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    err = parse_points(hub, response.body);
    response_release(&response);
    if (err != ESP_OK) {
        set_status(hub, false, "Invalid Point response");
    } else {
        char token[APP_HUB_TOKEN_LEN] = {0};
        if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            strlcpy(token, hub->api_token, sizeof(token));
            xSemaphoreGive(hub->mutex);
        }
        if (token[0] != '\0') {
            (void)save_api_token(token);
        }
        if (!hub_is_local(hub)) {
            const esp_err_t websocket_err = websocket_start(hub);
            if (websocket_err != ESP_OK) {
                ESP_LOGW(TAG, "WebSocket unavailable: %s",
                         esp_err_to_name(websocket_err));
            }
        }
    }
    return err;
}

static void fill_scene_actions(app_hub_scene_t *scene, const cJSON *item)
{
    scene->action_count = 0;
    const cJSON *steps = cJSON_GetObjectItemCaseSensitive(item, "steps");
    if (!cJSON_IsArray(steps) || cJSON_GetArraySize(steps) <= 0) {
        return;
    }
    const cJSON *step0 = cJSON_GetArrayItem(steps, 0);
    const cJSON *actions = cJSON_IsObject(step0) ?
                           cJSON_GetObjectItemCaseSensitive(step0, "actions") :
                           NULL;
    if (!cJSON_IsArray(actions)) {
        return;
    }
    const cJSON *action;
    cJSON_ArrayForEach(action, actions) {
        if (scene->action_count >= APP_HUB_PAGE_SIZE || !cJSON_IsObject(action)) {
            break;
        }
        const cJSON *point_id =
            cJSON_GetObjectItemCaseSensitive(action, "point_id");
        const cJSON *cmd = cJSON_GetObjectItemCaseSensitive(action, "cmd");
        if (!cJSON_IsString(point_id) || !point_id->valuestring) {
            continue;
        }
        strlcpy(scene->action_points[scene->action_count],
                point_id->valuestring,
                sizeof(scene->action_points[scene->action_count]));
        scene->action_on[scene->action_count] =
            cJSON_IsString(cmd) && cmd->valuestring &&
            strcmp(cmd->valuestring, "turn_on") == 0;
        ++scene->action_count;
    }
}

static void fill_scene_actions_from_command(app_hub_scene_t *scene,
                                            const hub_command_t *command)
{
    scene->action_count = 0;
    if (!command) {
        return;
    }
    const uint8_t n = command->scene_count < APP_HUB_PAGE_SIZE ?
                      command->scene_count : APP_HUB_PAGE_SIZE;
    for (uint8_t i = 0; i < n; ++i) {
        strlcpy(scene->action_points[i], command->scene_points[i],
                sizeof(scene->action_points[i]));
        scene->action_on[i] = command->scene_values[i];
    }
    scene->action_count = n;
}

static const cJSON *document_array(const cJSON *root, const char *key)
{
    if (cJSON_IsArray(root)) {
        return root;
    }
    return cJSON_IsObject(root) ?
           cJSON_GetObjectItemCaseSensitive(root, key) : NULL;
}

static esp_err_t sync_scenes(struct app_hub *hub)
{
    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_GET, "/api/scenes", NULL,
                            &response);
    if (err != ESP_OK || response.status != 200 || !response.body) {
        response_release(&response);
        return err == ESP_OK ? ESP_ERR_INVALID_RESPONSE : err;
    }
    cJSON *root = cJSON_Parse(response.body);
    char *document = response.body ? strdup(response.body) : NULL;
    response_release(&response);
    const cJSON *array = document_array(root, "scenes");
    if (!cJSON_IsArray(array)) {
        cJSON_Delete(root);
        free(document);
        return ESP_ERR_INVALID_RESPONSE;
    }
    app_hub_scene_t *parsed = calloc(HUB_MAX_SCENES, sizeof(*parsed));
    if (!parsed) {
        cJSON_Delete(root);
        free(document);
        return ESP_ERR_NO_MEM;
    }
    size_t count = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (count >= HUB_MAX_SCENES || !cJSON_IsObject(item)) {
            break;
        }
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        const cJSON *steps = cJSON_GetObjectItemCaseSensitive(item, "steps");
        if (!cJSON_IsString(id) || !id->valuestring) {
            continue;
        }
        strlcpy(parsed[count].id, id->valuestring, sizeof(parsed[count].id));
        strlcpy(parsed[count].name,
                cJSON_IsString(name) && name->valuestring ?
                name->valuestring : id->valuestring,
                sizeof(parsed[count].name));
        parsed[count].step_count = cJSON_IsArray(steps) ?
                                   (uint8_t)cJSON_GetArraySize(steps) : 0;
        fill_scene_actions(&parsed[count], item);
        ++count;
    }
    cJSON_Delete(root);
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        free(document);
        free(parsed);
        return ESP_ERR_TIMEOUT;
    }
    const bool keep_status =
        hub->scenes_status[0] != '\0' &&
        strcmp(hub->scenes_status, "Tap a scene to run") != 0 &&
        strcmp(hub->scenes_status, "No scenes") != 0 &&
        strcmp(hub->scenes_status, "Loading scenes...") != 0 &&
        strcmp(hub->scenes_status, "Creating scene...") != 0 &&
        strcmp(hub->scenes_status, "Deleting scene...") != 0 &&
        strcmp(hub->scenes_status, "Starting scene...") != 0;
    free(hub->scenes_json);
    hub->scenes_json = document;
    memcpy(hub->scenes, parsed, HUB_MAX_SCENES * sizeof(*parsed));
    hub->scene_count = count;
    hub->scenes_ready = true;
    if (!keep_status) {
        strlcpy(hub->scenes_status, count ? "Tap a scene to run" : "No scenes",
                sizeof(hub->scenes_status));
    }
    ++hub->scenes_generation;
    xSemaphoreGive(hub->mutex);
    free(parsed);
    return ESP_OK;
}

static esp_err_t sync_rules(struct app_hub *hub)
{
    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_GET, "/api/rules", NULL,
                            &response);
    if (err != ESP_OK || response.status != 200 || !response.body) {
        response_release(&response);
        return err == ESP_OK ? ESP_ERR_INVALID_RESPONSE : err;
    }
    cJSON *root = cJSON_Parse(response.body);
    const cJSON *array = document_array(root, "rules");
    if (!cJSON_IsArray(array)) {
        cJSON_Delete(root);
        response_release(&response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    app_hub_rule_t *parsed = calloc(HUB_MAX_RULES, sizeof(*parsed));
    if (!parsed) {
        cJSON_Delete(root);
        response_release(&response);
        return ESP_ERR_NO_MEM;
    }
    size_t count = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (count >= HUB_MAX_RULES || !cJSON_IsObject(item)) {
            break;
        }
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled");
        if (!cJSON_IsString(id) || !id->valuestring) {
            continue;
        }
        strlcpy(parsed[count].id, id->valuestring, sizeof(parsed[count].id));
        strlcpy(parsed[count].name,
                cJSON_IsString(name) && name->valuestring ?
                name->valuestring : id->valuestring,
                sizeof(parsed[count].name));
        parsed[count].enabled = !cJSON_IsFalse(enabled);
        ++count;
    }
    char *document = strdup(response.body);
    response_release(&response);
    cJSON_Delete(root);
    if (!document) {
        free(parsed);
        return ESP_ERR_NO_MEM;
    }
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        free(document);
        free(parsed);
        return ESP_ERR_TIMEOUT;
    }
    free(hub->rules_json);
    hub->rules_json = document;
    memcpy(hub->rules, parsed, HUB_MAX_RULES * sizeof(*parsed));
    hub->rule_count = count;
    hub->rules_ready = true;
    strlcpy(hub->rules_status, count ? "Tap to enable or disable" : "No automations",
            sizeof(hub->rules_status));
    ++hub->rules_generation;
    xSemaphoreGive(hub->mutex);
    free(parsed);
    return ESP_OK;
}

static uint32_t json_u32(const cJSON *object, const char *key)
{
    const cJSON *item = cJSON_IsObject(object) ?
                        cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
    return cJSON_IsNumber(item) && item->valuedouble > 0 ?
           (uint32_t)item->valuedouble : 0;
}

static esp_err_t sync_system(struct app_hub *hub)
{
    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_GET, "/api/system", NULL,
                            &response);
    if (err != ESP_OK || response.status != 200 || !response.body) {
        response_release(&response);
        return err == ESP_OK ? ESP_ERR_INVALID_RESPONSE : err;
    }
    cJSON *root = cJSON_Parse(response.body);
    response_release(&response);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    app_hub_system_snapshot_t *parsed = calloc(1, sizeof(*parsed));
    if (!parsed) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    *parsed = (app_hub_system_snapshot_t) {
        .ready = true,
        .hub_online = true,
        .uptime_s = json_u32(root, "uptime_s"),
    };
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    const cJSON *ram = cJSON_GetObjectItemCaseSensitive(root, "ram");
    const cJSON *storage = cJSON_GetObjectItemCaseSensitive(root, "storage");
    strlcpy(parsed->version,
            cJSON_IsString(version) && version->valuestring ?
            version->valuestring : "--", sizeof(parsed->version));
    parsed->internal_free_kb = json_u32(ram, "internal_free") / 1024U;
    parsed->psram_free_kb = json_u32(ram, "psram_free") / 1024U;
    parsed->storage_free_kb = json_u32(storage, "free") / 1024U;
    strlcpy(parsed->status, "MiniHub healthy", sizeof(parsed->status));
    cJSON_Delete(root);
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        free(parsed);
        return ESP_ERR_TIMEOUT;
    }
    parsed->generation = hub->system.generation + 1;
    strlcpy(parsed->ip, hub->local_ip, sizeof(parsed->ip));
    hub->system = *parsed;
    xSemaphoreGive(hub->mutex);
    free(parsed);
    return ESP_OK;
}

static bool remove_array_item_by_string(cJSON *array, const char *key,
                                        const char *value);

static void set_scene_result(struct app_hub *hub, const char *status)
{
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        hub->scene_pending = false;
        strlcpy(hub->scenes_status, status, sizeof(hub->scenes_status));
        ++hub->scenes_generation;
        xSemaphoreGive(hub->mutex);
    }
}

static bool process_scene_invoke(struct app_hub *hub,
                                 const hub_command_t *command)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        set_scene_result(hub, "Could not run scene");
        return false;
    }
    cJSON_AddStringToObject(root, "scene_id", command->point_id);
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    http_response_t response = {0};
    esp_err_t err = payload ?
                    request(hub, HTTP_METHOD_POST, "/api/scenes/invoke",
                            payload, &response) : ESP_ERR_NO_MEM;
    free(payload);
    const bool ok = err == ESP_OK && response.status >= 200 &&
                    response.status < 300;
    response_release(&response);
    set_scene_result(hub, ok ? "Scene started" : "Scene failed");
    if (ok) {
        (void)sync_points(hub);
    }
    return ok;
}

static app_hub_scene_t *upsert_local_scene(struct app_hub *hub, const char *id,
                                           const char *name, uint8_t step_count)
{
    app_hub_scene_t *slot = NULL;
    for (size_t i = 0; i < hub->scene_count; ++i) {
        if (strcmp(hub->scenes[i].id, id) == 0) {
            slot = &hub->scenes[i];
            break;
        }
    }
    if (!slot) {
        if (hub->scene_count >= HUB_MAX_SCENES) {
            return NULL;
        }
        slot = &hub->scenes[hub->scene_count++];
        memset(slot, 0, sizeof(*slot));
        strlcpy(slot->id, id, sizeof(slot->id));
    }
    strlcpy(slot->name, name, sizeof(slot->name));
    slot->step_count = step_count;
    return slot;
}

static void drop_local_scene(struct app_hub *hub, const char *id)
{
    for (size_t i = 0; i < hub->scene_count; ++i) {
        if (strcmp(hub->scenes[i].id, id) != 0) {
            continue;
        }
        if (i + 1U < hub->scene_count) {
            memmove(&hub->scenes[i], &hub->scenes[i + 1],
                    (hub->scene_count - i - 1U) * sizeof(hub->scenes[0]));
        }
        --hub->scene_count;
        memset(&hub->scenes[hub->scene_count], 0, sizeof(hub->scenes[0]));
        return;
    }
}

static bool scene_id_present(const struct app_hub *hub, const char *id)
{
    for (size_t i = 0; i < hub->scene_count; ++i) {
        if (strcmp(hub->scenes[i].id, id) == 0) {
            return true;
        }
    }
    return false;
}

static void fail_scene_create(struct app_hub *hub, const char *id)
{
    if (id && id[0] &&
            xSemaphoreTake(hub->mutex,
                           pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        drop_local_scene(hub, id);
        ++hub->scenes_generation;
        xSemaphoreGive(hub->mutex);
    }
    set_scene_result(hub, "Scene create failed");
}

static void process_scene_create(struct app_hub *hub,
                                 const hub_command_t *command)
{
    char *document = NULL;
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        document = hub->scenes_json ? strdup(hub->scenes_json) : NULL;
        xSemaphoreGive(hub->mutex);
    }
    cJSON *root = document ? cJSON_Parse(document) : cJSON_CreateObject();
    free(document);
    if (!root) {
        fail_scene_create(hub, command->point_id);
        return;
    }
    cJSON *array = cJSON_GetObjectItemCaseSensitive(root, "scenes");
    if (!cJSON_IsArray(array)) {
        array = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "scenes", array);
    }
    cJSON *scene = cJSON_CreateObject();
    cJSON *steps = cJSON_CreateArray();
    cJSON *step = cJSON_CreateObject();
    cJSON *actions = cJSON_CreateArray();
    if (!array || !scene || !steps || !step || !actions) {
        cJSON_Delete(scene);
        cJSON_Delete(steps);
        cJSON_Delete(step);
        cJSON_Delete(actions);
        cJSON_Delete(root);
        fail_scene_create(hub, command->point_id);
        return;
    }
    char id[APP_HUB_SCENE_ID_LEN];
    strlcpy(id, command->point_id[0] ? command->point_id : "panel_scene",
            sizeof(id));
    cJSON_AddStringToObject(scene, "id", id);
    cJSON_AddStringToObject(scene, "name", command->name);
    for (size_t i = 0; i < command->scene_count; ++i) {
        cJSON *action = cJSON_CreateObject();
        cJSON_AddStringToObject(action, "point_id", command->scene_points[i]);
        cJSON_AddStringToObject(action, "cmd",
                                command->scene_values[i] ?
                                "turn_on" : "turn_off");
        cJSON_AddItemToArray(actions, action);
    }
    cJSON_AddItemToObject(step, "actions", actions);
    cJSON_AddNumberToObject(step, "loop", 1);
    cJSON_AddNumberToObject(step, "delay_after_ms", 0);
    cJSON_AddItemToArray(steps, step);
    cJSON_AddItemToObject(scene, "steps", steps);
    cJSON_AddNumberToObject(scene, "max_duration_ms", 0);
    cJSON_AddItemToArray(array, scene);
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    http_response_t response = {0};
    esp_err_t err = payload ?
                    request(hub, HTTP_METHOD_POST, "/api/scenes/import",
                            payload, &response) : ESP_ERR_NO_MEM;
    free(payload);
    const bool ok = err == ESP_OK && response.status >= 200 &&
                    response.status < 300;
    ESP_LOGI(TAG, "scene import id=%s name=%s http=%d err=%s",
             id, command->name, response.status, esp_err_to_name(err));
    response_release(&response);
    if (!ok) {
        fail_scene_create(hub, id);
        return;
    }
    (void)sync_scenes(hub);
    bool missing = true;
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) ==
            pdTRUE) {
        missing = !scene_id_present(hub, id);
        if (missing) {
            app_hub_scene_t *slot = upsert_local_scene(
                hub, id, command->name,
                command->scene_count ? 1U : 0U);
            if (slot) {
                fill_scene_actions_from_command(slot, command);
            }
            ++hub->scenes_generation;
        }
        xSemaphoreGive(hub->mutex);
    }
    if (missing) {
        ESP_LOGW(TAG, "scene %s missing after import GET, keeping local copy",
                 id);
    }
    set_scene_result(hub, "Scene created");
}

static void process_scene_delete(struct app_hub *hub,
                                 const hub_command_t *command)
{
    char *document = NULL;
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        document = hub->scenes_json ? strdup(hub->scenes_json) : NULL;
        xSemaphoreGive(hub->mutex);
    }
    cJSON *root = document ? cJSON_Parse(document) : NULL;
    free(document);
    cJSON *array = root ? cJSON_GetObjectItemCaseSensitive(root, "scenes") : NULL;
    const bool removed = cJSON_IsArray(array) &&
                         remove_array_item_by_string(array, "id",
                                                     command->point_id);
    char *payload = (root && removed) ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    http_response_t response = {0};
    esp_err_t err = payload ?
                    request(hub, HTTP_METHOD_POST, "/api/scenes/import",
                            payload, &response) : ESP_ERR_INVALID_RESPONSE;
    free(payload);
    const bool ok = err == ESP_OK && response.status >= 200 &&
                    response.status < 300;
    response_release(&response);
    (void)sync_scenes(hub);
    set_scene_result(hub, ok ? "Scene deleted" : "Scene delete failed");
}

static void set_rule_result(struct app_hub *hub, const char *status)
{
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        hub->rule_pending = false;
        strlcpy(hub->rules_status, status, sizeof(hub->rules_status));
        ++hub->rules_generation;
        xSemaphoreGive(hub->mutex);
    }
}

static void process_rule_enabled(struct app_hub *hub,
                                 const hub_command_t *command)
{
    char *document = NULL;
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        document = hub->rules_json ? strdup(hub->rules_json) : NULL;
        xSemaphoreGive(hub->mutex);
    }
    cJSON *root = document ? cJSON_Parse(document) : NULL;
    free(document);
    const cJSON *array = document_array(root, "rules");
    bool found = false;
    cJSON *item;
    cJSON_ArrayForEach(item, array) {
        cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (cJSON_IsString(id) && id->valuestring &&
                strcmp(id->valuestring, command->point_id) == 0) {
            cJSON_ReplaceItemInObjectCaseSensitive(
                item, "enabled", cJSON_CreateBool(command->enabled));
            found = true;
            break;
        }
    }
    char *payload = found ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    http_response_t response = {0};
    esp_err_t err = payload ?
                    request(hub, HTTP_METHOD_POST, "/api/rules/import",
                            payload, &response) : ESP_ERR_INVALID_STATE;
    free(payload);
    const bool ok = err == ESP_OK && response.status >= 200 &&
                    response.status < 300;
    response_release(&response);
    if (ok && sync_rules(hub) == ESP_OK) {
        set_rule_result(hub, command->enabled ? "Automation enabled" :
                        "Automation disabled");
    } else {
        set_rule_result(hub, "Automation update failed");
        (void)sync_rules(hub);
    }
}

static void process_rule_delete(struct app_hub *hub,
                                const hub_command_t *command)
{
    char *document = NULL;
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        document = hub->rules_json ? strdup(hub->rules_json) : NULL;
        xSemaphoreGive(hub->mutex);
    }
    cJSON *root = document ? cJSON_Parse(document) : NULL;
    free(document);
    cJSON *array = root ? cJSON_GetObjectItemCaseSensitive(root, "rules") : NULL;
    const bool removed = cJSON_IsArray(array) &&
                         remove_array_item_by_string(array, "id",
                                                     command->point_id);
    char *payload = (root && removed) ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    http_response_t response = {0};
    esp_err_t err = payload ?
                    request(hub, HTTP_METHOD_POST, "/api/rules/import",
                            payload, &response) : ESP_ERR_INVALID_RESPONSE;
    free(payload);
    const bool ok = err == ESP_OK && response.status >= 200 &&
                    response.status < 300;
    response_release(&response);
    (void)sync_rules(hub);
    set_rule_result(hub, ok ? "Automation deleted" : "Automation delete failed");
}

static void process_rule_create(struct app_hub *hub,
                                const hub_command_t *command)
{
    char *document = NULL;
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        document = hub->rules_json ? strdup(hub->rules_json) : NULL;
        xSemaphoreGive(hub->mutex);
    }
    cJSON *root = document ? cJSON_Parse(document) : cJSON_CreateObject();
    free(document);
    if (!root) {
        set_rule_result(hub, "Automation create failed");
        return;
    }
    cJSON *array = cJSON_GetObjectItemCaseSensitive(root, "rules");
    if (!cJSON_IsArray(array)) {
        array = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "rules", array);
    }
    cJSON *rule = cJSON_CreateObject();
    cJSON *when = cJSON_CreateObject();
    cJSON *then = cJSON_CreateArray();
    cJSON *action = cJSON_CreateObject();
    if (!array || !rule || !when || !then || !action) {
        cJSON_Delete(rule);
        cJSON_Delete(when);
        cJSON_Delete(then);
        cJSON_Delete(action);
        cJSON_Delete(root);
        set_rule_result(hub, "Automation create failed");
        return;
    }
    char id[APP_HUB_RULE_ID_LEN];
    snprintf(id, sizeof(id), "panel_%08lx",
             (unsigned long)xTaskGetTickCount());
    cJSON_AddStringToObject(rule, "id", id);
    cJSON_AddStringToObject(rule, "name", command->name);
    cJSON_AddBoolToObject(rule, "enabled", true);
    cJSON_AddStringToObject(when, "type", "point");
    cJSON_AddStringToObject(when, "point_id", command->point_id);
    cJSON_AddStringToObject(when, "op", "eq");
    cJSON_AddStringToObject(when, "value", command->source_value);
    cJSON_AddItemToObject(rule, "when", when);
    cJSON_AddStringToObject(action, "type", "command");
    cJSON_AddStringToObject(action, "point_id", command->target_point);
    cJSON_AddStringToObject(action, "cmd",
                            command->enabled ? "turn_on" : "turn_off");
    cJSON_AddItemToArray(then, action);
    cJSON_AddItemToObject(rule, "then", then);
    cJSON_AddItemToArray(array, rule);
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    http_response_t response = {0};
    esp_err_t err = payload ?
                    request(hub, HTTP_METHOD_POST, "/api/rules/import",
                            payload, &response) : ESP_ERR_NO_MEM;
    free(payload);
    const bool ok = err == ESP_OK && response.status >= 200 &&
                    response.status < 300;
    response_release(&response);
    if (ok && sync_rules(hub) == ESP_OK) {
        set_rule_result(hub, "Automation created");
    } else {
        set_rule_result(hub, "Automation create failed");
        (void)sync_rules(hub);
    }
}

static void process_system_reboot(struct app_hub *hub)
{
    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_POST, "/api/system/reboot",
                            "{\"confirm\":true}", &response);
    if (err == ESP_OK && response.status >= 200 && response.status < 300) {
        set_status(hub, false, "MiniHub restarting");
    }
    response_release(&response);
}

static void process_point_reconnect(struct app_hub *hub, const char *point_id)
{
    if (!point_id || point_id[0] == '\0') {
        ESP_LOGW(TAG, "reconnect missing point_id");
        return;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return;
    }
    cJSON_AddStringToObject(root, "point_id", point_id);
    cJSON_AddStringToObject(root, "cmd", "reconnect");
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        return;
    }
    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_POST, "/api/command", payload,
                            &response);
    const bool ok = err == ESP_OK && response.status >= 200 &&
                    response.status < 300;
    ESP_LOGI(TAG, "reconnect %s http=%d err=%s", point_id, response.status,
             esp_err_to_name(err));
    free(payload);
    response_release(&response);
    (void)sync_points(hub);
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) ==
            pdTRUE) {
        strlcpy(hub->status, ok ? "Reconnecting" : "Reconnect failed",
                sizeof(hub->status));
        ++hub->generation;
        xSemaphoreGive(hub->mutex);
    }
}

static cJSON *adapter_slot(cJSON *root, const char *name)
{
    cJSON *adapters = cJSON_GetObjectItemCaseSensitive(root, "adapters");
    if (!cJSON_IsObject(adapters)) {
        adapters = cJSON_CreateObject();
        if (!adapters) {
            return NULL;
        }
        cJSON_AddItemToObject(root, "adapters", adapters);
    }
    cJSON *slot = cJSON_GetObjectItemCaseSensitive(adapters, name);
    if (!cJSON_IsObject(slot)) {
        slot = cJSON_CreateObject();
        if (!slot) {
            return NULL;
        }
        cJSON_ReplaceItemInObjectCaseSensitive(adapters, name, slot);
        if (cJSON_GetObjectItemCaseSensitive(adapters, name) != slot) {
            cJSON_AddItemToObject(adapters, name, slot);
        }
    }
    return slot;
}

static void json_set_bool(cJSON *object, const char *key, bool value)
{
    cJSON *item = cJSON_CreateBool(value);
    if (!cJSON_ReplaceItemInObjectCaseSensitive(object, key, item)) {
        cJSON_AddItemToObject(object, key, item);
    }
}

static void json_set_number(cJSON *object, const char *key, double value)
{
    cJSON *item = cJSON_CreateNumber(value);
    if (!cJSON_ReplaceItemInObjectCaseSensitive(object, key, item)) {
        cJSON_AddItemToObject(object, key, item);
    }
}

static void json_set_string(cJSON *object, const char *key, const char *value)
{
    cJSON *item = cJSON_CreateString(value ? value : "");
    if (!cJSON_ReplaceItemInObjectCaseSensitive(object, key, item)) {
        cJSON_AddItemToObject(object, key, item);
    }
}

static cJSON *load_adapters_document(struct app_hub *hub)
{
    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_GET, "/api/config/adapters",
                            NULL, &response);
    cJSON *root = NULL;
    if (err == ESP_OK && response.status == 200) {
        root = cJSON_Parse(response.body);
    }
    response_release(&response);
    return root;
}

static esp_err_t save_adapters_document(struct app_hub *hub, cJSON *root)
{
    json_set_bool(root, "reload", true);
    char *payload = cJSON_PrintUnformatted(root);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }
    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_PUT, "/api/config/adapters",
                            payload, &response);
    free(payload);
    if (err == ESP_OK && (response.status < 200 || response.status >= 300)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    response_release(&response);
    return err;
}

static cJSON *ensure_array(cJSON *object, const char *key)
{
    cJSON *array = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsArray(array)) {
        return array;
    }
    array = cJSON_CreateArray();
    if (!array) {
        return NULL;
    }
    if (!cJSON_ReplaceItemInObjectCaseSensitive(object, key, array)) {
        cJSON_AddItemToObject(object, key, array);
    }
    return array;
}

static void replace_array_item_by_string(cJSON *array, const char *key,
                                         const char *value, cJSON *replacement)
{
    int index = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        const cJSON *candidate = cJSON_GetObjectItemCaseSensitive(item, key);
        if (cJSON_IsString(candidate) && candidate->valuestring &&
                strcmp(candidate->valuestring, value) == 0) {
            cJSON_ReplaceItemInArray(array, index, replacement);
            return;
        }
        ++index;
    }
    cJSON_AddItemToArray(array, replacement);
}

static bool remove_array_item_by_string(cJSON *array, const char *key,
                                        const char *value)
{
    int index = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        const cJSON *candidate = cJSON_GetObjectItemCaseSensitive(item, key);
        if (cJSON_IsString(candidate) && candidate->valuestring &&
                strcmp(candidate->valuestring, value) == 0) {
            cJSON_DeleteItemFromArray(array, index);
            return true;
        }
        ++index;
    }
    return false;
}

static esp_err_t configure_add_adapter(struct app_hub *hub,
                                       const app_hub_add_request_t *add)
{
    cJSON *root = load_adapters_document(hub);
    if (!root) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    char *backup = cJSON_PrintUnformatted(root);
    if (!backup) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    free(hub->add_adapters_backup);
    hub->add_adapters_backup = backup;
    cJSON *slot = NULL;
    if (add->protocol == APP_HUB_PROTOCOL_ZIGBEE ||
            add->protocol == APP_HUB_PROTOCOL_BLE_MESH) {
        cJSON *zigbee = adapter_slot(root, "zigbee");
        cJSON *ble_mesh = adapter_slot(root, "ble_mesh");
        cJSON *thread = adapter_slot(root, "thread");
        if (!zigbee || !ble_mesh || !thread) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        const bool use_zigbee = add->protocol == APP_HUB_PROTOCOL_ZIGBEE;
        json_set_bool(zigbee, "enabled", use_zigbee);
        json_set_bool(ble_mesh, "enabled", !use_zigbee);
        json_set_bool(thread, "enabled", false);
    } else if (add->protocol == APP_HUB_PROTOCOL_MQTT) {
        slot = adapter_slot(root, "mqtt");
        cJSON *devices = slot ? ensure_array(slot, "devices") : NULL;
        cJSON *device = cJSON_CreateObject();
        if (!slot || !devices || !device) {
            cJSON_Delete(device);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        json_set_bool(slot, "enabled", true);
        json_set_string(slot, "broker", add->host);
        json_set_number(slot, "port", add->port ? add->port : 1883);
        json_set_string(slot, "username", add->username);
        json_set_string(slot, "password", add->password);
        json_set_string(device, "point_id", add->point_id);
        json_set_string(device, "type", add->type[0] ? add->type : "switch");
        json_set_string(device, "state_topic", add->state_topic);
        json_set_string(device, "command_topic", add->command_topic);
        replace_array_item_by_string(devices, "point_id", add->point_id, device);
    } else if (add->protocol == APP_HUB_PROTOCOL_MODBUS_TCP) {
        slot = adapter_slot(root, "modbus");
        cJSON *gateways = slot ? ensure_array(slot, "gateways") : NULL;
        cJSON *gateway = cJSON_CreateObject();
        cJSON *maps = cJSON_CreateArray();
        cJSON *map = cJSON_CreateObject();
        if (!slot || !gateways || !gateway || !maps || !map) {
            cJSON_Delete(gateway);
            cJSON_Delete(maps);
            cJSON_Delete(map);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        json_set_bool(slot, "enabled", true);
        json_set_number(slot, "poll_ms", 2000);
        json_set_string(gateway, "id", add->point_id);
        json_set_string(gateway, "transport", "tcp");
        json_set_string(gateway, "host", add->host);
        json_set_number(gateway, "port", add->port ? add->port : 502);
        json_set_number(gateway, "timeout_ms", 1000);
        json_set_number(gateway, "unit_id", add->unit_id ? add->unit_id : 1);
        json_set_string(map, "point_id", add->point_id);
        json_set_string(map, "type", add->type[0] ? add->type : "sensor");
        json_set_number(map, "unit_id", add->unit_id ? add->unit_id : 1);
        json_set_string(map, "fc", add->function[0] ? add->function : "holding");
        json_set_number(map, "address", add->address);
        json_set_number(map, "count", 1);
        json_set_string(map, "datatype", add->datatype[0] ? add->datatype : "u16");
        json_set_number(map, "scale", add->scale == 0.0f ? 1.0f : add->scale);
        json_set_bool(map, "writable", add->writable);
        cJSON_AddItemToArray(maps, map);
        cJSON_AddItemToObject(gateway, "maps", maps);
        replace_array_item_by_string(gateways, "id", add->point_id, gateway);
    }
    esp_err_t err = save_adapters_document(hub, root);
    cJSON_Delete(root);
    return err;
}

static esp_err_t post_simple_json(struct app_hub *hub, const char *endpoint,
                                  cJSON *payload_root)
{
    char *payload = cJSON_PrintUnformatted(payload_root);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }
    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_POST, endpoint, payload, &response);
    free(payload);
    if (err == ESP_OK && (response.status < 200 || response.status >= 300)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    response_release(&response);
    return err;
}

static bool radio_provision_ready(struct app_hub *hub,
                                  app_hub_protocol_t protocol)
{
    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_GET,
                            "/api/radio/provision", NULL, &response);
    cJSON *root = err == ESP_OK && response.status == 200 ?
                  cJSON_Parse(response.body) : NULL;
    response_release(&response);
    if (!root) {
        return false;
    }
    const cJSON *busy = cJSON_GetObjectItemCaseSensitive(root, "busy");
    const cJSON *available = cJSON_GetObjectItemCaseSensitive(
        root, protocol == APP_HUB_PROTOCOL_ZIGBEE ?
        "zigbee_available" : "ble_mesh_available");
    const cJSON *mesh_running = cJSON_GetObjectItemCaseSensitive(
        root, "ble_mesh_running");
    const bool ready = !cJSON_IsTrue(busy) &&
                       cJSON_IsTrue(available) &&
                       (protocol != APP_HUB_PROTOCOL_BLE_MESH ||
                        cJSON_IsTrue(mesh_running));
    cJSON_Delete(root);
    return ready;
}

static esp_err_t wait_radio_ready(struct app_hub *hub,
                                  app_hub_protocol_t protocol)
{
    for (int attempt = 0; attempt < 40; ++attempt) {
        if (hub->add_cancel_requested) {
            return ESP_ERR_INVALID_STATE;
        }
        if (radio_provision_ready(hub, protocol)) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    return ESP_ERR_TIMEOUT;
}

static void append_utf8(char *destination, size_t size, size_t *used,
                        const char *source, size_t source_limit)
{
    size_t consumed = 0;
    while (source && source[consumed] && consumed < source_limit) {
        const unsigned char lead = (unsigned char)source[consumed];
        const size_t glyph = lead < 0x80 ? 1 :
                             ((lead & 0xe0) == 0xc0 ? 2 :
                              ((lead & 0xf0) == 0xe0 ? 3 :
                               ((lead & 0xf8) == 0xf0 ? 4 : 1)));
        if (consumed + glyph > source_limit || *used + glyph >= size) {
            break;
        }
        memcpy(destination + *used, source + consumed, glyph);
        *used += glyph;
        consumed += glyph;
    }
    destination[*used] = '\0';
}

static void format_display_name(char *destination, size_t size,
                                const char *room, const char *name)
{
    size_t used = 0;
    destination[0] = '\0';
    if (room && room[0]) {
        append_utf8(destination, size, &used, room, 12);
        append_utf8(destination, size, &used, " · ", 4);
    }
    append_utf8(destination, size, &used, name, SIZE_MAX);
}

static esp_err_t rename_added_point(struct app_hub *hub, const char *point_id)
{
    if (!hub->active_add.name[0] || !point_id || !point_id[0]) {
        return ESP_OK;
    }
    char display_name[49];
    format_display_name(display_name, sizeof(display_name),
                        hub->active_add.room, hub->active_add.name);
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "point_id", point_id);
    cJSON_AddStringToObject(root, "name", display_name);
    const esp_err_t err = post_simple_json(
        hub, "/api/points/rename", root);
    cJSON_Delete(root);
    return err;
}

static const char *find_added_point(struct app_hub *hub)
{
    if ((hub->active_add.protocol == APP_HUB_PROTOCOL_MQTT ||
            hub->active_add.protocol == APP_HUB_PROTOCOL_MODBUS_TCP) &&
            hub->active_add.point_id[0]) {
        for (size_t i = 0; i < hub->point_count; ++i) {
            if (strcmp(hub->points[i].point_id,
                       hub->active_add.point_id) == 0) {
                return hub->points[i].point_id;
            }
        }
        return NULL;
    }
    if (hub->point_count > hub->add_baseline_points) {
        return hub->points[hub->point_count - 1].point_id;
    }
    return NULL;
}

static void process_matter_cancel(struct app_hub *hub,
                                  const hub_command_t *command);
static void process_matter_commission(struct app_hub *hub,
                                      const char *pairing_code);
static void cancel_and_delete_matter_node(struct app_hub *hub,
                                          uint32_t node_id,
                                          const char *point_id);
static void fill_hub_wifi_from_sta(struct app_hub *hub);

static void rollback_add_adapter(struct app_hub *hub)
{
    const app_hub_add_request_t *add = &hub->active_add;
    if (hub->add_adapters_backup) {
        cJSON *original = cJSON_Parse(hub->add_adapters_backup);
        if (original) {
            (void)save_adapters_document(hub, original);
            cJSON_Delete(original);
        }
        free(hub->add_adapters_backup);
        hub->add_adapters_backup = NULL;
        return;
    }
    if ((add->protocol != APP_HUB_PROTOCOL_MQTT &&
            add->protocol != APP_HUB_PROTOCOL_MODBUS_TCP) ||
            !add->point_id[0]) {
        return;
    }
    cJSON *root = load_adapters_document(hub);
    const char *slot_name = add->protocol == APP_HUB_PROTOCOL_MQTT ?
                            "mqtt" : "modbus";
    const char *array_name = add->protocol == APP_HUB_PROTOCOL_MQTT ?
                             "devices" : "gateways";
    cJSON *slot = root ? adapter_slot(root, slot_name) : NULL;
    cJSON *array = slot ? cJSON_GetObjectItemCaseSensitive(
                             slot, array_name) : NULL;
    if (cJSON_IsArray(array) &&
            remove_array_item_by_string(array,
                                        add->protocol == APP_HUB_PROTOCOL_MQTT ?
                                        "point_id" : "id",
                                        add->point_id)) {
        (void)save_adapters_document(hub, root);
    }
    cJSON_Delete(root);
}

static void process_add_cancel(struct app_hub *hub,
                               const hub_command_t *command)
{
    if (hub->add.protocol == APP_HUB_PROTOCOL_MATTER) {
        process_matter_cancel(hub, command);
    } else if (hub->add.protocol == APP_HUB_PROTOCOL_ZIGBEE ||
               hub->add.protocol == APP_HUB_PROTOCOL_BLE_MESH) {
        cJSON *permit = cJSON_CreateObject();
        if (permit) {
            cJSON_AddNumberToObject(permit, "seconds", 0);
            (void)post_simple_json(
                hub, hub->add.protocol == APP_HUB_PROTOCOL_ZIGBEE ?
                "/api/zigbee/permit_join" :
                "/api/ble_mesh/permit_provision", permit);
            cJSON_Delete(permit);
        }
        rollback_add_adapter(hub);
    } else {
        rollback_add_adapter(hub);
    }
    hub->add_cancel_requested = false;
    set_add_status(hub, APP_HUB_ADD_CANCELLED, 0, "Add cancelled", NULL);
}

static void process_add_device(struct app_hub *hub,
                               const app_hub_add_request_t *add)
{
    set_add_status(hub, APP_HUB_ADD_PREPARING, 10, "Preparing adapter", NULL);
    free(hub->add_adapters_backup);
    hub->add_adapters_backup = NULL;
    hub->active_add = *add;
    hub->add_baseline_points = hub->point_count;
    esp_err_t err = ESP_OK;
    if (hub->add_cancel_requested) {
        set_add_status(hub, APP_HUB_ADD_CANCELLED, 0, "Add cancelled", NULL);
        return;
    }
    if ((add->protocol == APP_HUB_PROTOCOL_ZIGBEE ||
            add->protocol == APP_HUB_PROTOCOL_BLE_MESH) &&
            (hub->commission_active ||
             hub->commission.state == APP_HUB_COMMISSION_RUNNING)) {
        set_add_status(hub, APP_HUB_ADD_FAILED, 0,
                       "Matter pairing is in progress", NULL);
        return;
    }
    if (add->protocol == APP_HUB_PROTOCOL_MATTER) {
        if (!app_hub_matter_pairing_code_ok(add->pairing_code)) {
            set_add_status(hub, APP_HUB_ADD_FAILED, 0,
                           "Invalid Matter pairing code", NULL);
            return;
        }
        process_matter_commission(hub, add->pairing_code);
        if (hub->commission.state == APP_HUB_COMMISSION_FAILED) {
            err = ESP_FAIL;
        }
    } else {
        const bool radio = add->protocol == APP_HUB_PROTOCOL_ZIGBEE ||
                           add->protocol == APP_HUB_PROTOCOL_BLE_MESH;
        if (!radio || !radio_provision_ready(hub, add->protocol)) {
            err = configure_add_adapter(hub, add);
        }
        if (err == ESP_OK && radio) {
            err = wait_radio_ready(hub, add->protocol);
            if (add->protocol == APP_HUB_PROTOCOL_ZIGBEE &&
                    err == ESP_OK &&
                    add->ieee[0] && add->install_code[0]) {
                cJSON *install = cJSON_CreateObject();
                cJSON_AddStringToObject(install, "ieee", add->ieee);
                cJSON_AddStringToObject(install, "install_code", add->install_code);
                err = post_simple_json(hub, "/api/zigbee/install_code", install);
                cJSON_Delete(install);
            }
            if (err == ESP_OK && !hub->add_cancel_requested) {
                cJSON *permit = cJSON_CreateObject();
                cJSON_AddNumberToObject(permit, "seconds", 120);
                err = post_simple_json(
                    hub, add->protocol == APP_HUB_PROTOCOL_ZIGBEE ?
                    "/api/zigbee/permit_join" :
                    "/api/ble_mesh/permit_provision", permit);
                cJSON_Delete(permit);
            }
        }
    }
    if (hub->add_cancel_requested) {
        set_add_status(hub, APP_HUB_ADD_CANCELLED, 0, "Add cancelled", NULL);
        return;
    }
    if (err != ESP_OK) {
        rollback_add_adapter(hub);
        set_add_status(hub, APP_HUB_ADD_FAILED, 0,
                       add->protocol == APP_HUB_PROTOCOL_MATTER &&
                       hub->commission.error[0] ?
                       hub->commission.error : "Could not start device add",
                       NULL);
        return;
    }
    hub->add_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(
                           add->protocol == APP_HUB_PROTOCOL_MATTER ?
                           HUB_MATTER_ADD_TIMEOUT_MS : 120000);
    set_add_status(hub, APP_HUB_ADD_WAITING, 35,
                   add->protocol == APP_HUB_PROTOCOL_MQTT ||
                   add->protocol == APP_HUB_PROTOCOL_MODBUS_TCP ?
                   "Waiting for adapter reload" : "Waiting for device", NULL);
}

static void update_add_progress(struct app_hub *hub)
{
    if (hub->add_cancel_requested) {
        return;
    }
    if (hub->add.state != APP_HUB_ADD_WAITING) {
        return;
    }
    if (hub->add.protocol == APP_HUB_PROTOCOL_MATTER) {
        if (hub->commission.state == APP_HUB_COMMISSION_FAILED) {
            set_add_status(hub, APP_HUB_ADD_FAILED, 0,
                           hub->commission.error[0] ? hub->commission.error :
                           "Matter commissioning failed", NULL);
            return;
        }
        if (hub->commission.state != APP_HUB_COMMISSION_OK) {
            if ((int32_t)(xTaskGetTickCount() - hub->add_deadline) >= 0) {
                cancel_and_delete_matter_node(hub, hub->commission.node_id, NULL);
                set_add_status(hub, APP_HUB_ADD_FAILED, 0,
                               hub->commission.error[0] ? hub->commission.error :
                               "Device add timed out", NULL);
            }
            return;
        }
    }
    const char *point_id = find_added_point(hub);
    if (point_id) {
        const esp_err_t rename_err = rename_added_point(hub, point_id);
        free(hub->add_adapters_backup);
        hub->add_adapters_backup = NULL;
        set_add_status(hub, APP_HUB_ADD_OK, 100,
                       rename_err == ESP_OK ? "Device added" :
                       "Added; name could not be saved", point_id);
        (void)sync_points(hub);
    } else if ((int32_t)(xTaskGetTickCount() - hub->add_deadline) >= 0) {
        rollback_add_adapter(hub);
        set_add_status(hub, APP_HUB_ADD_FAILED, 0, "Device add timed out", NULL);
    }
}

static void cancel_and_delete_matter_node(struct app_hub *hub,
                                          uint32_t node_id,
                                          const char *point_id);

static app_hub_commission_stage_t parse_commission_stage(const char *stage)
{
    if (!stage) {
        return APP_HUB_COMMISSION_STAGE_NONE;
    }
    if (strcmp(stage, "discovering") == 0) {
        return APP_HUB_COMMISSION_STAGE_DISCOVERING;
    }
    if (strcmp(stage, "binding") == 0) {
        return APP_HUB_COMMISSION_STAGE_BINDING;
    }
    if (strcmp(stage, "activating") == 0) {
        return APP_HUB_COMMISSION_STAGE_ACTIVATING;
    }
    return APP_HUB_COMMISSION_STAGE_NONE;
}

static void sync_matter_status(struct app_hub *hub)
{
    http_response_t response = {0};
    if (request(hub, HTTP_METHOD_GET, "/api/matter/status", NULL,
                &response) != ESP_OK) {
        return;
    }
    if (response.status != 200 || !response.body) {
        response_release(&response);
        return;
    }
    cJSON *root = cJSON_Parse(response.body);
    response_release(&response);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }

    const cJSON *comm = cJSON_GetObjectItemCaseSensitive(root, "commission");
    const cJSON *stage = cJSON_GetObjectItemCaseSensitive(root, "commission_stage");
    const cJSON *node = cJSON_GetObjectItemCaseSensitive(root, "commission_node_id");
    const cJSON *last_err = cJSON_GetObjectItemCaseSensitive(root, "last_error");
    const cJSON *res_err = cJSON_GetObjectItemCaseSensitive(root, "commission_result");

    app_hub_commission_state_t state = APP_HUB_COMMISSION_IDLE;
    if (cJSON_IsString(comm) && comm->valuestring) {
        if (strcmp(comm->valuestring, "running") == 0) {
            state = APP_HUB_COMMISSION_RUNNING;
        } else if (strcmp(comm->valuestring, "ok") == 0) {
            state = APP_HUB_COMMISSION_OK;
        } else if (strcmp(comm->valuestring, "failed") == 0) {
            state = APP_HUB_COMMISSION_FAILED;
        }
    }
    const app_hub_commission_stage_t parsed_stage =
        cJSON_IsString(stage) ? parse_commission_stage(stage->valuestring) :
        APP_HUB_COMMISSION_STAGE_NONE;
    const uint32_t node_id = cJSON_IsNumber(node) ?
                             (uint32_t)node->valuedouble : 0U;
    const char *err_str = "";
    if (cJSON_IsString(last_err) && last_err->valuestring &&
            last_err->valuestring[0]) {
        err_str = last_err->valuestring;
    } else if (cJSON_IsString(res_err) && res_err->valuestring &&
               res_err->valuestring[0]) {
        err_str = res_err->valuestring;
    }

    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        hub->commission.state = state;
        hub->commission.stage = parsed_stage;
        if (node_id > 0) {
            hub->commission.node_id = node_id;
        }
        strlcpy(hub->commission.error, err_str, sizeof(hub->commission.error));
        if (state != APP_HUB_COMMISSION_RUNNING) {
            hub->commission_active = false;
        }
        ++hub->generation;
        xSemaphoreGive(hub->mutex);
    }
    cJSON_Delete(root);
}

static void process_matter_commission(struct app_hub *hub,
                                      const char *pairing_code)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return;
    }
    cJSON_AddStringToObject(root, "pairing_code", pairing_code);
    cJSON_AddStringToObject(root, "endpoints", "1");
    cJSON_AddBoolToObject(root, "merge", true);
    cJSON_AddBoolToObject(root, "reload", true);
    cJSON_AddStringToObject(root, "discovery", "ble_wifi");
    cJSON_AddStringToObject(root, "transport", "wifi");
    fill_hub_wifi_from_sta(hub);
    if (hub->wifi_ssid[0] != '\0') {
        cJSON_AddStringToObject(root, "wifi_ssid", hub->wifi_ssid);
    }
    if (hub->wifi_password[0] != '\0') {
        cJSON_AddStringToObject(root, "wifi_password", hub->wifi_password);
    }
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        return;
    }

    http_response_t response = {0};
    esp_err_t err = request_with_timeout(
        hub, HTTP_METHOD_POST, "/api/matter/devices", payload,
        HUB_MATTER_START_TIMEOUT_MS, &response);
    free(payload);

    bool started = false;
    uint32_t node_id = 0;
    char err_msg[APP_HUB_STATUS_LEN] = "Commission request failed";
    if (err == ESP_OK && response.body) {
        cJSON *resp = cJSON_Parse(response.body);
        if (cJSON_IsObject(resp)) {
            const cJSON *res = cJSON_GetObjectItemCaseSensitive(resp, "result");
            const cJSON *comm = cJSON_GetObjectItemCaseSensitive(resp, "commission");
            const cJSON *nid = cJSON_GetObjectItemCaseSensitive(resp, "node_id");
            const cJSON *msg = cJSON_GetObjectItemCaseSensitive(resp, "message");
            if (cJSON_IsNumber(nid)) {
                node_id = (uint32_t)nid->valuedouble;
            }
            if (cJSON_IsString(res) && res->valuestring &&
                    strcmp(res->valuestring, "ok") == 0 &&
                    cJSON_IsString(comm) && comm->valuestring &&
                    strcmp(comm->valuestring, "started") == 0) {
                started = true;
            } else if (cJSON_IsString(msg) && msg->valuestring &&
                       msg->valuestring[0]) {
                strlcpy(err_msg, msg->valuestring, sizeof(err_msg));
            } else if (cJSON_IsString(comm) && comm->valuestring &&
                       comm->valuestring[0]) {
                strlcpy(err_msg, comm->valuestring, sizeof(err_msg));
            }
        }
        cJSON_Delete(resp);
    }
    response_release(&response);

    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        hub->commission.node_id = node_id;
        if (started) {
            hub->commission.state = APP_HUB_COMMISSION_RUNNING;
            hub->commission.stage = APP_HUB_COMMISSION_STAGE_DISCOVERING;
            hub->commission.error[0] = '\0';
            hub->commission_active = true;
        } else {
            hub->commission.state = APP_HUB_COMMISSION_FAILED;
            hub->commission.stage = APP_HUB_COMMISSION_STAGE_NONE;
            strlcpy(hub->commission.error, err_msg,
                    sizeof(hub->commission.error));
            hub->commission_active = false;
        }
        ++hub->generation;
        xSemaphoreGive(hub->mutex);
    }
}

static void cancel_and_delete_matter_node(struct app_hub *hub,
                                          uint32_t node_id,
                                          const char *point_id)
{
    if (node_id >= 2) {
        cJSON *croot = cJSON_CreateObject();
        if (croot) {
            cJSON_AddNumberToObject(croot, "node_id", (double)node_id);
            cJSON_AddBoolToObject(croot, "cancel", true);
            char *cpayload = cJSON_PrintUnformatted(croot);
            cJSON_Delete(croot);
            if (cpayload) {
                http_response_t cresp;
                (void)request(hub, HTTP_METHOD_POST, "/api/matter/commission",
                              cpayload, &cresp);
                response_release(&cresp);
                free(cpayload);
            }
        }
    }

    char resolved_point[APP_HUB_POINT_ID_LEN] = {0};
    if (point_id && point_id[0] != '\0') {
        strlcpy(resolved_point, point_id, sizeof(resolved_point));
    } else if (node_id >= 2) {
        snprintf(resolved_point, sizeof(resolved_point),
                 "wifi.matter_n%lu_ep1", (unsigned long)node_id);
    }

    if (resolved_point[0] != '\0') {
        cJSON *droot = cJSON_CreateObject();
        if (droot) {
            cJSON_AddStringToObject(droot, "point_id", resolved_point);
            cJSON_AddBoolToObject(droot, "reload", true);
            char *dpayload = cJSON_PrintUnformatted(droot);
            cJSON_Delete(droot);
            if (dpayload) {
                http_response_t dresp;
                (void)request(hub, HTTP_METHOD_POST, "/api/points/remove",
                              dpayload, &dresp);
                response_release(&dresp);
                free(dpayload);
            }
        }
    }
}

static void process_matter_cancel(struct app_hub *hub,
                                  const hub_command_t *command)
{
    uint32_t node_id = (uint32_t)command->value;
    if (node_id < 2 &&
            xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        node_id = hub->commission.node_id;
        xSemaphoreGive(hub->mutex);
    }
    cancel_and_delete_matter_node(hub, node_id, NULL);
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        hub->commission.state = APP_HUB_COMMISSION_IDLE;
        hub->commission.stage = APP_HUB_COMMISSION_STAGE_NONE;
        hub->commission.error[0] = '\0';
        hub->commission_active = false;
        ++hub->generation;
        xSemaphoreGive(hub->mutex);
    }
}

static void process_delete_point(struct app_hub *hub,
                                 const hub_command_t *command)
{
    const uint32_t node_id = (uint32_t)command->value;
    cancel_and_delete_matter_node(hub, node_id, command->point_id);
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        if (node_id >= 2 && hub->commission.node_id == node_id) {
            hub->commission.state = APP_HUB_COMMISSION_IDLE;
            hub->commission.stage = APP_HUB_COMMISSION_STAGE_NONE;
            hub->commission.error[0] = '\0';
            hub->commission_active = false;
        }
        ++hub->generation;
        xSemaphoreGive(hub->mutex);
    }
}

static esp_err_t process_command(struct app_hub *hub, const hub_command_t *command)
{
    app_hub_point_t point = {0};
    bool found = false;
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    for (size_t i = 0; i < hub->point_count; ++i) {
        if (strcmp(hub->points[i].point_id, command->point_id) == 0) {
            point = hub->points[i];
            found = true;
            break;
        }
    }
    xSemaphoreGive(hub->mutex);
    if (!found || !point.controllable) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    const char *endpoint = "/api/devices/control";
    if (command->has_value) {
        cJSON_AddStringToObject(root, "device", point.type);
        cJSON_AddStringToObject(root, "parameter", command->parameter);
        cJSON_AddNumberToObject(root, "value", command->value);
        cJSON_AddStringToObject(root, "point_id", point.point_id);
    } else {
        /* Matter lights have no props.params.power, so /api/devices/control
         * ignores point_id and hits the first catalog light. */
        endpoint = "/api/command";
        cJSON_AddStringToObject(root, "point_id", point.point_id);
        cJSON_AddStringToObject(root, "cmd", point.on ? "turn_off" : "turn_on");
    }
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }

    http_response_t response = {0};
    esp_err_t err = request(hub, HTTP_METHOD_POST, endpoint, payload, &response);
    free(payload);
    if (err == ESP_OK && response.status != 200) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    response_release(&response);
    if (err != ESP_OK) {
        set_status(hub, false, "Device command failed");
    }
    return err;
}

static void set_wifi_status(struct app_hub *hub, bool ready, bool needs_setup,
                            const char *status)
{
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return;
    }
    hub->wifi_ready = ready;
    hub->wifi_needs_setup = needs_setup;
    if (!ready) {
        hub->hub_connected = false;
    }
    strlcpy(hub->status, status, sizeof(hub->status));
    ++hub->generation;
    xSemaphoreGive(hub->mutex);
}

static void forget_saved_setup_once(void)
{
    nvs_handle_t nvs;
    if (nvs_open(HUB_NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    uint8_t token = 0;
    if (nvs_get_u8(nvs, HUB_NVS_SETUP_WIPE, &token) == ESP_OK &&
            token == HUB_SETUP_WIPE_TOKEN) {
        nvs_close(nvs);
        return;
    }
    ESP_LOGW(TAG, "Clearing saved Wi-Fi and Hub role for first-run setup");
    if (nvs_erase_all(nvs) == ESP_OK &&
            nvs_set_u8(nvs, HUB_NVS_SETUP_WIPE, HUB_SETUP_WIPE_TOKEN) == ESP_OK) {
        (void)nvs_commit(nvs);
    }
    nvs_close(nvs);
}

static void load_wifi_profile(struct app_hub *hub)
{
    if (CONFIG_MINIHUB_PANEL_WIFI_SSID[0] != '\0') {
        strlcpy(hub->wifi_ssid, CONFIG_MINIHUB_PANEL_WIFI_SSID,
                sizeof(hub->wifi_ssid));
        strlcpy(hub->wifi_password, CONFIG_MINIHUB_PANEL_WIFI_PASSWORD,
                sizeof(hub->wifi_password));
        hub->wifi_has_profile = true;
        return;
    }

    nvs_handle_t nvs;
    if (nvs_open(HUB_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    size_t ssid_len = sizeof(hub->wifi_ssid);
    size_t password_len = sizeof(hub->wifi_password);
    size_t token_len = sizeof(hub->api_token);
    uint8_t verified = 0;
    if (nvs_get_u8(nvs, HUB_NVS_WIFI_VERIFIED, &verified) == ESP_OK &&
            verified == 1 &&
            nvs_get_str(nvs, HUB_NVS_SSID, hub->wifi_ssid, &ssid_len) == ESP_OK &&
            hub->wifi_ssid[0] != '\0') {
        nvs_get_str(nvs, HUB_NVS_PASSWORD, hub->wifi_password, &password_len);
        hub->wifi_has_profile = true;
    }
    nvs_get_str(nvs, HUB_NVS_TOKEN, hub->api_token, &token_len);
    nvs_close(nvs);
}

static esp_err_t save_wifi_profile(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(HUB_NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG,
                        "nvs open");
    esp_err_t err = nvs_set_str(nvs, HUB_NVS_SSID, ssid != NULL ? ssid : "");
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, HUB_NVS_PASSWORD, password != NULL ? password : "");
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs, HUB_NVS_WIFI_VERIFIED, 1);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

static esp_err_t save_api_token(const char *token)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(HUB_NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG,
                        "nvs open");
    esp_err_t err = nvs_set_str(nvs, HUB_NVS_TOKEN, token != NULL ? token : "");
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

static void keep_wifi_awake(void)
{
    const esp_err_t err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi set_ps(NONE) failed: %s", esp_err_to_name(err));
    }
}

/* Same GOT_IP path as minihub_net.c: default route + AliDNS backup so CHIP
 * FindOperational mDNS uses WIFI_STA_DEF instead of a null/ETH netif. */
static void adopt_sta_netif(struct app_hub *hub)
{
    if (!hub) {
        return;
    }
    if (!hub->sta_netif) {
        hub->sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    }
    if (!hub->sta_netif) {
        return;
    }
    (void)esp_netif_set_default_netif(hub->sta_netif);

    esp_netif_dns_info_t backup = {0};
    if (esp_netif_get_dns_info(hub->sta_netif, ESP_NETIF_DNS_BACKUP, &backup) ==
            ESP_OK &&
        backup.ip.type == ESP_IPADDR_TYPE_V4 &&
        backup.ip.u_addr.ip4.addr != 0) {
        return;
    }
    esp_netif_dns_info_t dns = {0};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ESP_IP4TOADDR(223, 5, 5, 5);
    const esp_err_t err =
        esp_netif_set_dns_info(hub->sta_netif, ESP_NETIF_DNS_BACKUP, &dns);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set DNS backup 223.5.5.5 failed: %s",
                 esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "DNS backup set to 223.5.5.5");
}

static void publish_commission_wifi(const struct app_hub *hub)
{
    if (!hub || hub->wifi_ssid[0] == '\0') {
        return;
    }
    const esp_err_t err = minihub_provision_save_credentials(
        hub->wifi_ssid, hub->wifi_password);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MiniHub Wi-Fi credentials save failed: %s",
                 esp_err_to_name(err));
    }
}

static void fill_hub_wifi_from_sta(struct app_hub *hub)
{
    if (!hub || (hub->wifi_ssid[0] && hub->wifi_password[0])) {
        return;
    }
    wifi_config_t *cfg = heap_caps_calloc(
        1, sizeof(*cfg), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!cfg) {
        cfg = calloc(1, sizeof(*cfg));
    }
    if (!cfg) {
        return;
    }
    if (esp_wifi_get_config(WIFI_IF_STA, cfg) == ESP_OK) {
        if (!hub->wifi_ssid[0] && cfg->sta.ssid[0]) {
            strlcpy(hub->wifi_ssid, (const char *)cfg->sta.ssid,
                    sizeof(hub->wifi_ssid));
        }
        if (!hub->wifi_password[0] && cfg->sta.password[0]) {
            strlcpy(hub->wifi_password, (const char *)cfg->sta.password,
                    sizeof(hub->wifi_password));
        }
    }
    free(cfg);
}

static esp_err_t apply_wifi_profile(const struct app_hub *hub)
{
    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, hub->wifi_ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, hub->wifi_password,
            sizeof(config.sta.password));
    config.sta.threshold.authmode =
        hub->wifi_password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    return esp_wifi_set_config(WIFI_IF_STA, &config);
}

static void record_scan_result(struct app_hub *hub, const wifi_ap_record_t *record)
{
    char ssid[APP_HUB_SSID_LEN];
    memcpy(ssid, record->ssid, sizeof(record->ssid));
    ssid[sizeof(ssid) - 1] = '\0';
    if (ssid[0] == '\0') {
        return;
    }
    for (size_t i = 0; i < hub->wifi_ap_count; ++i) {
        if (strncmp(hub->wifi_aps[i].ssid, ssid, APP_HUB_SSID_LEN) == 0) {
            if (record->rssi > hub->wifi_aps[i].rssi) {
                hub->wifi_aps[i].rssi = record->rssi;
                hub->wifi_aps[i].secured = record->authmode != WIFI_AUTH_OPEN;
            }
            return;
        }
    }
    if (hub->wifi_ap_count >= APP_HUB_WIFI_AP_LIMIT) {
        return;
    }
    app_hub_wifi_ap_t *ap = &hub->wifi_aps[hub->wifi_ap_count++];
    strlcpy(ap->ssid, ssid, sizeof(ap->ssid));
    ap->rssi = record->rssi;
    ap->secured = record->authmode != WIFI_AUTH_OPEN;
}

static bool wifi_is_connecting(const struct app_hub *hub)
{
    return hub->wifi_connecting;
}

static void finish_wifi_scan(struct app_hub *hub)
{
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        hub->wifi_scanning = false;
        ++hub->generation;
        xSemaphoreGive(hub->mutex);
    }
}

static void process_wifi_scan(struct app_hub *hub)
{
    if (wifi_is_connecting(hub)) {
        return;
    }
    set_wifi_status(hub, hub->wifi_ready, hub->wifi_needs_setup, "Scanning...");
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        hub->wifi_scanning = true;
        xSemaphoreGive(hub->mutex);
    }

    const EventBits_t started = xEventGroupWaitBits(
        hub->wifi_events, HUB_STA_STARTED_BIT, pdFALSE, pdTRUE,
        pdMS_TO_TICKS(3000));
    if ((started & HUB_STA_STARTED_BIT) == 0) {
        ESP_LOGE(TAG, "Wi-Fi scan rejected: station did not start");
        finish_wifi_scan(hub);
        set_wifi_status(hub, false, true, "Wi-Fi not ready");
        return;
    }
    if (wifi_is_connecting(hub)) {
        finish_wifi_scan(hub);
        return;
    }

    const wifi_scan_config_t scan = {
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    uint16_t ap_count = 0;
    memset(hub->wifi_scan_records, 0, sizeof(hub->wifi_scan_records));
    xEventGroupClearBits(hub->wifi_events, HUB_SCAN_DONE_BIT);
    esp_err_t err = esp_wifi_scan_start(&scan, false);
    ESP_LOGI(TAG, "Wi-Fi scan start: %s", esp_err_to_name(err));
    if (err == ESP_ERR_WIFI_STATE && !wifi_is_connecting(hub)) {
        ESP_LOGW(TAG, "Wi-Fi is connecting; cancelling it before scan");
        xEventGroupClearBits(hub->wifi_events, HUB_STA_DISCONNECTED_BIT);
        hub->wifi_auto_reconnect = false;
        hub->wifi_expect_disconnect = true;
        const esp_err_t disconnect_err = esp_wifi_disconnect();
        if (disconnect_err == ESP_OK) {
            (void)xEventGroupWaitBits(
                hub->wifi_events, HUB_STA_DISCONNECTED_BIT, pdTRUE, pdTRUE,
                pdMS_TO_TICKS(3000));
        } else {
            hub->wifi_expect_disconnect = false;
        }
        xEventGroupClearBits(hub->wifi_events, HUB_SCAN_DONE_BIT);
        err = esp_wifi_scan_start(&scan, false);
        ESP_LOGI(TAG, "Wi-Fi scan retry: %s", esp_err_to_name(err));
    }
    if (err == ESP_OK) {
        const EventBits_t done = xEventGroupWaitBits(
            hub->wifi_events, HUB_SCAN_DONE_BIT, pdTRUE, pdTRUE,
            pdMS_TO_TICKS(HUB_WIFI_SCAN_TIMEOUT_MS));
        if ((done & HUB_SCAN_DONE_BIT) == 0) {
            ESP_LOGE(TAG, "Wi-Fi scan timed out after %d ms",
                     HUB_WIFI_SCAN_TIMEOUT_MS);
            (void)esp_wifi_scan_stop();
            err = ESP_ERR_TIMEOUT;
        }
    }
    if (err == ESP_OK) {
        ap_count = APP_HUB_WIFI_AP_LIMIT;
        err = esp_wifi_scan_get_ap_records(&ap_count,
                                           hub->wifi_scan_records);
        ESP_LOGI(TAG, "Wi-Fi scan result: %s, APs=%u",
                 esp_err_to_name(err), (unsigned)ap_count);
    } else {
        ESP_LOGE(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(err));
    }

    if (wifi_is_connecting(hub)) {
        finish_wifi_scan(hub);
        return;
    }

    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        if (err == ESP_OK) {
            hub->wifi_ap_count = 0;
            for (uint16_t i = 0; i < ap_count; ++i) {
                record_scan_result(hub, &hub->wifi_scan_records[i]);
            }
        }
        hub->wifi_scanning = false;
        ++hub->generation;
        xSemaphoreGive(hub->mutex);
    }
    set_wifi_status(hub, hub->wifi_ready,
                    !hub->wifi_ready,
                    err == ESP_OK ? "Select a network" : "Scan failed");
}

static const char *wifi_fail_status(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
        return "Network not found";
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "Wrong password";
    default:
        return "Failed";
    }
}

static void start_wifi_connection(struct app_hub *hub)
{
    hub->wifi_connect_started_at = xTaskGetTickCount();
    const esp_err_t err = esp_wifi_connect();
    ESP_LOGW(TAG, "Wi-Fi connect start: %s", esp_err_to_name(err));
    if (err != ESP_OK) {
        hub->wifi_connecting = false;
        hub->wifi_connect_started_at = 0;
        set_wifi_status(hub, false, true, "Failed");
    }
}

static void process_wifi_connect(struct app_hub *hub, const hub_command_t *command)
{
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        strlcpy(hub->wifi_ssid, command->point_id, sizeof(hub->wifi_ssid));
        strlcpy(hub->wifi_password, command->wifi_password,
                sizeof(hub->wifi_password));
        hub->wifi_connected_ssid[0] = '\0';
        hub->wifi_has_profile = hub->wifi_ssid[0] != '\0';
        hub->wifi_connecting = true;
        hub->wifi_auto_reconnect = false;
        xSemaphoreGive(hub->mutex);
    }
    if (!hub->wifi_has_profile) {
        set_wifi_status(hub, false, true, "Select a network");
        return;
    }
    ESP_LOGW(TAG, "Wi-Fi connect request: ssid=\"%s\", password_len=%u",
             hub->wifi_ssid, (unsigned)strlen(hub->wifi_password));
    if (apply_wifi_profile(hub) != ESP_OK) {
        hub->wifi_connecting = false;
        set_wifi_status(hub, false, true, "Failed");
        return;
    }
    set_wifi_status(hub, false, false, "Connecting...");
    xEventGroupClearBits(hub->wifi_events, HUB_STA_DISCONNECTED_BIT);
    hub->wifi_expect_disconnect = true;
    const esp_err_t err = esp_wifi_disconnect();
    if (err == ESP_OK) {
        (void)xEventGroupWaitBits(
            hub->wifi_events, HUB_STA_DISCONNECTED_BIT, pdTRUE, pdTRUE,
            pdMS_TO_TICKS(HUB_WIFI_DISCONNECT_TIMEOUT_MS));
        hub->wifi_expect_disconnect = false;
    } else {
        /*
         * No disconnect event will follow when the STA is already idle. Do
         * not let the forthcoming authentication-failure event be consumed
         * as the pre-connect disconnect.
         */
        hub->wifi_expect_disconnect = false;
        if (err != ESP_ERR_WIFI_NOT_CONNECT) {
            ESP_LOGW(TAG, "Wi-Fi disconnect before connect: %s",
                     esp_err_to_name(err));
        }
    }
    start_wifi_connection(hub);
}

static void check_wifi_connect_timeout(struct app_hub *hub)
{
    bool timed_out = false;
    if (xSemaphoreTake(hub->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        const TickType_t now = xTaskGetTickCount();
        if (hub->wifi_connecting && hub->wifi_connect_started_at != 0 &&
                now - hub->wifi_connect_started_at >=
                pdMS_TO_TICKS(HUB_WIFI_CONNECT_TIMEOUT_MS)) {
            hub->wifi_connecting = false;
            hub->wifi_auto_reconnect = false;
            hub->wifi_expect_disconnect = true;
            hub->wifi_connect_started_at = 0;
            timed_out = true;
        }
        xSemaphoreGive(hub->mutex);
    }
    if (!timed_out) {
        return;
    }
    ESP_LOGW(TAG, "Wi-Fi connection timed out after %d ms",
             HUB_WIFI_CONNECT_TIMEOUT_MS);
    set_wifi_status(hub, false, true, "Connection timeout");
    const esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        hub->wifi_expect_disconnect = false;
    }
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    struct app_hub *hub = arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        xEventGroupSetBits(hub->wifi_events, HUB_STA_STARTED_BIT);
        if (hub->wifi_auto_reconnect && hub->wifi_has_profile) {
            hub->wifi_connecting = true;
            start_wifi_connection(hub);
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        xEventGroupSetBits(hub->wifi_events, HUB_SCAN_DONE_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        const wifi_event_sta_connected_t *connected = data;
        if (connected != NULL &&
                xSemaphoreTake(hub->mutex,
                               pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            const size_t length =
                connected->ssid_len < sizeof(hub->wifi_connected_ssid) - 1U ?
                connected->ssid_len : sizeof(hub->wifi_connected_ssid) - 1U;
            memcpy(hub->wifi_connected_ssid, connected->ssid, length);
            hub->wifi_connected_ssid[length] = '\0';
            xSemaphoreGive(hub->mutex);
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disc = data;
        const uint8_t reason = disc != NULL ? disc->reason : 0;
        xEventGroupSetBits(hub->wifi_events, HUB_STA_DISCONNECTED_BIT);
        xEventGroupClearBits(hub->wifi_events, HUB_CONNECTED_BIT);
        if (xSemaphoreTake(hub->mutex,
                           pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            hub->wifi_connected_ssid[0] = '\0';
            xSemaphoreGive(hub->mutex);
        }
        if (hub->wifi_expect_disconnect) {
            hub->wifi_expect_disconnect = false;
            return;
        }
        ESP_LOGW(TAG, "Wi-Fi disconnected, reason=%u", (unsigned)reason);
        hub->wifi_connecting = false;
        hub->wifi_connect_started_at = 0;
        if (hub->wifi_auto_reconnect && hub->wifi_has_profile) {
            set_wifi_status(hub, false, false, "Connecting...");
            hub->wifi_connecting = true;
            start_wifi_connection(hub);
        } else if (hub->wifi_has_profile) {
            set_wifi_status(hub, false, true, wifi_fail_status(reason));
        } else {
            set_wifi_status(hub, false, true, "Select a network");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *got_ip = data;
        bool current_target = false;
        if (xSemaphoreTake(hub->mutex,
                           pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            current_target = hub->wifi_connecting &&
                             hub->wifi_connected_ssid[0] != '\0' &&
                             strncmp(hub->wifi_connected_ssid, hub->wifi_ssid,
                                     sizeof(hub->wifi_ssid)) == 0;
            if (current_target) {
                hub->wifi_connecting = false;
                hub->wifi_auto_reconnect = true;
                hub->wifi_expect_disconnect = false;
                hub->wifi_connect_started_at = 0;
            }
            xSemaphoreGive(hub->mutex);
        }
        if (!current_target) {
            ESP_LOGW(TAG, "Ignoring stale GOT_IP for previous Wi-Fi target");
            return;
        }
        xEventGroupSetBits(hub->wifi_events, HUB_CONNECTED_BIT);
        if (got_ip) {
            snprintf(hub->local_ip, sizeof(hub->local_ip), IPSTR,
                     IP2STR(&got_ip->ip_info.ip));
        }
        set_wifi_status(hub, true, false, "Connected");
        keep_wifi_awake();
        adopt_sta_netif(hub);
        /* NVS and MiniHub provision commits may disable the flash cache.
         * Defer them from the system event task to hub_client. */
        hub->wifi_profile_save_pending = true;
        if (hub->worker) {
            xTaskNotifyGive(hub->worker);
        }
    }
}

static esp_err_t network_stack_init(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    return ESP_OK;
}

app_hub_result_t app_hub_network_init(void)
{
    return network_stack_init() == ESP_OK ? APP_HUB_OK :
           APP_HUB_ERR_INTERNAL;
}

static esp_err_t wifi_start(struct app_hub *hub)
{
    ESP_RETURN_ON_ERROR(network_stack_init(), TAG, "network stack");
    hub->sta_netif = esp_netif_create_default_wifi_sta();
    if (!hub->sta_netif) {
        hub->sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    }
    if (!hub->sta_netif) {
        return ESP_ERR_NO_MEM;
    }
    const wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_init), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG,
                        "wifi storage");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   wifi_event, hub),
                        TAG, "wifi event");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   wifi_event, hub),
                        TAG, "ip event");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
    forget_saved_setup_once();
    load_wifi_profile(hub);
    load_hub_identity(hub);
    if (hub->role != APP_HUB_ROLE_UNSET && hub->wifi_has_profile) {
        ESP_RETURN_ON_ERROR(apply_wifi_profile(hub), TAG, "wifi config");
        hub->wifi_auto_reconnect = true;
        set_wifi_status(hub, false, false, "Connecting...");
        publish_commission_wifi(hub);
    } else {
        hub->wifi_auto_reconnect = false;
        set_wifi_status(hub, false, true, "Select a network");
    }
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
    keep_wifi_awake();
    return ESP_OK;
}

static void hub_worker(void *arg)
{
    struct app_hub *hub = arg;
    uint8_t metadata_cycle = 0;
    bool scan_requested = false;
    for (;;) {
        if (hub->wifi_profile_save_pending) {
            hub->wifi_profile_save_pending = false;
            const esp_err_t save_err =
                save_wifi_profile(hub->wifi_ssid, hub->wifi_password);
            if (save_err != ESP_OK) {
                ESP_LOGW(TAG, "Save Wi-Fi profile failed: %s",
                         esp_err_to_name(save_err));
            }
            publish_commission_wifi(hub);
        }
        check_wifi_connect_timeout(hub);
        hub_command_t *command = NULL;
        hub_command_t *connect_cmd = NULL;
        while (xQueueReceive(hub->command_queue, &command, 0) == pdTRUE) {
            if (command->kind == HUB_CMD_WIFI_CONNECT) {
                free(connect_cmd);
                connect_cmd = command;
                command = NULL;
            } else if (command->kind == HUB_CMD_WIFI_SCAN) {
                scan_requested = true;
                ESP_LOGI(TAG, "Wi-Fi scan command received");
            } else if (command->kind == HUB_CMD_MATTER_COMMISSION) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_matter_commission(hub, command->point_id);
                    sync_points(hub);
                } else if (xSemaphoreTake(
                               hub->mutex,
                               pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
                    hub->commission.state = APP_HUB_COMMISSION_FAILED;
                    hub->commission.stage = APP_HUB_COMMISSION_STAGE_NONE;
                    strlcpy(hub->commission.error, "Network is not ready",
                            sizeof(hub->commission.error));
                    hub->commission_active = false;
                    ++hub->generation;
                    xSemaphoreGive(hub->mutex);
                }
            } else if (command->kind == HUB_CMD_MATTER_CANCEL) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_matter_cancel(hub, command);
                    sync_points(hub);
                }
            } else if (command->kind == HUB_CMD_ADD_DEVICE) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_add_device(hub, &command->add);
                } else {
                    set_add_status(hub, APP_HUB_ADD_FAILED, 0,
                                   "Network is not ready", NULL);
                }
            } else if (command->kind == HUB_CMD_ADD_CANCEL) {
                process_add_cancel(hub, command);
            } else if (command->kind == HUB_CMD_DELETE_POINT) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_delete_point(hub, command);
                    sync_points(hub);
                } else {
                    set_status(hub, false, "Network is not ready");
                }
            } else if (command->kind == HUB_CMD_SCENE_INVOKE) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_scene_invoke(hub, command);
                } else {
                    set_scene_result(hub, "Network is not ready");
                }
            } else if (command->kind == HUB_CMD_SCENE_CREATE) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_scene_create(hub, command);
                } else {
                    set_scene_result(hub, "Network is not ready");
                }
            } else if (command->kind == HUB_CMD_SCENE_DELETE) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_scene_delete(hub, command);
                } else {
                    set_scene_result(hub, "Network is not ready");
                }
            } else if (command->kind == HUB_CMD_RULE_SET_ENABLED) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_rule_enabled(hub, command);
                } else {
                    set_rule_result(hub, "Network is not ready");
                }
            } else if (command->kind == HUB_CMD_RULE_CREATE) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_rule_create(hub, command);
                } else {
                    set_rule_result(hub, "Network is not ready");
                }
            } else if (command->kind == HUB_CMD_RULE_DELETE) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_rule_delete(hub, command);
                } else {
                    set_rule_result(hub, "Network is not ready");
                }
            } else if (command->kind == HUB_CMD_SYSTEM_REBOOT) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_system_reboot(hub);
                } else {
                    set_status(hub, false, "Network is not ready");
                }
            } else if (command->kind == HUB_CMD_HUB_RELOAD) {
                if (xEventGroupGetBits(hub->wifi_events) & HUB_CONNECTED_BIT) {
                    process_point_reconnect(hub, command->point_id);
                } else {
                    set_status(hub, false, "Network is not ready");
                }
            } else {
                if ((xEventGroupGetBits(hub->wifi_events) &
                        HUB_CONNECTED_BIT) == 0) {
                    set_status(hub, false, "Network is not ready");
                } else if (process_command(hub, command) == ESP_OK) {
                    sync_points(hub);
                }
            }
            free(command);
            command = NULL;
        }
        if (connect_cmd) {
            process_wifi_connect(hub, connect_cmd);
            free(connect_cmd);
        } else if (scan_requested && !hub->wifi_connecting) {
            scan_requested = false;
            ESP_LOGI(TAG, "Wi-Fi scan command executing");
            process_wifi_scan(hub);
        }

        const EventBits_t bits = xEventGroupGetBits(hub->wifi_events);
        char hub_url[HUB_URL_CAPACITY];
        if ((bits & HUB_CONNECTED_BIT) == 0 ||
                !copy_hub_url(hub, hub_url, sizeof(hub_url))) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            continue;
        }
        if (hub->commission_active) {
            sync_matter_status(hub);
        }
        sync_points(hub);
        update_add_progress(hub);
        if (metadata_cycle++ == 0) {
            (void)sync_scenes(hub);
            (void)sync_rules(hub);
            (void)sync_system(hub);
        }
        if (metadata_cycle >= HUB_METADATA_SYNC_CYCLES) {
            metadata_cycle = 0;
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(
                             hub->commission_active ? 1200 : HUB_SYNC_INTERVAL_MS));
    }
}

app_hub_result_t app_hub_start(app_hub_handle_t *out_handle)
{
    if (!out_handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    *out_handle = NULL;
    struct app_hub *hub = heap_caps_calloc(
        1, sizeof(*hub), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!hub) {
        hub = calloc(1, sizeof(*hub));
    }
    if (!hub) {
        return APP_HUB_ERR_NO_MEMORY;
    }
    hub->mutex = xSemaphoreCreateMutex();
    hub->wifi_events = xEventGroupCreate();
    hub->command_queue = xQueueCreate(HUB_COMMAND_QUEUE_LENGTH,
                                      sizeof(hub_command_t *));
    if (!hub->mutex || !hub->wifi_events || !hub->command_queue) {
        if (hub->mutex) {
            vSemaphoreDelete(hub->mutex);
        }
        if (hub->wifi_events) {
            vEventGroupDelete(hub->wifi_events);
        }
        if (hub->command_queue) {
            vQueueDelete(hub->command_queue);
        }
        free(hub);
        return APP_HUB_ERR_NO_MEMORY;
    }
    strlcpy(hub->status, "Starting Wi-Fi", sizeof(hub->status));
    hub->add.state = APP_HUB_ADD_IDLE;
    hub->add.generation = 1;
    strlcpy(hub->add.status, "Choose a protocol", sizeof(hub->add.status));
#if CONFIG_MINIHUB_MATTER_ADAPTER
    hub->add.capabilities |= APP_HUB_CAP_MATTER;
#endif
#if CONFIG_MINIHUB_ZIGBEE_SDK
    hub->add.capabilities |= APP_HUB_CAP_ZIGBEE;
#endif
#if CONFIG_MINIHUB_BLE_MESH_SDK
    hub->add.capabilities |= APP_HUB_CAP_BLE_MESH;
#endif
#if CONFIG_MINIHUB_MQTT_ADAPTER
    hub->add.capabilities |= APP_HUB_CAP_MQTT;
#endif
#if CONFIG_MINIHUB_MODBUS
    hub->add.capabilities |= APP_HUB_CAP_MODBUS_TCP;
#endif
    strlcpy(hub->scenes_status, "Loading scenes...",
            sizeof(hub->scenes_status));
    strlcpy(hub->rules_status, "Loading automations...",
            sizeof(hub->rules_status));
    strlcpy(hub->system.status, "Loading system status...",
            sizeof(hub->system.status));
    esp_err_t err = wifi_start(hub);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        set_status(hub, false, "Wi-Fi initialization failed");
        ESP_LOGE(TAG, "Wi-Fi initialization failed: %s", esp_err_to_name(err));
    }
    if (xTaskCreate(hub_worker, "hub_client", 16384, hub, 5,
                    &hub->worker) != pdPASS) {
        free(hub);
        return APP_HUB_ERR_NO_MEMORY;
    }
    *out_handle = hub;
    return APP_HUB_OK;
}

static app_hub_result_t queue_command(app_hub_handle_t handle,
                                      const hub_command_t *source)
{
    hub_command_t *command = heap_caps_malloc(
        sizeof(*command), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!command) {
        command = malloc(sizeof(*command));
    }
    if (!command) {
        return APP_HUB_ERR_NO_MEMORY;
    }
    memcpy(command, source, sizeof(*command));
    if (xQueueSend(handle->command_queue, &command, 0) != pdTRUE) {
        free(command);
        return APP_HUB_ERR_BUSY;
    }
    return APP_HUB_OK;
}

app_hub_result_t app_hub_snapshot(app_hub_handle_t handle, size_t page,
                                  app_hub_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    *out_snapshot = (app_hub_snapshot_t) {
        .generation = handle->generation,
        .total_points = handle->point_count,
        .online_points = handle->online_count,
        .connected = handle->hub_connected,
        .wifi_ready = handle->wifi_ready,
        .wifi_needs_setup = handle->wifi_needs_setup,
        .role = handle->role,
        .role_needed = handle->wifi_ready &&
                       handle->role == APP_HUB_ROLE_UNSET,
        .url_needed = handle->wifi_ready &&
                      handle->role == APP_HUB_ROLE_WEB &&
                      handle->hub_url[0] == '\0',
        .auth_needed = handle->wifi_ready &&
                       handle->role == APP_HUB_ROLE_WEB &&
                       handle->hub_url[0] != '\0' &&
                       !handle->hub_connected && !hub_is_local(handle),
    };
    strlcpy(out_snapshot->status, handle->status, sizeof(out_snapshot->status));
    out_snapshot->page_count =
        (handle->point_count + 1 + APP_HUB_PAGE_SIZE - 1) / APP_HUB_PAGE_SIZE;
    if (out_snapshot->page_count == 0) {
        out_snapshot->page_count = 1;
    }
    if (page >= out_snapshot->page_count) {
        page = out_snapshot->page_count - 1;
    }
    out_snapshot->page = page;
    const size_t start = page * APP_HUB_PAGE_SIZE;
    for (size_t i = 0; i < APP_HUB_PAGE_SIZE && start + i < handle->point_count;
            ++i) {
        out_snapshot->points[i] = handle->points[start + i];
        ++out_snapshot->visible_points;
    }
    xSemaphoreGive(handle->mutex);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_toggle(app_hub_handle_t handle, const char *point_id)
{
    if (!handle || !point_id || point_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {0};
    strlcpy(command.point_id, point_id, sizeof(command.point_id));
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_parameter(app_hub_handle_t handle,
                                       const char *point_id,
                                       const char *parameter, int32_t value)
{
    if (!handle || !point_id || point_id[0] == '\0' || !parameter ||
            parameter[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .value = value,
        .has_value = true,
    };
    strlcpy(command.point_id, point_id, sizeof(command.point_id));
    strlcpy(command.parameter, parameter, sizeof(command.parameter));
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_refresh(app_hub_handle_t handle)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_wifi_snapshot(app_hub_handle_t handle, size_t page,
                                       app_hub_wifi_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    *out_snapshot = (app_hub_wifi_snapshot_t) {
        .generation = handle->generation,
        .ready = handle->wifi_ready,
        .scanning = handle->wifi_scanning,
        .connecting = handle->wifi_connecting,
        .needs_setup = handle->wifi_needs_setup,
        .total_aps = handle->wifi_ap_count,
    };
    strlcpy(out_snapshot->ssid,
            handle->wifi_ready && handle->wifi_connected_ssid[0] != '\0' ?
            handle->wifi_connected_ssid : handle->wifi_ssid,
            sizeof(out_snapshot->ssid));
    strlcpy(out_snapshot->status, handle->status, sizeof(out_snapshot->status));
    out_snapshot->page_count =
        (handle->wifi_ap_count + APP_HUB_WIFI_PAGE_SIZE - 1) / APP_HUB_WIFI_PAGE_SIZE;
    if (out_snapshot->page_count == 0) {
        out_snapshot->page_count = 1;
    }
    if (page >= out_snapshot->page_count) {
        page = out_snapshot->page_count - 1;
    }
    out_snapshot->page = page;
    const size_t start = page * APP_HUB_WIFI_PAGE_SIZE;
    for (size_t i = 0;
            i < APP_HUB_WIFI_PAGE_SIZE && start + i < handle->wifi_ap_count; ++i) {
        out_snapshot->aps[i] = handle->wifi_aps[start + i];
        ++out_snapshot->visible_aps;
    }
    xSemaphoreGive(handle->mutex);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_wifi_scan(app_hub_handle_t handle)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex,
                       pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Wi-Fi scan request rejected: mutex busy");
        return APP_HUB_ERR_BUSY;
    }
    const bool connecting = handle->wifi_connecting;
    xSemaphoreGive(handle->mutex);
    if (connecting) {
        ESP_LOGW(TAG, "Wi-Fi scan request rejected: connection in progress");
        return APP_HUB_ERR_BUSY;
    }
    hub_command_t command = {
        .kind = HUB_CMD_WIFI_SCAN,
    };
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan request rejected: command queue full");
        return queued;
    }
    if (xSemaphoreTake(handle->mutex,
                       pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        handle->wifi_scanning = true;
        strlcpy(handle->status, "Scanning...", sizeof(handle->status));
        ++handle->generation;
        xSemaphoreGive(handle->mutex);
    }
    xTaskNotifyGive(handle->worker);
    ESP_LOGI(TAG, "Wi-Fi scan request queued");
    return APP_HUB_OK;
}

app_hub_result_t app_hub_wifi_connect(app_hub_handle_t handle, const char *ssid,
                                      const char *password)
{
    if (!handle || !ssid || ssid[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_WIFI_CONNECT,
    };
    strlcpy(command.point_id, ssid, sizeof(command.point_id));
    strlcpy(command.wifi_password, password != NULL ? password : "",
            sizeof(command.wifi_password));
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        strlcpy(handle->wifi_ssid, ssid, sizeof(handle->wifi_ssid));
        strlcpy(handle->wifi_password, password != NULL ? password : "",
                sizeof(handle->wifi_password));
        handle->wifi_connected_ssid[0] = '\0';
        handle->wifi_has_profile = true;
        handle->wifi_connecting = true;
        handle->wifi_connect_started_at = xTaskGetTickCount();
        handle->wifi_scanning = false;
        xSemaphoreGive(handle->mutex);
    }
    set_wifi_status(handle, false, false, "Connecting...");
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        if (xSemaphoreTake(handle->mutex,
                           pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            handle->wifi_connecting = false;
            handle->wifi_connect_started_at = 0;
            xSemaphoreGive(handle->mutex);
        }
        set_wifi_status(handle, false, true, "Failed");
        return queued;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_token(app_hub_handle_t handle, const char *token)
{
    if (!handle || !token) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    strlcpy(handle->api_token, token, sizeof(handle->api_token));
    handle->auth_failed = false;
    strlcpy(handle->status, "Connecting to MiniHub", sizeof(handle->status));
    ++handle->generation;
    xSemaphoreGive(handle->mutex);
    (void)save_api_token(token);
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_role(app_hub_handle_t handle, app_hub_role_t role,
                                  const char *url)
{
    if (!handle || (role != APP_HUB_ROLE_HUB && role != APP_HUB_ROLE_WEB)) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    handle->role = role;
    handle->hub_connected = false;
    if (role == APP_HUB_ROLE_HUB) {
        strlcpy(handle->hub_url, HUB_LOCAL_URL, sizeof(handle->hub_url));
        strlcpy(handle->status, "Using local MiniHub", sizeof(handle->status));
    } else if (url != NULL && url[0] != '\0') {
        normalize_hub_url(url, handle->hub_url, sizeof(handle->hub_url));
        strlcpy(handle->status, "Connecting to MiniHub", sizeof(handle->status));
    } else {
        handle->hub_url[0] = '\0';
        strlcpy(handle->status, "Enter Hub URL", sizeof(handle->status));
    }
    ++handle->generation;
    xSemaphoreGive(handle->mutex);
    (void)save_hub_identity(handle);
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_url(app_hub_handle_t handle, const char *url)
{
    char normalized[HUB_URL_CAPACITY];
    normalize_hub_url(url, normalized, sizeof(normalized));
    if (!handle || normalized[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    strlcpy(handle->hub_url, normalized, sizeof(handle->hub_url));
    handle->hub_connected = false;
    strlcpy(handle->status, "Connecting to MiniHub", sizeof(handle->status));
    ++handle->generation;
    xSemaphoreGive(handle->mutex);
    (void)save_hub_identity(handle);
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_matter_commission(app_hub_handle_t handle,
                                           const char *pairing_code)
{
    if (!handle || !pairing_code || pairing_code[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_MATTER_COMMISSION,
    };
    strlcpy(command.point_id, pairing_code, sizeof(command.point_id));
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        handle->commission.state = APP_HUB_COMMISSION_RUNNING;
        handle->commission.stage = APP_HUB_COMMISSION_STAGE_DISCOVERING;
        handle->commission.node_id = 0;
        handle->commission.error[0] = '\0';
        handle->commission_active = true;
        ++handle->generation;
        xSemaphoreGive(handle->mutex);
    }
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        if (xSemaphoreTake(handle->mutex,
                           pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            handle->commission.state = APP_HUB_COMMISSION_FAILED;
            handle->commission.stage = APP_HUB_COMMISSION_STAGE_NONE;
            strlcpy(handle->commission.error, "Command queue full",
                    sizeof(handle->commission.error));
            handle->commission_active = false;
            ++handle->generation;
            xSemaphoreGive(handle->mutex);
        }
        return queued;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_matter_commission_snapshot(
    app_hub_handle_t handle, app_hub_commission_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    *out_snapshot = handle->commission;
    xSemaphoreGive(handle->mutex);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_matter_cancel(app_hub_handle_t handle,
                                       uint32_t node_id)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_MATTER_CANCEL,
        .value = (int32_t)node_id,
    };
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        handle->commission_active = false;
        handle->commission.state = APP_HUB_COMMISSION_IDLE;
        handle->commission.stage = APP_HUB_COMMISSION_STAGE_NONE;
        ++handle->generation;
        xSemaphoreGive(handle->mutex);
    }
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_add_snapshot(app_hub_handle_t handle,
                                      app_hub_add_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    *out_snapshot = handle->add;
    xSemaphoreGive(handle->mutex);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_add_device(app_hub_handle_t handle,
                                    const app_hub_add_request_t *add)
{
    if (!handle || !add || add->protocol >= APP_HUB_PROTOCOL_COUNT) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if ((handle->add.capabilities & (1U << add->protocol)) == 0) {
        return APP_HUB_ERR_NOT_SUPPORTED;
    }
    if (!add->name[0] ||
            (add->protocol == APP_HUB_PROTOCOL_MATTER &&
             !app_hub_matter_pairing_code_ok(add->pairing_code)) ||
            (add->protocol == APP_HUB_PROTOCOL_ZIGBEE &&
             add->install_code[0] && !add->ieee[0]) ||
            (add->protocol == APP_HUB_PROTOCOL_MQTT &&
             (!add->host[0] || !add->point_id[0] ||
              !add->state_topic[0])) ||
            (add->protocol == APP_HUB_PROTOCOL_MODBUS_TCP &&
             (!add->host[0] || !add->point_id[0] ||
              add->unit_id == 0 || add->unit_id > 247))) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if ((xEventGroupGetBits(handle->wifi_events) & HUB_CONNECTED_BIT) == 0) {
        set_add_status(handle, APP_HUB_ADD_FAILED, 0,
                       "Network is not ready", NULL);
        return APP_HUB_ERR_INTERNAL;
    }
    if (handle->add.state == APP_HUB_ADD_PREPARING ||
            handle->add.state == APP_HUB_ADD_WAITING) {
        return APP_HUB_ERR_BUSY;
    }
    hub_command_t command = {
        .kind = HUB_CMD_ADD_DEVICE,
        .add = *add,
    };
    handle->add_cancel_requested = false;
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        handle->add.protocol = add->protocol;
        handle->add.state = APP_HUB_ADD_PREPARING;
        handle->add.progress = 5;
        handle->add.point_id[0] = '\0';
        strlcpy(handle->add.status, "Queued", sizeof(handle->add.status));
        ++handle->add.generation;
        xSemaphoreGive(handle->mutex);
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_add_cancel(app_hub_handle_t handle)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    handle->add_cancel_requested = true;
    set_add_status(handle, APP_HUB_ADD_CANCELLED, 0, "Add cancelled", NULL);
    hub_command_t command = { .kind = HUB_CMD_ADD_CANCEL };
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_delete_point(app_hub_handle_t handle,
                                      const char *point_id, uint32_t node_id)
{
    if (!handle || !point_id || point_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (strcmp(point_id, "mosaico.imu") == 0) {
        return APP_HUB_ERR_NOT_SUPPORTED;
    }
    hub_command_t command = {
        .kind = HUB_CMD_DELETE_POINT,
        .value = (int32_t)node_id,
    };
    strlcpy(command.point_id, point_id, sizeof(command.point_id));
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_scenes_snapshot(
    app_hub_handle_t handle, size_t page,
    app_hub_scenes_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    *out_snapshot = (app_hub_scenes_snapshot_t) {
        .generation = handle->scenes_generation,
        .total = handle->scene_count,
        .ready = handle->scenes_ready,
        .pending = handle->scene_pending,
    };
    strlcpy(out_snapshot->status, handle->scenes_status,
            sizeof(out_snapshot->status));
    out_snapshot->page_count =
        (handle->scene_count + APP_HUB_SCENES_PAGE_SIZE - 1) /
        APP_HUB_SCENES_PAGE_SIZE;
    if (out_snapshot->page_count == 0) {
        out_snapshot->page_count = 1;
    }
    if (page >= out_snapshot->page_count) {
        page = out_snapshot->page_count - 1;
    }
    out_snapshot->page = page;
    const size_t start = page * APP_HUB_SCENES_PAGE_SIZE;
    for (size_t i = 0;
            i < APP_HUB_SCENES_PAGE_SIZE && start + i < handle->scene_count;
            ++i) {
        out_snapshot->scenes[i] = handle->scenes[start + i];
        ++out_snapshot->visible;
    }
    xSemaphoreGive(handle->mutex);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_scene_invoke(app_hub_handle_t handle,
                                      const char *scene_id)
{
    if (!handle || !scene_id || scene_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_SCENE_INVOKE,
    };
    strlcpy(command.point_id, scene_id, sizeof(command.point_id));
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        handle->scene_pending = true;
        strlcpy(handle->scenes_status, "Starting scene...",
                sizeof(handle->scenes_status));
        ++handle->scenes_generation;
        xSemaphoreGive(handle->mutex);
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_scene_delete(app_hub_handle_t handle,
                                      const char *scene_id)
{
    if (!handle || !scene_id || scene_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_SCENE_DELETE,
    };
    strlcpy(command.point_id, scene_id, sizeof(command.point_id));
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        handle->scene_pending = true;
        strlcpy(handle->scenes_status, "Deleting scene...",
                sizeof(handle->scenes_status));
        ++handle->scenes_generation;
        xSemaphoreGive(handle->mutex);
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_scene_create(app_hub_handle_t handle,
                                      const char *name,
                                      const char *const *point_ids,
                                      const bool *on_values, size_t count)
{
    if (!handle || !name || !name[0] || !point_ids || !on_values ||
            count == 0 || count > APP_HUB_PAGE_SIZE) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_SCENE_CREATE,
        .scene_count = (uint8_t)count,
    };
    static uint32_t s_scene_seq;
    snprintf(command.point_id, sizeof(command.point_id), "panel_%lu",
             (unsigned long)++s_scene_seq);
    strlcpy(command.name, name, sizeof(command.name));
    for (size_t i = 0; i < count; ++i) {
        if (!point_ids[i] || !point_ids[i][0]) {
            return APP_HUB_ERR_INVALID_ARG;
        }
        strlcpy(command.scene_points[i], point_ids[i],
                sizeof(command.scene_points[i]));
        command.scene_values[i] = on_values[i];
    }
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        app_hub_scene_t *slot = upsert_local_scene(handle, command.point_id,
                                                   name,
                                                   count ? 1U : 0U);
        if (slot) {
            fill_scene_actions_from_command(slot, &command);
        }
        handle->scene_pending = true;
        strlcpy(handle->scenes_status, "Creating scene...",
                sizeof(handle->scenes_status));
        ++handle->scenes_generation;
        xSemaphoreGive(handle->mutex);
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_rules_snapshot(
    app_hub_handle_t handle, size_t page,
    app_hub_rules_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    *out_snapshot = (app_hub_rules_snapshot_t) {
        .generation = handle->rules_generation,
        .total = handle->rule_count,
        .ready = handle->rules_ready,
        .pending = handle->rule_pending,
    };
    strlcpy(out_snapshot->status, handle->rules_status,
            sizeof(out_snapshot->status));
    out_snapshot->page_count =
        (handle->rule_count + APP_HUB_RULES_PAGE_SIZE - 1) /
        APP_HUB_RULES_PAGE_SIZE;
    if (out_snapshot->page_count == 0) {
        out_snapshot->page_count = 1;
    }
    if (page >= out_snapshot->page_count) {
        page = out_snapshot->page_count - 1;
    }
    out_snapshot->page = page;
    const size_t start = page * APP_HUB_RULES_PAGE_SIZE;
    for (size_t i = 0;
            i < APP_HUB_RULES_PAGE_SIZE && start + i < handle->rule_count;
            ++i) {
        out_snapshot->rules[i] = handle->rules[start + i];
        ++out_snapshot->visible;
    }
    xSemaphoreGive(handle->mutex);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_rule_set_enabled(app_hub_handle_t handle,
                                          const char *rule_id, bool enabled)
{
    if (!handle || !rule_id || rule_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_RULE_SET_ENABLED,
        .enabled = enabled,
    };
    strlcpy(command.point_id, rule_id, sizeof(command.point_id));
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        handle->rule_pending = true;
        strlcpy(handle->rules_status, "Saving automation...",
                sizeof(handle->rules_status));
        ++handle->rules_generation;
        xSemaphoreGive(handle->mutex);
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_rule_delete(app_hub_handle_t handle,
                                     const char *rule_id)
{
    if (!handle || !rule_id || rule_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_RULE_DELETE,
    };
    strlcpy(command.point_id, rule_id, sizeof(command.point_id));
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        handle->rule_pending = true;
        strlcpy(handle->rules_status, "Deleting automation...",
                sizeof(handle->rules_status));
        ++handle->rules_generation;
        xSemaphoreGive(handle->mutex);
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_rule_create(app_hub_handle_t handle, const char *name,
                                     const char *source_point,
                                     const char *source_value,
                                     const char *target_point, bool target_on)
{
    if (!handle || !name || !name[0] || !source_point || !source_point[0] ||
            !source_value || !source_value[0] || !target_point ||
            !target_point[0]) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_RULE_CREATE,
        .enabled = target_on,
    };
    strlcpy(command.name, name, sizeof(command.name));
    strlcpy(command.point_id, source_point, sizeof(command.point_id));
    strlcpy(command.source_value, source_value, sizeof(command.source_value));
    strlcpy(command.target_point, target_point, sizeof(command.target_point));
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        handle->rule_pending = true;
        strlcpy(handle->rules_status, "Creating automation...",
                sizeof(handle->rules_status));
        ++handle->rules_generation;
        xSemaphoreGive(handle->mutex);
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_system_snapshot(
    app_hub_handle_t handle, app_hub_system_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(HUB_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return APP_HUB_ERR_BUSY;
    }
    *out_snapshot = handle->system;
    if (out_snapshot->ip[0] == '\0') {
        strlcpy(out_snapshot->ip, handle->local_ip, sizeof(out_snapshot->ip));
    }
    xSemaphoreGive(handle->mutex);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_system_reboot(app_hub_handle_t handle)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_SYSTEM_REBOOT,
    };
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_reconnect(app_hub_handle_t handle, const char *point_id)
{
    if (!handle || !point_id || point_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    hub_command_t command = {
        .kind = HUB_CMD_HUB_RELOAD,
    };
    strlcpy(command.point_id, point_id, sizeof(command.point_id));
    const app_hub_result_t queued = queue_command(handle, &command);
    if (queued != APP_HUB_OK) {
        return queued;
    }
    xTaskNotifyGive(handle->worker);
    return APP_HUB_OK;
}
