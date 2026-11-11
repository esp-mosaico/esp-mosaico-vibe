// SPDX-License-Identifier: Apache-2.0

#include "app_hub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MOCK_POINT_COUNT 6
#define MOCK_POINT_LIMIT 16
#define MOCK_SCAN_MS 800
#define MOCK_CONNECT_OK_MS 1200
#define MOCK_CONNECT_FAIL_MS 400
#define MOCK_COMMISSION_MS 900

struct app_hub {
    app_hub_point_t points[MOCK_POINT_LIMIT];
    size_t point_count;
    uint32_t generation;
    bool wifi_ready;
    bool wifi_needs_setup;
    bool wifi_scanning;
    bool wifi_connecting;
    bool pending_fail;
    bool pending_secured;
    uint64_t scan_done_ms;
    uint64_t connect_done_ms;
    char wifi_ssid[APP_HUB_SSID_LEN];
    char status[APP_HUB_STATUS_LEN];
    size_t wifi_ap_count;
    app_hub_wifi_ap_t wifi_aps[APP_HUB_WIFI_AP_LIMIT];
    app_hub_role_t role;
    char hub_url[APP_HUB_URL_LEN];
    bool connected;
    app_hub_commission_snapshot_t commission;
    uint64_t commission_done_ms;
    bool commission_pending_fail;
    app_hub_add_snapshot_t add;
    app_hub_add_request_t pending_add;
    uint64_t add_done_ms;
    bool add_pending_fail;
    app_hub_scene_t scenes[4];
    size_t scene_count;
    app_hub_rule_t rules[4];
    size_t rule_count;
    uint32_t scenes_generation;
    uint32_t rules_generation;
    char scenes_status[APP_HUB_STATUS_LEN];
    char rules_status[APP_HUB_STATUS_LEN];
    app_hub_system_snapshot_t system;
};

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

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

static void set_point(app_hub_point_t *point, const char *id, const char *type,
                      const char *value, bool controllable, bool on)
{
    snprintf(point->point_id, sizeof(point->point_id), "%s", id);
    snprintf(point->name, sizeof(point->name), "%s", id);
    snprintf(point->type, sizeof(point->type), "%s", type);
    snprintf(point->value, sizeof(point->value), "%s", value);
    point->online = true;
    point->controllable = controllable;
    point->on = on;
    if (strcmp(type, "light") == 0) {
        point->has_brightness = true;
        point->has_hue = true;
        point->has_saturation = true;
        point->has_cct = true;
        point->brightness = 60;
        point->hue = 180;
        point->saturation = 80;
        point->cct = 4000;
    }
}

static void fill_scan(struct app_hub *hub)
{
    static const app_hub_wifi_ap_t samples[] = {
        { .ssid = "Office", .rssi = -48, .secured = true },
        { .ssid = "Home", .rssi = -55, .secured = true },
        { .ssid = "家里", .rssi = -50, .secured = true },
        { .ssid = "MiniHub-Lab", .rssi = -62, .secured = true },
        { .ssid = "店里", .rssi = -58, .secured = true },
        { .ssid = "Guest", .rssi = -70, .secured = false },
        { .ssid = "Neighbor-5G", .rssi = -72, .secured = true },
        { .ssid = "Cafe", .rssi = -65, .secured = true },
        { .ssid = "Studio", .rssi = -60, .secured = true },
        { .ssid = "IoT-Open", .rssi = -78, .secured = false },
        { .ssid = "Locked-Fail", .rssi = -80, .secured = true },
    };
    hub->wifi_ap_count = sizeof(samples) / sizeof(samples[0]);
    memcpy(hub->wifi_aps, samples, sizeof(samples));
}

static void settle_async(struct app_hub *hub)
{
    const uint64_t now = now_ms();
    if (hub->wifi_scanning && now >= hub->scan_done_ms) {
        fill_scan(hub);
        hub->wifi_scanning = false;
        copy_string(hub->status, sizeof(hub->status), "Select a network");
        ++hub->generation;
    }
    if (hub->wifi_connecting && now >= hub->connect_done_ms) {
        hub->wifi_connecting = false;
        if (hub->pending_fail) {
            hub->wifi_ready = false;
            copy_string(hub->status, sizeof(hub->status), "Wrong password");
        } else {
            hub->wifi_ready = true;
            hub->wifi_needs_setup = false;
            copy_string(hub->status, sizeof(hub->status), "Connected");
        }
        ++hub->generation;
    }
    if (hub->commission.state == APP_HUB_COMMISSION_RUNNING &&
            now >= hub->commission_done_ms) {
        if (hub->commission_pending_fail) {
            hub->commission.state = APP_HUB_COMMISSION_FAILED;
            hub->commission.stage = APP_HUB_COMMISSION_STAGE_NONE;
            copy_string(hub->commission.error, sizeof(hub->commission.error),
                        "PASE session failed");
        } else {
            hub->commission.state = APP_HUB_COMMISSION_OK;
            hub->commission.stage = APP_HUB_COMMISSION_STAGE_NONE;
            if (hub->point_count < MOCK_POINT_LIMIT) {
                char id[APP_HUB_POINT_ID_LEN];
                snprintf(id, sizeof(id), "matter.node%u.ep1",
                         (unsigned)hub->commission.node_id);
                set_point(&hub->points[hub->point_count++], id, "light", "OFF",
                          true, false);
            }
        }
        ++hub->generation;
    }
    if ((hub->add.state == APP_HUB_ADD_PREPARING ||
         hub->add.state == APP_HUB_ADD_WAITING) &&
            now >= hub->add_done_ms) {
        hub->add.state = hub->add_pending_fail ?
                         APP_HUB_ADD_FAILED : APP_HUB_ADD_OK;
        hub->add.progress = 100;
        if (!hub->add_pending_fail && hub->point_count < MOCK_POINT_LIMIT) {
            char generated_id[APP_HUB_POINT_ID_LEN];
            snprintf(generated_id, sizeof(generated_id), "mock.%u.%u",
                     (unsigned)hub->pending_add.protocol,
                     (unsigned)hub->point_count);
            const char *id = hub->pending_add.point_id[0] ?
                             hub->pending_add.point_id : generated_id;
            const char *type = hub->pending_add.type[0] ?
                               hub->pending_add.type : "switch";
            app_hub_point_t *point = &hub->points[hub->point_count++];
            set_point(point, id, type, "OFF", true, false);
            if (hub->pending_add.name[0]) {
                format_display_name(point->name, sizeof(point->name),
                                    hub->pending_add.room,
                                    hub->pending_add.name);
            }
            copy_string(hub->add.point_id, sizeof(hub->add.point_id),
                        point->point_id);
        }
        copy_string(hub->add.status, sizeof(hub->add.status),
                    hub->add_pending_fail ? "Add failed" : "Device added");
        ++hub->add.generation;
    }
}

app_hub_result_t app_hub_start(app_hub_handle_t *out_handle)
{
    if (!out_handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    struct app_hub *hub = calloc(1, sizeof(*hub));
    if (!hub) {
        return APP_HUB_ERR_NO_MEMORY;
    }
    set_point(&hub->points[0], "living_room.light", "light", "ON", true, true);
    snprintf(hub->points[0].name, sizeof(hub->points[0].name), "客厅主灯");
    set_point(&hub->points[1], "mosaico.imu", "sensor", "still", false, false);
    snprintf(hub->points[1].name, sizeof(hub->points[1].name), "体感");
    set_point(&hub->points[2], "entry.relay", "switch", "OFF", true, false);
    snprintf(hub->points[2].name, sizeof(hub->points[2].name), "门开关");
    set_point(&hub->points[3], "desk.plug", "plug", "ON", true, true);
    snprintf(hub->points[3].name, sizeof(hub->points[3].name), "书房插座");
    set_point(&hub->points[4], "balcony.humidity", "sensor", "58.0",
              false, false);
    set_point(&hub->points[5], "hall.motion", "sensor", "idle", false, false);
    hub->point_count = MOCK_POINT_COUNT;
    hub->generation = 1;
    hub->add.capabilities = APP_HUB_CAP_MATTER | APP_HUB_CAP_ZIGBEE |
                            APP_HUB_CAP_BLE_MESH | APP_HUB_CAP_MQTT |
                            APP_HUB_CAP_MODBUS_TCP;
    hub->add.state = APP_HUB_ADD_IDLE;
    hub->add.generation = 1;
    copy_string(hub->add.status, sizeof(hub->add.status), "Choose a protocol");
    if (getenv("MINIHUB_SIM_READY") != NULL) {
        hub->wifi_ready = true;
        hub->wifi_needs_setup = false;
        hub->role = APP_HUB_ROLE_HUB;
        hub->connected = true;
        copy_string(hub->status, sizeof(hub->status), "MiniHub online");
    } else {
        hub->wifi_ready = false;
        hub->wifi_needs_setup = true;
        hub->role = APP_HUB_ROLE_UNSET;
        copy_string(hub->status, sizeof(hub->status), "Tap to set up Wi-Fi");
    }
    hub->scene_count = 0;
    hub->rule_count = 0;
    hub->scenes_generation = 1;
    hub->rules_generation = 1;
    copy_string(hub->scenes_status, sizeof(hub->scenes_status), "No scenes");
    copy_string(hub->rules_status, sizeof(hub->rules_status),
                "No automations");
    hub->system = (app_hub_system_snapshot_t) {
        .generation = 1,
        .ready = true,
        .hub_online = true,
        .uptime_s = 3661,
        .internal_free_kb = 420,
        .psram_free_kb = 6144,
        .storage_free_kb = 2048,
    };
    copy_string(hub->system.version, sizeof(hub->system.version), "sim-1.0.0");
    copy_string(hub->system.ip, sizeof(hub->system.ip), "192.168.1.88");
    copy_string(hub->system.status, sizeof(hub->system.status), "MiniHub healthy");
    *out_handle = hub;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_snapshot(app_hub_handle_t handle, size_t page,
                                  app_hub_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    settle_async(handle);
    const size_t page_count =
        (handle->point_count + 1 + APP_HUB_PAGE_SIZE - 1) / APP_HUB_PAGE_SIZE;
    *out_snapshot = (app_hub_snapshot_t) {
        .generation = handle->generation,
        .total_points = handle->point_count,
        .online_points = handle->point_count,
        .page_count = page_count > 0 ? page_count : 1,
        .connected = handle->connected,
        .wifi_ready = handle->wifi_ready,
        .wifi_needs_setup = handle->wifi_needs_setup,
        .role = handle->role,
        .role_needed = handle->wifi_ready &&
                       handle->role == APP_HUB_ROLE_UNSET,
        .url_needed = handle->wifi_ready &&
                      handle->role == APP_HUB_ROLE_WEB &&
                      handle->hub_url[0] == '\0',
        .auth_needed = false,
    };
    copy_string(out_snapshot->status, sizeof(out_snapshot->status),
                handle->status);
    if (page >= out_snapshot->page_count) {
        page = out_snapshot->page_count - 1;
    }
    out_snapshot->page = page;
    const size_t start = page * APP_HUB_PAGE_SIZE;
    for (size_t i = 0;
            i < APP_HUB_PAGE_SIZE && start + i < handle->point_count; ++i) {
        out_snapshot->points[i] = handle->points[start + i];
        ++out_snapshot->visible_points;
    }
    return APP_HUB_OK;
}

app_hub_result_t app_hub_toggle(app_hub_handle_t handle, const char *point_id)
{
    if (!handle || !point_id) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < handle->point_count; ++i) {
        app_hub_point_t *point = &handle->points[i];
        if (strcmp(point->point_id, point_id) != 0) {
            continue;
        }
        if (!point->controllable) {
            return APP_HUB_ERR_NOT_SUPPORTED;
        }
        point->on = !point->on;
        snprintf(point->value, sizeof(point->value), "%s",
                 point->on ? "ON" : "OFF");
        ++handle->generation;
        return APP_HUB_OK;
    }
    return APP_HUB_ERR_INVALID_ARG;
}

app_hub_result_t app_hub_set_parameter(app_hub_handle_t handle,
                                       const char *point_id,
                                       const char *parameter, int32_t value)
{
    if (!handle || !point_id || !parameter) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < handle->point_count; ++i) {
        app_hub_point_t *point = &handle->points[i];
        if (strcmp(point->point_id, point_id) != 0) {
            continue;
        }
        int32_t *target = NULL;
        if (strcmp(parameter, "brightness") == 0 && point->has_brightness) {
            target = &point->brightness;
        } else if (strcmp(parameter, "hue") == 0 && point->has_hue) {
            target = &point->hue;
        } else if (strcmp(parameter, "saturation") == 0 &&
                   point->has_saturation) {
            target = &point->saturation;
        } else if (strcmp(parameter, "cct") == 0 && point->has_cct) {
            target = &point->cct;
        }
        if (!target) {
            return APP_HUB_ERR_NOT_SUPPORTED;
        }
        *target = value;
        ++handle->generation;
        return APP_HUB_OK;
    }
    return APP_HUB_ERR_INVALID_ARG;
}

app_hub_result_t app_hub_refresh(app_hub_handle_t handle)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    ++handle->generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_reconnect(app_hub_handle_t handle, const char *point_id)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < handle->point_count; ++i) {
        if (point_id && point_id[0] &&
                strcmp(handle->points[i].point_id, point_id) != 0) {
            continue;
        }
        handle->points[i].online = true;
        handle->points[i].offline_strikes = 0;
    }
    ++handle->generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_wifi_snapshot(app_hub_handle_t handle, size_t page,
                                       app_hub_wifi_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    settle_async(handle);
    *out_snapshot = (app_hub_wifi_snapshot_t) {
        .generation = handle->generation,
        .ready = handle->wifi_ready,
        .scanning = handle->wifi_scanning,
        .connecting = handle->wifi_connecting,
        .needs_setup = handle->wifi_needs_setup,
        .total_aps = handle->wifi_ap_count,
    };
    copy_string(out_snapshot->ssid, sizeof(out_snapshot->ssid),
                handle->wifi_ssid);
    copy_string(out_snapshot->status, sizeof(out_snapshot->status),
                handle->status);
    out_snapshot->page_count = 1;
    out_snapshot->page = 0;
    (void)page;
    for (size_t i = 0; i < handle->wifi_ap_count && i < APP_HUB_WIFI_PAGE_SIZE;
            ++i) {
        out_snapshot->aps[i] = handle->wifi_aps[i];
        ++out_snapshot->visible_aps;
    }
    return APP_HUB_OK;
}

app_hub_result_t app_hub_wifi_scan(app_hub_handle_t handle)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (handle->wifi_connecting) {
        return APP_HUB_OK;
    }
    handle->wifi_scanning = true;
    handle->wifi_ap_count = 0;
    handle->scan_done_ms = now_ms() + MOCK_SCAN_MS;
    copy_string(handle->status, sizeof(handle->status), "Scanning...");
    ++handle->generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_wifi_connect(app_hub_handle_t handle, const char *ssid,
                                      const char *password)
{
    if (!handle || !ssid || ssid[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    bool secured = true;
    for (size_t i = 0; i < handle->wifi_ap_count; ++i) {
        if (strcmp(handle->wifi_aps[i].ssid, ssid) == 0) {
            secured = handle->wifi_aps[i].secured;
            break;
        }
    }
    const bool empty = password == NULL || password[0] == '\0';
    handle->pending_fail = (secured && empty) ||
                           (password != NULL && strcmp(password, "wrong") == 0) ||
                           strcmp(ssid, "Locked-Fail") == 0 ||
                           getenv("MINIHUB_SIM_WIFI_FAIL") != NULL;
    handle->pending_secured = secured;
    handle->wifi_scanning = false;
    handle->wifi_connecting = true;
    handle->wifi_ready = false;
    copy_string(handle->wifi_ssid, sizeof(handle->wifi_ssid), ssid);
    copy_string(handle->status, sizeof(handle->status), "Connecting...");
    handle->connect_done_ms = now_ms() +
                              (handle->pending_fail ? MOCK_CONNECT_FAIL_MS :
                               MOCK_CONNECT_OK_MS);
    ++handle->generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_token(app_hub_handle_t handle, const char *token)
{
    if (!handle || !token || token[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    handle->connected = true;
    copy_string(handle->status, sizeof(handle->status), "MiniHub online / simulator");
    ++handle->generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_role(app_hub_handle_t handle, app_hub_role_t role,
                                  const char *url)
{
    if (!handle || (role != APP_HUB_ROLE_HUB && role != APP_HUB_ROLE_WEB)) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    handle->role = role;
    handle->connected = false;
    if (role == APP_HUB_ROLE_HUB) {
        copy_string(handle->hub_url, sizeof(handle->hub_url), "http://127.0.0.1");
        copy_string(handle->status, sizeof(handle->status), "Using local MiniHub");
        handle->connected = true;
    } else if (url != NULL && url[0] != '\0') {
        copy_string(handle->hub_url, sizeof(handle->hub_url), url);
        copy_string(handle->status, sizeof(handle->status), "Connecting to MiniHub");
    } else {
        handle->hub_url[0] = '\0';
        copy_string(handle->status, sizeof(handle->status), "Enter Hub URL");
    }
    ++handle->generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_set_url(app_hub_handle_t handle, const char *url)
{
    if (!handle || !url || url[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    copy_string(handle->hub_url, sizeof(handle->hub_url), url);
    handle->connected = false;
    copy_string(handle->status, sizeof(handle->status), "Connecting to MiniHub");
    ++handle->generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_matter_commission(app_hub_handle_t handle,
                                           const char *pairing_code)
{
    if (!handle || !pairing_code || pairing_code[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    handle->commission_pending_fail = strcmp(pairing_code, "fail") == 0;
    handle->commission.state = APP_HUB_COMMISSION_RUNNING;
    handle->commission.stage = APP_HUB_COMMISSION_STAGE_DISCOVERING;
    handle->commission.node_id = (uint32_t)(handle->point_count + 2);
    handle->commission.error[0] = '\0';
    handle->commission_done_ms = now_ms() + MOCK_COMMISSION_MS;
    ++handle->generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_matter_commission_snapshot(
    app_hub_handle_t handle, app_hub_commission_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    settle_async(handle);
    *out_snapshot = handle->commission;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_matter_cancel(app_hub_handle_t handle,
                                       uint32_t node_id)
{
    (void)node_id;
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    handle->commission.state = APP_HUB_COMMISSION_IDLE;
    handle->commission.stage = APP_HUB_COMMISSION_STAGE_NONE;
    ++handle->generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_add_snapshot(app_hub_handle_t handle,
                                      app_hub_add_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    settle_async(handle);
    *out_snapshot = handle->add;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_add_device(app_hub_handle_t handle,
                                    const app_hub_add_request_t *request)
{
    if (!handle || !request || request->protocol >= APP_HUB_PROTOCOL_COUNT) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (!request->name[0] ||
            (request->protocol == APP_HUB_PROTOCOL_MATTER &&
             !app_hub_matter_pairing_code_ok(request->pairing_code)) ||
            (request->protocol == APP_HUB_PROTOCOL_ZIGBEE &&
             request->install_code[0] && !request->ieee[0]) ||
            (request->protocol == APP_HUB_PROTOCOL_MQTT &&
             (!request->host[0] || !request->point_id[0] ||
              !request->state_topic[0])) ||
            (request->protocol == APP_HUB_PROTOCOL_MODBUS_TCP &&
             (!request->host[0] || !request->point_id[0] ||
              request->unit_id == 0 || request->unit_id > 247))) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if ((handle->add.capabilities & (1U << request->protocol)) == 0) {
        return APP_HUB_ERR_NOT_SUPPORTED;
    }
    if (!handle->wifi_ready || !handle->connected) {
        handle->add.state = APP_HUB_ADD_FAILED;
        handle->add.progress = 0;
        copy_string(handle->add.status, sizeof(handle->add.status),
                    "MiniHub is not ready");
        ++handle->add.generation;
        return APP_HUB_ERR_INTERNAL;
    }
    if (handle->add.state == APP_HUB_ADD_PREPARING ||
            handle->add.state == APP_HUB_ADD_WAITING) {
        return APP_HUB_ERR_BUSY;
    }
    handle->add.protocol = request->protocol;
    handle->pending_add = *request;
    handle->add.state = APP_HUB_ADD_WAITING;
    handle->add.progress = 35;
    handle->add_done_ms = now_ms() + MOCK_COMMISSION_MS;
    handle->add_pending_fail = strstr(request->host, "fail") != NULL ||
                               strstr(request->pairing_code, "fail") != NULL;
    copy_string(handle->add.point_id, sizeof(handle->add.point_id),
                request->point_id[0] ? request->point_id : "new.device");
    copy_string(handle->add.status, sizeof(handle->add.status), "Adding device...");
    ++handle->add.generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_add_cancel(app_hub_handle_t handle)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    handle->add.state = APP_HUB_ADD_CANCELLED;
    handle->add.progress = 0;
    copy_string(handle->add.status, sizeof(handle->add.status), "Add cancelled");
    ++handle->add.generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_delete_point(app_hub_handle_t handle,
                                      const char *point_id, uint32_t node_id)
{
    (void)node_id;
    if (!handle || !point_id || point_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    if (strcmp(point_id, "mosaico.imu") == 0) {
        return APP_HUB_ERR_NOT_SUPPORTED;
    }
    for (size_t i = 0; i < handle->point_count; ++i) {
        if (strcmp(handle->points[i].point_id, point_id) == 0) {
            for (size_t j = i; j + 1 < handle->point_count; ++j) {
                handle->points[j] = handle->points[j + 1];
            }
            --handle->point_count;
            ++handle->generation;
            return APP_HUB_OK;
        }
    }
    return APP_HUB_ERR_INVALID_ARG;
}

app_hub_result_t app_hub_scenes_snapshot(
    app_hub_handle_t handle, size_t page,
    app_hub_scenes_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    *out_snapshot = (app_hub_scenes_snapshot_t) {
        .generation = handle->scenes_generation,
        .total = handle->scene_count,
        .page_count = 1,
        .ready = true,
    };
    (void)page;
    copy_string(out_snapshot->status, sizeof(out_snapshot->status),
                handle->scenes_status);
    for (size_t i = 0; i < handle->scene_count &&
            i < APP_HUB_SCENES_PAGE_SIZE; ++i) {
        out_snapshot->scenes[i] = handle->scenes[i];
        ++out_snapshot->visible;
    }
    return APP_HUB_OK;
}

app_hub_result_t app_hub_scene_invoke(app_hub_handle_t handle,
                                      const char *scene_id)
{
    if (!handle || !scene_id || scene_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    copy_string(handle->scenes_status, sizeof(handle->scenes_status),
                strcmp(scene_id, "fail") == 0 ? "Scene failed" : "Scene started");
    ++handle->scenes_generation;
    return strcmp(scene_id, "fail") == 0 ?
           APP_HUB_ERR_INTERNAL : APP_HUB_OK;
}

app_hub_result_t app_hub_scene_delete(app_hub_handle_t handle,
                                      const char *scene_id)
{
    if (!handle || !scene_id || scene_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < handle->scene_count; ++i) {
        if (strcmp(handle->scenes[i].id, scene_id) != 0) {
            continue;
        }
        memmove(&handle->scenes[i], &handle->scenes[i + 1],
                (handle->scene_count - i - 1U) * sizeof(handle->scenes[0]));
        --handle->scene_count;
        copy_string(handle->scenes_status, sizeof(handle->scenes_status),
                    "Scene deleted");
        ++handle->scenes_generation;
        return APP_HUB_OK;
    }
    return APP_HUB_ERR_INVALID_ARG;
}

app_hub_result_t app_hub_scene_create(app_hub_handle_t handle,
                                      const char *name,
                                      const char *const *point_ids,
                                      const bool *on_values, size_t count)
{
    if (!handle || !name || !name[0] || !point_ids || !on_values ||
            count == 0 || handle->scene_count >= 4) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    app_hub_scene_t *scene = &handle->scenes[handle->scene_count++];
    snprintf(scene->id, sizeof(scene->id), "panel_%u",
             (unsigned)handle->scenes_generation);
    copy_string(scene->name, sizeof(scene->name), name);
    scene->step_count = (uint8_t)count;
    scene->action_count = (uint8_t)count;
    for (size_t i = 0; i < count; ++i) {
        copy_string(scene->action_points[i], sizeof(scene->action_points[i]),
                    point_ids[i]);
        scene->action_on[i] = on_values[i];
    }
    copy_string(handle->scenes_status, sizeof(handle->scenes_status),
                "Scene created");
    ++handle->scenes_generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_rules_snapshot(
    app_hub_handle_t handle, size_t page,
    app_hub_rules_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    *out_snapshot = (app_hub_rules_snapshot_t) {
        .generation = handle->rules_generation,
        .total = handle->rule_count,
        .page_count = 1,
        .ready = true,
    };
    (void)page;
    copy_string(out_snapshot->status, sizeof(out_snapshot->status),
                handle->rules_status);
    for (size_t i = 0; i < handle->rule_count &&
            i < APP_HUB_RULES_PAGE_SIZE; ++i) {
        out_snapshot->rules[i] = handle->rules[i];
        ++out_snapshot->visible;
    }
    return APP_HUB_OK;
}

app_hub_result_t app_hub_rule_set_enabled(app_hub_handle_t handle,
                                          const char *rule_id, bool enabled)
{
    if (!handle || !rule_id || rule_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < handle->rule_count; ++i) {
        if (strcmp(handle->rules[i].id, rule_id) == 0) {
            handle->rules[i].enabled = enabled;
            copy_string(handle->rules_status, sizeof(handle->rules_status),
                        enabled ? "Automation enabled" : "Automation disabled");
            ++handle->rules_generation;
            return APP_HUB_OK;
        }
    }
    return APP_HUB_ERR_INVALID_ARG;
}

app_hub_result_t app_hub_rule_delete(app_hub_handle_t handle,
                                     const char *rule_id)
{
    if (!handle || !rule_id || rule_id[0] == '\0') {
        return APP_HUB_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < handle->rule_count; ++i) {
        if (strcmp(handle->rules[i].id, rule_id) != 0) {
            continue;
        }
        memmove(&handle->rules[i], &handle->rules[i + 1],
                (handle->rule_count - i - 1U) * sizeof(handle->rules[0]));
        --handle->rule_count;
        copy_string(handle->rules_status, sizeof(handle->rules_status),
                    "Automation deleted");
        ++handle->rules_generation;
        return APP_HUB_OK;
    }
    return APP_HUB_ERR_INVALID_ARG;
}

app_hub_result_t app_hub_rule_create(app_hub_handle_t handle, const char *name,
                                     const char *source_point,
                                     const char *source_value,
                                     const char *target_point, bool target_on)
{
    (void)target_on;
    if (!handle || !name || !name[0] || !source_point || !source_point[0] ||
            !source_value || !source_value[0] || !target_point ||
            !target_point[0] || handle->rule_count >= 4) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    app_hub_rule_t *rule = &handle->rules[handle->rule_count++];
    snprintf(rule->id, sizeof(rule->id), "panel_%u",
             (unsigned)handle->rules_generation);
    copy_string(rule->name, sizeof(rule->name), name);
    rule->enabled = true;
    copy_string(handle->rules_status, sizeof(handle->rules_status),
                "Automation created");
    ++handle->rules_generation;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_system_snapshot(
    app_hub_handle_t handle, app_hub_system_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    *out_snapshot = handle->system;
    return APP_HUB_OK;
}

app_hub_result_t app_hub_system_reboot(app_hub_handle_t handle)
{
    if (!handle) {
        return APP_HUB_ERR_INVALID_ARG;
    }
    copy_string(handle->system.status, sizeof(handle->system.status),
                "Restart requested");
    ++handle->system.generation;
    return APP_HUB_OK;
}
