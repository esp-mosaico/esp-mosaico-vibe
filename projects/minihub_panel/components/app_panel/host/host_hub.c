// SPDX-License-Identifier: Apache-2.0

#include "app_hub.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "cJSON.h"

#define HOST_POINT_LIMIT 64
#define HOST_COMMAND_LIMIT 8
#define HOST_SCENE_LIMIT 8
#define HOST_RULE_LIMIT 16

typedef enum {
    HOST_CMD_DEVICE = 0,
    HOST_CMD_SCENE,
    HOST_CMD_RULE,
    HOST_CMD_ADD,
    HOST_CMD_ADD_CANCEL,
    HOST_CMD_SCENE_CREATE,
    HOST_CMD_SCENE_DELETE,
    HOST_CMD_RULE_CREATE,
    HOST_CMD_RULE_DELETE,
    HOST_CMD_REBOOT,
    HOST_CMD_RELOAD,
} host_command_kind_t;

typedef struct {
    char *data;
    size_t size;
} response_buffer_t;

typedef struct {
    host_command_kind_t kind;
    char point_id[APP_HUB_POINT_ID_LEN];
    char parameter[APP_HUB_PARAMETER_LEN];
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
} host_command_t;

struct app_hub {
    pthread_mutex_t lock;
    pthread_cond_t wake;
    pthread_t worker;
    CURL *curl;
    char base_url[160];
    char username[48];
    char password[96];
    char token[160];
    app_hub_point_t points[HOST_POINT_LIMIT];
    size_t point_count;
    size_t online_count;
    uint32_t generation;
    uint32_t poll_ms;
    bool connected;
    bool running;
    bool wifi_ready;
    bool wifi_needs_setup;
    bool wifi_scanning;
    bool wifi_connecting;
    char wifi_ssid[APP_HUB_SSID_LEN];
    size_t wifi_ap_count;
    app_hub_wifi_ap_t wifi_aps[APP_HUB_WIFI_AP_LIMIT];
    char status[APP_HUB_STATUS_LEN];
    app_hub_role_t role;
    char hub_url[APP_HUB_URL_LEN];
    host_command_t commands[HOST_COMMAND_LIMIT];
    size_t command_head;
    size_t command_count;
    app_hub_scene_t scenes[HOST_SCENE_LIMIT];
    size_t scene_count;
    uint32_t scenes_generation;
    char scenes_status[APP_HUB_STATUS_LEN];
    char *scenes_json;
    app_hub_rule_t rules[HOST_RULE_LIMIT];
    size_t rule_count;
    uint32_t rules_generation;
    char rules_status[APP_HUB_STATUS_LEN];
    char *rules_json;
    app_hub_add_snapshot_t add;
    app_hub_add_request_t active_add;
    char *add_adapters_backup;
    size_t add_baseline_points;
    time_t add_deadline;
    app_hub_system_snapshot_t system;
};

static void copy_string(char *destination, size_t size, const char *source)
{
    snprintf(destination, size, "%s", source != NULL ? source : "");
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

static size_t response_write(void *data, size_t size, size_t count, void *user)
{
    response_buffer_t *response = user;
    size_t incoming = size * count;
    char *grown = realloc(response->data, response->size + incoming + 1);
    if (grown == NULL) {
        return 0;
    }
    response->data = grown;
    memcpy(response->data + response->size, data, incoming);
    response->size += incoming;
    response->data[response->size] = '\0';
    return incoming;
}

static bool request(struct app_hub *hub, const char *path, const char *method,
                    const char *payload, long *status, char **response_body)
{
    char url[320];
    snprintf(url, sizeof(url), "%s%s", hub->base_url, path);
    response_buffer_t response = {0};
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    if (method != NULL && strcmp(method, "GET") != 0) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    if (hub->token[0] != '\0') {
        char authorization[256];
        snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s",
                 hub->token);
        headers = curl_slist_append(headers, authorization);
    }

    curl_easy_reset(hub->curl);
    curl_easy_setopt(hub->curl, CURLOPT_URL, url);
    curl_easy_setopt(hub->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(hub->curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(hub->curl, CURLOPT_CONNECTTIMEOUT_MS, 1200L);
    curl_easy_setopt(hub->curl, CURLOPT_TIMEOUT_MS, 2500L);
    curl_easy_setopt(hub->curl, CURLOPT_WRITEFUNCTION, response_write);
    curl_easy_setopt(hub->curl, CURLOPT_WRITEDATA, &response);
    if (method != NULL && strcmp(method, "GET") != 0) {
        curl_easy_setopt(hub->curl, CURLOPT_CUSTOMREQUEST, method);
        curl_easy_setopt(hub->curl, CURLOPT_POST, 1L);
        curl_easy_setopt(hub->curl, CURLOPT_POSTFIELDS, payload != NULL ? payload : "");
    }

    CURLcode result = curl_easy_perform(hub->curl);
    if (result == CURLE_OK) {
        curl_easy_getinfo(hub->curl, CURLINFO_RESPONSE_CODE, status);
    }
    curl_slist_free_all(headers);
    if (result != CURLE_OK) {
        free(response.data);
        return false;
    }
    *response_body = response.data;
    return true;
}

static bool login(struct app_hub *hub)
{
    if (hub->token[0] != '\0') {
        return true;
    }
    if (hub->password[0] == '\0') {
        return false;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "username", hub->username);
    cJSON_AddStringToObject(root, "password", hub->password);
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    long status = 0;
    char *body = NULL;
    bool ok = payload != NULL &&
              request(hub, "/api/auth/login", "POST", payload, &status, &body);
    free(payload);
    free(body);
    return ok && status >= 200 && status < 300;
}

static const cJSON *point_power_value(const cJSON *point)
{
    const cJSON *props = cJSON_GetObjectItemCaseSensitive(point, "props");
    const cJSON *params = cJSON_IsObject(props)
                          ? cJSON_GetObjectItemCaseSensitive(props, "params")
                          : NULL;
    const cJSON *power = cJSON_IsObject(params)
                         ? cJSON_GetObjectItemCaseSensitive(params, "power")
                         : NULL;
    if (cJSON_IsObject(power)) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(power, "value");
        return value != NULL ? value : power;
    }
    return power;
}

static bool point_param_int(const cJSON *point, const char *name,
                            int32_t *out_value)
{
    const cJSON *props = cJSON_GetObjectItemCaseSensitive(point, "props");
    const cJSON *params = cJSON_IsObject(props)
                          ? cJSON_GetObjectItemCaseSensitive(props, "params")
                          : NULL;
    const cJSON *param = cJSON_IsObject(params)
                         ? cJSON_GetObjectItemCaseSensitive(params, name)
                         : NULL;
    const cJSON *value = cJSON_IsObject(param)
                         ? cJSON_GetObjectItemCaseSensitive(param, "value")
                         : param;
    if (cJSON_IsNumber(value)) {
        *out_value = (int32_t)value->valuedouble;
        return true;
    }
    if (cJSON_IsString(value) && value->valuestring != NULL) {
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
    return cJSON_IsString(item) && item->valuestring != NULL &&
           (strcasecmp(item->valuestring, "on") == 0 ||
            strcasecmp(item->valuestring, "true") == 0 ||
            strcmp(item->valuestring, "1") == 0);
}

static void format_value(const cJSON *item, char *output, size_t output_size)
{
    if (item == NULL) {
        copy_string(output, output_size, "--");
    } else if (cJSON_IsString(item)) {
        copy_string(output, output_size, item->valuestring);
    } else if (cJSON_IsTrue(item)) {
        copy_string(output, output_size, "ON");
    } else if (cJSON_IsFalse(item)) {
        copy_string(output, output_size, "OFF");
    } else if (cJSON_IsNumber(item)) {
        snprintf(output, output_size, "%.2f", item->valuedouble);
    } else {
        copy_string(output, output_size, "available");
    }
}

static bool type_is_controllable(const char *type)
{
    return strcmp(type, "light") == 0 || strcmp(type, "switch") == 0 ||
           strcmp(type, "plug") == 0 || strcmp(type, "outlet") == 0;
}

static bool parse_points(struct app_hub *hub, const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return false;
    }

    app_hub_point_t parsed[HOST_POINT_LIMIT] = {0};
    size_t count = 0;
    size_t online_count = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (count == HOST_POINT_LIMIT || !cJSON_IsObject(item)) {
            break;
        }
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "point_id");
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(item, "type");
        if (!cJSON_IsString(id) || !cJSON_IsString(type)) {
            continue;
        }
        app_hub_point_t *point = &parsed[count++];
        copy_string(point->point_id, sizeof(point->point_id), id->valuestring);
        const cJSON *props = cJSON_GetObjectItemCaseSensitive(item, "props");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        if (!cJSON_IsString(name) && cJSON_IsObject(props)) {
            name = cJSON_GetObjectItemCaseSensitive(props, "name");
        }
        copy_string(point->name, sizeof(point->name),
                    cJSON_IsString(name) ? name->valuestring : id->valuestring);
        copy_string(point->type, sizeof(point->type), type->valuestring);
        const cJSON *online = cJSON_GetObjectItemCaseSensitive(item, "online");
        point->online = !cJSON_IsFalse(online);
        online_count += point->online ? 1U : 0U;
        const cJSON *power = point_power_value(item);
        const cJSON *value = power != NULL
                             ? power
                             : cJSON_GetObjectItemCaseSensitive(item, "value");
        point->on = json_is_on(value);
        point->controllable = type_is_controllable(point->type) || power != NULL;
        format_value(value, point->value, sizeof(point->value));
        point->has_brightness = point_param_int(item, "brightness",
                                                &point->brightness);
        point->has_hue = point_param_int(item, "hue", &point->hue);
        point->has_saturation = point_param_int(item, "saturation",
                                                &point->saturation);
        point->has_cct = point_param_int(item, "cct", &point->cct);
    }
    cJSON_Delete(root);

    pthread_mutex_lock(&hub->lock);
    memcpy(hub->points, parsed, sizeof(parsed));
    hub->point_count = count;
    hub->online_count = online_count;
    hub->connected = true;
    copy_string(hub->status, sizeof(hub->status), "MiniHub online");
    ++hub->generation;
    pthread_mutex_unlock(&hub->lock);
    return true;
}

static bool refresh_points(struct app_hub *hub)
{
    long status = 0;
    char *body = NULL;
    bool ok = request(hub, "/api/points", "GET", NULL, &status, &body);
    if (ok && status == 401 && login(hub)) {
        free(body);
        body = NULL;
        ok = request(hub, "/api/points", "GET", NULL, &status, &body);
    }
    bool parsed = ok && status >= 200 && status < 300 && body != NULL &&
                  parse_points(hub, body);
    free(body);
    if (!parsed) {
        pthread_mutex_lock(&hub->lock);
        hub->connected = false;
        copy_string(hub->status, sizeof(hub->status),
                    status == 401 ? "MiniHub login required" : "MiniHub unreachable");
        ++hub->generation;
        pthread_mutex_unlock(&hub->lock);
    }
    return parsed;
}

static const cJSON *host_document_array(const cJSON *root, const char *key)
{
    return cJSON_IsArray(root) ? root :
           (cJSON_IsObject(root) ?
            cJSON_GetObjectItemCaseSensitive(root, key) : NULL);
}

static void refresh_metadata(struct app_hub *hub)
{
    long status = 0;
    char *body = NULL;
    if (request(hub, "/api/scenes", "GET", NULL, &status, &body) &&
            status >= 200 && status < 300 && body) {
        cJSON *root = cJSON_Parse(body);
        const cJSON *array = host_document_array(root, "scenes");
        app_hub_scene_t parsed[HOST_SCENE_LIMIT] = {0};
        size_t count = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, array) {
            if (count >= HOST_SCENE_LIMIT) break;
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
            const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
            const cJSON *steps = cJSON_GetObjectItemCaseSensitive(item, "steps");
            if (!cJSON_IsString(id)) continue;
            copy_string(parsed[count].id, sizeof(parsed[count].id),
                        id->valuestring);
            copy_string(parsed[count].name, sizeof(parsed[count].name),
                        cJSON_IsString(name) ? name->valuestring : id->valuestring);
            parsed[count].step_count = cJSON_IsArray(steps) ?
                                       (uint8_t)cJSON_GetArraySize(steps) : 0;
            parsed[count].action_count = 0;
            if (cJSON_IsArray(steps) && cJSON_GetArraySize(steps) > 0) {
                const cJSON *step0 = cJSON_GetArrayItem(steps, 0);
                const cJSON *actions = cJSON_IsObject(step0) ?
                    cJSON_GetObjectItemCaseSensitive(step0, "actions") : NULL;
                const cJSON *action;
                if (cJSON_IsArray(actions)) cJSON_ArrayForEach(action, actions) {
                    if (parsed[count].action_count >= APP_HUB_PAGE_SIZE) {
                        break;
                    }
                    const cJSON *point_id =
                        cJSON_GetObjectItemCaseSensitive(action, "point_id");
                    const cJSON *cmd =
                        cJSON_GetObjectItemCaseSensitive(action, "cmd");
                    if (!cJSON_IsString(point_id) || !point_id->valuestring) {
                        continue;
                    }
                    uint8_t n = parsed[count].action_count;
                    copy_string(parsed[count].action_points[n],
                                sizeof(parsed[count].action_points[n]),
                                point_id->valuestring);
                    parsed[count].action_on[n] =
                        cJSON_IsString(cmd) && cmd->valuestring &&
                        strcmp(cmd->valuestring, "turn_on") == 0;
                    ++parsed[count].action_count;
                }
            }
            ++count;
        }
        pthread_mutex_lock(&hub->lock);
        free(hub->scenes_json);
        hub->scenes_json = strdup(body);
        memcpy(hub->scenes, parsed, sizeof(parsed));
        hub->scene_count = count;
        copy_string(hub->scenes_status, sizeof(hub->scenes_status),
                    count ? "Tap a scene to run" : "No scenes");
        ++hub->scenes_generation;
        pthread_mutex_unlock(&hub->lock);
        cJSON_Delete(root);
    }
    free(body);
    body = NULL;
    if (request(hub, "/api/rules", "GET", NULL, &status, &body) &&
            status >= 200 && status < 300 && body) {
        cJSON *root = cJSON_Parse(body);
        const cJSON *array = host_document_array(root, "rules");
        app_hub_rule_t parsed[HOST_RULE_LIMIT] = {0};
        size_t count = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, array) {
            if (count >= HOST_RULE_LIMIT) break;
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
            const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
            const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled");
            if (!cJSON_IsString(id)) continue;
            copy_string(parsed[count].id, sizeof(parsed[count].id),
                        id->valuestring);
            copy_string(parsed[count].name, sizeof(parsed[count].name),
                        cJSON_IsString(name) ? name->valuestring : id->valuestring);
            parsed[count].enabled = !cJSON_IsFalse(enabled);
            ++count;
        }
        pthread_mutex_lock(&hub->lock);
        memcpy(hub->rules, parsed, sizeof(parsed));
        hub->rule_count = count;
        free(hub->rules_json);
        hub->rules_json = strdup(body);
        copy_string(hub->rules_status, sizeof(hub->rules_status),
                    count ? "Tap to enable or disable" : "No automations");
        ++hub->rules_generation;
        pthread_mutex_unlock(&hub->lock);
        cJSON_Delete(root);
    }
    free(body);
    body = NULL;
    if (request(hub, "/api/system", "GET", NULL, &status, &body) &&
            status >= 200 && status < 300 && body) {
        cJSON *root = cJSON_Parse(body);
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
        const cJSON *uptime = cJSON_GetObjectItemCaseSensitive(root, "uptime_s");
        const cJSON *ip = cJSON_GetObjectItemCaseSensitive(root, "local_ip");
        const cJSON *ram = cJSON_GetObjectItemCaseSensitive(root, "ram");
        const cJSON *internal = cJSON_GetObjectItemCaseSensitive(ram, "internal_free");
        const cJSON *psram = cJSON_GetObjectItemCaseSensitive(ram, "psram_free");
        pthread_mutex_lock(&hub->lock);
        hub->system.ready = true;
        hub->system.hub_online = true;
        hub->system.uptime_s = cJSON_IsNumber(uptime) ?
                               (uint32_t)uptime->valuedouble : 0;
        hub->system.internal_free_kb = cJSON_IsNumber(internal) ?
                                       (uint32_t)internal->valuedouble / 1024U : 0;
        hub->system.psram_free_kb = cJSON_IsNumber(psram) ?
                                    (uint32_t)psram->valuedouble / 1024U : 0;
        copy_string(hub->system.version, sizeof(hub->system.version),
                    cJSON_IsString(version) ? version->valuestring : "--");
        copy_string(hub->system.ip, sizeof(hub->system.ip),
                    cJSON_IsString(ip) ? ip->valuestring : "--");
        copy_string(hub->system.status, sizeof(hub->system.status),
                    "MiniHub healthy");
        ++hub->system.generation;
        pthread_mutex_unlock(&hub->lock);
        cJSON_Delete(root);
    }
    free(body);
}

static void process_host_scene(struct app_hub *hub, const char *scene_id)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "scene_id", scene_id);
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    long status = 0;
    char *body = NULL;
    const bool ok = payload &&
                    request(hub, "/api/scenes/invoke", "POST", payload,
                            &status, &body) &&
                    status >= 200 && status < 300;
    free(payload);
    free(body);
    pthread_mutex_lock(&hub->lock);
    copy_string(hub->scenes_status, sizeof(hub->scenes_status),
                ok ? "Scene started" : "Scene failed");
    ++hub->scenes_generation;
    pthread_mutex_unlock(&hub->lock);
}

static void process_host_rule(struct app_hub *hub, const char *rule_id,
                              bool enabled)
{
    pthread_mutex_lock(&hub->lock);
    char *document = hub->rules_json ? strdup(hub->rules_json) : NULL;
    pthread_mutex_unlock(&hub->lock);
    cJSON *root = document ? cJSON_Parse(document) : NULL;
    free(document);
    const cJSON *array = host_document_array(root, "rules");
    cJSON *item;
    cJSON_ArrayForEach(item, array) {
        cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (cJSON_IsString(id) && strcmp(id->valuestring, rule_id) == 0) {
            cJSON_ReplaceItemInObjectCaseSensitive(
                item, "enabled", cJSON_CreateBool(enabled));
            break;
        }
    }
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    long status = 0;
    char *body = NULL;
    if (payload) {
        (void)request(hub, "/api/rules/import", "POST", payload, &status, &body);
    }
    free(payload);
    free(body);
    refresh_metadata(hub);
}

static void control_point(struct app_hub *hub, const char *point_id,
                          const char *parameter, int32_t value,
                          bool has_value)
{
    app_hub_point_t point = {0};
    pthread_mutex_lock(&hub->lock);
    for (size_t i = 0; i < hub->point_count; ++i) {
        if (strcmp(hub->points[i].point_id, point_id) == 0) {
            point = hub->points[i];
            break;
        }
    }
    pthread_mutex_unlock(&hub->lock);
    if (point.point_id[0] == '\0' || !point.controllable) {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    const char *endpoint = "/api/devices/control";
    if (has_value) {
        cJSON_AddStringToObject(root, "device", point.type);
        cJSON_AddStringToObject(root, "parameter", parameter);
        cJSON_AddNumberToObject(root, "value", value);
        cJSON_AddStringToObject(root, "point_id", point.point_id);
    } else {
        endpoint = "/api/command";
        cJSON_AddStringToObject(root, "point_id", point.point_id);
        cJSON_AddStringToObject(root, "cmd", point.on ? "turn_off" : "turn_on");
    }
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    long status = 0;
    char *body = NULL;
    if (payload != NULL) {
        request(hub, endpoint, "POST", payload, &status, &body);
    }
    free(payload);
    free(body);
}

static cJSON *host_adapter_slot(cJSON *root, const char *name)
{
    cJSON *adapters = cJSON_GetObjectItemCaseSensitive(root, "adapters");
    if (!cJSON_IsObject(adapters)) {
        adapters = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "adapters", adapters);
    }
    cJSON *slot = cJSON_GetObjectItemCaseSensitive(adapters, name);
    if (!cJSON_IsObject(slot)) {
        slot = cJSON_CreateObject();
        cJSON_AddItemToObject(adapters, name, slot);
    }
    return slot;
}

static void host_set_bool(cJSON *object, const char *key, bool value)
{
    cJSON *item = cJSON_CreateBool(value);
    if (!cJSON_ReplaceItemInObjectCaseSensitive(object, key, item)) {
        cJSON_AddItemToObject(object, key, item);
    }
}

static void host_set_string(cJSON *object, const char *key, const char *value)
{
    cJSON *item = cJSON_CreateString(value ? value : "");
    if (!cJSON_ReplaceItemInObjectCaseSensitive(object, key, item)) {
        cJSON_AddItemToObject(object, key, item);
    }
}

static void host_set_number(cJSON *object, const char *key, double value)
{
    cJSON *item = cJSON_CreateNumber(value);
    if (!cJSON_ReplaceItemInObjectCaseSensitive(object, key, item)) {
        cJSON_AddItemToObject(object, key, item);
    }
}

static cJSON *host_ensure_array(cJSON *object, const char *key)
{
    cJSON *array = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsArray(array)) {
        array = cJSON_CreateArray();
        cJSON_AddItemToObject(object, key, array);
    }
    return array;
}

static void host_upsert(cJSON *array, const char *key, const char *value,
                        cJSON *replacement)
{
    int index = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, array) {
        const cJSON *candidate = cJSON_GetObjectItemCaseSensitive(item, key);
        if (cJSON_IsString(candidate) &&
                strcmp(candidate->valuestring, value) == 0) {
            cJSON_ReplaceItemInArray(array, index, replacement);
            return;
        }
        ++index;
    }
    cJSON_AddItemToArray(array, replacement);
}

static bool host_save_config(struct app_hub *hub, cJSON *root)
{
    host_set_bool(root, "reload", true);
    char *payload = cJSON_PrintUnformatted(root);
    long status = 0;
    char *body = NULL;
    const bool ok = payload &&
                    request(hub, "/api/config/adapters", "PUT", payload,
                            &status, &body) &&
                    status >= 200 && status < 300;
    free(payload);
    free(body);
    return ok;
}

static void host_restore_add_config(struct app_hub *hub)
{
    pthread_mutex_lock(&hub->lock);
    char *backup = hub->add_adapters_backup;
    hub->add_adapters_backup = NULL;
    pthread_mutex_unlock(&hub->lock);
    if (!backup) {
        return;
    }
    cJSON *root = cJSON_Parse(backup);
    free(backup);
    if (root) {
        (void)host_save_config(hub, root);
        cJSON_Delete(root);
    }
}

static bool host_wait_radio_ready(struct app_hub *hub,
                                  app_hub_protocol_t protocol)
{
    for (int attempt = 0; attempt < 20; ++attempt) {
        long status = 0;
        char *body = NULL;
        const bool requested = request(
            hub, "/api/radio/provision", "GET", NULL, &status, &body);
        cJSON *root = requested && status == 200 && body ?
                      cJSON_Parse(body) : NULL;
        free(body);
        if (root) {
            const cJSON *busy = cJSON_GetObjectItemCaseSensitive(root, "busy");
            const cJSON *available = cJSON_GetObjectItemCaseSensitive(
                root, protocol == APP_HUB_PROTOCOL_ZIGBEE ?
                "zigbee_available" : "ble_mesh_available");
            const cJSON *running = cJSON_GetObjectItemCaseSensitive(
                root, "ble_mesh_running");
            const bool ready = !cJSON_IsTrue(busy) &&
                               cJSON_IsTrue(available) &&
                               (protocol != APP_HUB_PROTOCOL_BLE_MESH ||
                                cJSON_IsTrue(running));
            cJSON_Delete(root);
            if (ready) {
                return true;
            }
        } else {
            cJSON_Delete(root);
        }
        const struct timespec pause = {
            .tv_sec = 0,
            .tv_nsec = 250000000L,
        };
        nanosleep(&pause, NULL);
    }
    return false;
}

static void process_host_add(struct app_hub *hub,
                             const app_hub_add_request_t *add)
{
    pthread_mutex_lock(&hub->lock);
    free(hub->add_adapters_backup);
    hub->add_adapters_backup = NULL;
    hub->active_add = *add;
    hub->add_baseline_points = hub->point_count;
    hub->add.state = APP_HUB_ADD_PREPARING;
    hub->add.progress = 10;
    copy_string(hub->add.status, sizeof(hub->add.status), "Preparing adapter");
    ++hub->add.generation;
    pthread_mutex_unlock(&hub->lock);

    bool ok = false;
    long status = 0;
    char *body = NULL;
    if (add->protocol == APP_HUB_PROTOCOL_MATTER) {
        cJSON *payload_root = cJSON_CreateObject();
        cJSON_AddStringToObject(payload_root, "pairing_code", add->pairing_code);
        cJSON_AddBoolToObject(payload_root, "merge", true);
        cJSON_AddBoolToObject(payload_root, "reload", true);
        char *payload = cJSON_PrintUnformatted(payload_root);
        cJSON_Delete(payload_root);
        ok = payload && request(hub, "/api/matter/devices", "POST", payload,
                                &status, &body) &&
             status >= 200 && status < 300;
        free(payload);
        free(body);
    } else {
        if (!request(hub, "/api/config/adapters", "GET", NULL, &status, &body) ||
                status != 200) {
            free(body);
            body = NULL;
        }
        if (body) {
            pthread_mutex_lock(&hub->lock);
            hub->add_adapters_backup = strdup(body);
            pthread_mutex_unlock(&hub->lock);
        }
        cJSON *root = body ? cJSON_Parse(body) : NULL;
        free(body);
        body = NULL;
        if (root) {
            if (add->protocol == APP_HUB_PROTOCOL_ZIGBEE ||
                    add->protocol == APP_HUB_PROTOCOL_BLE_MESH) {
                cJSON *zigbee = host_adapter_slot(root, "zigbee");
                cJSON *mesh = host_adapter_slot(root, "ble_mesh");
                cJSON *thread = host_adapter_slot(root, "thread");
                const bool zigbee_on =
                    add->protocol == APP_HUB_PROTOCOL_ZIGBEE;
                host_set_bool(zigbee, "enabled", zigbee_on);
                host_set_bool(mesh, "enabled", !zigbee_on);
                host_set_bool(thread, "enabled", false);
            } else if (add->protocol == APP_HUB_PROTOCOL_MQTT) {
                cJSON *slot = host_adapter_slot(root, "mqtt");
                cJSON *devices = host_ensure_array(slot, "devices");
                cJSON *device = cJSON_CreateObject();
                host_set_bool(slot, "enabled", true);
                host_set_string(slot, "broker", add->host);
                host_set_number(slot, "port", add->port ? add->port : 1883);
                host_set_string(slot, "username", add->username);
                host_set_string(slot, "password", add->password);
                host_set_string(device, "point_id", add->point_id);
                host_set_string(device, "type",
                                add->type[0] ? add->type : "switch");
                host_set_string(device, "state_topic", add->state_topic);
                host_set_string(device, "command_topic", add->command_topic);
                host_upsert(devices, "point_id", add->point_id, device);
            } else if (add->protocol == APP_HUB_PROTOCOL_MODBUS_TCP) {
                cJSON *slot = host_adapter_slot(root, "modbus");
                cJSON *gateways = host_ensure_array(slot, "gateways");
                cJSON *gateway = cJSON_CreateObject();
                cJSON *maps = cJSON_CreateArray();
                cJSON *map = cJSON_CreateObject();
                host_set_bool(slot, "enabled", true);
                host_set_number(slot, "poll_ms", 2000);
                host_set_string(gateway, "id", add->point_id);
                host_set_string(gateway, "transport", "tcp");
                host_set_string(gateway, "host", add->host);
                host_set_number(gateway, "port", add->port ? add->port : 502);
                host_set_number(gateway, "unit_id",
                                add->unit_id ? add->unit_id : 1);
                host_set_string(map, "point_id", add->point_id);
                host_set_string(map, "type",
                                add->type[0] ? add->type : "sensor");
                host_set_number(map, "unit_id",
                                add->unit_id ? add->unit_id : 1);
                host_set_string(map, "fc",
                                add->function[0] ? add->function : "holding");
                host_set_number(map, "address", add->address);
                host_set_string(map, "datatype",
                                add->datatype[0] ? add->datatype : "u16");
                host_set_number(map, "scale",
                                add->scale == 0.0f ? 1.0f : add->scale);
                host_set_bool(map, "writable", add->writable);
                cJSON_AddItemToArray(maps, map);
                cJSON_AddItemToObject(gateway, "maps", maps);
                host_upsert(gateways, "id", add->point_id, gateway);
            }
            ok = host_save_config(hub, root);
            cJSON_Delete(root);
        }
        if (ok && (add->protocol == APP_HUB_PROTOCOL_ZIGBEE ||
                   add->protocol == APP_HUB_PROTOCOL_BLE_MESH)) {
            ok = host_wait_radio_ready(hub, add->protocol);
            if (add->protocol == APP_HUB_PROTOCOL_ZIGBEE &&
                    ok &&
                    add->ieee[0] && add->install_code[0]) {
                cJSON *install = cJSON_CreateObject();
                cJSON_AddStringToObject(install, "ieee", add->ieee);
                cJSON_AddStringToObject(install, "install_code",
                                        add->install_code);
                char *install_payload = cJSON_PrintUnformatted(install);
                cJSON_Delete(install);
                ok = install_payload && request(
                    hub, "/api/zigbee/install_code", "POST",
                    install_payload, &status, &body) &&
                    status >= 200 && status < 300;
                free(install_payload);
                free(body);
                body = NULL;
            }
            if (ok) {
                cJSON *permit = cJSON_CreateObject();
                cJSON_AddNumberToObject(permit, "seconds", 120);
                char *payload = cJSON_PrintUnformatted(permit);
                cJSON_Delete(permit);
                ok = payload && request(
                    hub, add->protocol == APP_HUB_PROTOCOL_ZIGBEE ?
                    "/api/zigbee/permit_join" :
                    "/api/ble_mesh/permit_provision",
                    "POST", payload, &status, &body) &&
                    status >= 200 && status < 300;
                free(payload);
                free(body);
            }
        }
    }
    if (!ok) {
        host_restore_add_config(hub);
    }
    pthread_mutex_lock(&hub->lock);
    hub->add.state = ok ? APP_HUB_ADD_WAITING : APP_HUB_ADD_FAILED;
    hub->add.progress = ok ? 35 : 0;
    hub->add_deadline = time(NULL) + 120;
    hub->add.point_id[0] = '\0';
    copy_string(hub->add.status, sizeof(hub->add.status),
                ok ? "Waiting for device" : "Add failed");
    ++hub->add.generation;
    pthread_mutex_unlock(&hub->lock);
}

static void update_host_add(struct app_hub *hub)
{
    app_hub_add_request_t add;
    char point_id[APP_HUB_POINT_ID_LEN] = {0};
    bool timed_out = false;
    pthread_mutex_lock(&hub->lock);
    if (hub->add.state != APP_HUB_ADD_WAITING) {
        pthread_mutex_unlock(&hub->lock);
        return;
    }
    add = hub->active_add;
    if ((add.protocol == APP_HUB_PROTOCOL_MQTT ||
            add.protocol == APP_HUB_PROTOCOL_MODBUS_TCP) &&
            add.point_id[0]) {
        for (size_t i = 0; i < hub->point_count; ++i) {
            if (strcmp(hub->points[i].point_id, add.point_id) == 0) {
                copy_string(point_id, sizeof(point_id),
                            hub->points[i].point_id);
                break;
            }
        }
    } else if (hub->point_count > hub->add_baseline_points) {
        copy_string(point_id, sizeof(point_id),
                    hub->points[hub->point_count - 1].point_id);
    }
    timed_out = time(NULL) >= hub->add_deadline;
    pthread_mutex_unlock(&hub->lock);

    bool renamed = true;
    if (point_id[0] && add.name[0]) {
        char display_name[49];
        format_display_name(display_name, sizeof(display_name),
                            add.room, add.name);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "point_id", point_id);
        cJSON_AddStringToObject(root, "name", display_name);
        char *payload = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        long status = 0;
        char *body = NULL;
        renamed = payload && request(hub, "/api/points/rename", "POST",
                                     payload, &status, &body) &&
                  status >= 200 && status < 300;
        free(payload);
        free(body);
    }
    if (!point_id[0] && !timed_out) {
        return;
    }
    if (timed_out && !point_id[0]) {
        host_restore_add_config(hub);
    } else {
        pthread_mutex_lock(&hub->lock);
        free(hub->add_adapters_backup);
        hub->add_adapters_backup = NULL;
        pthread_mutex_unlock(&hub->lock);
    }
    pthread_mutex_lock(&hub->lock);
    hub->add.state = point_id[0] ? APP_HUB_ADD_OK : APP_HUB_ADD_FAILED;
    hub->add.progress = point_id[0] ? 100 : 0;
    copy_string(hub->add.point_id, sizeof(hub->add.point_id), point_id);
    copy_string(hub->add.status, sizeof(hub->add.status),
                point_id[0] ?
                (renamed ? "Device added" : "Added; rename failed") :
                "Device add timed out");
    ++hub->add.generation;
    pthread_mutex_unlock(&hub->lock);
}

static void process_host_add_cancel(struct app_hub *hub)
{
    pthread_mutex_lock(&hub->lock);
    const app_hub_protocol_t protocol = hub->active_add.protocol;
    pthread_mutex_unlock(&hub->lock);
    const char *path = NULL;
    const char *payload = "{\"seconds\":0}";
    if (protocol == APP_HUB_PROTOCOL_MATTER) {
        path = "/api/matter/devices";
        payload = "{\"cancel\":true,\"node_id\":0}";
    } else if (protocol == APP_HUB_PROTOCOL_ZIGBEE) {
        path = "/api/zigbee/permit_join";
    } else if (protocol == APP_HUB_PROTOCOL_BLE_MESH) {
        path = "/api/ble_mesh/permit_provision";
    }
    if (path) {
        long status = 0;
        char *body = NULL;
        (void)request(hub, path, "POST", payload, &status, &body);
        free(body);
    }
    host_restore_add_config(hub);
    pthread_mutex_lock(&hub->lock);
    hub->add.state = APP_HUB_ADD_CANCELLED;
    hub->add.progress = 0;
    copy_string(hub->add.status, sizeof(hub->add.status), "Add cancelled");
    ++hub->add.generation;
    pthread_mutex_unlock(&hub->lock);
}

static void process_host_scene_create(struct app_hub *hub,
                                      const host_command_t *command)
{
    pthread_mutex_lock(&hub->lock);
    char *document = hub->scenes_json ? strdup(hub->scenes_json) : NULL;
    pthread_mutex_unlock(&hub->lock);
    cJSON *root = document ? cJSON_Parse(document) : cJSON_CreateObject();
    free(document);
    cJSON *array = root ? cJSON_GetObjectItemCaseSensitive(root, "scenes") : NULL;
    char id[APP_HUB_SCENE_ID_LEN] = {0};
    if (root && !cJSON_IsArray(array)) {
        array = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "scenes", array);
    }
    if (array) {
        cJSON *scene = cJSON_CreateObject();
        cJSON *steps = cJSON_CreateArray();
        cJSON *step = cJSON_CreateObject();
        cJSON *actions = cJSON_CreateArray();
        snprintf(id, sizeof(id), "panel_%08lx", (unsigned long)time(NULL));
        cJSON_AddStringToObject(scene, "id", id);
        cJSON_AddStringToObject(scene, "name", command->name);
        for (size_t i = 0; i < command->scene_count; ++i) {
            cJSON *action = cJSON_CreateObject();
            cJSON_AddStringToObject(action, "point_id",
                                    command->scene_points[i]);
            cJSON_AddStringToObject(action, "cmd",
                                    command->scene_values[i] ?
                                    "turn_on" : "turn_off");
            cJSON_AddItemToArray(actions, action);
        }
        cJSON_AddItemToObject(step, "actions", actions);
        cJSON_AddItemToArray(steps, step);
        cJSON_AddItemToObject(scene, "steps", steps);
        cJSON_AddItemToArray(array, scene);
    }
    char *payload = root ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    long status = 0;
    char *body = NULL;
    const bool imported = payload &&
                          request(hub, "/api/scenes/import", "POST", payload,
                                  &status, &body) &&
                          status >= 200 && status < 300;
    free(payload);
    free(body);
    if (imported && id[0]) {
        process_host_scene(hub, id);
    }
    refresh_metadata(hub);
}

static void process_host_scene_delete(struct app_hub *hub, const char *scene_id)
{
    pthread_mutex_lock(&hub->lock);
    char *document = hub->scenes_json ? strdup(hub->scenes_json) : NULL;
    pthread_mutex_unlock(&hub->lock);
    cJSON *root = document ? cJSON_Parse(document) : NULL;
    free(document);
    cJSON *array = root ? cJSON_GetObjectItemCaseSensitive(root, "scenes") : NULL;
    bool removed = false;
    if (cJSON_IsArray(array)) {
        int index = 0;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, array) {
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
            if (cJSON_IsString(id) && id->valuestring &&
                    strcmp(id->valuestring, scene_id) == 0) {
                cJSON_DeleteItemFromArray(array, index);
                removed = true;
                break;
            }
            ++index;
        }
    }
    char *payload = (root && removed) ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    long status = 0;
    char *body = NULL;
    if (payload) {
        (void)request(hub, "/api/scenes/import", "POST", payload, &status, &body);
    }
    free(payload);
    free(body);
    pthread_mutex_lock(&hub->lock);
    copy_string(hub->scenes_status, sizeof(hub->scenes_status),
                removed && status >= 200 && status < 300 ?
                "Scene deleted" : "Scene delete failed");
    ++hub->scenes_generation;
    pthread_mutex_unlock(&hub->lock);
    refresh_metadata(hub);
}

static void process_host_rule_delete(struct app_hub *hub, const char *rule_id)
{
    pthread_mutex_lock(&hub->lock);
    char *document = hub->rules_json ? strdup(hub->rules_json) : NULL;
    pthread_mutex_unlock(&hub->lock);
    cJSON *root = document ? cJSON_Parse(document) : NULL;
    free(document);
    cJSON *array = root ? cJSON_GetObjectItemCaseSensitive(root, "rules") : NULL;
    bool removed = false;
    if (cJSON_IsArray(array)) {
        int index = 0;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, array) {
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
            if (cJSON_IsString(id) && id->valuestring &&
                    strcmp(id->valuestring, rule_id) == 0) {
                cJSON_DeleteItemFromArray(array, index);
                removed = true;
                break;
            }
            ++index;
        }
    }
    char *payload = (root && removed) ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    long status = 0;
    char *body = NULL;
    if (payload) {
        (void)request(hub, "/api/rules/import", "POST", payload, &status, &body);
    }
    free(payload);
    free(body);
    pthread_mutex_lock(&hub->lock);
    copy_string(hub->rules_status, sizeof(hub->rules_status),
                removed && status >= 200 && status < 300 ?
                "Automation deleted" : "Automation delete failed");
    ++hub->rules_generation;
    pthread_mutex_unlock(&hub->lock);
    refresh_metadata(hub);
}

static void process_host_rule_create(struct app_hub *hub,
                                     const host_command_t *command)
{
    pthread_mutex_lock(&hub->lock);
    char *document = hub->rules_json ? strdup(hub->rules_json) : NULL;
    pthread_mutex_unlock(&hub->lock);
    cJSON *root = document ? cJSON_Parse(document) : cJSON_CreateObject();
    free(document);
    cJSON *array = root ? cJSON_GetObjectItemCaseSensitive(root, "rules") : NULL;
    if (root && !cJSON_IsArray(array)) {
        array = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "rules", array);
    }
    if (array) {
        cJSON *rule = cJSON_CreateObject();
        cJSON *when = cJSON_CreateObject();
        cJSON *then = cJSON_CreateArray();
        cJSON *action = cJSON_CreateObject();
        char id[APP_HUB_RULE_ID_LEN];
        snprintf(id, sizeof(id), "panel_%08lx", (unsigned long)time(NULL));
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
    }
    char *payload = root ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    long status = 0;
    char *body = NULL;
    if (payload) {
        (void)request(hub, "/api/rules/import", "POST", payload,
                      &status, &body);
    }
    free(payload);
    free(body);
    refresh_metadata(hub);
}

static void *worker_main(void *argument)
{
    struct app_hub *hub = argument;
    unsigned metadata_cycle = 0;
    while (true) {
        host_command_t command = {0};
        bool has_command = false;
        pthread_mutex_lock(&hub->lock);
        if (!hub->running) {
            pthread_mutex_unlock(&hub->lock);
            break;
        }
        if (hub->command_count > 0) {
            command = hub->commands[hub->command_head];
            hub->command_head = (hub->command_head + 1) % HOST_COMMAND_LIMIT;
            --hub->command_count;
            has_command = true;
        }
        pthread_mutex_unlock(&hub->lock);

        if (has_command && command.kind == HOST_CMD_DEVICE) {
            control_point(hub, command.point_id, command.parameter,
                          command.value, command.has_value);
        } else if (has_command && command.kind == HOST_CMD_SCENE) {
            process_host_scene(hub, command.point_id);
        } else if (has_command && command.kind == HOST_CMD_RULE) {
            process_host_rule(hub, command.point_id, command.enabled);
        } else if (has_command && command.kind == HOST_CMD_ADD) {
            process_host_add(hub, &command.add);
        } else if (has_command && command.kind == HOST_CMD_ADD_CANCEL) {
            process_host_add_cancel(hub);
        } else if (has_command && command.kind == HOST_CMD_SCENE_CREATE) {
            process_host_scene_create(hub, &command);
        } else if (has_command && command.kind == HOST_CMD_SCENE_DELETE) {
            process_host_scene_delete(hub, command.point_id);
        } else if (has_command && command.kind == HOST_CMD_RULE_CREATE) {
            process_host_rule_create(hub, &command);
        } else if (has_command && command.kind == HOST_CMD_RULE_DELETE) {
            process_host_rule_delete(hub, command.point_id);
        } else if (has_command && command.kind == HOST_CMD_REBOOT) {
            long status = 0;
            char *body = NULL;
            (void)request(hub, "/api/system/reboot", "POST",
                          "{\"confirm\":true}", &status, &body);
            free(body);
        } else if (has_command && command.kind == HOST_CMD_RELOAD) {
            long status = 0;
            char *body = NULL;
            cJSON *root = cJSON_CreateObject();
            char *payload = NULL;
            if (root) {
                host_set_string(root, "point_id", command.point_id);
                host_set_string(root, "cmd", "reconnect");
                payload = cJSON_PrintUnformatted(root);
                cJSON_Delete(root);
            }
            if (payload) {
                (void)request(hub, "/api/command", "POST", payload,
                              &status, &body);
                free(payload);
            }
            free(body);
        }
        refresh_points(hub);
        update_host_add(hub);
        if (metadata_cycle++ == 0) {
            refresh_metadata(hub);
        }
        if (metadata_cycle >= 50) {
            metadata_cycle = 0;
        }

        struct timespec until;
        clock_gettime(CLOCK_REALTIME, &until);
        until.tv_sec += hub->poll_ms / 1000;
        until.tv_nsec += (long)(hub->poll_ms % 1000) * 1000000L;
        if (until.tv_nsec >= 1000000000L) {
            ++until.tv_sec;
            until.tv_nsec -= 1000000000L;
        }
        pthread_mutex_lock(&hub->lock);
        if (hub->running && hub->command_count == 0) {
            pthread_cond_timedwait(&hub->wake, &hub->lock, &until);
        }
        pthread_mutex_unlock(&hub->lock);
    }
    return NULL;
}

app_hub_result_t app_hub_start(app_hub_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    struct app_hub *hub = calloc(1, sizeof(*hub));
    if (hub == NULL) {
        return APP_HUB_ERR_NO_MEMORY;
    }
    pthread_mutex_init(&hub->lock, NULL);
    pthread_cond_init(&hub->wake, NULL);
    const char *url = getenv("MINIHUB_URL");
    const char *username = getenv("MINIHUB_USERNAME");
    const char *poll_ms = getenv("MINIHUB_POLL_MS");
    copy_string(hub->base_url, sizeof(hub->base_url),
                url != NULL ? url : "http://192.168.1.103");
    copy_string(hub->username, sizeof(hub->username),
                username != NULL ? username : "admin");
    copy_string(hub->password, sizeof(hub->password), getenv("MINIHUB_PASSWORD"));
    copy_string(hub->token, sizeof(hub->token), getenv("MINIHUB_TOKEN"));
    hub->poll_ms = poll_ms != NULL ? (uint32_t)strtoul(poll_ms, NULL, 10) : 100;
    if (hub->poll_ms < 50 || hub->poll_ms > 5000) {
        hub->poll_ms = 100;
    }
    hub->wifi_ready = true;
    hub->add.capabilities = APP_HUB_CAP_MATTER | APP_HUB_CAP_ZIGBEE |
                            APP_HUB_CAP_BLE_MESH | APP_HUB_CAP_MQTT |
                            APP_HUB_CAP_MODBUS_TCP;
    hub->add.state = APP_HUB_ADD_IDLE;
    hub->add.generation = 1;
    copy_string(hub->add.status, sizeof(hub->add.status), "Choose a protocol");
    copy_string(hub->wifi_ssid, sizeof(hub->wifi_ssid), "Simulator LAN");
    copy_string(hub->status, sizeof(hub->status), "Connecting to MiniHub");

    curl_global_init(CURL_GLOBAL_DEFAULT);
    hub->curl = curl_easy_init();
    hub->running = hub->curl != NULL;
    if (!hub->running || pthread_create(&hub->worker, NULL, worker_main, hub) != 0) {
        free(hub);
        return APP_HUB_ERR_INTERNAL;
    }
    *out_handle = hub;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_snapshot(app_hub_handle_t hub, size_t page,
                                  app_hub_snapshot_t *snapshot)
{
    if (hub == NULL || snapshot == NULL) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->generation = hub->generation;
    snapshot->total_points = hub->point_count;
    snapshot->online_points = hub->online_count;
    snapshot->page_count =
        (hub->point_count + 1 + APP_HUB_PAGE_SIZE - 1) / APP_HUB_PAGE_SIZE;
    if (snapshot->page_count == 0) {
        snapshot->page_count = 1;
    }
    snapshot->page = page < snapshot->page_count ? page : snapshot->page_count - 1;
    snapshot->connected = hub->connected;
    snapshot->wifi_ready = hub->wifi_ready;
    snapshot->wifi_needs_setup = hub->wifi_needs_setup;
    snapshot->role = hub->role;
    snapshot->role_needed = hub->wifi_ready && hub->role == APP_HUB_ROLE_UNSET;
    snapshot->url_needed = hub->wifi_ready &&
                           hub->role == APP_HUB_ROLE_WEB &&
                           hub->hub_url[0] == '\0';
    snapshot->auth_needed = hub->wifi_ready &&
                            hub->role == APP_HUB_ROLE_WEB &&
                            hub->hub_url[0] != '\0' &&
                            !hub->connected;
    copy_string(snapshot->status, sizeof(snapshot->status), hub->status);
    const size_t start = snapshot->page * APP_HUB_PAGE_SIZE;
    for (size_t i = 0; i < APP_HUB_PAGE_SIZE && start + i < hub->point_count; ++i) {
        snapshot->points[i] = hub->points[start + i];
        ++snapshot->visible_points;
    }
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_toggle(app_hub_handle_t hub, const char *point_id)
{
    if (hub == NULL || point_id == NULL || point_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    if (hub->command_count == HOST_COMMAND_LIMIT) {
        pthread_mutex_unlock(&hub->lock);
        return APP_HUB_ERR_BUSY;
    }
    const size_t tail =
        (hub->command_head + hub->command_count) % HOST_COMMAND_LIMIT;
    host_command_t *command = &hub->commands[tail];
    memset(command, 0, sizeof(*command));
    copy_string(command->point_id, sizeof(command->point_id), point_id);
    ++hub->command_count;
    pthread_cond_signal(&hub->wake);
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_parameter(app_hub_handle_t hub,
                                       const char *point_id,
                                       const char *parameter, int32_t value)
{
    if (hub == NULL || point_id == NULL || point_id[0] == '\0' ||
            parameter == NULL || parameter[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    if (hub->command_count == HOST_COMMAND_LIMIT) {
        pthread_mutex_unlock(&hub->lock);
        return APP_HUB_ERR_BUSY;
    }
    const size_t tail =
        (hub->command_head + hub->command_count) % HOST_COMMAND_LIMIT;
    host_command_t *command = &hub->commands[tail];
    memset(command, 0, sizeof(*command));
    copy_string(command->point_id, sizeof(command->point_id), point_id);
    copy_string(command->parameter, sizeof(command->parameter),
                parameter);
    command->value = value;
    command->has_value = true;
    ++hub->command_count;
    pthread_cond_signal(&hub->wake);
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_refresh(app_hub_handle_t hub)
{
    if (hub == NULL) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    pthread_cond_signal(&hub->wake);
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

static void host_fill_scan(struct app_hub *hub)
{
    static const app_hub_wifi_ap_t samples[] = {
        { .ssid = "Office", .rssi = -48, .secured = true },
        { .ssid = "Home", .rssi = -55, .secured = true },
        { .ssid = "MiniHub-Lab", .rssi = -62, .secured = true },
        { .ssid = "Guest", .rssi = -70, .secured = false },
    };
    hub->wifi_ap_count = sizeof(samples) / sizeof(samples[0]);
    memcpy(hub->wifi_aps, samples, sizeof(samples));
    hub->wifi_scanning = false;
    ++hub->generation;
}

app_hub_result_t app_hub_wifi_snapshot(app_hub_handle_t hub, size_t page,
                                       app_hub_wifi_snapshot_t *snapshot)
{
    if (hub == NULL || snapshot == NULL) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->generation = hub->generation;
    snapshot->ready = hub->wifi_ready;
    snapshot->scanning = hub->wifi_scanning;
    snapshot->connecting = hub->wifi_connecting;
    snapshot->needs_setup = hub->wifi_needs_setup;
    snapshot->total_aps = hub->wifi_ap_count;
    copy_string(snapshot->ssid, sizeof(snapshot->ssid), hub->wifi_ssid);
    copy_string(snapshot->status, sizeof(snapshot->status), hub->status);
    snapshot->page_count =
        (hub->wifi_ap_count + APP_HUB_WIFI_PAGE_SIZE - 1) / APP_HUB_WIFI_PAGE_SIZE;
    if (snapshot->page_count == 0) {
        snapshot->page_count = 1;
    }
    snapshot->page = page < snapshot->page_count ? page : snapshot->page_count - 1;
    const size_t start = snapshot->page * APP_HUB_WIFI_PAGE_SIZE;
    for (size_t i = 0; i < APP_HUB_WIFI_PAGE_SIZE && start + i < hub->wifi_ap_count; ++i) {
        snapshot->aps[i] = hub->wifi_aps[start + i];
        ++snapshot->visible_aps;
    }
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_wifi_scan(app_hub_handle_t hub)
{
    if (hub == NULL) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    host_fill_scan(hub);
    copy_string(hub->status, sizeof(hub->status), "Select a network");
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_wifi_connect(app_hub_handle_t hub, const char *ssid,
                                      const char *password)
{
    if (hub == NULL || ssid == NULL || ssid[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    (void)password;
    pthread_mutex_lock(&hub->lock);
    copy_string(hub->wifi_ssid, sizeof(hub->wifi_ssid), ssid);
    hub->wifi_ready = true;
    hub->wifi_needs_setup = false;
    copy_string(hub->status, sizeof(hub->status), "Wi-Fi connected");
    ++hub->generation;
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_token(app_hub_handle_t hub, const char *token)
{
    if (hub == NULL || token == NULL || token[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    copy_string(hub->token, sizeof(hub->token), token);
    copy_string(hub->status, sizeof(hub->status), "Connecting to MiniHub");
    ++hub->generation;
    pthread_cond_signal(&hub->wake);
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_role(app_hub_handle_t hub, app_hub_role_t role,
                                  const char *url)
{
    if (hub == NULL || (role != APP_HUB_ROLE_HUB && role != APP_HUB_ROLE_WEB)) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    hub->role = role;
    hub->connected = false;
    if (role == APP_HUB_ROLE_HUB) {
        copy_string(hub->hub_url, sizeof(hub->hub_url), "http://127.0.0.1");
        copy_string(hub->status, sizeof(hub->status), "Using local MiniHub");
        hub->connected = true;
    } else if (url != NULL && url[0] != '\0') {
        copy_string(hub->hub_url, sizeof(hub->hub_url), url);
        copy_string(hub->status, sizeof(hub->status), "Connecting to MiniHub");
    } else {
        hub->hub_url[0] = '\0';
        copy_string(hub->status, sizeof(hub->status), "Enter Hub URL");
    }
    ++hub->generation;
    pthread_cond_signal(&hub->wake);
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_url(app_hub_handle_t hub, const char *url)
{
    if (hub == NULL || url == NULL || url[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    copy_string(hub->hub_url, sizeof(hub->hub_url), url);
    hub->connected = false;
    copy_string(hub->status, sizeof(hub->status), "Connecting to MiniHub");
    ++hub->generation;
    pthread_cond_signal(&hub->wake);
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_matter_commission(app_hub_handle_t hub,
                                           const char *pairing_code)
{
    if (hub == NULL || pairing_code == NULL || pairing_code[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    return APP_HUB_OK;
}

app_hub_result_t app_hub_matter_commission_snapshot(
    app_hub_handle_t hub, app_hub_commission_snapshot_t *out_snapshot)
{
    if (hub == NULL || out_snapshot == NULL) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    memset(out_snapshot, 0, sizeof(*out_snapshot));
    return APP_HUB_OK;
}

app_hub_result_t app_hub_matter_cancel(app_hub_handle_t hub,
                                       uint32_t node_id)
{
    (void)node_id;
    return hub != NULL ? APP_HUB_OK : APP_HUB_ERR_INVALID_ARG;
}

static app_hub_result_t queue_host_command(struct app_hub *hub,
                                           const host_command_t *command)
{
    pthread_mutex_lock(&hub->lock);
    if (hub->command_count == HOST_COMMAND_LIMIT) {
        pthread_mutex_unlock(&hub->lock);
        return APP_HUB_ERR_BUSY;
    }
    const size_t tail =
        (hub->command_head + hub->command_count) % HOST_COMMAND_LIMIT;
    hub->commands[tail] = *command;
    ++hub->command_count;
    pthread_cond_signal(&hub->wake);
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_add_snapshot(app_hub_handle_t hub,
                                      app_hub_add_snapshot_t *snapshot)
{
    if (!hub || !snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&hub->lock);
    *snapshot = hub->add;
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_add_device(app_hub_handle_t hub,
                                    const app_hub_add_request_t *add)
{
    if (!hub || !add || add->protocol >= APP_HUB_PROTOCOL_COUNT) {
        return APP_HUB_ERR_INVALID_ARG;
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
    pthread_mutex_lock(&hub->lock);
    const bool connected = hub->connected;
    pthread_mutex_unlock(&hub->lock);
    if (!connected) {
        pthread_mutex_lock(&hub->lock);
        hub->add.state = APP_HUB_ADD_FAILED;
        hub->add.progress = 0;
        copy_string(hub->add.status, sizeof(hub->add.status),
                    "MiniHub is not connected");
        ++hub->add.generation;
        pthread_mutex_unlock(&hub->lock);
        return APP_HUB_ERR_INTERNAL;
    }
    host_command_t command = {
        .kind = HOST_CMD_ADD,
        .add = *add,
    };
    pthread_mutex_lock(&hub->lock);
    hub->add.protocol = add->protocol;
    hub->add.state = APP_HUB_ADD_PREPARING;
    hub->add.progress = 5;
    copy_string(hub->add.status, sizeof(hub->add.status), "Queued");
    ++hub->add.generation;
    pthread_mutex_unlock(&hub->lock);
    return queue_host_command(hub, &command);
}

app_hub_result_t app_hub_add_cancel(app_hub_handle_t hub)
{
    if (!hub) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    const host_command_t command = {
        .kind = HOST_CMD_ADD_CANCEL,
    };
    return queue_host_command(hub, &command);
}

app_hub_result_t app_hub_delete_point(app_hub_handle_t hub,
                                      const char *point_id, uint32_t node_id)
{
    (void)node_id;
    if (hub == NULL || point_id == NULL || point_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (strcmp(point_id, "mosaico.imu") == 0) {
        return APP_HUB_ERR_NOT_SUPPORTED;
    }
    return APP_HUB_OK;
}

app_hub_result_t app_hub_scenes_snapshot(
    app_hub_handle_t hub, size_t page,
    app_hub_scenes_snapshot_t *snapshot)
{
    if (!hub || !snapshot) return APP_HUB_ERR_INVALID_ARG;
    pthread_mutex_lock(&hub->lock);
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->generation = hub->scenes_generation;
    snapshot->total = hub->scene_count;
    snapshot->ready = hub->scenes_generation > 0;
    snapshot->page_count = (hub->scene_count + APP_HUB_SCENES_PAGE_SIZE - 1) /
                           APP_HUB_SCENES_PAGE_SIZE;
    if (!snapshot->page_count) snapshot->page_count = 1;
    snapshot->page = page < snapshot->page_count ? page :
                     snapshot->page_count - 1;
    copy_string(snapshot->status, sizeof(snapshot->status), hub->scenes_status);
    const size_t start = snapshot->page * APP_HUB_SCENES_PAGE_SIZE;
    for (size_t i = 0; i < APP_HUB_SCENES_PAGE_SIZE &&
            start + i < hub->scene_count; ++i) {
        snapshot->scenes[i] = hub->scenes[start + i];
        ++snapshot->visible;
    }
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

static app_hub_result_t host_enqueue(app_hub_handle_t hub,
                                     host_command_kind_t kind,
                                     const char *id, bool enabled)
{
    if (!hub) return APP_HUB_ERR_INVALID_ARG;
    pthread_mutex_lock(&hub->lock);
    if (hub->command_count == HOST_COMMAND_LIMIT) {
        pthread_mutex_unlock(&hub->lock);
        return APP_HUB_ERR_BUSY;
    }
    const size_t tail =
        (hub->command_head + hub->command_count) % HOST_COMMAND_LIMIT;
    host_command_t *command = &hub->commands[tail];
    memset(command, 0, sizeof(*command));
    command->kind = kind;
    command->enabled = enabled;
    copy_string(command->point_id, sizeof(command->point_id), id);
    ++hub->command_count;
    pthread_cond_signal(&hub->wake);
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_scene_invoke(app_hub_handle_t hub,
                                      const char *scene_id)
{
    if (!scene_id || !scene_id[0]) return APP_HUB_ERR_INVALID_ARG;
    return host_enqueue(hub, HOST_CMD_SCENE, scene_id, false);
}

app_hub_result_t app_hub_scene_delete(app_hub_handle_t hub,
                                      const char *scene_id)
{
    if (!scene_id || !scene_id[0]) return APP_HUB_ERR_INVALID_ARG;
    return host_enqueue(hub, HOST_CMD_SCENE_DELETE, scene_id, false);
}

app_hub_result_t app_hub_scene_create(app_hub_handle_t hub, const char *name,
                                      const char *const *point_ids,
                                      const bool *on_values, size_t count)
{
    if (!hub || !name || !name[0] || !point_ids || !on_values ||
            count == 0 || count > APP_HUB_PAGE_SIZE) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    host_command_t command = {
        .kind = HOST_CMD_SCENE_CREATE,
        .scene_count = (uint8_t)count,
    };
    copy_string(command.name, sizeof(command.name), name);
    for (size_t i = 0; i < count; ++i) {
        if (!point_ids[i] || !point_ids[i][0]) {
            return APP_HUB_ERR_INVALID_ARG;
        }
        copy_string(command.scene_points[i], sizeof(command.scene_points[i]),
                    point_ids[i]);
        command.scene_values[i] = on_values[i];
    }
    return queue_host_command(hub, &command);
}

app_hub_result_t app_hub_rules_snapshot(
    app_hub_handle_t hub, size_t page,
    app_hub_rules_snapshot_t *snapshot)
{
    if (!hub || !snapshot) return APP_HUB_ERR_INVALID_ARG;
    pthread_mutex_lock(&hub->lock);
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->generation = hub->rules_generation;
    snapshot->total = hub->rule_count;
    snapshot->ready = hub->rules_generation > 0;
    snapshot->page_count = (hub->rule_count + APP_HUB_RULES_PAGE_SIZE - 1) /
                           APP_HUB_RULES_PAGE_SIZE;
    if (!snapshot->page_count) snapshot->page_count = 1;
    snapshot->page = page < snapshot->page_count ? page :
                     snapshot->page_count - 1;
    copy_string(snapshot->status, sizeof(snapshot->status), hub->rules_status);
    const size_t start = snapshot->page * APP_HUB_RULES_PAGE_SIZE;
    for (size_t i = 0; i < APP_HUB_RULES_PAGE_SIZE &&
            start + i < hub->rule_count; ++i) {
        snapshot->rules[i] = hub->rules[start + i];
        ++snapshot->visible;
    }
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_rule_set_enabled(app_hub_handle_t hub,
                                          const char *rule_id, bool enabled)
{
    if (!rule_id || !rule_id[0]) return APP_HUB_ERR_INVALID_ARG;
    return host_enqueue(hub, HOST_CMD_RULE, rule_id, enabled);
}

app_hub_result_t app_hub_rule_delete(app_hub_handle_t hub, const char *rule_id)
{
    if (!rule_id || !rule_id[0]) return APP_HUB_ERR_INVALID_ARG;
    return host_enqueue(hub, HOST_CMD_RULE_DELETE, rule_id, false);
}

app_hub_result_t app_hub_rule_create(app_hub_handle_t hub, const char *name,
                                     const char *source_point,
                                     const char *source_value,
                                     const char *target_point, bool target_on)
{
    if (!hub || !name || !name[0] || !source_point || !source_point[0] ||
            !source_value || !source_value[0] || !target_point ||
            !target_point[0]) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    host_command_t command = {
        .kind = HOST_CMD_RULE_CREATE,
        .enabled = target_on,
    };
    copy_string(command.name, sizeof(command.name), name);
    copy_string(command.point_id, sizeof(command.point_id), source_point);
    copy_string(command.source_value, sizeof(command.source_value), source_value);
    copy_string(command.target_point, sizeof(command.target_point), target_point);
    return queue_host_command(hub, &command);
}

app_hub_result_t app_hub_system_snapshot(
    app_hub_handle_t hub, app_hub_system_snapshot_t *snapshot)
{
    if (!hub || !snapshot) return APP_HUB_ERR_INVALID_ARG;
    pthread_mutex_lock(&hub->lock);
    *snapshot = hub->system;
    pthread_mutex_unlock(&hub->lock);
    return APP_HUB_OK;
}

app_hub_result_t app_hub_system_reboot(app_hub_handle_t hub)
{
    return host_enqueue(hub, HOST_CMD_REBOOT, "", false);
}

app_hub_result_t app_hub_reconnect(app_hub_handle_t hub, const char *point_id)
{
    if (!point_id || !point_id[0]) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    return host_enqueue(hub, HOST_CMD_RELOAD, point_id, false);
}
