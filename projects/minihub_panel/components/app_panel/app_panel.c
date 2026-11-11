// SPDX-License-Identifier: Apache-2.0

#include "app_panel.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#include "nvs.h"
#define PANEL_LOGI(...) ESP_LOGI("app_panel", __VA_ARGS__)
#else
#define PANEL_LOGI(...) ((void)0)
#endif

#if !defined(ESP_PLATFORM)
static size_t strlcpy(char *destination, const char *source, size_t size)
{
    const size_t length = strlen(source);
    if (size > 0) {
        const size_t copy = length < size - 1 ? length : size - 1;
        memcpy(destination, source, copy);
        destination[copy] = '\0';
    }
    return length;
}
#endif

#define GSP_BUNDLE_ENABLE_RAW_IDS
#include "bundle_gsp.h"

#define PANEL_REFRESH_MS 50
#ifndef PANEL_WELCOME_TICKS
#define PANEL_WELCOME_TICKS 70
#endif
#define PANEL_WIFI_SUCCESS_TICKS 30
#define PANEL_ADD_SUCCESS_TICKS 24
#define PANEL_RECONNECT_WAIT_TICKS 900
#define PANEL_RECONNECT_RESULT_TICKS 40

typedef enum {
    RECONNECT_IDLE = 0,
    RECONNECT_WAIT,
    RECONNECT_OK,
    RECONNECT_FAIL,
} reconnect_state_t;

typedef enum {
    PANEL_LANG_EN = 0,
    PANEL_LANG_ZH,
} panel_lang_t;

typedef enum {
    PANEL_THEME_DARK = 0,
    PANEL_THEME_LIGHT,
} panel_theme_t;

#define PANEL_NVS_NAMESPACE "minihub_ui"
#define PANEL_NVS_THEME "theme"
#define PANEL_THEME_HOST_PATH "minihub_panel.theme"

typedef struct {
    uint32_t bg;
    uint32_t card;
    uint32_t key;
    uint32_t ink;
    uint32_t muted;
    uint32_t line;
    uint32_t accent;
    uint32_t cta;
} panel_theme_colors_t;

typedef enum {
    PANEL_SETUP_NONE = 0,
    PANEL_SETUP_WELCOME,
    PANEL_SETUP_LANGUAGE,
    PANEL_SETUP_WIFI,
    PANEL_SETUP_ROLE,
    PANEL_SETUP_TOKEN,
    PANEL_SETUP_SETTINGS,
    PANEL_SETUP_ADD,
} panel_setup_page_t;

typedef enum {
    PANEL_KEYBOARD_NONE = 0,
    PANEL_KEYBOARD_ADD,
    PANEL_KEYBOARD_WIFI,
    PANEL_KEYBOARD_TOKEN,
} panel_keyboard_t;

typedef enum {
    PANEL_TAB_DEVICES = 0,
    PANEL_TAB_SCENES,
    PANEL_TAB_AUTOMATION,
    PANEL_TAB_SETTINGS,
} panel_tab_t;

typedef enum {
    COPY_SETUP = 0,
    COPY_HOME_TITLE,
    COPY_HOME_SUBTITLE,
    COPY_HOME_DEVICES,
    COPY_HOME_ONLINE,
    COPY_LANGUAGE,
    COPY_DEFAULT,
    COPY_CONTINUE,
    COPY_NETWORK,
    COPY_SCANNING,
    COPY_SELECT_NET,
    COPY_SCAN_FAILED,
    COPY_CONNECTING,
    COPY_CONNECT_TIMEOUT,
    COPY_CONNECTED,
    COPY_PASSWORD,
    COPY_ENTER_PASSWORD,
    COPY_WRONG_PASSWORD,
    COPY_PRODUCT,
    COPY_CHOOSE_MODE,
    COPY_WIFI_READY,
    COPY_HUB,
    COPY_HUB_META,
    COPY_PANEL,
    COPY_PANEL_META,
    COPY_HUB_URL,
    COPY_TOKEN,
    COPY_TOKEN_META,
    COPY_SETTINGS,
    COPY_SETTINGS_WIFI_META,
    COPY_DEVICE_ROLE,
    COPY_SETTINGS_ROLE_META,
    COPY_ADD_CARD_TITLE,
    COPY_ADD_CARD_META,
    COPY_PICK_DEVICE,
    COPY_NO_OTHER_DEVICE,
    COPY_NO_OTHER_META,
    COPY_ADD_TITLE,
    COPY_ADD_META,
    COPY_ADD_DISCOVERING,
    COPY_ADD_BINDING,
    COPY_ADD_ACTIVATING,
    COPY_ADD_SUCCESS,
    COPY_ADD_FAILED,
    COPY_CANCEL_PAIRING,
    COPY_DELETE_DEVICE,
    COPY_DELETE,
    COPY_DEVICES,
    COPY_SCENES,
    COPY_AUTOMATION,
    COPY_TAP_SCENE,
    COPY_TAP_AUTOMATION,
    COPY_MODE,
    COPY_RESTART,
    COPY_UPDATE_VIBE,
    COPY_RESTART_QUESTION,
    COPY_RESTART_META,
    COPY_TURN_ON,
    COPY_TURN_OFF,
    COPY_CANCEL,
    COPY_CONFIRM_RESTART,
    COPY_RECONNECT,
    COPY_RECONNECT_WAIT,
    COPY_RECONNECT_OK,
    COPY_RECONNECT_FAIL,
    COPY_QUEUE_FULL,
    COPY_APPEARANCE,
    COPY_DARK,
    COPY_LIGHT,
    COPY_COUNT,
} copy_id_t;

typedef esp_gsp_err_t (*text_setter_t)(esp_gsp_handle_t, const char *);
typedef esp_gsp_err_t (*visible_setter_t)(esp_gsp_handle_t, bool);

struct app_panel {
    esp_gsp_handle_t ui;
    app_hub_handle_t hub;
    void *refresh_timer;
    size_t page;
    app_hub_snapshot_t snapshot;
    esp_gsp_err_t last_error;
    bool rendered;
    bool refresh_pending;
    uint8_t refresh_feedback_ticks;
    uint8_t refresh_wait_ticks;
    uint8_t active_bank;
    uint32_t refresh_generation;
    bool bank_valid[2];
    app_hub_snapshot_t bank_snapshot[2];
    bool detail_open;
    char detail_point_id[APP_HUB_POINT_ID_LEN];
    uint32_t detail_node_id;
    bool welcome_open;
    bool lang_open;
    bool wifi_open;
    bool wifi_password_open;
    bool wifi_password_pending;
    bool wifi_connect_submitted;
    bool wifi_scan_pending;
    bool wifi_error_dismissed;
    bool wifi_prompted;
    bool role_open;
    bool role_prompted;
    bool token_open;
    bool token_prompted;
    bool token_url_step;
    bool settings_open;
    bool add_open;
    bool add_commissioning;
    bool add_failed;
    bool add_field_error;
    bool add_status_shown;
    bool add_overlay_dirty;
    bool gsp_batch_failed;
    panel_keyboard_t keyboard_owner;
    bool home_chrome_ready;
    bool home_chrome_visible;
    app_hub_add_state_t add_painted_state;
    char add_painted_status[APP_HUB_STATUS_LEN];
    uint8_t add_success_ticks;
    app_hub_add_request_t add_request;
    app_hub_add_snapshot_t add_snapshot;
    uint8_t add_step;
    bool add_protocol_selected;
    bool role_from_settings;
    bool wifi_from_settings;
    bool lang_from_settings;
    panel_lang_t lang;
    panel_theme_t theme;
    uint8_t welcome_ticks;
    uint8_t wifi_dot_ticks;
    uint8_t wifi_cursor_delay_ticks;
    uint8_t wifi_success_ticks;
    uint32_t wifi_generation;
    esp_gsp_list_t wifi_list;
    esp_gsp_list_t wifi_list_light;
    size_t wifi_aps_count;
    app_hub_wifi_ap_t wifi_aps[APP_HUB_WIFI_AP_LIMIT];
    char wifi_ssid[APP_HUB_SSID_LEN];
    panel_tab_t tab;
    bool tab_shell_ready;
    int8_t pending_device_swipe;
    bool reboot_confirm_open;
    reconnect_state_t reconnect_state;
    uint16_t reconnect_ticks;
    app_hub_scenes_snapshot_t scenes;
    app_hub_rules_snapshot_t rules;
    app_hub_system_snapshot_t system;
    bool scene_building;
    bool scene_picking;
    bool scene_empty_notice;
    uint8_t scene_member_count;
    int8_t scene_member_point[APP_HUB_PAGE_SIZE];
    uint8_t scene_member_action[APP_HUB_PAGE_SIZE];
    bool rule_building;
    bool rule_save_needed;
    int8_t rule_source;
    int8_t rule_target;
    bool rule_target_on;
    char rule_source_value[APP_HUB_VALUE_LEN];
    uint8_t builder_name_mode;
    char builder_name[APP_HUB_NAME_LEN];
};

static const char *const k_copy[COPY_COUNT][2] = {
    [COPY_SETUP] = { "SETUP", "设置" },
    [COPY_HOME_TITLE] = { "My Home", "我的家" },
    [COPY_HOME_SUBTITLE] = { "Powered by MiniHub", "由 MiniHub 驱动" },
    [COPY_HOME_DEVICES] = { "DEVICES", "设备" },
    [COPY_HOME_ONLINE] = { "ONLINE", "在线" },
    [COPY_LANGUAGE] = { "Language", "语言" },
    [COPY_DEFAULT] = { "Default", "默认" },
    [COPY_CONTINUE] = { "Continue", "继续" },
    [COPY_NETWORK] = { "NETWORK", "网络" },
    [COPY_SCANNING] = { "Scanning...", "正在搜索..." },
    [COPY_SELECT_NET] = { "Select a network", "选择网络" },
    [COPY_SCAN_FAILED] = { "Scan failed", "搜索错误" },
    [COPY_CONNECTING] = { "Connecting...", "正在连接..." },
    [COPY_CONNECT_TIMEOUT] = { "Connection timed out", "连接超时" },
    [COPY_CONNECTED] = { "Connected", "已连接" },
    [COPY_PASSWORD] = { "Password", "密码" },
    [COPY_ENTER_PASSWORD] = { "Enter the Wi-Fi password", "输入 Wi-Fi 密码" },
    [COPY_WRONG_PASSWORD] = { "Wrong password", "密码错误" },
    [COPY_PRODUCT] = { "PRODUCT", "模式" },
    [COPY_CHOOSE_MODE] = { "Choose mode", "选择模式" },
    [COPY_WIFI_READY] = { "Wi-Fi is ready", "Wi-Fi 已就绪" },
    [COPY_HUB] = { "Hub", "Hub" },
    [COPY_HUB_META] = { "Run MiniHub on this Mosaico.", "在这台 Mosaico 上运行 MiniHub" },
    [COPY_PANEL] = { "Panel", "Panel" },
    [COPY_PANEL_META] = { "Connect to another MiniHub.", "连接其它 MiniHub" },
    [COPY_HUB_URL] = { "Hub URL", "Hub 地址" },
    [COPY_TOKEN] = { "MiniHub token", "MiniHub Token" },
    [COPY_TOKEN_META] = { "Enter the Bearer token", "输入 Bearer Token" },
    [COPY_SETTINGS] = { "Settings", "设置" },
    [COPY_SETTINGS_WIFI_META] = {
        "Choose another network", "更换 Wi-Fi 网络"
    },
    [COPY_DEVICE_ROLE] = { "Device mode", "设备模式" },
    [COPY_SETTINGS_ROLE_META] = {
        "Switch between Hub and Panel", "切换 Hub 或 Panel"
    },
    [COPY_ADD_CARD_TITLE] = { "Add device", "添加设备" },
    [COPY_ADD_CARD_META] = { "Tap to add", "轻触添加" },
    [COPY_PICK_DEVICE] = { "Tap to include", "轻触加入" },
    [COPY_NO_OTHER_DEVICE] = { "No other devices", "无其它设备" },
    [COPY_NO_OTHER_META] = { "Nothing left to add", "没有可添加的灯" },
    [COPY_ADD_TITLE] = { "Choose device type", "选择设备类型" },
    [COPY_ADD_META] = {
        "Only supported protocols are shown", "仅显示当前固件支持的协议"
    },
    [COPY_ADD_DISCOVERING] = { "Discovering...", "正在发现..." },
    [COPY_ADD_BINDING] = { "Binding...", "正在连接..." },
    [COPY_ADD_ACTIVATING] = { "Activating...", "正在入网..." },
    [COPY_ADD_SUCCESS] = { "Added", "入网完成" },
    [COPY_ADD_FAILED] = { "Add failed", "入网失败" },
    [COPY_CANCEL_PAIRING] = { "Cancel pairing", "取消入网" },
    [COPY_DELETE_DEVICE] = { "Delete device", "删除设备" },
    [COPY_DELETE] = { "Delete", "删除" },
    [COPY_DEVICES] = { "Devices", "设备" },
    [COPY_SCENES] = { "Scenes", "场景" },
    [COPY_AUTOMATION] = { "Automation", "自动化" },
    [COPY_TAP_SCENE] = { "Tap once to run", "轻触一次即可执行" },
    [COPY_TAP_AUTOMATION] = { "Tap to enable or disable", "轻触即可启用或停用" },
    [COPY_MODE] = { "Mode", "模式" },
    [COPY_RESTART] = { "Restart MiniHub", "重启 MiniHub" },
    [COPY_UPDATE_VIBE] = {
        "Updates use Mosaico Vibe Mode", "系统更新使用 Mosaico Vibe Mode"
    },
    [COPY_RESTART_QUESTION] = { "Restart MiniHub?", "重启 MiniHub？" },
    [COPY_RESTART_META] = {
        "Devices will be unavailable briefly.", "设备将短暂离线"
    },
    [COPY_TURN_ON] = { "Turn light on", "开启灯" },
    [COPY_TURN_OFF] = { "Turn light off", "关闭灯" },
    [COPY_CANCEL] = { "Cancel", "取消" },
    [COPY_CONFIRM_RESTART] = { "Restart", "重启" },
    [COPY_RECONNECT] = { "Reconnect device", "重连设备" },
    [COPY_RECONNECT_WAIT] = { "Reconnecting...", "正在重连" },
    [COPY_RECONNECT_OK] = { "Connected", "已连上" },
    [COPY_RECONNECT_FAIL] = { "Not connected", "未连上" },
    [COPY_QUEUE_FULL] = { "Queue full", "队列已满" },
    [COPY_APPEARANCE] = { "Appearance", "外观" },
    [COPY_DARK] = { "Dark", "深色" },
    [COPY_LIGHT] = { "Light", "浅色" },
};

static text_setter_t const card_name_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_name_set_text,
        gsp_panel_bank_0_card_1_name_set_text,
        gsp_panel_bank_0_card_2_name_set_text,
        gsp_panel_bank_0_card_3_name_set_text,
    },
    {
        gsp_panel_bank_1_card_0_name_set_text,
        gsp_panel_bank_1_card_1_name_set_text,
        gsp_panel_bank_1_card_2_name_set_text,
        gsp_panel_bank_1_card_3_name_set_text,
    },
};


static text_setter_t const card_name_light_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_name_light_set_text,
        gsp_panel_bank_0_card_1_name_light_set_text,
        gsp_panel_bank_0_card_2_name_light_set_text,
        gsp_panel_bank_0_card_3_name_light_set_text,
    },
    {
        gsp_panel_bank_1_card_0_name_light_set_text,
        gsp_panel_bank_1_card_1_name_light_set_text,
        gsp_panel_bank_1_card_2_name_light_set_text,
        gsp_panel_bank_1_card_3_name_light_set_text,
    },
};

static text_setter_t const card_meta_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_meta_set_text,
        gsp_panel_bank_0_card_1_meta_set_text,
        gsp_panel_bank_0_card_2_meta_set_text,
        gsp_panel_bank_0_card_3_meta_set_text,
    },
    {
        gsp_panel_bank_1_card_0_meta_set_text,
        gsp_panel_bank_1_card_1_meta_set_text,
        gsp_panel_bank_1_card_2_meta_set_text,
        gsp_panel_bank_1_card_3_meta_set_text,
    },
};

static text_setter_t const card_meta_light_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_meta_light_set_text,
        gsp_panel_bank_0_card_1_meta_light_set_text,
        gsp_panel_bank_0_card_2_meta_light_set_text,
        gsp_panel_bank_0_card_3_meta_light_set_text,
    },
    {
        gsp_panel_bank_1_card_0_meta_light_set_text,
        gsp_panel_bank_1_card_1_meta_light_set_text,
        gsp_panel_bank_1_card_2_meta_light_set_text,
        gsp_panel_bank_1_card_3_meta_light_set_text,
    },
};

static text_setter_t const card_value_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_value_set_text,
        gsp_panel_bank_0_card_1_value_set_text,
        gsp_panel_bank_0_card_2_value_set_text,
        gsp_panel_bank_0_card_3_value_set_text,
    },
    {
        gsp_panel_bank_1_card_0_value_set_text,
        gsp_panel_bank_1_card_1_value_set_text,
        gsp_panel_bank_1_card_2_value_set_text,
        gsp_panel_bank_1_card_3_value_set_text,
    },
};

static visible_setter_t const card_visible_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_set_visible,
        gsp_panel_bank_0_card_1_set_visible,
        gsp_panel_bank_0_card_2_set_visible,
        gsp_panel_bank_0_card_3_set_visible,
    },
    {
        gsp_panel_bank_1_card_0_set_visible,
        gsp_panel_bank_1_card_1_set_visible,
        gsp_panel_bank_1_card_2_set_visible,
        gsp_panel_bank_1_card_3_set_visible,
    },
};

static visible_setter_t const card_face_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_face_set_visible,
        gsp_panel_bank_0_card_1_face_set_visible,
        gsp_panel_bank_0_card_2_face_set_visible,
        gsp_panel_bank_0_card_3_face_set_visible,
    },
    {
        gsp_panel_bank_1_card_0_face_set_visible,
        gsp_panel_bank_1_card_1_face_set_visible,
        gsp_panel_bank_1_card_2_face_set_visible,
        gsp_panel_bank_1_card_3_face_set_visible,
    },
};

static visible_setter_t const card_add_ring_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_add_ring_set_visible,
        gsp_panel_bank_0_card_1_add_ring_set_visible,
        gsp_panel_bank_0_card_2_add_ring_set_visible,
        gsp_panel_bank_0_card_3_add_ring_set_visible,
    },
    {
        gsp_panel_bank_1_card_0_add_ring_set_visible,
        gsp_panel_bank_1_card_1_add_ring_set_visible,
        gsp_panel_bank_1_card_2_add_ring_set_visible,
        gsp_panel_bank_1_card_3_add_ring_set_visible,
    },
};

static visible_setter_t const card_add_hole_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_add_hole_set_visible,
        gsp_panel_bank_0_card_1_add_hole_set_visible,
        gsp_panel_bank_0_card_2_add_hole_set_visible,
        gsp_panel_bank_0_card_3_add_hole_set_visible,
    },
    {
        gsp_panel_bank_1_card_0_add_hole_set_visible,
        gsp_panel_bank_1_card_1_add_hole_set_visible,
        gsp_panel_bank_1_card_2_add_hole_set_visible,
        gsp_panel_bank_1_card_3_add_hole_set_visible,
    },
};

static visible_setter_t const card_add_plus_setters[2][APP_HUB_PAGE_SIZE] = {
    {
        gsp_panel_bank_0_card_0_add_plus_set_visible,
        gsp_panel_bank_0_card_1_add_plus_set_visible,
        gsp_panel_bank_0_card_2_add_plus_set_visible,
        gsp_panel_bank_0_card_3_add_plus_set_visible,
    },
    {
        gsp_panel_bank_1_card_0_add_plus_set_visible,
        gsp_panel_bank_1_card_1_add_plus_set_visible,
        gsp_panel_bank_1_card_2_add_plus_set_visible,
        gsp_panel_bank_1_card_3_add_plus_set_visible,
    },
};

static visible_setter_t const bank_visible_setters[2] = {
    gsp_panel_bank_0_set_visible,
    gsp_panel_bank_1_set_visible,
};

static const uint32_t card_name_keys[2][APP_HUB_PAGE_SIZE] = {
    {
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_0_NAME,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_1_NAME,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_2_NAME,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_3_NAME,
    },
    {
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_0_NAME,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_1_NAME,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_2_NAME,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_3_NAME,
    },
};


static const uint32_t card_name_light_keys[2][APP_HUB_PAGE_SIZE] = {
    {
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_0_NAME_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_1_NAME_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_2_NAME_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_3_NAME_LIGHT,
    },
    {
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_0_NAME_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_1_NAME_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_2_NAME_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_3_NAME_LIGHT,
    },
};

static const uint32_t card_meta_keys[2][APP_HUB_PAGE_SIZE] = {
    {
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_0_META,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_1_META,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_2_META,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_3_META,
    },
    {
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_0_META,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_1_META,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_2_META,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_3_META,
    },
};

static const uint32_t card_meta_light_keys[2][APP_HUB_PAGE_SIZE] = {
    {
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_0_META_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_1_META_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_2_META_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_3_META_LIGHT,
    },
    {
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_0_META_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_1_META_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_2_META_LIGHT,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_3_META_LIGHT,
    },
};

static const uint32_t card_value_keys[2][APP_HUB_PAGE_SIZE] = {
    {
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_0_VALUE,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_1_VALUE,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_2_VALUE,
        GSP_PANEL_OBJ_KEY_BANK_0_CARD_3_VALUE,
    },
    {
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_0_VALUE,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_1_VALUE,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_2_VALUE,
        GSP_PANEL_OBJ_KEY_BANK_1_CARD_3_VALUE,
    },
};

static visible_setter_t const wifi_dot_setters[3] = {
    gsp_panel_wifi_dot_0_set_visible,
    gsp_panel_wifi_dot_1_set_visible,
    gsp_panel_wifi_dot_2_set_visible,
};

static visible_setter_t const scene_visible_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_set_visible,
    gsp_panel_scene_1_set_visible,
    gsp_panel_scene_2_set_visible,
    gsp_panel_scene_3_set_visible,
};

static visible_setter_t const scene_face_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_face_set_visible,
    gsp_panel_scene_1_face_set_visible,
    gsp_panel_scene_2_face_set_visible,
    gsp_panel_scene_3_face_set_visible,
};

static const uint32_t scene_name_keys[APP_HUB_SCENES_PAGE_SIZE] = {
    GSP_PANEL_OBJ_KEY_SCENE_0_NAME, GSP_PANEL_OBJ_KEY_SCENE_1_NAME,
    GSP_PANEL_OBJ_KEY_SCENE_2_NAME, GSP_PANEL_OBJ_KEY_SCENE_3_NAME,
};

static const uint32_t scene_name_light_keys[APP_HUB_SCENES_PAGE_SIZE] = {
    GSP_PANEL_OBJ_KEY_SCENE_0_NAME_LIGHT, GSP_PANEL_OBJ_KEY_SCENE_1_NAME_LIGHT,
    GSP_PANEL_OBJ_KEY_SCENE_2_NAME_LIGHT, GSP_PANEL_OBJ_KEY_SCENE_3_NAME_LIGHT,
};

static const uint32_t scene_meta_keys[APP_HUB_SCENES_PAGE_SIZE] = {
    GSP_PANEL_OBJ_KEY_SCENE_0_META, GSP_PANEL_OBJ_KEY_SCENE_1_META,
    GSP_PANEL_OBJ_KEY_SCENE_2_META, GSP_PANEL_OBJ_KEY_SCENE_3_META,
};

static const uint32_t scene_meta_light_keys[APP_HUB_SCENES_PAGE_SIZE] = {
    GSP_PANEL_OBJ_KEY_SCENE_0_META_LIGHT, GSP_PANEL_OBJ_KEY_SCENE_1_META_LIGHT,
    GSP_PANEL_OBJ_KEY_SCENE_2_META_LIGHT, GSP_PANEL_OBJ_KEY_SCENE_3_META_LIGHT,
};

static visible_setter_t const scene_delete_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_delete_set_visible,
    gsp_panel_scene_1_delete_set_visible,
    gsp_panel_scene_2_delete_set_visible,
    gsp_panel_scene_3_delete_set_visible,
};

static visible_setter_t const scene_delete_face_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_delete_face_set_visible,
    gsp_panel_scene_1_delete_face_set_visible,
    gsp_panel_scene_2_delete_face_set_visible,
    gsp_panel_scene_3_delete_face_set_visible,
};

static visible_setter_t const scene_delete_text_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_delete_text_set_visible,
    gsp_panel_scene_1_delete_text_set_visible,
    gsp_panel_scene_2_delete_text_set_visible,
    gsp_panel_scene_3_delete_text_set_visible,
};

static text_setter_t const scene_delete_label_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_delete_text_set_text,
    gsp_panel_scene_1_delete_text_set_text,
    gsp_panel_scene_2_delete_text_set_text,
    gsp_panel_scene_3_delete_text_set_text,
};

static text_setter_t const scene_name_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_name_set_text,
    gsp_panel_scene_1_name_set_text,
    gsp_panel_scene_2_name_set_text,
    gsp_panel_scene_3_name_set_text,
};

static text_setter_t const scene_name_light_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_name_light_set_text,
    gsp_panel_scene_1_name_light_set_text,
    gsp_panel_scene_2_name_light_set_text,
    gsp_panel_scene_3_name_light_set_text,
};

static text_setter_t const scene_meta_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_meta_set_text,
    gsp_panel_scene_1_meta_set_text,
    gsp_panel_scene_2_meta_set_text,
    gsp_panel_scene_3_meta_set_text,
};

static text_setter_t const scene_meta_light_setters[APP_HUB_SCENES_PAGE_SIZE] = {
    gsp_panel_scene_0_meta_light_set_text,
    gsp_panel_scene_1_meta_light_set_text,
    gsp_panel_scene_2_meta_light_set_text,
    gsp_panel_scene_3_meta_light_set_text,
};


static visible_setter_t const rule_visible_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_set_visible,
    gsp_panel_rule_1_set_visible,
    gsp_panel_rule_2_set_visible,
    gsp_panel_rule_3_set_visible,
};

static visible_setter_t const rule_face_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_face_set_visible,
    gsp_panel_rule_1_face_set_visible,
    gsp_panel_rule_2_face_set_visible,
    gsp_panel_rule_3_face_set_visible,
};

static const uint32_t rule_name_keys[APP_HUB_RULES_PAGE_SIZE] = {
    GSP_PANEL_OBJ_KEY_RULE_0_NAME, GSP_PANEL_OBJ_KEY_RULE_1_NAME,
    GSP_PANEL_OBJ_KEY_RULE_2_NAME, GSP_PANEL_OBJ_KEY_RULE_3_NAME,
};

static const uint32_t rule_name_light_keys[APP_HUB_RULES_PAGE_SIZE] = {
    GSP_PANEL_OBJ_KEY_RULE_0_NAME_LIGHT, GSP_PANEL_OBJ_KEY_RULE_1_NAME_LIGHT,
    GSP_PANEL_OBJ_KEY_RULE_2_NAME_LIGHT, GSP_PANEL_OBJ_KEY_RULE_3_NAME_LIGHT,
};

static const uint32_t rule_state_keys[APP_HUB_RULES_PAGE_SIZE] = {
    GSP_PANEL_OBJ_KEY_RULE_0_STATE, GSP_PANEL_OBJ_KEY_RULE_1_STATE,
    GSP_PANEL_OBJ_KEY_RULE_2_STATE, GSP_PANEL_OBJ_KEY_RULE_3_STATE,
};

static const uint32_t rule_state_light_keys[APP_HUB_RULES_PAGE_SIZE] = {
    GSP_PANEL_OBJ_KEY_RULE_0_STATE_LIGHT, GSP_PANEL_OBJ_KEY_RULE_1_STATE_LIGHT,
    GSP_PANEL_OBJ_KEY_RULE_2_STATE_LIGHT, GSP_PANEL_OBJ_KEY_RULE_3_STATE_LIGHT,
};

static text_setter_t const rule_name_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_name_set_text,
    gsp_panel_rule_1_name_set_text,
    gsp_panel_rule_2_name_set_text,
    gsp_panel_rule_3_name_set_text,
};

static text_setter_t const rule_name_light_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_name_light_set_text,
    gsp_panel_rule_1_name_light_set_text,
    gsp_panel_rule_2_name_light_set_text,
    gsp_panel_rule_3_name_light_set_text,
};

static text_setter_t const rule_state_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_state_set_text,
    gsp_panel_rule_1_state_set_text,
    gsp_panel_rule_2_state_set_text,
    gsp_panel_rule_3_state_set_text,
};

static text_setter_t const rule_state_light_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_state_light_set_text,
    gsp_panel_rule_1_state_light_set_text,
    gsp_panel_rule_2_state_light_set_text,
    gsp_panel_rule_3_state_light_set_text,
};

static visible_setter_t const rule_delete_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_delete_set_visible,
    gsp_panel_rule_1_delete_set_visible,
    gsp_panel_rule_2_delete_set_visible,
    gsp_panel_rule_3_delete_set_visible,
};

static visible_setter_t const rule_delete_face_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_delete_face_set_visible,
    gsp_panel_rule_1_delete_face_set_visible,
    gsp_panel_rule_2_delete_face_set_visible,
    gsp_panel_rule_3_delete_face_set_visible,
};

static visible_setter_t const rule_delete_text_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_delete_text_set_visible,
    gsp_panel_rule_1_delete_text_set_visible,
    gsp_panel_rule_2_delete_text_set_visible,
    gsp_panel_rule_3_delete_text_set_visible,
};

static text_setter_t const rule_delete_label_setters[APP_HUB_RULES_PAGE_SIZE] = {
    gsp_panel_rule_0_delete_text_set_text,
    gsp_panel_rule_1_delete_text_set_text,
    gsp_panel_rule_2_delete_text_set_text,
    gsp_panel_rule_3_delete_text_set_text,
};

static const char *copy_text(const struct app_panel *panel, copy_id_t id)
{
    return k_copy[id][panel->lang == PANEL_LANG_ZH ? 1 : 0];
}

static const char *localized_status(const struct app_panel *panel,
                                    const char *status)
{
    if (panel->lang != PANEL_LANG_ZH || !status) {
        return status;
    }
    if (strncmp(status, "MiniHub online", strlen("MiniHub online")) == 0) {
        return "MiniHub 在线";
    }
    if (strcmp(status, "Connected") == 0) {
        return "已连接";
    }
    if (strcmp(status, "Connecting...") == 0) {
        return "正在连接...";
    }
    if (strcmp(status, "Connection timeout") == 0) {
        return "连接超时";
    }
    if (strcmp(status, "Wrong password") == 0) {
        return "密码错误";
    }
    if (strcmp(status, "Scan failed") == 0) {
        return "搜索错误";
    }
    if (strcmp(status, "Updated") == 0) {
        return "已更新";
    }
    if (strcmp(status, "Refreshing...") == 0) {
        return "正在刷新...";
    }
    if (strcmp(status, "Refresh failed") == 0) {
        return "刷新失败";
    }
    if (strcmp(status, "Command queued") == 0) {
        return "命令已发送";
    }
    if (strcmp(status, "Command queue full") == 0) {
        return "命令队列已满";
    }
    if (strcmp(status, "Device command failed") == 0) {
        return "设备命令失败";
    }
    if (strcmp(status, "Model busy") == 0) {
        return "系统繁忙";
    }
    if (strcmp(status, "Loading scenes...") == 0) {
        return "正在加载场景";
    }
    if (strcmp(status, "Starting scene...") == 0) {
        return "正在执行场景...";
    }
    if (strcmp(status, "Scene started") == 0) {
        return "场景已执行";
    }
    if (strcmp(status, "Scene failed") == 0) {
        return "场景执行失败";
    }
    if (strcmp(status, "Scene created and started") == 0) {
        return "场景已创建并执行";
    }
    if (strcmp(status, "Scene created; start failed") == 0) {
        return "场景已创建，执行失败";
    }
    if (strcmp(status, "Creating scene...") == 0) {
        return "正在创建场景...";
    }
    if (strcmp(status, "Scene create failed") == 0) {
        return "场景保存失败";
    }
    if (strcmp(status, "Scene created") == 0) {
        return "场景已保存，轻触执行";
    }
    if (strcmp(status, "Already in this state") == 0) {
        return "已是这个状态";
    }
    if (strcmp(status, "Deleting scene...") == 0) {
        return "正在删除场景...";
    }
    if (strcmp(status, "Scene deleted") == 0) {
        return "场景已删除";
    }
    if (strcmp(status, "Scene delete failed") == 0) {
        return "场景删除失败";
    }
    if (strcmp(status, "Could not start scene") == 0) {
        return "无法执行场景";
    }
    if (strcmp(status, "Tap a scene to run") == 0) {
        return "轻触执行，右侧删除";
    }
    if (strcmp(status, "Tap to enable or disable") == 0) {
        return "轻触启用或停用，右侧删除";
    }
    if (strcmp(status, "No automations") == 0) {
        return "暂无自动化，点右上角创建";
    }
    if (strcmp(status, "Saving automation...") == 0) {
        return "正在保存自动化...";
    }
    if (strcmp(status, "Creating a simple automation...") == 0) {
        return "正在创建自动化...";
    }
    if (strcmp(status, "Loading automations...") == 0) {
        return "正在加载自动化";
    }
    if (strcmp(status, "Automation enabled") == 0) {
        return "自动化已启用";
    }
    if (strcmp(status, "Automation disabled") == 0) {
        return "自动化已停用";
    }
    if (strcmp(status, "Automation update failed") == 0) {
        return "自动化更新失败";
    }
    if (strcmp(status, "Automation deleted") == 0) {
        return "自动化已删除";
    }
    if (strcmp(status, "Automation delete failed") == 0) {
        return "自动化删除失败";
    }
    if (strcmp(status, "Deleting automation...") == 0) {
        return "正在删除自动化...";
    }
    return status;
}

static bool is_light_point(const app_hub_point_t *point);
static bool is_onboard_imu(const app_hub_point_t *point);

static bool name_glyphs_supported(const char *text)
{
    static const char supported[] =
        "，。！？：；·（）中文简体设置语言默认继续网络正在搜索选择连接已密码"
        "输入错误模式就绪在这台上运行其它地址家里店里设备更换切换温度开关"
        "传感器添加码二维码发现入网完成失败请重试删除取消停除场景自动化"
        "轻触一次即可执行启用重系统更新使用短暂离线创建类型仅显示当前固件"
        "支持协议暂无点右上角填写有效内容面板页没有可控制状态保存常用至少"
        "需要两个正在客厅卧室厨房卫生间阳台书房门窗灯插座空调风扇温湿度"
        "烟雾水浸人体红外窗帘空气净化电视冰箱洗衣热水主次儿童老人宠物"
        "办公室会议仓库车库花园东西南北一二三楼将或时的超从令功务口名命"
        "安寄户据放数服称站端缩能装题忽略关闭开启作值再不为后我的由驱动"
        "刷新发送队列满繁忙先未触晃倾斜翻转静止卡片此侧";
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor) {
        if (*cursor < 0x80) {
            ++cursor;
            continue;
        }
        size_t length = (*cursor & 0xe0) == 0xc0 ? 2 :
                        ((*cursor & 0xf0) == 0xe0 ? 3 :
                         ((*cursor & 0xf8) == 0xf0 ? 4 : 0));
        if (length == 0) {
            return false;
        }
        char glyph[5] = {0};
        for (size_t i = 0; i < length; ++i) {
            if (cursor[i] == '\0' ||
                    (i > 0 && (cursor[i] & 0xc0) != 0x80)) {
                return false;
            }
            glyph[i] = (char)cursor[i];
        }
        if (!strstr(supported, glyph)) {
            return false;
        }
        cursor += length;
    }
    return true;
}

static bool name_is_generic(const char *name, const char *type, const char *id)
{
    static const char *const generic[] = {
        "light", "lamp", "bulb", "switch", "plug", "outlet", "sensor",
        "imu", "onoff", "relay", "temperature", "humidity", "motion",
        "pair", "test_product", "test_vendor",
    };
    if (!name || !name[0]) {
        return true;
    }
    for (const char *cursor = name; *cursor; ++cursor) {
        if ((unsigned char)*cursor >= 0x80) {
            return false;
        }
        if (*cursor == '.' || *cursor == '_') {
            return true;
        }
    }
    if ((type && strcasecmp(name, type) == 0) ||
            (id && strcasecmp(name, id) == 0)) {
        return true;
    }
    for (size_t i = 0; i < sizeof(generic) / sizeof(generic[0]); ++i) {
        if (strcasecmp(name, generic[i]) == 0) {
            return true;
        }
    }
    return false;
}

static const char *type_label(const struct app_panel *panel,
                              const app_hub_point_t *point)
{
    if (!point) {
        return panel->lang == PANEL_LANG_ZH ? "设备" : "Device";
    }
    if (is_onboard_imu(point)) {
        return panel->lang == PANEL_LANG_ZH ? "体感" : "Motion";
    }
    if (is_light_point(point) || strstr(point->type, "light") ||
            strstr(point->type, "bulb") || strstr(point->type, "lamp")) {
        return panel->lang == PANEL_LANG_ZH ? "灯" : "Light";
    }
    if (strstr(point->type, "switch") || strstr(point->type, "relay")) {
        return panel->lang == PANEL_LANG_ZH ? "开关" : "Switch";
    }
    if (strstr(point->type, "plug") || strstr(point->type, "outlet")) {
        return panel->lang == PANEL_LANG_ZH ? "插座" : "Plug";
    }
    if (strstr(point->type, "sensor")) {
        return panel->lang == PANEL_LANG_ZH ? "传感器" : "Sensor";
    }
    return panel->lang == PANEL_LANG_ZH ? "设备" : "Device";
}

static const char *protocol_label(const app_hub_point_t *point)
{
    if (!point) {
        return "";
    }
    if (is_onboard_imu(point) || strstr(point->point_id, "mosaico")) {
        return "";
    }
    if (strstr(point->point_id, "zigbee") || strstr(point->type, "zigbee")) {
        return "Zigbee";
    }
    if (strstr(point->point_id, "mesh") || strstr(point->point_id, "ble")) {
        return "BLE Mesh";
    }
    if (strstr(point->point_id, "mqtt")) {
        return "MQTT";
    }
    if (strstr(point->point_id, "modbus")) {
        return "Modbus";
    }
    if (point->node_id != 0 || strstr(point->point_id, "matter") ||
            is_light_point(point)) {
        return "Matter";
    }
    return "";
}

static const char *safe_name(const struct app_panel *panel, const char *name,
                             const char *id, char *output, size_t output_size)
{
    const char *candidate = name && name[0] ? name : id;
    if (candidate && candidate[0] && name_glyphs_supported(candidate) &&
            !name_is_generic(candidate, NULL, id)) {
        strlcpy(output, candidate, output_size);
    } else {
        snprintf(output, output_size, "%s",
                 panel->lang == PANEL_LANG_ZH ? "设备" : "Device");
    }
    return output;
}

static const char *point_display_name(const struct app_panel *panel,
                                      const app_hub_point_t *point,
                                      char *output, size_t output_size)
{
    if (!point) {
        strlcpy(output, panel->lang == PANEL_LANG_ZH ? "设备" : "Device",
                output_size);
        return output;
    }
    if (!name_is_generic(point->name, point->type, point->point_id) &&
            name_glyphs_supported(point->name)) {
        strlcpy(output, point->name, output_size);
        return output;
    }
    strlcpy(output, type_label(panel, point), output_size);
    return output;
}

static void format_point_meta(const struct app_panel *panel,
                              const app_hub_point_t *point,
                              char *meta, size_t meta_size)
{
    if (point->commission_failed) {
        strlcpy(meta, panel->lang == PANEL_LANG_ZH ? "失败" : "Failed",
                meta_size);
        return;
    }
    if (point->commissioning) {
        strlcpy(meta, panel->lang == PANEL_LANG_ZH ? "入网中" : "Pairing",
                meta_size);
        return;
    }
    if (!point->online) {
        strlcpy(meta, panel->lang == PANEL_LANG_ZH ? "离线" : "Offline",
                meta_size);
        return;
    }
    const char *protocol = protocol_label(point);
    strlcpy(meta, protocol[0] ? protocol :
            (panel->lang == PANEL_LANG_ZH ? "在线" : "Online"), meta_size);
}

static const char *safe_text(const char *text, const char *fallback,
                             char *output, size_t output_size)
{
    strlcpy(output,
            text && text[0] && name_glyphs_supported(text) ? text : fallback,
            output_size);
    return output;
}

static const char *point_value_text(const struct app_panel *panel,
                                    const app_hub_point_t *point,
                                    char *output, size_t output_size)
{
    if (point->commission_failed) {
        strlcpy(output, panel->lang == PANEL_LANG_ZH ? "失败" : "FAIL",
                output_size);
    } else if (point->commissioning) {
        strlcpy(output, panel->lang == PANEL_LANG_ZH ? "入网中" : "PAIR",
                output_size);
    } else if (is_onboard_imu(point)) {
        const char *gesture = point->value;
        if (strcmp(gesture, "shake") == 0) {
            strlcpy(output, panel->lang == PANEL_LANG_ZH ? "晃动" : "Shake",
                    output_size);
        } else if (strcmp(gesture, "tilt") == 0) {
            strlcpy(output, panel->lang == PANEL_LANG_ZH ? "倾斜" : "Tilt",
                    output_size);
        } else if (strcmp(gesture, "flip") == 0) {
            strlcpy(output, panel->lang == PANEL_LANG_ZH ? "翻转" : "Flip",
                    output_size);
        } else {
            strlcpy(output, panel->lang == PANEL_LANG_ZH ? "静止" : "Still",
                    output_size);
        }
    } else if (point->controllable) {
        strlcpy(output, point->on ?
                (panel->lang == PANEL_LANG_ZH ? "开" : "ON") :
                (panel->lang == PANEL_LANG_ZH ? "关" : "OFF"),
                output_size);
    } else if (point->value[0] && name_glyphs_supported(point->value)) {
        strlcpy(output, point->value, output_size);
    } else {
        strlcpy(output, "--", output_size);
    }
    return output;
}

static void remember_error(struct app_panel *panel, esp_gsp_err_t err)
{
    if (err != ESP_GSP_OK) {
        panel->last_error = err;
        panel->gsp_batch_failed = true;
    }
}

static const panel_theme_colors_t k_theme_colors[] = {
    [PANEL_THEME_DARK] = {
        .bg = 0x000000u,
        .card = 0x191B1Du,
        .key = 0x272A2Du,
        .ink = 0xF3F4F2u,
        .muted = 0xB2B7BAu,
        .line = 0x272A2Du,
        .accent = 0xFF4C01u,
        .cta = 0xFF4C01u,
    },
    [PANEL_THEME_LIGHT] = {
        .bg = 0xF6F6F3u,
        .card = 0xFFFFFFu,
        .key = 0xCECECAu,
        .ink = 0x101010u,
        .muted = 0x70706Cu,
        .line = 0xCBCBC6u,
        .accent = 0xFF4C01u,
        .cta = 0x101010u,
    },
};

static panel_theme_t load_theme_preference(void)
{
    uint8_t value = PANEL_THEME_DARK;
#if defined(ESP_PLATFORM)
    nvs_handle_t nvs;
    if (nvs_open(PANEL_NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        (void)nvs_get_u8(nvs, PANEL_NVS_THEME, &value);
        nvs_close(nvs);
    }
#else
    FILE *file = fopen(PANEL_THEME_HOST_PATH, "r");
    if (file) {
        unsigned parsed = 0;
        if (fscanf(file, "%u", &parsed) == 1 && parsed <= PANEL_THEME_LIGHT) {
            value = (uint8_t)parsed;
        }
        fclose(file);
    }
#endif
    return value == PANEL_THEME_LIGHT ? PANEL_THEME_LIGHT : PANEL_THEME_DARK;
}

static void save_theme_preference(panel_theme_t theme)
{
#if defined(ESP_PLATFORM)
    nvs_handle_t nvs;
    if (nvs_open(PANEL_NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    if (nvs_set_u8(nvs, PANEL_NVS_THEME, (uint8_t)theme) == ESP_OK) {
        (void)nvs_commit(nvs);
    }
    nvs_close(nvs);
#else
    FILE *file = fopen(PANEL_THEME_HOST_PATH, "w");
    if (!file) {
        return;
    }
    fprintf(file, "%u\n", (unsigned)theme);
    fclose(file);
#endif
}

static void update_tab_labels(struct app_panel *panel, bool write_text);
static void apply_welcome_marks(struct app_panel *panel);


static void set_card_shown(struct app_panel *panel, uint8_t bank, size_t slot,
                           bool visible)
{
    remember_error(panel, card_visible_setters[bank][slot](panel->ui, visible));
    remember_error(panel, card_face_setters[bank][slot](panel->ui, visible));
}

static void set_scene_row(struct app_panel *panel, size_t slot, bool visible)
{
    remember_error(panel, scene_visible_setters[slot](panel->ui, visible));
    remember_error(panel, scene_face_setters[slot](panel->ui, visible));
}

static void set_scene_delete_row(struct app_panel *panel, size_t slot, bool visible)
{
    remember_error(panel, scene_delete_setters[slot](panel->ui, visible));
    remember_error(panel, scene_delete_face_setters[slot](panel->ui, visible));
}

static void set_rule_row(struct app_panel *panel, size_t slot, bool visible)
{
    remember_error(panel, rule_visible_setters[slot](panel->ui, visible));
    remember_error(panel, rule_face_setters[slot](panel->ui, visible));
}

static void set_rule_delete_row(struct app_panel *panel, size_t slot, bool visible)
{
    remember_error(panel, rule_delete_setters[slot](panel->ui, visible));
    remember_error(panel, rule_delete_face_setters[slot](panel->ui, visible));
}


static void show_ink(struct app_panel *panel, uint32_t dark, uint32_t light,
                     bool visible)
{
    const bool theme_light = panel->theme == PANEL_THEME_LIGHT;
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, dark, visible && !theme_light));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, light, visible && theme_light));
}

static void sync_ink_pairs(struct app_panel *panel)
{
    static const uint32_t pairs[][2] = {
        { GSP_PANEL_OBJ_KEY_HOME_TITLE, GSP_PANEL_OBJ_KEY_HOME_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_TOTAL, GSP_PANEL_OBJ_KEY_TOTAL_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_0_CARD_0_NAME, GSP_PANEL_OBJ_KEY_BANK_0_CARD_0_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_0_CARD_1_NAME, GSP_PANEL_OBJ_KEY_BANK_0_CARD_1_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_0_CARD_2_NAME, GSP_PANEL_OBJ_KEY_BANK_0_CARD_2_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_0_CARD_3_NAME, GSP_PANEL_OBJ_KEY_BANK_0_CARD_3_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_1_CARD_0_NAME, GSP_PANEL_OBJ_KEY_BANK_1_CARD_0_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_1_CARD_1_NAME, GSP_PANEL_OBJ_KEY_BANK_1_CARD_1_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_1_CARD_2_NAME, GSP_PANEL_OBJ_KEY_BANK_1_CARD_2_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_1_CARD_3_NAME, GSP_PANEL_OBJ_KEY_BANK_1_CARD_3_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENES_TITLE, GSP_PANEL_OBJ_KEY_SCENES_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENE_0_NAME, GSP_PANEL_OBJ_KEY_SCENE_0_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENE_1_NAME, GSP_PANEL_OBJ_KEY_SCENE_1_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENE_2_NAME, GSP_PANEL_OBJ_KEY_SCENE_2_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENE_3_NAME, GSP_PANEL_OBJ_KEY_SCENE_3_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_AUTOMATION_TITLE, GSP_PANEL_OBJ_KEY_AUTOMATION_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_RULE_0_NAME, GSP_PANEL_OBJ_KEY_RULE_0_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_RULE_1_NAME, GSP_PANEL_OBJ_KEY_RULE_1_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_RULE_2_NAME, GSP_PANEL_OBJ_KEY_RULE_2_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_RULE_3_NAME, GSP_PANEL_OBJ_KEY_RULE_3_NAME_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_TITLE, GSP_PANEL_OBJ_KEY_SYSTEM_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_SUMMARY, GSP_PANEL_OBJ_KEY_SYSTEM_SUMMARY_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_WIFI_INK, GSP_PANEL_OBJ_KEY_SYSTEM_WIFI_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_ROLE_TEXT, GSP_PANEL_OBJ_KEY_SYSTEM_ROLE_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_LANGUAGE_TEXT, GSP_PANEL_OBJ_KEY_SYSTEM_LANGUAGE_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_THEME_TEXT, GSP_PANEL_OBJ_KEY_SYSTEM_THEME_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_TITLE, GSP_PANEL_OBJ_KEY_DETAIL_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_BRIGHTNESS_VALUE, GSP_PANEL_OBJ_KEY_DETAIL_BRIGHTNESS_VALUE_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_HUE_VALUE, GSP_PANEL_OBJ_KEY_DETAIL_HUE_VALUE_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_SATURATION_VALUE, GSP_PANEL_OBJ_KEY_DETAIL_SATURATION_VALUE_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_CCT_VALUE, GSP_PANEL_OBJ_KEY_DETAIL_CCT_VALUE_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_DELETE_TEXT, GSP_PANEL_OBJ_KEY_DETAIL_DELETE_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_BACK_INK, GSP_PANEL_OBJ_KEY_LANG_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_TITLE, GSP_PANEL_OBJ_KEY_LANG_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_EN_TEXT, GSP_PANEL_OBJ_KEY_LANG_EN_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_ZH_TEXT, GSP_PANEL_OBJ_KEY_LANG_ZH_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_BACK_INK, GSP_PANEL_OBJ_KEY_WIFI_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_TITLE, GSP_PANEL_OBJ_KEY_WIFI_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_RESULT_TITLE, GSP_PANEL_OBJ_KEY_WIFI_RESULT_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_BACK_INK, GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_TITLE, GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_BACK_INK, GSP_PANEL_OBJ_KEY_ROLE_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_TITLE, GSP_PANEL_OBJ_KEY_ROLE_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_HUB_TITLE, GSP_PANEL_OBJ_KEY_ROLE_HUB_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_WEB_TITLE, GSP_PANEL_OBJ_KEY_ROLE_WEB_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_TOKEN_BACK_INK, GSP_PANEL_OBJ_KEY_TOKEN_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_TOKEN_TITLE, GSP_PANEL_OBJ_KEY_TOKEN_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_BACK_INK, GSP_PANEL_OBJ_KEY_ADD_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_TITLE, GSP_PANEL_OBJ_KEY_ADD_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MATTER_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MATTER_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_ZIGBEE_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_ZIGBEE_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MESH_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MESH_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MQTT_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MQTT_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MODBUS_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MODBUS_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_RESULT_TITLE, GSP_PANEL_OBJ_KEY_ADD_RESULT_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_CANCEL_TEXT, GSP_PANEL_OBJ_KEY_ADD_CANCEL_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_REBOOT_CONFIRM_TITLE, GSP_PANEL_OBJ_KEY_REBOOT_CONFIRM_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_REBOOT_CANCEL_TEXT, GSP_PANEL_OBJ_KEY_REBOOT_CANCEL_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_HOME_SUBTITLE, GSP_PANEL_OBJ_KEY_HOME_SUBTITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_HOME_DEVICES, GSP_PANEL_OBJ_KEY_HOME_DEVICES_LIGHT },
        { GSP_PANEL_OBJ_KEY_HOME_ONLINE, GSP_PANEL_OBJ_KEY_HOME_ONLINE_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_0_CARD_0_META, GSP_PANEL_OBJ_KEY_BANK_0_CARD_0_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_0_CARD_1_META, GSP_PANEL_OBJ_KEY_BANK_0_CARD_1_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_0_CARD_2_META, GSP_PANEL_OBJ_KEY_BANK_0_CARD_2_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_0_CARD_3_META, GSP_PANEL_OBJ_KEY_BANK_0_CARD_3_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_1_CARD_0_META, GSP_PANEL_OBJ_KEY_BANK_1_CARD_0_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_1_CARD_1_META, GSP_PANEL_OBJ_KEY_BANK_1_CARD_1_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_1_CARD_2_META, GSP_PANEL_OBJ_KEY_BANK_1_CARD_2_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_BANK_1_CARD_3_META, GSP_PANEL_OBJ_KEY_BANK_1_CARD_3_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENES_STATUS, GSP_PANEL_OBJ_KEY_SCENES_STATUS_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENE_0_META, GSP_PANEL_OBJ_KEY_SCENE_0_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENE_1_META, GSP_PANEL_OBJ_KEY_SCENE_1_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENE_2_META, GSP_PANEL_OBJ_KEY_SCENE_2_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENE_3_META, GSP_PANEL_OBJ_KEY_SCENE_3_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_AUTOMATION_STATUS, GSP_PANEL_OBJ_KEY_AUTOMATION_STATUS_LIGHT },
        { GSP_PANEL_OBJ_KEY_RULE_0_STATE, GSP_PANEL_OBJ_KEY_RULE_0_STATE_LIGHT },
        { GSP_PANEL_OBJ_KEY_RULE_1_STATE, GSP_PANEL_OBJ_KEY_RULE_1_STATE_LIGHT },
        { GSP_PANEL_OBJ_KEY_RULE_2_STATE, GSP_PANEL_OBJ_KEY_RULE_2_STATE_LIGHT },
        { GSP_PANEL_OBJ_KEY_RULE_3_STATE, GSP_PANEL_OBJ_KEY_RULE_3_STATE_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_NETWORK, GSP_PANEL_OBJ_KEY_SYSTEM_NETWORK_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_MEMORY, GSP_PANEL_OBJ_KEY_SYSTEM_MEMORY_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_UPDATE_HINT, GSP_PANEL_OBJ_KEY_SYSTEM_UPDATE_HINT_LIGHT },
        { GSP_PANEL_OBJ_KEY_TAB_DEVICES_TEXT, GSP_PANEL_OBJ_KEY_TAB_DEVICES_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_TAB_SCENES_TEXT, GSP_PANEL_OBJ_KEY_TAB_SCENES_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_TAB_AUTOMATION_TEXT, GSP_PANEL_OBJ_KEY_TAB_AUTOMATION_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_TAB_SETTINGS_TEXT, GSP_PANEL_OBJ_KEY_TAB_SETTINGS_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_BACK_INK, GSP_PANEL_OBJ_KEY_DETAIL_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_META, GSP_PANEL_OBJ_KEY_DETAIL_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_BRIGHTNESS_CAPTION, GSP_PANEL_OBJ_KEY_DETAIL_BRIGHTNESS_CAPTION_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_HUE_CAPTION, GSP_PANEL_OBJ_KEY_DETAIL_HUE_CAPTION_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_SATURATION_CAPTION, GSP_PANEL_OBJ_KEY_DETAIL_SATURATION_CAPTION_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_CCT_CAPTION, GSP_PANEL_OBJ_KEY_DETAIL_CCT_CAPTION_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_KICKER, GSP_PANEL_OBJ_KEY_LANG_KICKER_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_EN_META, GSP_PANEL_OBJ_KEY_LANG_EN_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_ZH_META, GSP_PANEL_OBJ_KEY_LANG_ZH_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_KICKER, GSP_PANEL_OBJ_KEY_WIFI_KICKER_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_STATUS, GSP_PANEL_OBJ_KEY_WIFI_STATUS_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_RESULT_META, GSP_PANEL_OBJ_KEY_WIFI_RESULT_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_META, GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_KICKER, GSP_PANEL_OBJ_KEY_ROLE_KICKER_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_META, GSP_PANEL_OBJ_KEY_ROLE_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_HUB_META, GSP_PANEL_OBJ_KEY_ROLE_HUB_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_WEB_META, GSP_PANEL_OBJ_KEY_ROLE_WEB_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_TOKEN_META, GSP_PANEL_OBJ_KEY_TOKEN_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_META, GSP_PANEL_OBJ_KEY_ADD_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MATTER_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MATTER_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_ZIGBEE_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_ZIGBEE_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MESH_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MESH_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MQTT_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MQTT_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MODBUS_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MODBUS_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_RESULT_META, GSP_PANEL_OBJ_KEY_ADD_RESULT_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_REBOOT_CONFIRM_META, GSP_PANEL_OBJ_KEY_REBOOT_CONFIRM_META_LIGHT },
    };
    const bool theme_light = panel->theme == PANEL_THEME_LIGHT;
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
        bool dark_on = true;
        bool light_on = false;
        if (esp_gsp_component_get_visible(panel->ui, pairs[i][0],
                                          &dark_on) != ESP_GSP_OK) {
            dark_on = true;
        }
        if (esp_gsp_component_get_visible(panel->ui, pairs[i][1],
                                          &light_on) != ESP_GSP_OK) {
            light_on = false;
        }
        const bool on = dark_on || light_on;
        remember_error(panel, esp_gsp_component_set_visible(
                           panel->ui, pairs[i][0], on && !theme_light));
        remember_error(panel, esp_gsp_component_set_visible(
                           panel->ui, pairs[i][1], on && theme_light));
    }
}

static void set_card_name(struct app_panel *panel, uint8_t bank, size_t slot,
                          const char *text)
{
    remember_error(panel, card_name_setters[bank][slot](panel->ui, text));
    remember_error(panel, card_name_light_setters[bank][slot](panel->ui, text));
}

static void set_scene_name(struct app_panel *panel, size_t slot,
                           const char *text)
{
    remember_error(panel, scene_name_setters[slot](panel->ui, text));
    remember_error(panel, scene_name_light_setters[slot](panel->ui, text));
}

static void set_rule_name(struct app_panel *panel, size_t slot, const char *text)
{
    remember_error(panel, rule_name_setters[slot](panel->ui, text));
    remember_error(panel, rule_name_light_setters[slot](panel->ui, text));
}

static void set_card_meta(struct app_panel *panel, uint8_t bank, size_t slot,
                          const char *text)
{
    remember_error(panel, card_meta_setters[bank][slot](panel->ui, text));
    remember_error(panel, card_meta_light_setters[bank][slot](panel->ui, text));
}

static void set_scene_meta(struct app_panel *panel, size_t slot, const char *text)
{
    remember_error(panel, scene_meta_setters[slot](panel->ui, text));
    remember_error(panel, scene_meta_light_setters[slot](panel->ui, text));
}

static void set_rule_state(struct app_panel *panel, size_t slot, const char *text)
{
    remember_error(panel, rule_state_setters[slot](panel->ui, text));
    remember_error(panel, rule_state_light_setters[slot](panel->ui, text));
}


static text_setter_t ink_companion(text_setter_t dark)
{
    if (dark == gsp_panel_detail_brightness_value_set_text) {
        return gsp_panel_detail_brightness_value_light_set_text;
    }
    if (dark == gsp_panel_detail_hue_value_set_text) {
        return gsp_panel_detail_hue_value_light_set_text;
    }
    if (dark == gsp_panel_detail_saturation_value_set_text) {
        return gsp_panel_detail_saturation_value_light_set_text;
    }
    if (dark == gsp_panel_detail_cct_value_set_text) {
        return gsp_panel_detail_cct_value_light_set_text;
    }
    return NULL;
}

static void apply_welcome_marks(struct app_panel *panel)
{
    /* The splash images have their own visibility slots and paint even when
     * welcome_page is hidden. Only show the one that matches the open splash. */
    remember_error(panel, gsp_panel_welcome_mark_set_visible(
                       panel->ui, panel->welcome_open &&
                       panel->theme == PANEL_THEME_DARK));
    remember_error(panel, gsp_panel_welcome_mark_light_set_visible(
                       panel->ui, panel->welcome_open &&
                       panel->theme == PANEL_THEME_LIGHT));
}

static void apply_theme(struct app_panel *panel)
{
    const panel_theme_colors_t *colors = &k_theme_colors[panel->theme];
    char label[48];
    remember_error(panel, gsp_panel_theme_set_bg_rgb888(panel->ui, colors->bg));
    remember_error(panel, gsp_panel_theme_set_card_rgb888(panel->ui, colors->card));
    remember_error(panel, gsp_panel_theme_set_key_rgb888(panel->ui, colors->key));
    remember_error(panel, gsp_panel_theme_set_ink_rgb888(panel->ui, colors->ink));
    remember_error(panel, gsp_panel_theme_set_muted_rgb888(panel->ui, colors->muted));
    remember_error(panel, gsp_panel_theme_set_line_rgb888(panel->ui, colors->line));
    remember_error(panel, gsp_panel_theme_set_primary_rgb888(panel->ui, colors->cta));
    apply_welcome_marks(panel);
    snprintf(label, sizeof(label), "%s  %s",
             copy_text(panel, COPY_APPEARANCE),
             copy_text(panel, panel->theme == PANEL_THEME_DARK ?
                       COPY_DARK : COPY_LIGHT));
    remember_error(panel, gsp_panel_system_theme_text_set_text(panel->ui, label));
    remember_error(panel, gsp_panel_system_theme_text_light_set_text(panel->ui, label));
    sync_ink_pairs(panel);
    static const uint32_t always_on[][2] = {
        { GSP_PANEL_OBJ_KEY_SCENES_TITLE, GSP_PANEL_OBJ_KEY_SCENES_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_SCENES_STATUS, GSP_PANEL_OBJ_KEY_SCENES_STATUS_LIGHT },
        { GSP_PANEL_OBJ_KEY_AUTOMATION_TITLE, GSP_PANEL_OBJ_KEY_AUTOMATION_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_AUTOMATION_STATUS, GSP_PANEL_OBJ_KEY_AUTOMATION_STATUS_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_TITLE, GSP_PANEL_OBJ_KEY_SYSTEM_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_SUMMARY, GSP_PANEL_OBJ_KEY_SYSTEM_SUMMARY_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_NETWORK, GSP_PANEL_OBJ_KEY_SYSTEM_NETWORK_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_MEMORY, GSP_PANEL_OBJ_KEY_SYSTEM_MEMORY_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_WIFI_INK, GSP_PANEL_OBJ_KEY_SYSTEM_WIFI_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_ROLE_TEXT, GSP_PANEL_OBJ_KEY_SYSTEM_ROLE_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_LANGUAGE_TEXT, GSP_PANEL_OBJ_KEY_SYSTEM_LANGUAGE_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_THEME_TEXT, GSP_PANEL_OBJ_KEY_SYSTEM_THEME_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_SYSTEM_UPDATE_HINT, GSP_PANEL_OBJ_KEY_SYSTEM_UPDATE_HINT_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_BACK_INK, GSP_PANEL_OBJ_KEY_DETAIL_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_TITLE, GSP_PANEL_OBJ_KEY_DETAIL_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_META, GSP_PANEL_OBJ_KEY_DETAIL_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_BRIGHTNESS_CAPTION, GSP_PANEL_OBJ_KEY_DETAIL_BRIGHTNESS_CAPTION_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_HUE_CAPTION, GSP_PANEL_OBJ_KEY_DETAIL_HUE_CAPTION_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_SATURATION_CAPTION, GSP_PANEL_OBJ_KEY_DETAIL_SATURATION_CAPTION_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_CCT_CAPTION, GSP_PANEL_OBJ_KEY_DETAIL_CCT_CAPTION_LIGHT },
        { GSP_PANEL_OBJ_KEY_DETAIL_DELETE_TEXT, GSP_PANEL_OBJ_KEY_DETAIL_DELETE_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_BACK_INK, GSP_PANEL_OBJ_KEY_LANG_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_KICKER, GSP_PANEL_OBJ_KEY_LANG_KICKER_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_TITLE, GSP_PANEL_OBJ_KEY_LANG_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_EN_TEXT, GSP_PANEL_OBJ_KEY_LANG_EN_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_EN_META, GSP_PANEL_OBJ_KEY_LANG_EN_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_ZH_TEXT, GSP_PANEL_OBJ_KEY_LANG_ZH_TEXT_LIGHT },
        { GSP_PANEL_OBJ_KEY_LANG_ZH_META, GSP_PANEL_OBJ_KEY_LANG_ZH_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_BACK_INK, GSP_PANEL_OBJ_KEY_WIFI_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_KICKER, GSP_PANEL_OBJ_KEY_WIFI_KICKER_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_TITLE, GSP_PANEL_OBJ_KEY_WIFI_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_STATUS, GSP_PANEL_OBJ_KEY_WIFI_STATUS_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_RESULT_TITLE, GSP_PANEL_OBJ_KEY_WIFI_RESULT_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_RESULT_META, GSP_PANEL_OBJ_KEY_WIFI_RESULT_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_BACK_INK, GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_TITLE, GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_META, GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_BACK_INK, GSP_PANEL_OBJ_KEY_ROLE_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_KICKER, GSP_PANEL_OBJ_KEY_ROLE_KICKER_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_TITLE, GSP_PANEL_OBJ_KEY_ROLE_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_META, GSP_PANEL_OBJ_KEY_ROLE_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_HUB_TITLE, GSP_PANEL_OBJ_KEY_ROLE_HUB_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_HUB_META, GSP_PANEL_OBJ_KEY_ROLE_HUB_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_WEB_TITLE, GSP_PANEL_OBJ_KEY_ROLE_WEB_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ROLE_WEB_META, GSP_PANEL_OBJ_KEY_ROLE_WEB_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_TOKEN_BACK_INK, GSP_PANEL_OBJ_KEY_TOKEN_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_TOKEN_TITLE, GSP_PANEL_OBJ_KEY_TOKEN_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_TOKEN_META, GSP_PANEL_OBJ_KEY_TOKEN_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_BACK_INK, GSP_PANEL_OBJ_KEY_ADD_BACK_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_TITLE, GSP_PANEL_OBJ_KEY_ADD_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_META, GSP_PANEL_OBJ_KEY_ADD_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MATTER_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MATTER_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MATTER_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MATTER_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_ZIGBEE_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_ZIGBEE_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_ZIGBEE_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_ZIGBEE_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MESH_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MESH_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MESH_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MESH_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MQTT_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MQTT_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MQTT_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MQTT_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MODBUS_INK, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MODBUS_INK_LIGHT },
        { GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MODBUS_META, GSP_PANEL_OBJ_KEY_ADD_PROTOCOL_MODBUS_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_REBOOT_CONFIRM_TITLE, GSP_PANEL_OBJ_KEY_REBOOT_CONFIRM_TITLE_LIGHT },
        { GSP_PANEL_OBJ_KEY_REBOOT_CONFIRM_META, GSP_PANEL_OBJ_KEY_REBOOT_CONFIRM_META_LIGHT },
        { GSP_PANEL_OBJ_KEY_REBOOT_CANCEL_TEXT, GSP_PANEL_OBJ_KEY_REBOOT_CANCEL_TEXT_LIGHT },
    };
    for (size_t i = 0; i < sizeof(always_on) / sizeof(always_on[0]); ++i) {
        show_ink(panel, always_on[i][0], always_on[i][1], true);
    }
    update_tab_labels(panel, true);
}

static void set_card_add_mark(struct app_panel *panel, uint8_t bank, size_t slot,
                              bool visible)
{
    remember_error(panel, card_add_ring_setters[bank][slot](panel->ui, visible));
    remember_error(panel, card_add_hole_setters[bank][slot](panel->ui, visible));
    remember_error(panel, card_add_plus_setters[bank][slot](panel->ui, visible));
}

static void set_card_labels_visible(struct app_panel *panel, uint8_t bank,
                                    size_t slot, bool visible)
{
    show_ink(panel, card_name_keys[bank][slot],
             card_name_light_keys[bank][slot], visible);
    show_ink(panel, card_meta_keys[bank][slot],
             card_meta_light_keys[bank][slot], visible);
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, card_value_keys[bank][slot], visible));
}

static void hide_bank_cards(struct app_panel *panel, uint8_t bank)
{
    remember_error(panel, bank_visible_setters[bank](panel->ui, false));
}

static bool overlay_open(const struct app_panel *panel)
{
    return panel->welcome_open || panel->lang_open || panel->wifi_open ||
           panel->wifi_password_open || panel->role_open || panel->token_open ||
           panel->settings_open || panel->add_open ||
           panel->reboot_confirm_open;
}

static bool blocking_surface(const struct app_panel *panel)
{
    return overlay_open(panel) || panel->detail_open;
}

static bool setup_pending(const app_hub_snapshot_t *snapshot)
{
    return snapshot->role == APP_HUB_ROLE_UNSET ||
           snapshot->wifi_needs_setup ||
           snapshot->url_needed || snapshot->auth_needed;
}

static bool onboarding(const struct app_panel *panel)
{
    return panel->snapshot.role == APP_HUB_ROLE_UNSET;
}

static bool wrong_password(const char *status)
{
    return status != NULL && strcmp(status, "Wrong password") == 0;
}

static void switch_setup_page(struct app_panel *panel,
                              panel_setup_page_t page);

static void hide_hub_status_chip(struct app_panel *panel)
{
    remember_error(panel, gsp_panel_wifi_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_wifi_face_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_hub_status_dot_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_status_set_visible(panel->ui, false));
}

static void set_home_interactive(struct app_panel *panel, bool visible)
{
    if (panel->home_chrome_ready && panel->home_chrome_visible == visible) {
        return;
    }
    show_ink(panel, GSP_PANEL_OBJ_KEY_HOME_TITLE,
             GSP_PANEL_OBJ_KEY_HOME_TITLE_LIGHT, visible);
    show_ink(panel, GSP_PANEL_OBJ_KEY_HOME_SUBTITLE,
             GSP_PANEL_OBJ_KEY_HOME_SUBTITLE_LIGHT, visible);
    show_ink(panel, GSP_PANEL_OBJ_KEY_HOME_DEVICES,
             GSP_PANEL_OBJ_KEY_HOME_DEVICES_LIGHT, visible);
    show_ink(panel, GSP_PANEL_OBJ_KEY_HOME_ONLINE,
             GSP_PANEL_OBJ_KEY_HOME_ONLINE_LIGHT, visible);
    show_ink(panel, GSP_PANEL_OBJ_KEY_TOTAL, GSP_PANEL_OBJ_KEY_TOTAL_LIGHT,
             visible);
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, GSP_PANEL_OBJ_KEY_ONLINE, visible));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, GSP_PANEL_OBJ_KEY_PAGE, false));
    remember_error(panel, gsp_panel_previous_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_previous_face_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_next_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_next_face_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_refresh_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_refresh_face_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_device_swipe_set_visible(
                       panel->ui, visible && !panel->detail_open &&
                       panel->tab == PANEL_TAB_DEVICES));
    hide_hub_status_chip(panel);
    panel->home_chrome_visible = visible;
    panel->home_chrome_ready = true;
}

static void set_scenes_overlay_visible(struct app_panel *panel, bool visible);
static void set_automation_overlay_visible(struct app_panel *panel,
                                           bool visible);
static void render_bank(struct app_panel *panel, uint8_t bank,
                        const app_hub_snapshot_t *snapshot);
static bool same_page_layout(const app_hub_snapshot_t *left,
                             const app_hub_snapshot_t *right);
static void paint_device_bank(struct app_panel *panel,
                              const app_hub_snapshot_t *snapshot,
                              bool show);

static void set_shell_visible(struct app_panel *panel, bool visible)
{
    remember_error(panel, gsp_panel_tab_bar_set_visible(panel->ui, visible));
    if (!visible) {
        remember_error(panel, gsp_panel_device_swipe_set_visible(
                           panel->ui, false));
        set_scenes_overlay_visible(panel, false);
        set_automation_overlay_visible(panel, false);
        remember_error(panel, gsp_panel_system_page_set_visible(panel->ui, false));
    }
}

static void paint_tab(struct app_panel *panel, bool selected, const char *label,
                      bool write_text,
                      esp_gsp_err_t (*set_off_text)(esp_gsp_handle_t, const char *),
                      esp_gsp_err_t (*set_off_light_text)(esp_gsp_handle_t, const char *),
                      esp_gsp_err_t (*set_on_text)(esp_gsp_handle_t, const char *),
                      esp_gsp_err_t (*set_on_light_text)(esp_gsp_handle_t, const char *),
                      esp_gsp_err_t (*set_fill)(esp_gsp_handle_t, bool),
                      esp_gsp_err_t (*set_fill_light)(esp_gsp_handle_t, bool),
                      gsp_component_key_t off_key, gsp_component_key_t off_light_key,
                      gsp_component_key_t on_key, gsp_component_key_t on_light_key)
{
    const bool light = panel->theme == PANEL_THEME_LIGHT;
    if (write_text) {
        remember_error(panel, set_off_text(panel->ui, label));
        remember_error(panel, set_off_light_text(panel->ui, label));
        remember_error(panel, set_on_text(panel->ui, label));
        remember_error(panel, set_on_light_text(panel->ui, label));
    }
    remember_error(panel, set_fill(panel->ui, selected && !light));
    remember_error(panel, set_fill_light(panel->ui, selected && light));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, off_key, !selected && !light));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, off_light_key, !selected && light));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, on_key, selected && !light));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, on_light_key, selected && light));
}

static void paint_tab_id(struct app_panel *panel, panel_tab_t tab,
                         bool write_text)
{
    const bool selected = panel->tab == tab;
    switch (tab) {
    case PANEL_TAB_DEVICES:
        paint_tab(panel, selected, copy_text(panel, COPY_DEVICES), write_text,
                  gsp_panel_tab_devices_text_set_text,
                  gsp_panel_tab_devices_text_light_set_text,
                  gsp_panel_tab_devices_text_on_set_text,
                  gsp_panel_tab_devices_text_on_light_set_text,
                  gsp_panel_tab_devices_fill_set_visible,
                  gsp_panel_tab_devices_fill_light_set_visible,
                  GSP_PANEL_OBJ_KEY_TAB_DEVICES_TEXT,
                  GSP_PANEL_OBJ_KEY_TAB_DEVICES_TEXT_LIGHT,
                  GSP_PANEL_OBJ_KEY_TAB_DEVICES_TEXT_ON,
                  GSP_PANEL_OBJ_KEY_TAB_DEVICES_TEXT_ON_LIGHT);
        break;
    case PANEL_TAB_SCENES:
        paint_tab(panel, selected, copy_text(panel, COPY_SCENES), write_text,
                  gsp_panel_tab_scenes_text_set_text,
                  gsp_panel_tab_scenes_text_light_set_text,
                  gsp_panel_tab_scenes_text_on_set_text,
                  gsp_panel_tab_scenes_text_on_light_set_text,
                  gsp_panel_tab_scenes_fill_set_visible,
                  gsp_panel_tab_scenes_fill_light_set_visible,
                  GSP_PANEL_OBJ_KEY_TAB_SCENES_TEXT,
                  GSP_PANEL_OBJ_KEY_TAB_SCENES_TEXT_LIGHT,
                  GSP_PANEL_OBJ_KEY_TAB_SCENES_TEXT_ON,
                  GSP_PANEL_OBJ_KEY_TAB_SCENES_TEXT_ON_LIGHT);
        break;
    case PANEL_TAB_AUTOMATION:
        paint_tab(panel, selected, copy_text(panel, COPY_AUTOMATION), write_text,
                  gsp_panel_tab_automation_text_set_text,
                  gsp_panel_tab_automation_text_light_set_text,
                  gsp_panel_tab_automation_text_on_set_text,
                  gsp_panel_tab_automation_text_on_light_set_text,
                  gsp_panel_tab_automation_fill_set_visible,
                  gsp_panel_tab_automation_fill_light_set_visible,
                  GSP_PANEL_OBJ_KEY_TAB_AUTOMATION_TEXT,
                  GSP_PANEL_OBJ_KEY_TAB_AUTOMATION_TEXT_LIGHT,
                  GSP_PANEL_OBJ_KEY_TAB_AUTOMATION_TEXT_ON,
                  GSP_PANEL_OBJ_KEY_TAB_AUTOMATION_TEXT_ON_LIGHT);
        break;
    case PANEL_TAB_SETTINGS:
        paint_tab(panel, selected, copy_text(panel, COPY_SETTINGS), write_text,
                  gsp_panel_tab_settings_text_set_text,
                  gsp_panel_tab_settings_text_light_set_text,
                  gsp_panel_tab_settings_text_on_set_text,
                  gsp_panel_tab_settings_text_on_light_set_text,
                  gsp_panel_tab_settings_fill_set_visible,
                  gsp_panel_tab_settings_fill_light_set_visible,
                  GSP_PANEL_OBJ_KEY_TAB_SETTINGS_TEXT,
                  GSP_PANEL_OBJ_KEY_TAB_SETTINGS_TEXT_LIGHT,
                  GSP_PANEL_OBJ_KEY_TAB_SETTINGS_TEXT_ON,
                  GSP_PANEL_OBJ_KEY_TAB_SETTINGS_TEXT_ON_LIGHT);
        break;
    }
}

static void update_tab_labels(struct app_panel *panel, bool write_text)
{
    paint_tab_id(panel, PANEL_TAB_DEVICES, write_text);
    paint_tab_id(panel, PANEL_TAB_SCENES, write_text);
    paint_tab_id(panel, PANEL_TAB_AUTOMATION, write_text);
    paint_tab_id(panel, PANEL_TAB_SETTINGS, write_text);
}

static void hide_scene_slots(struct app_panel *panel);
static void hide_rule_slots(struct app_panel *panel);

static void set_scenes_overlay_visible(struct app_panel *panel, bool visible)
{
    remember_error(panel, gsp_panel_scenes_page_set_visible(panel->ui, visible));
}

static void set_automation_overlay_visible(struct app_panel *panel, bool visible)
{
    remember_error(panel, gsp_panel_automation_page_set_visible(panel->ui,
                                                                visible));
}

static void set_keyboard_pages_visible(struct app_panel *panel, bool visible,
                                       uint32_t keyboard_key,
                                       esp_err_t (*set_lower)(esp_gsp_handle_t,
                                                              bool),
                                       esp_err_t (*set_sym)(esp_gsp_handle_t,
                                                            bool),
                                       esp_err_t (*set_upper)(esp_gsp_handle_t,
                                                              bool))
{
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, keyboard_key, visible));
    /*
     * Keyboard pages have their own visibility slots. Parent set_visible
     * does not turn the default lower page on after hidden:true, and does
     * not hide a page that was shown independently. Always restore the
     * lower page as the only visible layout when a keyboard is reopened;
     * otherwise a previous symbol/upper page can overlap it.
     */
    remember_error(panel, set_lower(panel->ui, visible));
    remember_error(panel, set_sym(panel->ui, false));
    remember_error(panel, set_upper(panel->ui, false));
}

static void set_add_keyboard_visible(struct app_panel *panel, bool visible)
{
    set_keyboard_pages_visible(panel, visible,
                               GSP_PANEL_OBJ_KEY_ADD_KEYBOARD,
                               gsp_panel_add_keyboard_lower_set_visible,
                               gsp_panel_add_keyboard_sym_set_visible,
                               gsp_panel_add_keyboard_upper_set_visible);
}

static void set_wifi_keyboard_visible(struct app_panel *panel, bool visible)
{
    const bool light = visible && panel->theme == PANEL_THEME_LIGHT;
    const bool dark = visible && !light;
    remember_error(panel, gsp_panel_wifi_keyboard_host_set_visible(
                       panel->ui, dark));
    remember_error(panel, gsp_panel_wifi_keyboard_light_host_set_visible(
                       panel->ui, light));
    set_keyboard_pages_visible(panel, dark,
                               GSP_PANEL_OBJ_KEY_WIFI_KEYBOARD,
                               gsp_panel_wifi_keyboard_lower_set_visible,
                               gsp_panel_wifi_keyboard_sym_set_visible,
                               gsp_panel_wifi_keyboard_upper_set_visible);
    set_keyboard_pages_visible(panel, light,
                               GSP_PANEL_OBJ_KEY_WIFI_KEYBOARD_LIGHT,
                               gsp_panel_wifi_keyboard_light_lower_set_visible,
                               gsp_panel_wifi_keyboard_light_sym_set_visible,
                               gsp_panel_wifi_keyboard_light_upper_set_visible);
}

static void set_wifi_list_visible(struct app_panel *panel, bool visible)
{
    const bool light = visible && panel->theme == PANEL_THEME_LIGHT;
    const bool dark = visible && !light;
    const uint32_t total = visible ? (uint32_t)panel->wifi_aps_count : 0U;
    remember_error(panel, gsp_panel_wifi_list_set_visible(panel->ui, dark));
    remember_error(panel, gsp_panel_wifi_list_light_set_visible(panel->ui, light));
    if (panel->wifi_list != ESP_GSP_LIST_NONE) {
        remember_error(panel, esp_gsp_list_set_total(panel->ui, panel->wifi_list,
                                                     dark ? total : 0U));
        remember_error(panel, esp_gsp_list_refresh(panel->ui, panel->wifi_list));
    }
    if (panel->wifi_list_light != ESP_GSP_LIST_NONE) {
        remember_error(panel, esp_gsp_list_set_total(
                           panel->ui, panel->wifi_list_light,
                           light ? total : 0U));
        remember_error(panel, esp_gsp_list_refresh(panel->ui,
                                                  panel->wifi_list_light));
    }
    if (!visible) {
        for (size_t i = 0; i < 3; ++i) {
            remember_error(panel, wifi_dot_setters[i](panel->ui, false));
        }
    }
}

static void set_token_keyboard_visible(struct app_panel *panel, bool visible)
{
    set_keyboard_pages_visible(panel, visible,
                               GSP_PANEL_OBJ_KEY_TOKEN_KEYBOARD,
                               gsp_panel_token_keyboard_lower_set_visible,
                               gsp_panel_token_keyboard_sym_set_visible,
                               gsp_panel_token_keyboard_upper_set_visible);
}

static uint16_t keyboard_text_bind(panel_keyboard_t owner)
{
    switch (owner) {
    case PANEL_KEYBOARD_WIFI:
        return GSP_PANEL_BIND_WIFI_PASSWORD;
    case PANEL_KEYBOARD_TOKEN:
        return GSP_PANEL_BIND_TOKEN;
    case PANEL_KEYBOARD_ADD:
    default:
        return GSP_PANEL_BIND_ADD_CODE;
    }
}

static void detach_keyboard(struct app_panel *panel,
                            panel_keyboard_t expected_owner)
{
    if (panel->keyboard_owner != expected_owner) {
        return;
    }
    panel->wifi_cursor_delay_ticks = 0;
    remember_error(panel, esp_gsp_set_cursor(panel->ui, ESP_GSP_NO_CURSOR));
    remember_error(panel, esp_gsp_keyboard_attach(
                       panel->ui, ESP_GSP_KEYBOARD_NONE,
                       keyboard_text_bind(expected_owner)));
    panel->keyboard_owner = PANEL_KEYBOARD_NONE;
}

static void detach_active_keyboard(struct app_panel *panel)
{
    if (panel->keyboard_owner == PANEL_KEYBOARD_NONE) {
        return;
    }
    detach_keyboard(panel, panel->keyboard_owner);
}

static void attach_keyboard(struct app_panel *panel, panel_keyboard_t owner,
                            uint16_t action_id, uint16_t text_bind)
{
    const esp_gsp_err_t err =
        esp_gsp_keyboard_attach(panel->ui, action_id, text_bind);
    remember_error(panel, err);
    if (err == ESP_GSP_OK) {
        panel->keyboard_owner = owner;
    }
}

static void set_add_status_visible(struct app_panel *panel, bool visible,
                                   bool cancel)
{
    const bool failed_before = panel->gsp_batch_failed;
    panel->gsp_batch_failed = false;
    remember_error(panel, gsp_panel_add_status_page_set_visible(panel->ui, visible));
    remember_error(panel, gsp_panel_add_result_set_visible(panel->ui, visible));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, GSP_PANEL_OBJ_KEY_ADD_RESULT_TITLE, visible));
    remember_error(panel, gsp_panel_add_result_title_light_set_visible(
                       panel->ui, visible));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, GSP_PANEL_OBJ_KEY_ADD_RESULT_META, visible));
    remember_error(panel, gsp_panel_add_result_meta_light_set_visible(
                       panel->ui, visible));
    remember_error(panel, gsp_panel_add_cancel_set_visible(panel->ui, visible && cancel));
    remember_error(panel, gsp_panel_add_cancel_face_set_visible(panel->ui, visible && cancel));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, GSP_PANEL_OBJ_KEY_ADD_CANCEL_TEXT,
                       visible && cancel));
    remember_error(panel, gsp_panel_add_cancel_text_light_set_visible(
                       panel->ui, visible && cancel));
    if (visible) {
        panel->add_status_shown = true;
        panel->add_overlay_dirty = true;
    } else if (!panel->gsp_batch_failed) {
        panel->add_status_shown = false;
    } else {
        panel->add_overlay_dirty = true;
    }
    panel->gsp_batch_failed = failed_before || panel->gsp_batch_failed;
}

static void hide_add_surface(struct app_panel *panel)
{
    const bool failed_before = panel->gsp_batch_failed;
    panel->gsp_batch_failed = false;
    detach_keyboard(panel, PANEL_KEYBOARD_ADD);
    set_add_keyboard_visible(panel, false);
    remember_error(panel, gsp_panel_add_input_box_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_input_box_face_set_visible(panel->ui, false));
    set_add_status_visible(panel, false, false);
    remember_error(panel, gsp_panel_add_page_set_visible(panel->ui, false));
    if (!panel->gsp_batch_failed) {
        panel->add_status_shown = false;
        panel->add_overlay_dirty = false;
    } else {
        panel->add_overlay_dirty = true;
    }
    panel->gsp_batch_failed = failed_before || panel->gsp_batch_failed;
}

static void freeze_add_inputs(struct app_panel *panel)
{
    detach_keyboard(panel, PANEL_KEYBOARD_ADD);
    set_add_keyboard_visible(panel, false);
    remember_error(panel, gsp_panel_add_input_box_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_input_box_face_set_visible(panel->ui, false));
}

static void close_stale_add_overlay(struct app_panel *panel)
{
    if (panel->add_open) {
        return;
    }
    if (!panel->add_overlay_dirty && !panel->add_status_shown) {
        return;
    }
    hide_add_surface(panel);
}

static void attach_add_keyboard(struct app_panel *panel)
{
    remember_error(panel, gsp_panel_add_protocols_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_matter_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_matter_face_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_zigbee_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_zigbee_face_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_mesh_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_mesh_face_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_mqtt_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_mqtt_face_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_modbus_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_modbus_face_set_visible(panel->ui, false));
    set_add_status_visible(panel, false, false);
    remember_error(panel, gsp_panel_add_input_box_set_visible(panel->ui, true));
    remember_error(panel, gsp_panel_add_input_box_face_set_visible(panel->ui, true));
    set_add_keyboard_visible(panel, true);
    panel->add_overlay_dirty = true;
    remember_error(panel, esp_gsp_set_cursor(
                       panel->ui, GSP_PANEL_BIND_ADD_CODE));
    attach_keyboard(panel, PANEL_KEYBOARD_ADD,
                    GSP_PANEL_ACT_ID_ADD_KEYBOARD_KEY,
                    GSP_PANEL_BIND_ADD_CODE);
}

static void hide_blocking_overlays(struct app_panel *panel)
{
    hide_add_surface(panel);
    set_wifi_keyboard_visible(panel, false);
    set_wifi_list_visible(panel, false);
    remember_error(panel, gsp_panel_wifi_result_set_visible(panel->ui, false));
    set_token_keyboard_visible(panel, false);
    remember_error(panel, gsp_panel_wifi_password_page_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_welcome_page_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_lang_page_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_wifi_page_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_role_page_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_token_page_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_detail_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_reboot_confirm_set_visible(panel->ui, false));
}

static void show_tab(struct app_panel *panel, panel_tab_t tab)
{
    const panel_tab_t previous = panel->tab;
    const bool closing_overlay = overlay_open(panel) || panel->detail_open ||
                                 panel->reboot_confirm_open;
    panel->tab = tab;
    panel->detail_open = false;
    panel->detail_point_id[0] = '\0';
    panel->reboot_confirm_open = false;
    if (closing_overlay) {
        switch_setup_page(panel, PANEL_SETUP_NONE);
    }
    if (previous == tab && panel->tab_shell_ready && !closing_overlay) {
        return;
    }

    remember_error(panel, gsp_panel_scenes_page_set_visible(
                       panel->ui, tab == PANEL_TAB_SCENES));
    remember_error(panel, gsp_panel_automation_page_set_visible(
                       panel->ui, tab == PANEL_TAB_AUTOMATION));
    remember_error(panel, gsp_panel_system_page_set_visible(
                       panel->ui, tab == PANEL_TAB_SETTINGS));
    remember_error(panel, gsp_panel_device_swipe_set_visible(
                       panel->ui, tab == PANEL_TAB_DEVICES));

    if (!panel->tab_shell_ready) {
        paint_device_bank(panel, &panel->snapshot, true);
        set_shell_visible(panel, true);
        update_tab_labels(panel, false);
        panel->tab_shell_ready = true;
        return;
    }

    if (closing_overlay) {
        set_shell_visible(panel, true);
    }
    if (tab == PANEL_TAB_DEVICES) {
        paint_device_bank(panel, &panel->snapshot, true);
    }
    if (previous != tab) {
        paint_tab_id(panel, previous, false);
        paint_tab_id(panel, tab, false);
    }
}

static void apply_i18n(struct app_panel *panel)
{
    remember_error(panel, esp_gsp_component_set_text(
                       panel->ui, GSP_PANEL_OBJ_KEY_HOME_TITLE,
                       copy_text(panel, COPY_HOME_TITLE)));
    remember_error(panel, esp_gsp_component_set_text(
                       panel->ui, GSP_PANEL_OBJ_KEY_HOME_TITLE_LIGHT,
                       copy_text(panel, COPY_HOME_TITLE)));
    remember_error(panel, esp_gsp_component_set_text(
                       panel->ui, GSP_PANEL_OBJ_KEY_HOME_SUBTITLE,
                       copy_text(panel, COPY_HOME_SUBTITLE)));
    remember_error(panel, esp_gsp_component_set_text(
                       panel->ui, GSP_PANEL_OBJ_KEY_HOME_SUBTITLE_LIGHT,
                       copy_text(panel, COPY_HOME_SUBTITLE)));
    remember_error(panel, esp_gsp_component_set_text(
                       panel->ui, GSP_PANEL_OBJ_KEY_HOME_DEVICES,
                       copy_text(panel, COPY_HOME_DEVICES)));
    remember_error(panel, esp_gsp_component_set_text(
                       panel->ui, GSP_PANEL_OBJ_KEY_HOME_DEVICES_LIGHT,
                       copy_text(panel, COPY_HOME_DEVICES)));
    remember_error(panel, esp_gsp_component_set_text(
                       panel->ui, GSP_PANEL_OBJ_KEY_HOME_ONLINE,
                       copy_text(panel, COPY_HOME_ONLINE)));
    remember_error(panel, esp_gsp_component_set_text(
                       panel->ui, GSP_PANEL_OBJ_KEY_HOME_ONLINE_LIGHT,
                       copy_text(panel, COPY_HOME_ONLINE)));
    hide_hub_status_chip(panel);
    remember_error(panel, gsp_panel_lang_kicker_set_text(
                       panel->ui, copy_text(panel, COPY_SETUP)));
    remember_error(panel, gsp_panel_lang_kicker_light_set_text(
                       panel->ui, copy_text(panel, COPY_SETUP)));
    remember_error(panel, gsp_panel_lang_title_set_text(
                       panel->ui, copy_text(panel, COPY_LANGUAGE)));
    remember_error(panel, gsp_panel_lang_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_LANGUAGE)));
    remember_error(panel, gsp_panel_lang_en_meta_set_text(
                       panel->ui, copy_text(panel, COPY_DEFAULT)));
    remember_error(panel, gsp_panel_lang_en_meta_light_set_text(
                       panel->ui, copy_text(panel, COPY_DEFAULT)));
    remember_error(panel, gsp_panel_lang_continue_text_set_text(
                       panel->ui, copy_text(panel, COPY_CONTINUE)));
    remember_error(panel, gsp_panel_lang_en_mark_set_visible(
                       panel->ui, panel->lang == PANEL_LANG_EN));
    remember_error(panel, gsp_panel_lang_zh_mark_set_visible(
                       panel->ui, panel->lang == PANEL_LANG_ZH));
    remember_error(panel, gsp_panel_wifi_kicker_set_text(
                       panel->ui, copy_text(panel, COPY_NETWORK)));
    remember_error(panel, gsp_panel_wifi_kicker_light_set_text(
                       panel->ui, copy_text(panel, COPY_NETWORK)));
    remember_error(panel, gsp_panel_wifi_password_title_set_text(
                       panel->ui, copy_text(panel, COPY_PASSWORD)));
    remember_error(panel, gsp_panel_wifi_password_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_PASSWORD)));
    remember_error(panel, gsp_panel_role_kicker_set_text(
                       panel->ui, copy_text(panel, COPY_PRODUCT)));
    remember_error(panel, gsp_panel_role_kicker_light_set_text(
                       panel->ui, copy_text(panel, COPY_PRODUCT)));
    remember_error(panel, gsp_panel_role_title_set_text(
                       panel->ui, copy_text(panel, COPY_CHOOSE_MODE)));
    remember_error(panel, gsp_panel_role_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_CHOOSE_MODE)));
    remember_error(panel, gsp_panel_role_hub_title_set_text(
                       panel->ui, copy_text(panel, COPY_HUB)));
    remember_error(panel, gsp_panel_role_hub_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_HUB)));
    remember_error(panel, gsp_panel_role_hub_meta_set_text(
                       panel->ui, copy_text(panel, COPY_HUB_META)));
    remember_error(panel, gsp_panel_role_hub_meta_light_set_text(
                       panel->ui, copy_text(panel, COPY_HUB_META)));
    remember_error(panel, gsp_panel_role_web_title_set_text(
                       panel->ui, copy_text(panel, COPY_PANEL)));
    remember_error(panel, gsp_panel_role_web_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_PANEL)));
    remember_error(panel, gsp_panel_role_web_meta_set_text(
                       panel->ui, copy_text(panel, COPY_PANEL_META)));
    remember_error(panel, gsp_panel_role_web_meta_light_set_text(
                       panel->ui, copy_text(panel, COPY_PANEL_META)));
    {
        const bool web = panel->snapshot.role == APP_HUB_ROLE_WEB;
        remember_error(panel, gsp_panel_role_hub_mark_set_visible(panel->ui, !web));
        remember_error(panel, gsp_panel_role_web_mark_set_visible(panel->ui, web));
    }
    remember_error(panel, gsp_panel_add_protocol_matter_meta_set_text(
                       panel->ui, panel->lang == PANEL_LANG_ZH ?
                       "11/21 位码 · MT:" : "11/21 digits · MT:"));
    remember_error(panel, gsp_panel_add_protocol_matter_meta_light_set_text(
                       panel->ui, panel->lang == PANEL_LANG_ZH ?
                       "11/21 位码 · MT:" : "11/21 digits · MT:"));
    remember_error(panel, gsp_panel_add_protocol_zigbee_meta_set_text(
                       panel->ui, panel->lang == PANEL_LANG_ZH ?
                       "802.15.4 配网" : "802.15.4 pairing"));
    remember_error(panel, gsp_panel_add_protocol_zigbee_meta_light_set_text(
                       panel->ui, panel->lang == PANEL_LANG_ZH ?
                       "802.15.4 配网" : "802.15.4 pairing"));
    remember_error(panel, gsp_panel_add_protocol_mesh_meta_set_text(
                       panel->ui, panel->lang == PANEL_LANG_ZH ?
                       "蓝牙 Mesh 节点" : "Bluetooth Mesh node"));
    remember_error(panel, gsp_panel_add_protocol_mesh_meta_light_set_text(
                       panel->ui, panel->lang == PANEL_LANG_ZH ?
                       "蓝牙 Mesh 节点" : "Bluetooth Mesh node"));
    remember_error(panel, gsp_panel_add_protocol_mqtt_meta_set_text(
                       panel->ui, "Broker · Topic"));
    remember_error(panel, gsp_panel_add_protocol_mqtt_meta_light_set_text(
                       panel->ui, "Broker · Topic"));
    remember_error(panel, gsp_panel_add_protocol_modbus_meta_set_text(
                       panel->ui, panel->lang == PANEL_LANG_ZH ?
                       "网关 · 从站 · 寄存器" : "Gateway · slave · register"));
    remember_error(panel, gsp_panel_add_protocol_modbus_meta_light_set_text(
                       panel->ui, panel->lang == PANEL_LANG_ZH ?
                       "网关 · 从站 · 寄存器" : "Gateway · slave · register"));
    remember_error(panel, gsp_panel_add_title_set_text(
                       panel->ui, copy_text(panel, COPY_ADD_TITLE)));
    remember_error(panel, gsp_panel_add_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_ADD_TITLE)));
    remember_error(panel, gsp_panel_add_meta_set_text(
                       panel->ui, copy_text(panel, COPY_ADD_META)));
    remember_error(panel, gsp_panel_add_meta_light_set_text(
                       panel->ui, copy_text(panel, COPY_ADD_META)));
    remember_error(panel, gsp_panel_add_cancel_text_set_text(
                       panel->ui, copy_text(panel, COPY_CANCEL_PAIRING)));
    remember_error(panel, gsp_panel_add_cancel_text_light_set_text(
                       panel->ui, copy_text(panel, COPY_CANCEL_PAIRING)));
    remember_error(panel, gsp_panel_detail_delete_text_set_text(
                       panel->ui, copy_text(panel, COPY_DELETE_DEVICE)));
    remember_error(panel, gsp_panel_detail_delete_text_light_set_text(
                       panel->ui, copy_text(panel, COPY_DELETE_DEVICE)));
    remember_error(panel, gsp_panel_scenes_title_set_text(
                       panel->ui, copy_text(panel, COPY_SCENES)));
    remember_error(panel, gsp_panel_scenes_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_SCENES)));
    remember_error(panel, gsp_panel_scene_create_text_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ? "创建" : "Create"));
    {
        const char *delete_label = copy_text(panel, COPY_DELETE);
        for (size_t i = 0; i < APP_HUB_SCENES_PAGE_SIZE; ++i) {
            remember_error(panel, scene_delete_label_setters[i](
                               panel->ui, delete_label));
        }
        for (size_t i = 0; i < APP_HUB_RULES_PAGE_SIZE; ++i) {
            remember_error(panel, rule_delete_label_setters[i](
                               panel->ui, delete_label));
        }
    }
    remember_error(panel, gsp_panel_automation_title_set_text(
                       panel->ui, copy_text(panel, COPY_AUTOMATION)));
    remember_error(panel, gsp_panel_automation_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_AUTOMATION)));
    remember_error(panel, gsp_panel_automation_create_text_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ? "创建" : "Create"));
    remember_error(panel, gsp_panel_system_title_set_text(
                       panel->ui, copy_text(panel, COPY_SETTINGS)));
    remember_error(panel, gsp_panel_system_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_SETTINGS)));
    remember_error(panel, gsp_panel_system_role_text_set_text(
                       panel->ui, copy_text(panel, COPY_MODE)));
    remember_error(panel, gsp_panel_system_role_text_light_set_text(
                       panel->ui, copy_text(panel, COPY_MODE)));
    remember_error(panel, gsp_panel_system_language_text_set_text(
                       panel->ui, copy_text(panel, COPY_LANGUAGE)));
    remember_error(panel, gsp_panel_system_language_text_light_set_text(
                       panel->ui, copy_text(panel, COPY_LANGUAGE)));
    apply_theme(panel);
    remember_error(panel, gsp_panel_system_reboot_text_set_text(
                       panel->ui, copy_text(panel, COPY_RESTART)));
    remember_error(panel, gsp_panel_system_update_hint_set_text(
                       panel->ui, copy_text(panel, COPY_UPDATE_VIBE)));
    remember_error(panel, gsp_panel_system_update_hint_light_set_text(
                       panel->ui, copy_text(panel, COPY_UPDATE_VIBE)));
    remember_error(panel, gsp_panel_reboot_confirm_title_set_text(
                       panel->ui, copy_text(panel, COPY_RESTART_QUESTION)));
    remember_error(panel, gsp_panel_reboot_confirm_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_RESTART_QUESTION)));
    remember_error(panel, gsp_panel_reboot_confirm_meta_set_text(
                       panel->ui, copy_text(panel, COPY_RESTART_META)));
    remember_error(panel, gsp_panel_reboot_confirm_meta_light_set_text(
                       panel->ui, copy_text(panel, COPY_RESTART_META)));
    remember_error(panel, gsp_panel_reboot_cancel_text_set_text(
                       panel->ui, copy_text(panel, COPY_CANCEL)));
    remember_error(panel, gsp_panel_reboot_cancel_text_light_set_text(
                       panel->ui, copy_text(panel, COPY_CANCEL)));
    remember_error(panel, gsp_panel_reboot_ok_text_set_text(
                       panel->ui, copy_text(panel, COPY_CONFIRM_RESTART)));
    remember_error(panel, gsp_panel_detail_reconnect_text_set_text(
                       panel->ui, copy_text(panel, COPY_RECONNECT)));
    update_tab_labels(panel, true);
    panel->bank_valid[0] = false;
    panel->bank_valid[1] = false;
}

static bool point_equal(const app_hub_point_t *left,
                        const app_hub_point_t *right)
{
    return strcmp(left->point_id, right->point_id) == 0 &&
           strcmp(left->name, right->name) == 0 &&
           strcmp(left->type, right->type) == 0 &&
           strcmp(left->value, right->value) == 0 &&
           left->online == right->online &&
           left->controllable == right->controllable &&
           left->on == right->on &&
           left->commissioning == right->commissioning &&
           left->commission_failed == right->commission_failed &&
           left->node_id == right->node_id &&
           left->has_brightness == right->has_brightness &&
           left->brightness == right->brightness &&
           left->has_hue == right->has_hue &&
           left->hue == right->hue &&
           left->has_saturation == right->has_saturation &&
           left->saturation == right->saturation &&
           left->has_cct == right->has_cct &&
           left->cct == right->cct;
}

static bool snapshot_equal(const app_hub_snapshot_t *left,
                           const app_hub_snapshot_t *right)
{
    if (left->total_points != right->total_points ||
            left->online_points != right->online_points ||
            left->page != right->page ||
            left->page_count != right->page_count ||
            left->visible_points != right->visible_points ||
            left->connected != right->connected ||
            left->wifi_ready != right->wifi_ready ||
            left->wifi_needs_setup != right->wifi_needs_setup ||
            left->auth_needed != right->auth_needed ||
            left->role_needed != right->role_needed ||
            left->url_needed != right->url_needed ||
            left->role != right->role ||
            strcmp(left->status, right->status) != 0) {
        return false;
    }
    for (size_t i = 0; i < left->visible_points; ++i) {
        if (!point_equal(&left->points[i], &right->points[i])) {
            return false;
        }
    }
    return true;
}

static bool is_light_point(const app_hub_point_t *point)
{
    return point->has_brightness || point->has_hue ||
           point->has_saturation || point->has_cct ||
           strstr(point->type, "bulb") != NULL ||
           strstr(point->type, "light") != NULL;
}

static bool is_onboard_imu(const app_hub_point_t *point)
{
    return point && strcmp(point->point_id, "mosaico.imu") == 0;
}

static const char *next_imu_trigger(const char *current)
{
    if (current && strcmp(current, "shake") == 0) {
        return "tilt";
    }
    if (current && strcmp(current, "tilt") == 0) {
        return "flip";
    }
    return "shake";
}

static const char *imu_value_label(const struct app_panel *panel,
                                   const char *value)
{
    if (value && strcmp(value, "shake") == 0) {
        return panel->lang == PANEL_LANG_ZH ? "晃动" : "Shake";
    }
    if (value && strcmp(value, "tilt") == 0) {
        return panel->lang == PANEL_LANG_ZH ? "倾斜" : "Tilt";
    }
    if (value && strcmp(value, "flip") == 0) {
        return panel->lang == PANEL_LANG_ZH ? "翻转" : "Flip";
    }
    return panel->lang == PANEL_LANG_ZH ? "静止" : "Still";
}

static void capture_rule_source(struct app_panel *panel, int8_t index)
{
    const app_hub_point_t *point = &panel->snapshot.points[index];
    panel->rule_source = index;
    panel->rule_target = -1;
    panel->rule_save_needed = false;
    const char *value = "off";
    if (is_onboard_imu(point)) {
        value = "shake";
    } else if (point->controllable) {
        value = point->on ? "on" : "off";
    } else if (point->value[0]) {
        value = point->value;
    }
    strlcpy(panel->rule_source_value, value,
            sizeof(panel->rule_source_value));
}

static bool rule_builder_shows(const struct app_panel *panel,
                               const app_hub_point_t *point, size_t index)
{
    if (!point || point->commissioning || point->commission_failed) {
        return false;
    }
    if (panel->rule_source < 0) {
        return true;
    }
    return (int8_t)index == panel->rule_source || point->controllable;
}

static const app_hub_point_t *find_detail_point(
    const struct app_panel *panel, const app_hub_snapshot_t *snapshot)
{
    if (!panel->detail_open) {
        return NULL;
    }
    for (size_t i = 0; i < snapshot->visible_points; ++i) {
        if (strcmp(snapshot->points[i].point_id,
                   panel->detail_point_id) == 0) {
            return &snapshot->points[i];
        }
    }
    return NULL;
}

static void set_power_fill(struct app_panel *panel, bool accent)
{
    remember_error(panel, gsp_panel_detail_power_fill_set_visible(panel->ui, !accent));
    remember_error(panel, gsp_panel_detail_power_accent_set_visible(panel->ui, accent));
}

static void finish_reconnect(struct app_panel *panel, reconnect_state_t state,
                             copy_id_t copy)
{
    panel->reconnect_state = state;
    panel->reconnect_ticks = PANEL_RECONNECT_RESULT_TICKS;
    remember_error(panel, gsp_panel_detail_power_text_set_text(
                       panel->ui, copy_text(panel, copy)));
    set_power_fill(panel, false);
    remember_error(panel, gsp_panel_detail_meta_set_text(
                       panel->ui, copy_text(panel, copy)));
    remember_error(panel, gsp_panel_detail_meta_light_set_text(
                       panel->ui, copy_text(panel, copy)));
}

static void request_device_reconnect(struct app_panel *panel)
{
    if (panel->reconnect_state == RECONNECT_WAIT) {
        return;
    }
    if (!panel->snapshot.wifi_ready || !panel->snapshot.connected ||
            panel->detail_point_id[0] == '\0') {
        finish_reconnect(panel, RECONNECT_FAIL, COPY_RECONNECT_FAIL);
        return;
    }
    if (app_hub_reconnect(panel->hub, panel->detail_point_id) != APP_HUB_OK) {
        finish_reconnect(panel, RECONNECT_FAIL, COPY_QUEUE_FULL);
        return;
    }
    panel->reconnect_state = RECONNECT_WAIT;
    panel->reconnect_ticks = PANEL_RECONNECT_WAIT_TICKS;
    remember_error(panel, gsp_panel_detail_power_text_set_text(
                       panel->ui, copy_text(panel, COPY_RECONNECT_WAIT)));
    set_power_fill(panel, false);
    remember_error(panel, gsp_panel_detail_meta_set_text(
                       panel->ui, copy_text(panel, COPY_RECONNECT_WAIT)));
    remember_error(panel, gsp_panel_detail_meta_light_set_text(
                       panel->ui, copy_text(panel, COPY_RECONNECT_WAIT)));
}

static void show_reboot_confirm(struct app_panel *panel)
{
    panel->reboot_confirm_open = true;
    remember_error(panel, gsp_panel_reboot_confirm_title_set_text(
                       panel->ui, copy_text(panel, COPY_RESTART_QUESTION)));
    remember_error(panel, gsp_panel_reboot_confirm_title_light_set_text(
                       panel->ui, copy_text(panel, COPY_RESTART_QUESTION)));
    remember_error(panel, gsp_panel_reboot_confirm_meta_set_text(
                       panel->ui, copy_text(panel, COPY_RESTART_META)));
    remember_error(panel, gsp_panel_reboot_confirm_meta_light_set_text(
                       panel->ui, copy_text(panel, COPY_RESTART_META)));
    remember_error(panel, gsp_panel_reboot_cancel_text_set_text(
                       panel->ui, copy_text(panel, COPY_CANCEL)));
    remember_error(panel, gsp_panel_reboot_cancel_text_light_set_text(
                       panel->ui, copy_text(panel, COPY_CANCEL)));
    remember_error(panel, gsp_panel_reboot_ok_text_set_text(
                       panel->ui, copy_text(panel, COPY_CONFIRM_RESTART)));
    remember_error(panel, gsp_panel_reboot_confirm_set_visible(panel->ui, true));
}

static void set_detail_value(struct app_panel *panel, int32_t value,
                             esp_gsp_err_t (*set_value)(esp_gsp_handle_t,
                                                        int32_t),
                             text_setter_t set_text)
{
    char text[16];
    snprintf(text, sizeof(text), "%ld", (long)value);
    remember_error(panel, set_value(panel->ui, value));
    remember_error(panel, set_text(panel->ui, text));
    text_setter_t light_text = ink_companion(set_text);
    if (light_text) {
        remember_error(panel, light_text(panel->ui, text));
    }
}

static void set_detail_shell(struct app_panel *panel, bool open)
{
    remember_error(panel, gsp_panel_detail_set_visible(panel->ui, open));
    remember_error(panel, gsp_panel_tab_bar_set_visible(panel->ui, !open));
    remember_error(panel, gsp_panel_device_swipe_set_visible(
                       panel->ui, !open && panel->tab == PANEL_TAB_DEVICES));
}

static void close_detail(struct app_panel *panel)
{
    panel->detail_open = false;
    panel->detail_point_id[0] = '\0';
    panel->detail_node_id = 0;
    panel->reconnect_state = RECONNECT_IDLE;
    panel->reconnect_ticks = 0;
    remember_error(panel, gsp_panel_detail_reconnect_set_visible(panel->ui,
                                                                false));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, GSP_PANEL_OBJ_KEY_DETAIL_RECONNECT_TEXT,
                       false));
    set_detail_shell(panel, false);
}

static void render_detail(struct app_panel *panel,
                          const app_hub_point_t *point)
{
    if (!point) {
        close_detail(panel);
        return;
    }

    panel->detail_node_id = point->node_id;
    char display_name[APP_HUB_NAME_LEN];
    remember_error(panel, gsp_panel_detail_title_set_text(
                       panel->ui,
                       point_display_name(panel, point, display_name,
                                          sizeof(display_name))));
    remember_error(panel, gsp_panel_detail_title_light_set_text(
                       panel->ui,
                       point_display_name(panel, point, display_name,
                                          sizeof(display_name))));
    char meta[64];
    const char *state_label = point->online ?
                              (panel->lang == PANEL_LANG_ZH ? "在线" : "Online") :
                              (panel->lang == PANEL_LANG_ZH ? "离线" : "Offline");
    copy_id_t reconnect_copy = COPY_COUNT;
    if (panel->reconnect_state == RECONNECT_WAIT) {
        reconnect_copy = COPY_RECONNECT_WAIT;
        state_label = copy_text(panel, COPY_RECONNECT_WAIT);
    } else if (panel->reconnect_state == RECONNECT_OK) {
        reconnect_copy = COPY_RECONNECT_OK;
        state_label = copy_text(panel, COPY_RECONNECT_OK);
    } else if (panel->reconnect_state == RECONNECT_FAIL) {
        reconnect_copy = COPY_RECONNECT_FAIL;
        state_label = copy_text(panel, COPY_RECONNECT_FAIL);
    } else if (point->commission_failed) {
        state_label = panel->lang == PANEL_LANG_ZH ? "入网失败" : "Pairing failed";
    } else if (point->commissioning) {
        state_label = panel->lang == PANEL_LANG_ZH ? "入网中" : "Pairing...";
    }
    const char *protocol = protocol_label(point);
    if (protocol[0]) {
        snprintf(meta, sizeof(meta), "%s  ·  %s", protocol, state_label);
    } else {
        strlcpy(meta, state_label, sizeof(meta));
    }
    remember_error(panel, gsp_panel_detail_meta_set_text(panel->ui, meta));
    remember_error(panel, gsp_panel_detail_meta_light_set_text(panel->ui, meta));
    const bool can_power = point->controllable && !point->commissioning &&
                           !point->commission_failed;
    remember_error(panel, gsp_panel_detail_power_set_visible(
                       panel->ui, can_power));
    if (can_power) {
        copy_id_t power_copy = reconnect_copy;
        if (power_copy == COPY_COUNT) {
            power_copy = !point->online ? COPY_RECONNECT :
                         (point->on ? COPY_TURN_OFF : COPY_TURN_ON);
        }
        remember_error(panel, gsp_panel_detail_power_text_set_text(
                           panel->ui, copy_text(panel, power_copy)));
        set_power_fill(panel, power_copy == COPY_TURN_OFF);
    }
    remember_error(panel, gsp_panel_detail_delete_text_set_text(
                       panel->ui,
                       point->commissioning ?
                       copy_text(panel, COPY_CANCEL_PAIRING) :
                       copy_text(panel, COPY_DELETE_DEVICE)));
    remember_error(panel, gsp_panel_detail_delete_text_light_set_text(
                       panel->ui,
                       point->commissioning ?
                       copy_text(panel, COPY_CANCEL_PAIRING) :
                       copy_text(panel, COPY_DELETE_DEVICE)));
    remember_error(panel, gsp_panel_detail_delete_set_visible(
                       panel->ui, !is_onboard_imu(point)));
    remember_error(panel, gsp_panel_detail_delete_face_set_visible(
                       panel->ui, !is_onboard_imu(point)));
    remember_error(panel, gsp_panel_detail_reconnect_set_visible(
                       panel->ui, false));
    remember_error(panel, esp_gsp_component_set_visible(
                       panel->ui, GSP_PANEL_OBJ_KEY_DETAIL_RECONNECT_TEXT,
                       false));

    remember_error(panel, gsp_panel_detail_brightness_row_set_visible(
                       panel->ui, point->has_brightness));
    if (point->has_brightness) {
        set_detail_value(panel, point->brightness,
                         gsp_panel_detail_brightness_set_value,
                         gsp_panel_detail_brightness_value_set_text);
    }
    remember_error(panel, gsp_panel_detail_hue_row_set_visible(
                       panel->ui, point->has_hue));
    if (point->has_hue) {
        set_detail_value(panel, point->hue,
                         gsp_panel_detail_hue_set_value,
                         gsp_panel_detail_hue_value_set_text);
    }
    remember_error(panel, gsp_panel_detail_saturation_row_set_visible(
                       panel->ui, point->has_saturation));
    if (point->has_saturation) {
        set_detail_value(panel, point->saturation,
                         gsp_panel_detail_saturation_set_value,
                         gsp_panel_detail_saturation_value_set_text);
    }
    remember_error(panel, gsp_panel_detail_cct_row_set_visible(
                       panel->ui, point->has_cct));
    if (point->has_cct) {
        set_detail_value(panel, point->cct,
                         gsp_panel_detail_cct_set_value,
                         gsp_panel_detail_cct_value_set_text);
    }
}

static void close_token_page(struct app_panel *panel);
static void close_role_page(struct app_panel *panel);
static void open_role_page(struct app_panel *panel);
static void show_wifi_page(struct app_panel *panel, bool scan);
static void show_wifi_password_surface(struct app_panel *panel);
static void set_wifi_password_error(struct app_panel *panel, bool visible);
static void open_wifi_password(struct app_panel *panel,
                               const app_hub_wifi_ap_t *ap);

static gsp_err_t wifi_list_row(esp_gsp_handle_t ui, esp_gsp_row_t row,
                               uint32_t index, void *ctx)
{
    struct app_panel *panel = ctx;
    if (index >= panel->wifi_aps_count) {
        return esp_gsp_row_text(ui, row, "") == ESP_GSP_OK ?
               GSP_OK : GSP_ERR_INVALID_ARG;
    }
    const app_hub_wifi_ap_t *ap = &panel->wifi_aps[index];
    char safe_ssid[APP_HUB_SSID_LEN];
    char text[48];
    char fallback[24];
    snprintf(fallback, sizeof(fallback), "Wi-Fi %u",
             (unsigned)(index + 1));
    snprintf(text, sizeof(text), "%s%s",
             safe_text(ap->ssid, fallback, safe_ssid, sizeof(safe_ssid)),
             ap->secured ? "  ·" : "");
    return esp_gsp_row_text(ui, row, text) == ESP_GSP_OK ?
           GSP_OK : GSP_ERR_INVALID_ARG;
}

static void set_wifi_dots(struct app_panel *panel, bool scanning)
{
    if (!scanning) {
        for (size_t i = 0; i < 3; ++i) {
            remember_error(panel, wifi_dot_setters[i](panel->ui, false));
        }
        return;
    }
    const uint8_t on = (panel->wifi_dot_ticks / 4U) % 3U;
    for (size_t i = 0; i < 3; ++i) {
        remember_error(panel, wifi_dot_setters[i](panel->ui, i == on));
    }
}

static void render_wifi(struct app_panel *panel)
{
    app_hub_wifi_snapshot_t snapshot;
    if (app_hub_wifi_snapshot(panel->hub, 0, &snapshot) != APP_HUB_OK) {
        return;
    }
    panel->wifi_generation = snapshot.generation;
    panel->wifi_aps_count = snapshot.visible_aps;
    if (panel->wifi_aps_count > APP_HUB_WIFI_AP_LIMIT) {
        panel->wifi_aps_count = APP_HUB_WIFI_AP_LIMIT;
    }
    memcpy(panel->wifi_aps, snapshot.aps,
           panel->wifi_aps_count * sizeof(panel->wifi_aps[0]));

    if (panel->wifi_password_open) {
        if (wrong_password(snapshot.status) && !snapshot.connecting) {
            panel->wifi_connect_submitted = false;
            set_wifi_password_error(panel, true);
        }
        /* Keep the attached edit buffer. Re-attach / page-show every
         * refresh starts an empty buffer, so only the last key remains. */
        return;
    }

    if (!panel->wifi_error_dismissed && panel->wifi_ssid[0] != '\0' &&
            wrong_password(snapshot.status) && !snapshot.connecting) {
        app_hub_wifi_ap_t ap = {
            .secured = true,
        };
        panel->wifi_connect_submitted = false;
        strlcpy(ap.ssid, panel->wifi_ssid, sizeof(ap.ssid));
        open_wifi_password(panel, &ap);
        set_wifi_password_error(panel, true);
        return;
    }
    if (snapshot.ready) {
        panel->wifi_connect_submitted = false;
    }

    const bool scanning = panel->wifi_scan_pending || snapshot.scanning;
    const bool show_result = panel->wifi_connect_submitted ||
                             snapshot.connecting ||
                             (snapshot.ready && panel->wifi_success_ticks > 0);
    const char *status = copy_text(panel, COPY_SELECT_NET);
    if (panel->wifi_connect_submitted || snapshot.connecting) {
        status = copy_text(panel, COPY_CONNECTING);
    } else if (scanning) {
        status = copy_text(panel, COPY_SCANNING);
    } else if (strcmp(snapshot.status, "Connection timeout") == 0) {
        status = copy_text(panel, COPY_CONNECT_TIMEOUT);
    } else if (snapshot.ready) {
        status = copy_text(panel, COPY_CONNECTED);
    } else if (wrong_password(snapshot.status)) {
        status = copy_text(panel, COPY_WRONG_PASSWORD);
    } else if (snapshot.total_aps == 0 &&
               strcmp(snapshot.status, "Scan failed") == 0) {
        status = copy_text(panel, COPY_SCAN_FAILED);
    }
    char connected_status[APP_HUB_STATUS_LEN];
    if (snapshot.ready && snapshot.ssid[0] != '\0' && !snapshot.connecting) {
        char safe_ssid[APP_HUB_SSID_LEN];
        snprintf(connected_status, sizeof(connected_status),
                 panel->lang == PANEL_LANG_ZH ?
                 (scanning ? "已连接：%s · 正在扫描" : "已连接：%s") :
                 (scanning ? "Connected: %s · Scanning" :
                                      "Connected: %s"),
                 safe_text(snapshot.ssid, "Wi-Fi",
                           safe_ssid, sizeof(safe_ssid)));
        status = connected_status;
    }
    remember_error(panel, gsp_panel_wifi_status_set_text(panel->ui, status));
    remember_error(panel, gsp_panel_wifi_status_light_set_text(panel->ui, status));
    set_wifi_dots(panel, scanning && !show_result);

    const bool show_list = panel->wifi_open && !panel->wifi_password_open &&
                           !show_result;
    set_wifi_list_visible(panel, show_list);

    if (show_result) {
        char safe_ssid[APP_HUB_SSID_LEN];
        remember_error(panel, gsp_panel_wifi_result_title_set_text(
                           panel->ui,
                           snapshot.ready ? copy_text(panel, COPY_CONNECTED) :
                           copy_text(panel, COPY_CONNECTING)));
        remember_error(panel, gsp_panel_wifi_result_title_light_set_text(
                           panel->ui,
                           snapshot.ready ? copy_text(panel, COPY_CONNECTED) :
                           copy_text(panel, COPY_CONNECTING)));
        remember_error(panel, gsp_panel_wifi_result_meta_set_text(
                           panel->ui,
                           safe_text(panel->wifi_ssid, "Wi-Fi",
                                     safe_ssid, sizeof(safe_ssid))));
        remember_error(panel, gsp_panel_wifi_result_meta_light_set_text(
                           panel->ui,
                           safe_text(panel->wifi_ssid, "Wi-Fi",
                                     safe_ssid, sizeof(safe_ssid))));
    }
    remember_error(panel, gsp_panel_wifi_result_set_visible(panel->ui,
                                                            show_result));

    if (panel->wifi_ssid[0] != '\0') {
        char safe_ssid[APP_HUB_SSID_LEN];
        remember_error(panel, gsp_panel_wifi_password_title_set_text(
                           panel->ui,
                           safe_text(panel->wifi_ssid, "Wi-Fi",
                                     safe_ssid, sizeof(safe_ssid))));
        remember_error(panel, gsp_panel_wifi_password_title_light_set_text(
                           panel->ui,
                           safe_text(panel->wifi_ssid, "Wi-Fi",
                                     safe_ssid, sizeof(safe_ssid))));
    }
    remember_error(panel, gsp_panel_wifi_page_set_visible(
                       panel->ui,
                       panel->wifi_open || panel->wifi_password_open));
    remember_error(panel, gsp_panel_wifi_password_page_set_visible(
                       panel->ui, panel->wifi_password_open));

}

static void close_token_page(struct app_panel *panel)
{
    panel->token_open = false;
    set_token_keyboard_visible(panel, false);
    detach_keyboard(panel, PANEL_KEYBOARD_TOKEN);
    remember_error(panel, gsp_panel_token_page_set_visible(panel->ui, false));
}

static void close_role_page(struct app_panel *panel)
{
    panel->role_open = false;
    remember_error(panel, gsp_panel_role_page_set_visible(panel->ui, false));
}

static void switch_setup_page(struct app_panel *panel,
                              panel_setup_page_t page)
{
    /*
     * GSP paints later objects above earlier ones, and visibility commands
     * are queued asynchronously. Hide every modal (including keyboard pages
     * with independent slots) before showing the destination so a leftover
     * later page cannot stay on top.
     */
    detach_active_keyboard(panel);
    hide_blocking_overlays(panel);
    if (page != PANEL_SETUP_NONE) {
        set_shell_visible(panel, false);
    }
    switch (page) {
    case PANEL_SETUP_WELCOME:
        remember_error(panel, gsp_panel_welcome_page_set_visible(
                           panel->ui, true));
        break;
    case PANEL_SETUP_LANGUAGE:
        remember_error(panel, gsp_panel_lang_page_set_visible(
                           panel->ui, true));
        break;
    case PANEL_SETUP_WIFI:
        remember_error(panel, gsp_panel_wifi_page_set_visible(
                           panel->ui, true));
        break;
    case PANEL_SETUP_ROLE:
        remember_error(panel, gsp_panel_role_page_set_visible(
                           panel->ui, true));
        break;
    case PANEL_SETUP_TOKEN:
        remember_error(panel, gsp_panel_token_page_set_visible(
                           panel->ui, true));
        break;
    case PANEL_SETUP_SETTINGS:
        break;
    case PANEL_SETUP_ADD:
        remember_error(panel, gsp_panel_add_page_set_visible(
                           panel->ui, true));
        break;
    case PANEL_SETUP_NONE:
        break;
    }
    panel->welcome_open = page == PANEL_SETUP_WELCOME;
    panel->lang_open = page == PANEL_SETUP_LANGUAGE;
    panel->wifi_open = page == PANEL_SETUP_WIFI;
    panel->wifi_password_open = false;
    panel->wifi_password_pending = false;
    panel->role_open = page == PANEL_SETUP_ROLE;
    panel->token_open = page == PANEL_SETUP_TOKEN;
    panel->settings_open = page == PANEL_SETUP_SETTINGS;
    panel->add_open = page == PANEL_SETUP_ADD;
    if (panel->add_open) {
        panel->add_overlay_dirty = true;
    }
    panel->reboot_confirm_open = false;
    apply_welcome_marks(panel);
}

static void render_role(struct app_panel *panel)
{
    if (!panel->role_open) {
        return;
    }
    remember_error(panel, gsp_panel_role_back_set_visible(
                       panel->ui, panel->role_from_settings));
    remember_error(panel, gsp_panel_role_back_face_set_visible(
                       panel->ui, panel->role_from_settings));
    remember_error(panel, gsp_panel_role_meta_set_text(
                       panel->ui, copy_text(panel, COPY_WIFI_READY)));
    remember_error(panel, gsp_panel_role_meta_light_set_text(
                       panel->ui, copy_text(panel, COPY_WIFI_READY)));
    remember_error(panel, gsp_panel_role_page_set_visible(panel->ui, true));
}

static void open_role_page(struct app_panel *panel)
{
    panel->detail_open = false;
    panel->role_prompted = true;
    remember_error(panel, gsp_panel_detail_set_visible(panel->ui, false));
    set_home_interactive(panel, false);
    switch_setup_page(panel, PANEL_SETUP_ROLE);
    render_role(panel);
}

static void show_home_page(struct app_panel *panel)
{
    show_tab(panel, PANEL_TAB_DEVICES);
}

static void open_settings_page(struct app_panel *panel)
{
    panel->detail_open = false;
    panel->role_from_settings = false;
    panel->wifi_from_settings = false;
    panel->lang_from_settings = false;
    apply_i18n(panel);
    remember_error(panel, gsp_panel_detail_set_visible(panel->ui, false));
    show_tab(panel, PANEL_TAB_SETTINGS);
}

static const char *add_protocol_title(const struct app_panel *panel,
                                      app_hub_protocol_t protocol)
{
    (void)panel;
    switch (protocol) {
    case APP_HUB_PROTOCOL_MATTER: return "Matter";
    case APP_HUB_PROTOCOL_ZIGBEE: return "Zigbee";
    case APP_HUB_PROTOCOL_BLE_MESH: return "BLE Mesh";
    case APP_HUB_PROTOCOL_MQTT: return "MQTT";
    case APP_HUB_PROTOCOL_MODBUS_TCP: return "Modbus TCP";
    default: return "Device";
    }
}

static uint8_t add_field_count(app_hub_protocol_t protocol)
{
    switch (protocol) {
    case APP_HUB_PROTOCOL_MATTER: return 3;
    case APP_HUB_PROTOCOL_ZIGBEE: return 4;
    case APP_HUB_PROTOCOL_BLE_MESH: return 2;
    case APP_HUB_PROTOCOL_MQTT: return 10;
    case APP_HUB_PROTOCOL_MODBUS_TCP: return 12;
    default: return 0;
    }
}

static const char *add_field_prompt(const struct app_panel *panel,
                                    app_hub_protocol_t protocol, uint8_t step)
{
    static const char *const matter_en[] = {
        "Pairing code", "Device name", "Room (optional)"
    };
    static const char *const matter_zh[] = {
        "入网码", "设备名称", "房间（可选）"
    };
    static const char *const zigbee_en[] = {
        "IEEE address (optional)", "Install code (optional)",
        "Device name", "Room (optional)"
    };
    static const char *const zigbee_zh[] = {
        "IEEE 地址（可选）", "安装码（可选）",
        "设备名称", "房间（可选）"
    };
    static const char *const mesh_en[] = {
        "Device name", "Room (optional)"
    };
    static const char *const mesh_zh[] = {
        "设备名称", "房间（可选）"
    };
    static const char *const mqtt_en[] = {
        "Broker host", "Broker port", "Username (optional)",
        "Password (optional)", "Point ID", "Device type",
        "State topic", "Command topic (optional)",
        "Device name", "Room (optional)"
    };
    static const char *const mqtt_zh[] = {
        "服务器地址", "服务器端口", "用户名（可选）",
        "密码（可选）", "设备 ID", "设备类型",
        "状态主题", "命令主题（可选）",
        "设备名称", "房间（可选）"
    };
    static const char *const modbus_en[] = {
        "Gateway host", "TCP port", "Unit ID", "Point ID",
        "Device type", "Register function", "Register address",
        "Data type", "Scale", "Writable: 0 or 1",
        "Device name", "Room (optional)"
    };
    static const char *const modbus_zh[] = {
        "网关地址", "TCP 端口", "从站地址", "设备 ID",
        "设备类型", "寄存器功能", "寄存器地址",
        "数据类型", "缩放系数", "可写：0 或 1",
        "设备名称", "房间（可选）"
    };
    const bool zh = panel->lang == PANEL_LANG_ZH;
    switch (protocol) {
    case APP_HUB_PROTOCOL_MATTER:
        return zh ? matter_zh[step] : matter_en[step];
    case APP_HUB_PROTOCOL_ZIGBEE:
        return zh ? zigbee_zh[step] : zigbee_en[step];
    case APP_HUB_PROTOCOL_BLE_MESH:
        return zh ? mesh_zh[step] : mesh_en[step];
    case APP_HUB_PROTOCOL_MQTT:
        return zh ? mqtt_zh[step] : mqtt_en[step];
    case APP_HUB_PROTOCOL_MODBUS_TCP:
        return zh ? modbus_zh[step] : modbus_en[step];
    default: return "";
    }
}

static const char *add_field_default(const app_hub_add_request_t *request,
                                     uint8_t step)
{
    if (request->protocol == APP_HUB_PROTOCOL_MQTT) {
        switch (step) {
        case 1: return "1883";
        case 5: return "switch";
        default: return "";
        }
    }
    if (request->protocol == APP_HUB_PROTOCOL_MODBUS_TCP) {
        switch (step) {
        case 1: return "502";
        case 2: return "1";
        case 4: return "sensor";
        case 5: return "holding";
        case 6: return "0";
        case 7: return "u16";
        case 8: return "1";
        case 9: return "0";
        default: return "";
        }
    }
    return "";
}

static void render_add_field(struct app_panel *panel)
{
    const char *protocol = add_protocol_title(panel, panel->add_request.protocol);
    char title[48];
    char meta[96];
    snprintf(title, sizeof(title), panel->lang == PANEL_LANG_ZH ?
             "添加 %s 设备" : "Add %s", protocol);
    snprintf(meta, sizeof(meta), "%u/%u  %s",
             (unsigned)(panel->add_step + 1),
             (unsigned)add_field_count(panel->add_request.protocol),
             add_field_prompt(panel, panel->add_request.protocol,
                              panel->add_step));
    if (panel->add_request.protocol == APP_HUB_PROTOCOL_MATTER &&
            panel->add_step == 0) {
        app_hub_wifi_snapshot_t wifi = {0};
        if (app_hub_wifi_snapshot(panel->hub, 0, &wifi) == APP_HUB_OK &&
                wifi.ready && wifi.ssid[0]) {
            snprintf(meta, sizeof(meta),
                     panel->lang == PANEL_LANG_ZH ?
                     "1/3  入网码 · 加入当前 Wi-Fi：%s" :
                     "1/3  Pairing code · Current Wi-Fi: %s",
                     wifi.ssid);
        }
    }
    remember_error(panel, gsp_panel_add_title_set_text(panel->ui, title));
    remember_error(panel, gsp_panel_add_title_light_set_text(panel->ui, title));
    remember_error(panel, gsp_panel_add_meta_set_text(panel->ui, meta));
    remember_error(panel, gsp_panel_add_meta_light_set_text(panel->ui, meta));
    remember_error(panel, gsp_panel_add_code_set_text(
                       panel->ui,
                       add_field_default(&panel->add_request, panel->add_step)));
    remember_error(panel, gsp_panel_add_protocols_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_matter_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_matter_face_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_zigbee_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_zigbee_face_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_mesh_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_mesh_face_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_mqtt_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_mqtt_face_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_modbus_set_visible(
                       panel->ui, false));
    remember_error(panel, gsp_panel_add_protocol_modbus_face_set_visible(
                       panel->ui, false));
    attach_add_keyboard(panel);
}

static void select_add_protocol(struct app_panel *panel,
                                app_hub_protocol_t protocol)
{
    memset(&panel->add_request, 0, sizeof(panel->add_request));
    panel->add_request.protocol = protocol;
    panel->add_request.port =
        protocol == APP_HUB_PROTOCOL_MQTT ? 1883 :
        (protocol == APP_HUB_PROTOCOL_MODBUS_TCP ? 502 : 0);
    panel->add_request.unit_id = 1;
    panel->add_request.scale = 1.0f;
    panel->add_step = 0;
    panel->add_protocol_selected = true;
    render_add_field(panel);
}

static void locate_added_point(struct app_panel *panel, const char *point_id)
{
    if (!point_id || !point_id[0]) {
        return;
    }
    app_hub_snapshot_t snapshot;
    if (app_hub_snapshot(panel->hub, 0, &snapshot) != APP_HUB_OK) {
        return;
    }
    for (size_t page = 0; page < snapshot.page_count; ++page) {
        if (page != 0 &&
                app_hub_snapshot(panel->hub, page, &snapshot) != APP_HUB_OK) {
            return;
        }
        for (size_t i = 0; i < snapshot.visible_points; ++i) {
            if (strcmp(snapshot.points[i].point_id, point_id) == 0) {
                panel->page = page;
                return;
            }
        }
    }
}

static void render_add_page(struct app_panel *panel)
{
    if (!panel->add_open) {
        return;
    }
    (void)app_hub_add_snapshot(panel->hub, &panel->add_snapshot);

    if (panel->add_field_error) {
        return;
    }

    if (panel->add_commissioning) {
        app_hub_commission_snapshot_t commission = {0};
        if (panel->add_snapshot.protocol == APP_HUB_PROTOCOL_MATTER) {
            (void)app_hub_matter_commission_snapshot(panel->hub, &commission);
        }
        if (panel->add_status_shown &&
                panel->add_snapshot.state == panel->add_painted_state &&
                panel->add_snapshot.protocol != APP_HUB_PROTOCOL_MATTER &&
                panel->add_snapshot.state != APP_HUB_ADD_OK &&
                panel->add_snapshot.state != APP_HUB_ADD_FAILED &&
                panel->add_snapshot.state != APP_HUB_ADD_CANCELLED &&
                strcmp(panel->add_snapshot.status,
                       panel->add_painted_status) == 0) {
            return;
        }
        panel->add_painted_state = panel->add_snapshot.state;
        strlcpy(panel->add_painted_status, panel->add_snapshot.status,
                sizeof(panel->add_painted_status));
        if (panel->add_snapshot.state == APP_HUB_ADD_OK) {
            panel->add_commissioning = false;
            panel->add_failed = false;
            panel->add_success_ticks = PANEL_ADD_SUCCESS_TICKS;
            locate_added_point(panel, panel->add_snapshot.point_id);
            remember_error(panel, gsp_panel_add_result_title_set_text(
                               panel->ui, copy_text(panel, COPY_ADD_SUCCESS)));
            remember_error(panel, gsp_panel_add_result_title_light_set_text(
                               panel->ui, copy_text(panel, COPY_ADD_SUCCESS)));
            remember_error(panel, gsp_panel_add_result_meta_set_text(
                               panel->ui,
                               panel->add_snapshot.point_id[0] ?
                               panel->add_snapshot.point_id :
                               panel->add_snapshot.status));
            remember_error(panel, gsp_panel_add_result_meta_light_set_text(
                               panel->ui,
                               panel->add_snapshot.point_id[0] ?
                               panel->add_snapshot.point_id :
                               panel->add_snapshot.status));
            set_add_status_visible(panel, true, false);
            return;
        }
        if (panel->add_snapshot.state == APP_HUB_ADD_FAILED ||
                panel->add_snapshot.state == APP_HUB_ADD_CANCELLED) {
            panel->add_commissioning = false;
            panel->add_failed = true;
            remember_error(panel, gsp_panel_add_result_title_set_text(
                               panel->ui, copy_text(panel, COPY_ADD_FAILED)));
            remember_error(panel, gsp_panel_add_result_title_light_set_text(
                               panel->ui, copy_text(panel, COPY_ADD_FAILED)));
            remember_error(panel, gsp_panel_add_result_meta_set_text(
                               panel->ui, panel->add_snapshot.status));
            remember_error(panel, gsp_panel_add_result_meta_light_set_text(
                               panel->ui, panel->add_snapshot.status));
            remember_error(panel, gsp_panel_add_cancel_text_set_text(
                               panel->ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "重试" : "Retry"));
            remember_error(panel, gsp_panel_add_cancel_text_light_set_text(
                               panel->ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "重试" : "Retry"));
            set_add_status_visible(panel, true, true);
            return;
        }
        copy_id_t stage_copy = COPY_ADD_DISCOVERING;
        if (commission.stage == APP_HUB_COMMISSION_STAGE_BINDING) {
            stage_copy = COPY_ADD_BINDING;
        } else if (commission.stage == APP_HUB_COMMISSION_STAGE_ACTIVATING) {
            stage_copy = COPY_ADD_ACTIVATING;
        }
        remember_error(panel, gsp_panel_add_result_title_set_text(
                           panel->ui, copy_text(panel, stage_copy)));
        remember_error(panel, gsp_panel_add_result_title_light_set_text(
                           panel->ui, copy_text(panel, stage_copy)));
        remember_error(panel, gsp_panel_add_result_meta_set_text(
                           panel->ui, panel->add_snapshot.status));
        remember_error(panel, gsp_panel_add_result_meta_light_set_text(
                           panel->ui, panel->add_snapshot.status));
        remember_error(panel, gsp_panel_add_cancel_text_set_text(
                           panel->ui,
                           copy_text(panel, COPY_CANCEL_PAIRING)));
        remember_error(panel, gsp_panel_add_cancel_text_light_set_text(
                           panel->ui,
                           copy_text(panel, COPY_CANCEL_PAIRING)));
        set_add_status_visible(panel, true, true);
        return;
    }

    if (panel->add_success_ticks == 0 && !panel->add_failed &&
            !panel->add_field_error) {
        set_add_status_visible(panel, false, false);
    }
}

static void open_add_page(struct app_panel *panel)
{
    panel->detail_open = false;
    panel->add_commissioning = false;
    panel->add_failed = false;
    panel->add_field_error = false;
    panel->add_success_ticks = 0;
    panel->add_step = 0;
    panel->add_protocol_selected = false;
    memset(&panel->add_request, 0, sizeof(panel->add_request));
    remember_error(panel, gsp_panel_detail_set_visible(panel->ui, false));
    set_home_interactive(panel, false);
    remember_error(panel, gsp_panel_add_input_box_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_input_box_face_set_visible(panel->ui, false));
    set_add_keyboard_visible(panel, false);
    switch_setup_page(panel, PANEL_SETUP_ADD);
    remember_error(panel, gsp_panel_add_title_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ?
                       "选择设备类型" : "Choose device type"));
    remember_error(panel, gsp_panel_add_title_light_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ?
                       "选择设备类型" : "Choose device type"));
    remember_error(panel, gsp_panel_add_meta_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ?
                       "仅显示当前固件支持的协议" :
                       "Only supported protocols are shown"));
    remember_error(panel, gsp_panel_add_meta_light_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ?
                       "仅显示当前固件支持的协议" :
                       "Only supported protocols are shown"));
    remember_error(panel, gsp_panel_add_code_set_text(panel->ui, ""));
    remember_error(panel, gsp_panel_add_protocols_set_visible(panel->ui, true));
    remember_error(panel, gsp_panel_add_input_box_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_input_box_face_set_visible(panel->ui, false));
    set_add_keyboard_visible(panel, false);
    (void)app_hub_add_snapshot(panel->hub, &panel->add_snapshot);
    remember_error(panel, gsp_panel_add_protocol_matter_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_MATTER) != 0));
    remember_error(panel, gsp_panel_add_protocol_matter_face_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_MATTER) != 0));
    remember_error(panel, gsp_panel_add_protocol_zigbee_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_ZIGBEE) != 0));
    remember_error(panel, gsp_panel_add_protocol_zigbee_face_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_ZIGBEE) != 0));
    remember_error(panel, gsp_panel_add_protocol_mesh_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_BLE_MESH) != 0));
    remember_error(panel, gsp_panel_add_protocol_mesh_face_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_BLE_MESH) != 0));
    remember_error(panel, gsp_panel_add_protocol_mqtt_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_MQTT) != 0));
    remember_error(panel, gsp_panel_add_protocol_mqtt_face_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_MQTT) != 0));
    remember_error(panel, gsp_panel_add_protocol_modbus_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_MODBUS_TCP) != 0));
    remember_error(panel, gsp_panel_add_protocol_modbus_face_set_visible(
                       panel->ui,
                       (panel->add_snapshot.capabilities & APP_HUB_CAP_MODBUS_TCP) != 0));
    set_add_status_visible(panel, false, false);
}

static void open_builder_name(struct app_panel *panel, uint8_t mode)
{
    panel->builder_name_mode = mode;
    strlcpy(panel->builder_name,
            mode == 1 ?
            (panel->lang == PANEL_LANG_ZH ? "新场景" : "New scene") :
            (panel->lang == PANEL_LANG_ZH ? "新自动化" : "New automation"),
            sizeof(panel->builder_name));
    set_home_interactive(panel, false);
    switch_setup_page(panel, PANEL_SETUP_ADD);
    remember_error(panel, gsp_panel_add_title_set_text(
                       panel->ui,
                       mode == 1 ?
                       (panel->lang == PANEL_LANG_ZH ?
                        "场景名称" : "Scene name") :
                       (panel->lang == PANEL_LANG_ZH ?
                        "自动化名称" : "Automation name")));
    remember_error(panel, gsp_panel_add_title_light_set_text(
                       panel->ui,
                       mode == 1 ?
                       (panel->lang == PANEL_LANG_ZH ?
                        "场景名称" : "Scene name") :
                       (panel->lang == PANEL_LANG_ZH ?
                        "自动化名称" : "Automation name")));
    remember_error(panel, gsp_panel_add_meta_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ?
                       "输入名称后点添加设备" :
                       "Name it, then add devices"));
    remember_error(panel, gsp_panel_add_meta_light_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ?
                       "输入名称后点添加设备" :
                       "Name it, then add devices"));
    remember_error(panel, gsp_panel_add_code_set_text(
                       panel->ui,
                       panel->builder_name));
    remember_error(panel, gsp_panel_add_protocols_set_visible(panel->ui, false));
    attach_add_keyboard(panel);
}

static void maybe_open_role_after_wifi(struct app_panel *panel)
{
    /*
     * Role selection belongs only to the first-run flow. A Wi-Fi change
     * launched from Settings must keep its Settings return destination even
     * when the device has not completed onboarding yet.
     */
    if (panel->wifi_from_settings || !onboarding(panel) ||
            !panel->snapshot.wifi_ready ||
            panel->role_open || panel->wifi_password_open ||
            panel->wifi_success_ticks > 0) {
        return;
    }
    open_role_page(panel);
}

static void render_token(struct app_panel *panel)
{
    if (!panel->token_open) {
        return;
    }
    if (panel->token_url_step) {
        remember_error(panel, gsp_panel_token_title_set_text(
                           panel->ui, copy_text(panel, COPY_HUB_URL)));
        remember_error(panel, gsp_panel_token_title_light_set_text(
                           panel->ui, copy_text(panel, COPY_HUB_URL)));
        remember_error(panel, gsp_panel_token_meta_set_text(
                           panel->ui, "Example: 192.168.1.103"));
        remember_error(panel, gsp_panel_token_meta_light_set_text(
                           panel->ui, "Example: 192.168.1.103"));
    } else {
        remember_error(panel, gsp_panel_token_title_set_text(
                           panel->ui, copy_text(panel, COPY_TOKEN)));
        remember_error(panel, gsp_panel_token_title_light_set_text(
                           panel->ui, copy_text(panel, COPY_TOKEN)));
        remember_error(panel, gsp_panel_token_meta_set_text(
                           panel->ui, copy_text(panel, COPY_TOKEN_META)));
        remember_error(panel, gsp_panel_token_meta_light_set_text(
                           panel->ui, copy_text(panel, COPY_TOKEN_META)));
    }
    remember_error(panel, gsp_panel_token_page_set_visible(panel->ui, true));
}

static void open_token_page_with_back(struct app_panel *panel, bool show_back)
{
    panel->detail_open = false;
    panel->token_prompted = true;
    panel->token_url_step = panel->snapshot.url_needed;
    remember_error(panel, gsp_panel_detail_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_token_set_text(
                       panel->ui, panel->token_url_step ? "http://" : ""));
    remember_error(panel, gsp_panel_token_back_set_visible(panel->ui,
                                                          show_back));
    remember_error(panel, gsp_panel_token_back_face_set_visible(panel->ui,
                                                          show_back));
    set_home_interactive(panel, false);
    switch_setup_page(panel, PANEL_SETUP_TOKEN);
    set_token_keyboard_visible(panel, true);
    attach_keyboard(panel, PANEL_KEYBOARD_TOKEN,
                    GSP_PANEL_ACT_ID_TOKEN_KEYBOARD_KEY,
                    GSP_PANEL_BIND_TOKEN);
    render_token(panel);
}

static void open_token_page(struct app_panel *panel)
{
    open_token_page_with_back(panel, true);
}

static void request_wifi_scan(struct app_panel *panel)
{
    const app_hub_result_t result = app_hub_wifi_scan(panel->hub);
    panel->wifi_scan_pending = result != APP_HUB_OK;
    PANEL_LOGI("Wi-Fi scan request result=%d open=%d password_open=%d pending=%d",
               (int)result, panel->wifi_open, panel->wifi_password_open,
               panel->wifi_scan_pending);
    panel->wifi_dot_ticks = 0;
}

static void show_wifi_page(struct app_panel *panel, bool scan)
{
    panel->wifi_prompted = true;
    remember_error(panel, gsp_panel_detail_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_wifi_back_set_visible(
                       panel->ui, true));
    remember_error(panel, gsp_panel_wifi_back_face_set_visible(
                       panel->ui, true));
    set_home_interactive(panel, false);
    switch_setup_page(panel, PANEL_SETUP_WIFI);
    if (scan) {
        request_wifi_scan(panel);
    }
    render_wifi(panel);
}

static void open_wifi_page(struct app_panel *panel)
{
    show_wifi_page(panel, true);
}

static void open_lang_page(struct app_panel *panel)
{
    apply_i18n(panel);
    remember_error(panel, gsp_panel_lang_back_set_visible(
                       panel->ui, panel->lang_from_settings));
    remember_error(panel, gsp_panel_lang_back_face_set_visible(
                       panel->ui, panel->lang_from_settings));
    remember_error(panel, gsp_panel_detail_set_visible(panel->ui, false));
    set_home_interactive(panel, false);
    switch_setup_page(panel, PANEL_SETUP_LANGUAGE);
}

static void open_welcome_page(struct app_panel *panel)
{
    panel->welcome_ticks = 0;
    remember_error(panel, gsp_panel_detail_set_visible(panel->ui, false));
    set_home_interactive(panel, false);
    switch_setup_page(panel, PANEL_SETUP_WELCOME);
    remember_error(panel, gsp_panel_welcome_page_set_visible(panel->ui, true));
}

static void after_language(struct app_panel *panel)
{
    if (panel->lang_from_settings) {
        panel->lang_from_settings = false;
        open_settings_page(panel);
        return;
    }
    panel->wifi_from_settings = false;
    panel->role_from_settings = false;
    open_wifi_page(panel);
}

static void connect_selected_wifi(struct app_panel *panel, const char *password)
{
    if (panel->wifi_ssid[0] == '\0') {
        return;
    }
    if (app_hub_wifi_connect(panel->hub, panel->wifi_ssid, password) !=
            APP_HUB_OK) {
        remember_error(panel, gsp_panel_wifi_password_meta_set_text(
                           panel->ui, "Connect queue full"));
        remember_error(panel, gsp_panel_wifi_password_meta_light_set_text(
                           panel->ui, "Connect queue full"));
        return;
    }
    panel->wifi_connect_submitted = true;
    panel->wifi_password_open = false;
    panel->wifi_password_pending = false;
    panel->wifi_cursor_delay_ticks = 0;
    panel->wifi_open = true;
    /*
     * The keyboard hide is many visibility commands. Hide the page first
     * so a dark page background cannot sit in the keyboard slot while those
     * commands are still queued.
     */
    remember_error(panel, gsp_panel_wifi_password_page_set_visible(panel->ui,
                                                                  false));
    set_wifi_keyboard_visible(panel, false);
    detach_keyboard(panel, PANEL_KEYBOARD_WIFI);
    render_wifi(panel);
}

static void attach_wifi_keyboard(struct app_panel *panel)
{
    attach_keyboard(panel, PANEL_KEYBOARD_WIFI,
                    panel->theme == PANEL_THEME_LIGHT ?
                    GSP_PANEL_ACT_ID_WIFI_KEYBOARD_LIGHT_KEY :
                    GSP_PANEL_ACT_ID_WIFI_KEYBOARD_KEY,
                    GSP_PANEL_BIND_WIFI_PASSWORD);
}

static void set_wifi_password_error(struct app_panel *panel, bool visible)
{
    if (visible) {
        remember_error(panel, gsp_panel_wifi_password_error_set_text(
                           panel->ui,
                           panel->lang == PANEL_LANG_ZH ?
                           "密码错误，请重新输入" :
                           "Wrong password. Try again."));
    }
    remember_error(panel, gsp_panel_wifi_password_error_set_visible(
                       panel->ui, visible));
    show_ink(panel, GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_META,
             GSP_PANEL_OBJ_KEY_WIFI_PASSWORD_META_LIGHT, !visible);
}

static void show_wifi_password_surface(struct app_panel *panel)
{
    set_wifi_list_visible(panel, false);
    remember_error(panel, gsp_panel_wifi_result_set_visible(panel->ui, false));
    /*
     * Queue the keyboard before the page is revealed. The page background
     * is black in the dark theme; showing the page first leaves that
     * rectangle in the keyboard slot until the key pages are painted.
     */
    set_wifi_keyboard_visible(panel, true);
    remember_error(panel, gsp_panel_wifi_page_set_visible(panel->ui, true));
    remember_error(panel, gsp_panel_wifi_password_page_set_visible(panel->ui,
                                                                  true));
    attach_wifi_keyboard(panel);
}

static void open_wifi_password(struct app_panel *panel, const app_hub_wifi_ap_t *ap)
{
    snprintf(panel->wifi_ssid, sizeof(panel->wifi_ssid), "%s", ap->ssid);
    panel->wifi_open = true;
    panel->wifi_password_pending = false;
    panel->wifi_password_open = true;
    panel->wifi_connect_submitted = false;
    panel->wifi_error_dismissed = false;
    /*
     * Join detaches this keyboard before the failure returns. Input bindings
     * are applied asynchronously, and a caret hidden during the clear keeps
     * the previous text advance. Finish that session on the password bind,
     * then show the caret only after the empty field has been published.
     */
    if (panel->keyboard_owner == PANEL_KEYBOARD_NONE) {
        panel->wifi_cursor_delay_ticks = 0;
        remember_error(panel, esp_gsp_set_cursor(panel->ui, ESP_GSP_NO_CURSOR));
        remember_error(panel, esp_gsp_keyboard_attach(
                           panel->ui, ESP_GSP_KEYBOARD_NONE,
                           GSP_PANEL_BIND_WIFI_PASSWORD));
    } else {
        detach_keyboard(panel, PANEL_KEYBOARD_WIFI);
    }
    remember_error(panel, gsp_panel_wifi_password_set_text(panel->ui, ""));
    set_wifi_password_error(panel, false);
    show_wifi_password_surface(panel);
    char safe_ssid[APP_HUB_SSID_LEN];
    remember_error(panel, gsp_panel_wifi_password_title_set_text(
                       panel->ui,
                       safe_text(panel->wifi_ssid, "Wi-Fi",
                                 safe_ssid, sizeof(safe_ssid))));
    remember_error(panel, gsp_panel_wifi_password_title_light_set_text(
                       panel->ui,
                       safe_text(panel->wifi_ssid, "Wi-Fi",
                                 safe_ssid, sizeof(safe_ssid))));
    remember_error(panel, gsp_panel_wifi_password_meta_set_text(
                       panel->ui, copy_text(panel, COPY_ENTER_PASSWORD)));
    remember_error(panel, gsp_panel_wifi_password_meta_light_set_text(
                       panel->ui, copy_text(panel, COPY_ENTER_PASSWORD)));
    panel->wifi_cursor_delay_ticks = 2;
}

static void finish_open_wifi_password(struct app_panel *panel)
{
    if (!panel->wifi_password_pending) {
        return;
    }
    panel->wifi_password_pending = false;
    if (panel->wifi_ssid[0] == '\0') {
        return;
    }
    app_hub_wifi_ap_t ap = {0};
    strlcpy(ap.ssid, panel->wifi_ssid, sizeof(ap.ssid));
    ap.secured = true;
    open_wifi_password(panel, &ap);
}

static void select_wifi_ap(struct app_panel *panel, uint32_t index)
{
    if (index >= panel->wifi_aps_count) {
        return;
    }
    const app_hub_wifi_ap_t *ap = &panel->wifi_aps[index];
    snprintf(panel->wifi_ssid, sizeof(panel->wifi_ssid), "%s", ap->ssid);
    panel->wifi_scan_pending = false;
    panel->wifi_error_dismissed = false;
    if (ap->secured) {
        open_wifi_password(panel, ap);
    } else {
        connect_selected_wifi(panel, "");
    }
}

static bool can_create_scene(const app_hub_snapshot_t *snapshot)
{
    for (size_t i = 0; i < snapshot->visible_points; ++i) {
        if (snapshot->points[i].controllable) {
            return true;
        }
    }
    return false;
}

static bool can_create_rule(const app_hub_snapshot_t *snapshot)
{
    size_t controllable = 0;
    bool has_imu = false;
    for (size_t i = 0; i < snapshot->visible_points; ++i) {
        if (snapshot->points[i].controllable) {
            ++controllable;
        }
        if (is_onboard_imu(&snapshot->points[i])) {
            has_imu = true;
        }
    }
    return controllable >= 2 || (has_imu && controllable >= 1);
}

static const char *rule_create_blocked_hint(const struct app_panel *panel,
                                            const app_hub_snapshot_t *snapshot)
{
    if (can_create_scene(snapshot)) {
        return panel->lang == PANEL_LANG_ZH ?
               "再添加一台设备" : "Need another device";
    }
    return panel->lang == PANEL_LANG_ZH ?
           "请先添加可控制设备" : "Add a controllable device first";
}

static void refresh_home_snapshot(struct app_panel *panel)
{
    app_hub_snapshot_t snapshot;
    if (app_hub_snapshot(panel->hub, panel->page, &snapshot) == APP_HUB_OK) {
        panel->snapshot = snapshot;
    }
}

static int add_card_slot(const app_hub_snapshot_t *snapshot)
{
    const size_t start = snapshot->page * APP_HUB_PAGE_SIZE;
    if (snapshot->total_points >= start &&
            snapshot->total_points - start < APP_HUB_PAGE_SIZE) {
        return (int)(snapshot->total_points - start);
    }
    return -1;
}

static void render_bank(struct app_panel *panel, uint8_t bank,
                        const app_hub_snapshot_t *snapshot)
{
    const int add_slot = add_card_slot(snapshot);
    for (size_t i = 0; i < APP_HUB_PAGE_SIZE; ++i) {
        const bool is_add = (int)i == add_slot;
        const bool visible = i < snapshot->visible_points || is_add;
        set_card_shown(panel, bank, i, false);
        set_card_add_mark(panel, bank, i, false);
        set_card_labels_visible(panel, bank, i, false);
        if (!visible) {
            set_card_name(panel, bank, i, "");
            set_card_meta(panel, bank, i, "");
            remember_error(panel,
                           card_value_setters[bank][i](panel->ui, ""));
            continue;
        }
        if (is_add) {
            set_card_name(panel, bank, i, copy_text(panel, COPY_ADD_CARD_TITLE));
            set_card_meta(panel, bank, i, "");
            remember_error(panel,
                           card_value_setters[bank][i](panel->ui, ""));
            show_ink(panel, card_name_keys[bank][i],
                     card_name_light_keys[bank][i], true);
            show_ink(panel, card_meta_keys[bank][i],
                     card_meta_light_keys[bank][i], false);
            remember_error(panel, esp_gsp_component_set_visible(
                               panel->ui, card_value_keys[bank][i], false));
            set_card_add_mark(panel, bank, i, true);
            set_card_shown(panel, bank, i, true);
            continue;
        }

        const app_hub_point_t *point = &snapshot->points[i];
        char display_name[APP_HUB_NAME_LEN];
        set_card_name(panel, bank, i,
                      point_display_name(panel, point, display_name,
                                         sizeof(display_name)));
        char meta[64];
        format_point_meta(panel, point, meta, sizeof(meta));
        set_card_meta(panel, bank, i, meta);
        char value[16];
        remember_error(panel,
                       card_value_setters[bank][i](panel->ui,
                           point_value_text(panel, point, value,
                                            sizeof(value))));
        set_card_labels_visible(panel, bank, i, true);
        set_card_shown(panel, bank, i, true);
    }
    panel->bank_snapshot[bank] = *snapshot;
    panel->bank_valid[bank] = true;
}

static bool same_page_layout(const app_hub_snapshot_t *left,
                             const app_hub_snapshot_t *right)
{
    if (left->page != right->page ||
            left->visible_points != right->visible_points ||
            add_card_slot(left) != add_card_slot(right)) {
        return false;
    }
    for (size_t i = 0; i < left->visible_points; ++i) {
        if (strcmp(left->points[i].point_id, right->points[i].point_id) != 0) {
            return false;
        }
    }
    return true;
}

static void update_bank_in_place(struct app_panel *panel, uint8_t bank,
                                 const app_hub_snapshot_t *previous,
                                 const app_hub_snapshot_t *snapshot)
{
    for (size_t i = 0; i < snapshot->visible_points; ++i) {
        const app_hub_point_t *old_point = &previous->points[i];
        const app_hub_point_t *point = &snapshot->points[i];
        if (strcmp(old_point->name, point->name) != 0) {
            char display_name[APP_HUB_NAME_LEN];
            set_card_name(panel, bank, i,
                          point_display_name(panel, point, display_name,
                                             sizeof(display_name)));
        }
        if (strcmp(old_point->type, point->type) != 0 ||
                old_point->online != point->online ||
                old_point->controllable != point->controllable ||
                old_point->commissioning != point->commissioning ||
                old_point->commission_failed != point->commission_failed ||
                old_point->node_id != point->node_id) {
            char meta[64];
            format_point_meta(panel, point, meta, sizeof(meta));
            set_card_meta(panel, bank, i, meta);
        }
        if (old_point->on != point->on ||
                old_point->commissioning != point->commissioning ||
                old_point->commission_failed != point->commission_failed ||
                strcmp(old_point->value, point->value) != 0) {
            char value[16];
            remember_error(panel,
                           card_value_setters[bank][i](panel->ui,
                               point_value_text(panel, point, value,
                                                sizeof(value))));
        }
    }
    for (uint8_t other = 0; other < 2; ++other) {
        if (other != bank && panel->bank_valid[other] &&
                panel->bank_snapshot[other].page == snapshot->page) {
            panel->bank_valid[other] = false;
        }
    }
    panel->bank_snapshot[bank] = *snapshot;
    panel->bank_valid[bank] = true;
}

static int find_cached_bank(const struct app_panel *panel,
                            const app_hub_snapshot_t *snapshot)
{
    for (uint8_t bank = 0; bank < 2; ++bank) {
        if (panel->bank_valid[bank] &&
                snapshot_equal(snapshot, &panel->bank_snapshot[bank])) {
            return bank;
        }
    }
    return -1;
}

static void paint_device_bank(struct app_panel *panel,
                              const app_hub_snapshot_t *snapshot,
                              bool show)
{
    uint8_t target = panel->active_bank;
    const bool stale = !panel->bank_valid[target] ||
                       !same_page_layout(&panel->bank_snapshot[target], snapshot);
    if (stale) {
        const int cached = find_cached_bank(panel, snapshot);
        if (cached >= 0) {
            target = (uint8_t)cached;
        } else {
            if (show && panel->rendered && panel->bank_valid[panel->active_bank]) {
                target = (uint8_t)(panel->active_bank ^ 1U);
            }
            render_bank(panel, target, snapshot);
        }
        if (show && panel->rendered && target != panel->active_bank) {
            hide_bank_cards(panel, panel->active_bank);
        }
        panel->active_bank = target;
    } else if (show) {
        update_bank_in_place(panel, target, &panel->bank_snapshot[target],
                             snapshot);
    }
    if (show) {
        remember_error(panel,
                       bank_visible_setters[panel->active_bank](panel->ui, true));
        set_home_interactive(panel, true);
    }
}

static void prefetch_adjacent_page(struct app_panel *panel)
{
    size_t page = panel->snapshot.page;
    if (page + 1 < panel->snapshot.page_count) {
        ++page;
    } else if (page > 0) {
        --page;
    } else {
        return;
    }

    app_hub_snapshot_t adjacent;
    if (app_hub_snapshot(panel->hub, page, &adjacent) != APP_HUB_OK) {
        return;
    }
    const uint8_t bank = panel->active_bank ^ 1U;
    if (!panel->bank_valid[bank] ||
            !snapshot_equal(&adjacent, &panel->bank_snapshot[bank])) {
        render_bank(panel, bank, &adjacent);
    }
}

static void refresh_wifi_if_needed(struct app_panel *panel)
{
    if (!panel->wifi_open && !panel->wifi_password_open) {
        return;
    }
    app_hub_wifi_snapshot_t wifi;
    if (app_hub_wifi_snapshot(panel->hub, 0, &wifi) == APP_HUB_OK &&
            wifi.generation != panel->wifi_generation) {
        render_wifi(panel);
    }
}

static void apply_scene_slot(struct app_panel *panel, size_t i, bool visible,
                             const char *name, const char *meta,
                             bool show_delete)
{
    if (!visible) {
        set_scene_row(panel, i, false);
        set_scene_delete_row(panel, i, false);
        remember_error(panel, scene_delete_text_setters[i](panel->ui, false));
        show_ink(panel, scene_name_keys[i], scene_name_light_keys[i], false);
        show_ink(panel, scene_meta_keys[i], scene_meta_light_keys[i], false);
        set_scene_name(panel, i, "");
        set_scene_meta(panel, i, "");
        return;
    }
    set_scene_name(panel, i, name);
    set_scene_meta(panel, i, meta);
    set_scene_row(panel, i, true);
    set_scene_delete_row(panel, i, show_delete);
    remember_error(panel, scene_delete_text_setters[i](panel->ui, show_delete));
    show_ink(panel, scene_name_keys[i], scene_name_light_keys[i], true);
    show_ink(panel, scene_meta_keys[i], scene_meta_light_keys[i], true);
}

static void apply_rule_slot(struct app_panel *panel, size_t i, bool visible,
                            const char *name, const char *state,
                            bool show_delete)
{
    set_rule_row(panel, i, false);
    set_rule_delete_row(panel, i, false);
    remember_error(panel, rule_delete_text_setters[i](panel->ui, false));
    show_ink(panel, rule_name_keys[i], rule_name_light_keys[i], false);
    show_ink(panel, rule_state_keys[i], rule_state_light_keys[i], false);
    if (!visible) {
        set_rule_name(panel, i, "");
        set_rule_state(panel, i, "");
        return;
    }
    set_rule_name(panel, i, name);
    set_rule_state(panel, i, state);
    set_rule_row(panel, i, true);
    set_rule_delete_row(panel, i, show_delete);
    remember_error(panel, rule_delete_text_setters[i](panel->ui, show_delete));
    show_ink(panel, rule_name_keys[i], rule_name_light_keys[i], true);
    show_ink(panel, rule_state_keys[i], rule_state_light_keys[i], true);
}

static const app_hub_point_t *find_snapshot_point(const app_hub_snapshot_t *snapshot,
                                                   const char *point_id)
{
    if (!snapshot || !point_id) {
        return NULL;
    }
    for (size_t i = 0; i < snapshot->visible_points; ++i) {
        if (strcmp(snapshot->points[i].point_id, point_id) == 0) {
            return &snapshot->points[i];
        }
    }
    return NULL;
}

static void scene_action_meta(struct app_panel *panel, const app_hub_scene_t *scene,
                              char *meta, size_t meta_size)
{
    size_t on_n = 0;
    size_t off_n = 0;
    char single_name[APP_HUB_NAME_LEN] = {0};
    for (uint8_t i = 0; i < scene->action_count; ++i) {
        if (scene->action_on[i]) {
            ++on_n;
        } else {
            ++off_n;
        }
        if (scene->action_count == 1) {
            const app_hub_point_t *point =
                find_snapshot_point(&panel->snapshot, scene->action_points[i]);
            if (point) {
                point_display_name(panel, point, single_name,
                                   sizeof(single_name));
            } else {
                safe_name(panel, NULL, scene->action_points[i],
                          single_name, sizeof(single_name));
            }
        }
    }
    if (scene->action_count == 1 && single_name[0]) {
        snprintf(meta, meta_size, "%s %s",
                 scene->action_on[0] ?
                 (panel->lang == PANEL_LANG_ZH ? "开" : "On") :
                 (panel->lang == PANEL_LANG_ZH ? "关" : "Off"),
                 single_name);
    } else if (scene->action_count > 0 && off_n == 0) {
        snprintf(meta, meta_size,
                 panel->lang == PANEL_LANG_ZH ? "开 %u" : "On %u",
                 (unsigned)on_n);
    } else if (scene->action_count > 0 && on_n == 0) {
        snprintf(meta, meta_size,
                 panel->lang == PANEL_LANG_ZH ? "关 %u" : "Off %u",
                 (unsigned)off_n);
    } else if (scene->action_count > 0) {
        snprintf(meta, meta_size,
                 panel->lang == PANEL_LANG_ZH ? "开 %u · 关 %u" : "On %u · Off %u",
                 (unsigned)on_n, (unsigned)off_n);
    } else if (panel->lang == PANEL_LANG_ZH) {
        snprintf(meta, meta_size, "%u 步", (unsigned)scene->step_count);
    } else {
        snprintf(meta, meta_size, "%u %s",
                 (unsigned)scene->step_count,
                 scene->step_count == 1 ? "step" : "steps");
    }
}

static bool scene_member_uses_point(const struct app_panel *panel, int point_index)
{
    for (uint8_t i = 0; i < panel->scene_member_count; ++i) {
        if (panel->scene_member_point[i] == (int8_t)point_index) {
            return true;
        }
    }
    return false;
}

static size_t collect_unused_scene_points(const struct app_panel *panel,
                                          int *out, size_t max)
{
    size_t count = 0;
    for (size_t i = 0; i < panel->snapshot.visible_points && count < max; ++i) {
        if (panel->snapshot.points[i].controllable &&
                !scene_member_uses_point(panel, (int)i)) {
            out[count++] = (int)i;
        }
    }
    return count;
}

static void hide_scene_slots(struct app_panel *panel)
{
    for (size_t i = 0; i < APP_HUB_SCENES_PAGE_SIZE; ++i) {
        apply_scene_slot(panel, i, false, "", "", false);
    }
}

static void hide_rule_slots(struct app_panel *panel)
{
    for (size_t i = 0; i < APP_HUB_RULES_PAGE_SIZE; ++i) {
        apply_rule_slot(panel, i, false, "", "", false);
    }
}

static void render_scenes_tab(struct app_panel *panel)
{
    if (panel->scene_building) {
        remember_error(panel, gsp_panel_scene_create_text_set_text(
                           panel->ui,
                           panel->lang == PANEL_LANG_ZH ? "保存" : "Save"));
        char hint[80];
        int unused[APP_HUB_PAGE_SIZE];
        const size_t unused_count =
            collect_unused_scene_points(panel, unused, APP_HUB_PAGE_SIZE);
        if (unused_count > 0) {
            panel->scene_empty_notice = false;
        }
        if (panel->scene_picking) {
            remember_error(panel, gsp_panel_scenes_status_set_text(
                               panel->ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "点要加入的设备" :
                               "Tap a device to include"));
            remember_error(panel, gsp_panel_scenes_status_light_set_text(
                               panel->ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "点要加入的设备" :
                               "Tap a device to include"));
        } else {
            snprintf(hint, sizeof(hint), "%s",
                     panel->lang == PANEL_LANG_ZH ?
                     "点添加设备，再选开或关" :
                     "Add a device, set On/Off, then Save");
            remember_error(panel, gsp_panel_scenes_status_set_text(
                               panel->ui, hint));
            remember_error(panel, gsp_panel_scenes_status_light_set_text(
                               panel->ui, hint));
        }
        for (size_t i = 0; i < APP_HUB_SCENES_PAGE_SIZE; ++i) {
            if (panel->scene_picking) {
                if (i >= unused_count) {
                    apply_scene_slot(panel, i, false, "", "", false);
                    continue;
                }
                char display_name[APP_HUB_NAME_LEN];
                point_display_name(panel, &panel->snapshot.points[unused[i]],
                                   display_name, sizeof(display_name));
                apply_scene_slot(panel, i, true, display_name,
                                 copy_text(panel, COPY_PICK_DEVICE), false);
                continue;
            }
            if (i < panel->scene_member_count) {
                const int point_index = panel->scene_member_point[i];
                if (point_index < 0 ||
                        (size_t)point_index >= panel->snapshot.visible_points) {
                    apply_scene_slot(panel, i, false, "", "", false);
                    continue;
                }
                char display_name[APP_HUB_NAME_LEN];
                char card_name[APP_HUB_NAME_LEN];
                const uint8_t target = panel->scene_member_action[i];
                const char *action = target == 2 ?
                                     (panel->lang == PANEL_LANG_ZH ? "开" : "On") :
                                     (target == 1 ?
                                      (panel->lang == PANEL_LANG_ZH ? "关" : "Off") :
                                      (panel->lang == PANEL_LANG_ZH ? "请选择" : "Set"));
                point_display_name(panel,
                                   &panel->snapshot.points[point_index],
                                   display_name, sizeof(display_name));
                strlcpy(card_name, display_name, sizeof(card_name));
                const size_t used = strlen(card_name);
                if (used < sizeof(card_name)) {
                    (void)snprintf(card_name + used,
                                   sizeof(card_name) - used,
                                   " · %s", action);
                }
                apply_scene_slot(panel, i, true, card_name,
                                 panel->lang == PANEL_LANG_ZH ?
                                 "轻触切换" : "Tap to change", false);
                continue;
            }
            if (i == panel->scene_member_count) {
                const bool empty = panel->scene_empty_notice &&
                                   unused_count == 0;
                apply_scene_slot(panel, i, true,
                                 copy_text(panel, empty ?
                                           COPY_NO_OTHER_DEVICE :
                                           COPY_ADD_CARD_TITLE),
                                 copy_text(panel, empty ?
                                           COPY_NO_OTHER_META :
                                           COPY_ADD_CARD_META),
                                 false);
                continue;
            }
            apply_scene_slot(panel, i, false, "", "", false);
        }
        return;
    }
    remember_error(panel, gsp_panel_scene_create_text_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ? "创建" : "Create"));
    app_hub_scenes_snapshot_t snap;
    const uint32_t previous = panel->scenes.generation;
    if (app_hub_scenes_snapshot(panel->hub, 0, &snap) != APP_HUB_OK) {
        if (previous == 0) {
            hide_scene_slots(panel);
            remember_error(panel, gsp_panel_scenes_status_set_text(
                               panel->ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "正在加载场景" : "Loading scenes..."));
            remember_error(panel, gsp_panel_scenes_status_light_set_text(
                               panel->ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "正在加载场景" : "Loading scenes..."));
        }
        return;
    }
    const bool same_list = previous != 0 && previous == snap.generation &&
                           panel->scenes.visible == snap.visible &&
                           snap.ready;
    panel->scenes = snap;
    remember_error(panel, gsp_panel_scenes_status_set_text(
                       panel->ui,
                       panel->scenes.total == 0 ?
                       (can_create_scene(&panel->snapshot) ?
                        (panel->lang == PANEL_LANG_ZH ?
                         "暂无场景，点右上角创建" :
                         "No scenes. Tap Create.") :
                        (panel->lang == PANEL_LANG_ZH ?
                         "请先添加可控制设备" :
                         "Add a controllable device first")) :
                       (panel->scenes.ready ?
                        localized_status(panel, panel->scenes.status) :
                        (panel->lang == PANEL_LANG_ZH ?
                         "正在加载场景" : "Loading scenes..."))));
    remember_error(panel, gsp_panel_scenes_status_light_set_text(
                       panel->ui,
                       panel->scenes.total == 0 ?
                       (can_create_scene(&panel->snapshot) ?
                        (panel->lang == PANEL_LANG_ZH ?
                         "暂无场景，点右上角创建" :
                         "No scenes. Tap Create.") :
                        (panel->lang == PANEL_LANG_ZH ?
                         "请先添加可控制设备" :
                         "Add a controllable device first")) :
                       (panel->scenes.ready ?
                        localized_status(panel, panel->scenes.status) :
                        (panel->lang == PANEL_LANG_ZH ?
                         "正在加载场景" : "Loading scenes..."))));
    if (same_list) {
        return;
    }
    for (size_t i = 0; i < APP_HUB_SCENES_PAGE_SIZE; ++i) {
        if (i >= panel->scenes.visible) {
            apply_scene_slot(panel, i, false, "", "", false);
            continue;
        }
        const app_hub_scene_t *scene = &panel->scenes.scenes[i];
        char meta[APP_HUB_NAME_LEN];
        scene_action_meta(panel, scene, meta, sizeof(meta));
        char display_name[APP_HUB_NAME_LEN];
        apply_scene_slot(panel, i, true,
                         safe_name(panel, scene->name, scene->id,
                                   display_name, sizeof(display_name)),
                         meta, true);
    }
}

static void render_rules_tab(struct app_panel *panel)
{
    if (panel->rule_building) {
        remember_error(panel, gsp_panel_automation_create_text_set_text(
                           panel->ui,
                           panel->lang == PANEL_LANG_ZH ? "保存" : "Save"));
        const app_hub_point_t *source_point =
            panel->rule_source >= 0 &&
            (size_t)panel->rule_source < panel->snapshot.visible_points ?
            &panel->snapshot.points[panel->rule_source] : NULL;
        const bool imu_source = is_onboard_imu(source_point);
        const char *status;
        if (panel->rule_source < 0) {
            status = panel->lang == PANEL_LANG_ZH ?
                     "1/2  选择触发" :
                     "1/2  Choose when";
        } else if (panel->rule_target < 0) {
            status = panel->rule_save_needed ?
                     (panel->lang == PANEL_LANG_ZH ?
                      "请选择要控制的灯后保存" :
                      "Pick a light, then Save") :
                     (imu_source ?
                      (panel->lang == PANEL_LANG_ZH ?
                       "2/2  选择要控制的灯；再点体感切换" :
                       "2/2  Pick a light; tap motion to cycle") :
                      (panel->lang == PANEL_LANG_ZH ?
                       "2/2  选择要控制的灯；再点触发切换" :
                       "2/2  Pick a light; tap trigger to toggle"));
        } else {
            status = panel->lang == PANEL_LANG_ZH ?
                     "点右上角保存" : "Tap Save";
        }
        remember_error(panel, gsp_panel_automation_status_set_text(
                           panel->ui, status));
        remember_error(panel, gsp_panel_automation_status_light_set_text(
                           panel->ui, status));
        for (size_t i = 0; i < APP_HUB_RULES_PAGE_SIZE; ++i) {
            const app_hub_point_t *point =
                i < panel->snapshot.visible_points ?
                &panel->snapshot.points[i] : NULL;
            if (!rule_builder_shows(panel, point, i)) {
                apply_rule_slot(panel, i, false, "", "", false);
                continue;
            }
            const bool is_source = (int8_t)i == panel->rule_source;
            char display_name[APP_HUB_NAME_LEN];
            const char *state;
            if (is_source) {
                if (is_onboard_imu(point)) {
                    state = imu_value_label(panel,
                                            panel->rule_source_value[0] ?
                                            panel->rule_source_value : "shake");
                } else {
                    state = strcmp(panel->rule_source_value, "on") == 0 ?
                            (panel->lang == PANEL_LANG_ZH ? "开" : "ON") :
                            (panel->lang == PANEL_LANG_ZH ? "关" : "OFF");
                }
            } else if ((int8_t)i == panel->rule_target) {
                state = panel->rule_target_on ?
                        (panel->lang == PANEL_LANG_ZH ? "开" : "ON") :
                        (panel->lang == PANEL_LANG_ZH ? "关" : "OFF");
            } else {
                state = panel->lang == PANEL_LANG_ZH ? "选择" : "SELECT";
            }
            apply_rule_slot(panel, i, true,
                            point_display_name(panel, point, display_name,
                                               sizeof(display_name)),
                            state, false);
        }
        return;
    }
    remember_error(panel, gsp_panel_automation_create_text_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ? "创建" : "Create"));
    app_hub_rules_snapshot_t snap;
    const uint32_t previous = panel->rules.generation;
    if (app_hub_rules_snapshot(panel->hub, 0, &snap) != APP_HUB_OK) {
        if (previous == 0) {
            hide_rule_slots(panel);
            remember_error(panel, gsp_panel_automation_status_set_text(
                               panel->ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "正在加载自动化" : "Loading automations..."));
            remember_error(panel, gsp_panel_automation_status_light_set_text(
                               panel->ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "正在加载自动化" : "Loading automations..."));
        }
        return;
    }
    const bool same_list = previous != 0 && previous == snap.generation &&
                           panel->rules.visible == snap.visible &&
                           snap.ready;
    panel->rules = snap;
    remember_error(panel, gsp_panel_automation_status_set_text(
                       panel->ui,
                       panel->rules.total == 0 ?
                        (can_create_rule(&panel->snapshot) ?
                        (panel->lang == PANEL_LANG_ZH ?
                         "暂无自动化，点右上角创建" :
                         "No automations. Tap Create.") :
                        rule_create_blocked_hint(panel, &panel->snapshot)) :
                       (panel->rules.ready ?
                        localized_status(panel, panel->rules.status) :
                        (panel->lang == PANEL_LANG_ZH ?
                         "正在加载自动化" : "Loading automations..."))));
    remember_error(panel, gsp_panel_automation_status_light_set_text(
                       panel->ui,
                       panel->rules.total == 0 ?
                        (can_create_rule(&panel->snapshot) ?
                        (panel->lang == PANEL_LANG_ZH ?
                         "暂无自动化，点右上角创建" :
                         "No automations. Tap Create.") :
                        rule_create_blocked_hint(panel, &panel->snapshot)) :
                       (panel->rules.ready ?
                        localized_status(panel, panel->rules.status) :
                        (panel->lang == PANEL_LANG_ZH ?
                         "正在加载自动化" : "Loading automations..."))));
    if (same_list) {
        return;
    }
    for (size_t i = 0; i < APP_HUB_RULES_PAGE_SIZE; ++i) {
        if (i >= panel->rules.visible) {
            apply_rule_slot(panel, i, false, "", "", false);
            continue;
        }
        char display_name[APP_HUB_NAME_LEN];
        apply_rule_slot(panel, i, true,
                        safe_name(panel, panel->rules.rules[i].name,
                                  panel->rules.rules[i].id,
                                  display_name, sizeof(display_name)),
                        panel->rules.rules[i].enabled ?
                        (panel->lang == PANEL_LANG_ZH ? "开" : "ON") :
                        (panel->lang == PANEL_LANG_ZH ? "关" : "OFF"),
                        true);
    }
}

static void render_system_tab(struct app_panel *panel)
{
    const uint32_t previous = panel->system.generation;
    if (app_hub_system_snapshot(panel->hub, &panel->system) != APP_HUB_OK ||
            (previous == panel->system.generation && previous != 0)) {
        return;
    }
    char text[96];
    snprintf(text, sizeof(text), "MiniHub  %s",
             panel->system.hub_online ?
             (panel->lang == PANEL_LANG_ZH ? "在线" : "Online") :
             (panel->lang == PANEL_LANG_ZH ? "离线" : "Offline"));
    remember_error(panel, gsp_panel_system_summary_set_text(panel->ui, text));
    remember_error(panel, gsp_panel_system_summary_light_set_text(panel->ui, text));
    snprintf(text, sizeof(text), "%s",
             panel->system.ip[0] ? panel->system.ip : "--");
    remember_error(panel, gsp_panel_system_network_set_text(panel->ui, text));
    remember_error(panel, gsp_panel_system_network_light_set_text(panel->ui, text));
    snprintf(text, sizeof(text),
             panel->lang == PANEL_LANG_ZH ? "可用 %lu KB" : "%lu KB free",
             (unsigned long)panel->system.internal_free_kb);
    remember_error(panel, gsp_panel_system_memory_set_text(panel->ui, text));
    remember_error(panel, gsp_panel_system_memory_light_set_text(panel->ui, text));
}

static void render_active_tab(struct app_panel *panel)
{
    if (blocking_surface(panel)) {
        return;
    }
    if (panel->tab == PANEL_TAB_SCENES) {
        render_scenes_tab(panel);
    } else if (panel->tab == PANEL_TAB_AUTOMATION) {
        render_rules_tab(panel);
    } else if (panel->tab == PANEL_TAB_SETTINGS) {
        render_system_tab(panel);
    }
}

static void follow_setup(struct app_panel *panel, bool wifi_just_ready)
{
    if (panel->welcome_open || panel->lang_open) {
        return;
    }
    /*
     * Tab switches close setup overlays. Do not immediately reopen Wi-Fi /
     * token / role and steal Scenes/Automation/Settings.
     */
    if ((panel->tab == PANEL_TAB_SCENES ||
            panel->tab == PANEL_TAB_AUTOMATION ||
            panel->tab == PANEL_TAB_SETTINGS) &&
            !panel->wifi_open && !panel->wifi_password_open &&
            !panel->role_open && !panel->token_open && !panel->add_open) {
        return;
    }
    if (!panel->snapshot.wifi_ready) {
        if (!panel->wifi_open && !panel->wifi_password_open) {
            if (panel->wifi_prompted) {
                show_wifi_page(panel, false);
            }
        } else {
            render_wifi(panel);
        }
        return;
    }
    if (wifi_just_ready &&
            (panel->wifi_open || panel->wifi_password_open)) {
        panel->wifi_success_ticks = PANEL_WIFI_SUCCESS_TICKS;
        if (!panel->wifi_open && !panel->wifi_password_open) {
            show_wifi_page(panel, false);
        } else {
            render_wifi(panel);
        }
        return;
    }
    if (onboarding(panel) && !panel->wifi_from_settings &&
            !panel->role_open &&
            !panel->wifi_password_open && !panel->detail_open) {
        maybe_open_role_after_wifi(panel);
        if (panel->wifi_open || panel->wifi_password_open) {
            render_wifi(panel);
        }
        return;
    }
    if (panel->snapshot.wifi_ready &&
            (panel->snapshot.url_needed || panel->snapshot.auth_needed) &&
            !panel->token_open && !panel->detail_open &&
            !panel->wifi_open && !panel->wifi_password_open &&
            !panel->role_open) {
        open_token_page(panel);
        return;
    }
    if (panel->role_open) {
        render_role(panel);
    } else if (panel->token_open) {
        render_token(panel);
    } else if (panel->add_open) {
        render_add_page(panel);
    } else if (panel->wifi_open || panel->wifi_password_open) {
        render_wifi(panel);
    }
}

static void fill_scene_name(struct app_panel *panel)
{
    size_t on_n = 0;
    size_t off_n = 0;
    for (uint8_t i = 0; i < panel->scene_member_count; ++i) {
        if (panel->scene_member_action[i] == 2) {
            ++on_n;
        } else if (panel->scene_member_action[i] == 1) {
            ++off_n;
        }
    }
    strlcpy(panel->builder_name,
            off_n == 0 ? (panel->lang == PANEL_LANG_ZH ? "全开" : "All on") :
            (on_n == 0 ? (panel->lang == PANEL_LANG_ZH ? "全关" : "All off") :
             (panel->lang == PANEL_LANG_ZH ? "场景" : "Scene")),
            sizeof(panel->builder_name));
}

static void fill_rule_name(struct app_panel *panel,
                           const app_hub_point_t *source,
                           const app_hub_point_t *target)
{
    char source_name[APP_HUB_NAME_LEN];
    char target_name[APP_HUB_NAME_LEN];
    point_display_name(panel, source, source_name, sizeof(source_name));
    point_display_name(panel, target, target_name, sizeof(target_name));
    const char *when;
    if (is_onboard_imu(source)) {
        when = imu_value_label(panel, panel->rule_source_value);
        snprintf(panel->builder_name, sizeof(panel->builder_name),
                 panel->lang == PANEL_LANG_ZH ? "%s后%s%s" : "%s then %s %s",
                 when,
                 panel->rule_target_on ?
                 (panel->lang == PANEL_LANG_ZH ? "开" : "on") :
                 (panel->lang == PANEL_LANG_ZH ? "关" : "off"),
                 target_name);
        return;
    }
    when = strcmp(panel->rule_source_value, "on") == 0 ?
           (panel->lang == PANEL_LANG_ZH ? "开" : "on") :
           (panel->lang == PANEL_LANG_ZH ? "关" : "off");
    snprintf(panel->builder_name, sizeof(panel->builder_name),
             panel->lang == PANEL_LANG_ZH ? "%s%s后%s%s" : "%s %s then %s %s",
             source_name, when,
             panel->rule_target_on ?
             (panel->lang == PANEL_LANG_ZH ? "开" : "on") :
             (panel->lang == PANEL_LANG_ZH ? "关" : "off"),
             target_name);
}

static void begin_scene_builder(struct app_panel *panel)
{
    (void)app_hub_snapshot(panel->hub, 0, &panel->snapshot);
    panel->scene_building = true;
    panel->scene_picking = false;
    panel->scene_empty_notice = false;
    panel->scene_member_count = 0;
    memset(panel->scene_member_point, -1, sizeof(panel->scene_member_point));
    memset(panel->scene_member_action, 0, sizeof(panel->scene_member_action));
    strlcpy(panel->builder_name,
            panel->lang == PANEL_LANG_ZH ? "场景" : "Scene",
            sizeof(panel->builder_name));
    show_tab(panel, PANEL_TAB_SCENES);
    render_scenes_tab(panel);
}

static void begin_rule_builder(struct app_panel *panel)
{
    (void)app_hub_snapshot(panel->hub, 0, &panel->snapshot);
    panel->rule_building = true;
    panel->rule_save_needed = false;
    panel->rule_source = -1;
    panel->rule_target = -1;
    panel->rule_target_on = false;
    panel->rule_source_value[0] = '\0';
    strlcpy(panel->builder_name,
            panel->lang == PANEL_LANG_ZH ? "自动化" : "Automation",
            sizeof(panel->builder_name));
    show_tab(panel, PANEL_TAB_AUTOMATION);
    render_rules_tab(panel);
}

static void update_ui(struct app_panel *panel)
{
    app_hub_snapshot_t snapshot;
    const size_t snap_page =
        (panel->rule_building || panel->scene_building) ? 0 : panel->page;
    if (app_hub_snapshot(panel->hub, snap_page, &snapshot) != APP_HUB_OK) {
        hide_hub_status_chip(panel);
        return;
    }
    const bool refresh_completed = panel->refresh_pending &&
                                   snapshot.generation != panel->refresh_generation;
    if (refresh_completed) {
        panel->refresh_pending = false;
        panel->refresh_wait_ticks = 0;
        panel->refresh_feedback_ticks = 8;
        hide_hub_status_chip(panel);
    }
    if (panel->rendered && snapshot_equal(&snapshot, &panel->snapshot)) {
        panel->snapshot.generation = snapshot.generation;
        if (panel->tab == PANEL_TAB_DEVICES &&
                !overlay_open(panel) && !setup_pending(&snapshot) &&
                (!panel->bank_valid[panel->active_bank] ||
                 !same_page_layout(&panel->bank_snapshot[panel->active_bank],
                                   &snapshot))) {
            paint_device_bank(panel, &snapshot, true);
        }
        close_stale_add_overlay(panel);
        refresh_wifi_if_needed(panel);
        render_active_tab(panel);
        follow_setup(panel, false);
        return;
    }

    const app_hub_snapshot_t previous = panel->snapshot;
    const bool show_home = panel->tab == PANEL_TAB_DEVICES &&
                           !overlay_open(panel) && !setup_pending(&snapshot);
    if (overlay_open(panel) ||
            (setup_pending(&snapshot) && panel->tab == PANEL_TAB_DEVICES)) {
        set_home_interactive(panel, false);
        if (panel->tab == PANEL_TAB_DEVICES) {
            paint_device_bank(panel, &snapshot, false);
        }
    } else if (show_home) {
        paint_device_bank(panel, &snapshot, true);
    }

    char text[24];
    if (!panel->rendered || snapshot.total_points != previous.total_points) {
        snprintf(text, sizeof(text), "%u", (unsigned)snapshot.total_points);
        remember_error(panel, gsp_panel_total_set_text(panel->ui, text));
        remember_error(panel, gsp_panel_total_light_set_text(panel->ui, text));
    }
    if (!panel->rendered || snapshot.online_points != previous.online_points) {
        snprintf(text, sizeof(text), "%u", (unsigned)snapshot.online_points);
        remember_error(panel, gsp_panel_online_set_text(panel->ui, text));
    }
    if (!panel->rendered || snapshot.page != previous.page ||
            snapshot.page_count != previous.page_count) {
        snprintf(text, sizeof(text), "%u / %u", (unsigned)(snapshot.page + 1),
                 (unsigned)snapshot.page_count);
        remember_error(panel, gsp_panel_page_set_text(panel->ui, text));
    }
    hide_hub_status_chip(panel);

    panel->page = snapshot.page;
    panel->snapshot = snapshot;
    panel->rendered = true;
    if (panel->detail_open) {
        render_detail(panel, find_detail_point(panel, &snapshot));
    }
    if (snapshot.connected && !previous.connected) {
        close_token_page(panel);
        close_role_page(panel);
    }
    follow_setup(panel, snapshot.wifi_ready && !previous.wifi_ready);
    close_stale_add_overlay(panel);
    render_active_tab(panel);
    if (panel->tab == PANEL_TAB_DEVICES && !overlay_open(panel)) {
        prefetch_adjacent_page(panel);
    }
}

static void device_swipe_event(
    esp_gsp_handle_t ui, const esp_gsp_component_event_t *event, void *ctx)
{
    struct app_panel *panel = ctx;
    if (event->key != GSP_PANEL_OBJ_KEY_DEVICE_SWIPE ||
            !(event->flags & ESP_GSP_COMPONENT_EVENT_VALUE_CHANGED) ||
            event->state.value == 1) {
        return;
    }
    if (panel->tab == PANEL_TAB_DEVICES && !overlay_open(panel) &&
            !panel->detail_open) {
        panel->pending_device_swipe = event->state.value == 0 ? -1 : 1;
    }
    remember_error(panel, esp_gsp_page_flow_set_page(
                       ui, GSP_PANEL_OBJ_KEY_DEVICE_SWIPE, 1, false));
}

static void tick_welcome(struct app_panel *panel)
{
    if (!panel->welcome_open) {
        return;
    }
    ++panel->welcome_ticks;
    if (panel->welcome_ticks >= PANEL_WELCOME_TICKS) {
        open_lang_page(panel);
    }
}

static void tick_wifi_dots(struct app_panel *panel)
{
    if (!panel->wifi_open) {
        return;
    }
    app_hub_wifi_snapshot_t wifi;
    if (app_hub_wifi_snapshot(panel->hub, 0, &wifi) != APP_HUB_OK ||
            !wifi.scanning) {
        return;
    }
    ++panel->wifi_dot_ticks;
    set_wifi_dots(panel, true);
}

static void tick_reconnect(struct app_panel *panel)
{
    if (!panel->detail_open || panel->reconnect_state == RECONNECT_IDLE) {
        return;
    }
    const app_hub_point_t *point = find_detail_point(panel, &panel->snapshot);
    if (panel->reconnect_state == RECONNECT_WAIT) {
        if (point && point->online) {
            panel->reconnect_state = RECONNECT_OK;
            panel->reconnect_ticks = PANEL_RECONNECT_RESULT_TICKS;
            render_detail(panel, point);
            return;
        }
        if (panel->reconnect_ticks > 0) {
            --panel->reconnect_ticks;
        }
        if (panel->reconnect_ticks == 0) {
            panel->reconnect_state = RECONNECT_FAIL;
            panel->reconnect_ticks = PANEL_RECONNECT_RESULT_TICKS;
            if (point) {
                render_detail(panel, point);
            }
        }
        return;
    }
    if (panel->reconnect_ticks > 0) {
        --panel->reconnect_ticks;
    }
    if (panel->reconnect_ticks == 0) {
        panel->reconnect_state = RECONNECT_IDLE;
        if (point) {
            render_detail(panel, point);
        }
    }
}

static void refresh_tick(esp_gsp_handle_t ui, void *ctx)
{
    (void)ui;
    struct app_panel *panel = ctx;
    const int8_t swipe = panel->pending_device_swipe;
    panel->pending_device_swipe = 0;
    if (swipe < 0 && panel->page > 0) {
        --panel->page;
    } else if (swipe > 0 &&
               panel->page + 1 < panel->snapshot.page_count) {
        ++panel->page;
    }
    finish_open_wifi_password(panel);
    if (panel->wifi_scan_pending && panel->wifi_open &&
            !panel->wifi_password_open) {
        request_wifi_scan(panel);
    }
    tick_welcome(panel);
    tick_wifi_dots(panel);
    if (panel->add_commissioning) {
        render_add_page(panel);
        close_stale_add_overlay(panel);
    } else {
        update_ui(panel);
        tick_reconnect(panel);
    }
    if (panel->wifi_cursor_delay_ticks > 0) {
        --panel->wifi_cursor_delay_ticks;
        if (panel->wifi_cursor_delay_ticks == 0 &&
                panel->wifi_password_open &&
                panel->keyboard_owner == PANEL_KEYBOARD_WIFI) {
            remember_error(panel, esp_gsp_set_cursor(
                               panel->ui, GSP_PANEL_BIND_WIFI_PASSWORD));
            /*
             * The field is already empty on screen. set_cursor still uses
             * the advance cached while the caret was hidden, which is why
             * a failed password comes back indented. Publishing the empty
             * string again while the caret is active remeasures it.
             */
            remember_error(panel, gsp_panel_wifi_password_set_text(
                               panel->ui, ""));
        }
    }
    if (panel->refresh_pending && panel->refresh_wait_ticks > 0) {
        --panel->refresh_wait_ticks;
        if (panel->refresh_wait_ticks == 0) {
            panel->refresh_pending = false;
            panel->refresh_feedback_ticks = 8;
            if (!panel->add_commissioning) {
                hide_hub_status_chip(panel);
            }
        }
    } else if (panel->refresh_feedback_ticks > 0) {
        --panel->refresh_feedback_ticks;
        if (panel->refresh_feedback_ticks == 0 && !panel->add_commissioning) {
            hide_hub_status_chip(panel);
        }
    }
    if (panel->wifi_success_ticks > 0) {
        --panel->wifi_success_ticks;
        if (panel->wifi_success_ticks == 0) {
            if (panel->wifi_from_settings) {
                panel->wifi_from_settings = false;
                open_settings_page(panel);
            } else {
                panel->role_from_settings = false;
                open_role_page(panel);
            }
        }
    }
    if (panel->add_success_ticks > 0) {
        --panel->add_success_ticks;
        if (panel->add_success_ticks == 0 && panel->add_open) {
            show_home_page(panel);
        }
    }
}

static int selected_card(const esp_gsp_event_t *event)
{
    if (gsp_panel_event_is_bank_0_card_0(event) ||
            gsp_panel_event_is_bank_1_card_0(event)) {
        return 0;
    }
    if (gsp_panel_event_is_bank_0_card_1(event) ||
            gsp_panel_event_is_bank_1_card_1(event)) {
        return 1;
    }
    if (gsp_panel_event_is_bank_0_card_2(event) ||
            gsp_panel_event_is_bank_1_card_2(event)) {
        return 2;
    }
    if (gsp_panel_event_is_bank_0_card_3(event) ||
            gsp_panel_event_is_bank_1_card_3(event)) {
        return 3;
    }
    return -1;
}

static int selected_scene(const esp_gsp_event_t *event)
{
    if (gsp_panel_event_is_scene_0(event)) return 0;
    if (gsp_panel_event_is_scene_1(event)) return 1;
    if (gsp_panel_event_is_scene_2(event)) return 2;
    if (gsp_panel_event_is_scene_3(event)) return 3;
    return -1;
}

static int selected_rule(const esp_gsp_event_t *event)
{
    if (gsp_panel_event_is_rule_0(event)) return 0;
    if (gsp_panel_event_is_rule_1(event)) return 1;
    if (gsp_panel_event_is_rule_2(event)) return 2;
    if (gsp_panel_event_is_rule_3(event)) return 3;
    return -1;
}

static void detail_parameter_event(struct app_panel *panel,
                                   const char *parameter, int32_t value,
                                   text_setter_t set_text)
{
    char text[16];
    snprintf(text, sizeof(text), "%ld", (long)value);
    remember_error(panel, set_text(panel->ui, text));
    text_setter_t light_text = ink_companion(set_text);
    if (light_text) {
        remember_error(panel, light_text(panel->ui, text));
    }
    if (app_hub_set_parameter(panel->hub, panel->detail_point_id,
                              parameter, value) != APP_HUB_OK) {
        remember_error(panel, gsp_panel_detail_meta_set_text(
                           panel->ui, "Command queue full"));
        remember_error(panel, gsp_panel_detail_meta_light_set_text(
                           panel->ui, "Command queue full"));
    }
}

static bool add_field_required(app_hub_protocol_t protocol, uint8_t step)
{
    switch (protocol) {
    case APP_HUB_PROTOCOL_MATTER:
        return step == 0 || step == 1;
    case APP_HUB_PROTOCOL_ZIGBEE:
    case APP_HUB_PROTOCOL_BLE_MESH:
        return step == add_field_count(protocol) - 2;
    case APP_HUB_PROTOCOL_MQTT:
        return step == 0 || step == 1 || step == 4 ||
               step == 5 || step == 6 || step == 8;
    case APP_HUB_PROTOCOL_MODBUS_TCP:
        return step <= 9 || step == 10;
    default:
        return false;
    }
}

static bool parse_unsigned(const char *text, unsigned long maximum,
                           unsigned long *value)
{
    if (!text || !text[0] || text[0] == '-') {
        return false;
    }
    char *end = NULL;
    errno = 0;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (errno == ERANGE || !end || *end != '\0' || parsed > maximum) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parse_scale(const char *text, float *value)
{
    if (!text || !text[0]) {
        return false;
    }
    char *end = NULL;
    errno = 0;
    const float parsed = strtof(text, &end);
    if (errno == ERANGE || !end || *end != '\0' ||
            !isfinite(parsed) || parsed == 0.0f) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool store_add_field(struct app_panel *panel, const char *text)
{
    app_hub_add_request_t *request = &panel->add_request;
    const uint8_t step = panel->add_step;
    unsigned long number = 0;
    if (add_field_required(request->protocol, step) && (!text || !text[0])) {
        return false;
    }
    if (request->protocol == APP_HUB_PROTOCOL_MATTER) {
        if (step == 0) {
            if (!app_hub_matter_pairing_code_ok(text)) {
                return false;
            }
            strlcpy(request->pairing_code, text, sizeof(request->pairing_code));
        } else if (step == 1) {
            strlcpy(request->name, text, sizeof(request->name));
        } else {
            strlcpy(request->room, text, sizeof(request->room));
        }
    } else if (request->protocol == APP_HUB_PROTOCOL_ZIGBEE) {
        if (step == 0) strlcpy(request->ieee, text, sizeof(request->ieee));
        else if (step == 1) strlcpy(request->install_code, text,
                                    sizeof(request->install_code));
        else if (step == 2) strlcpy(request->name, text, sizeof(request->name));
        else strlcpy(request->room, text, sizeof(request->room));
        if (step == 1 && request->install_code[0] && !request->ieee[0]) {
            return false;
        }
    } else if (request->protocol == APP_HUB_PROTOCOL_BLE_MESH) {
        if (step == 0) strlcpy(request->name, text, sizeof(request->name));
        else strlcpy(request->room, text, sizeof(request->room));
    } else if (request->protocol == APP_HUB_PROTOCOL_MQTT) {
        switch (step) {
        case 0: strlcpy(request->host, text, sizeof(request->host)); break;
        case 1:
            if (!parse_unsigned(text, UINT16_MAX, &number) || number == 0) {
                return false;
            }
            request->port = (uint16_t)number;
            break;
        case 2: strlcpy(request->username, text, sizeof(request->username)); break;
        case 3: strlcpy(request->password, text, sizeof(request->password)); break;
        case 4: strlcpy(request->point_id, text, sizeof(request->point_id)); break;
        case 5: strlcpy(request->type, text, sizeof(request->type)); break;
        case 6: strlcpy(request->state_topic, text,
                        sizeof(request->state_topic)); break;
        case 7: strlcpy(request->command_topic, text,
                        sizeof(request->command_topic)); break;
        case 8: strlcpy(request->name, text, sizeof(request->name)); break;
        default: strlcpy(request->room, text, sizeof(request->room)); break;
        }
    } else if (request->protocol == APP_HUB_PROTOCOL_MODBUS_TCP) {
        switch (step) {
        case 0: strlcpy(request->host, text, sizeof(request->host)); break;
        case 1:
            if (!parse_unsigned(text, UINT16_MAX, &number) || number == 0) {
                return false;
            }
            request->port = (uint16_t)number;
            break;
        case 2:
            if (!parse_unsigned(text, 247, &number) || number == 0) return false;
            request->unit_id = (uint8_t)number;
            break;
        case 3: strlcpy(request->point_id, text, sizeof(request->point_id)); break;
        case 4: strlcpy(request->type, text, sizeof(request->type)); break;
        case 5:
            if (strcasecmp(text, "holding") != 0 &&
                    strcasecmp(text, "coil") != 0 &&
                    strcasecmp(text, "coils") != 0 &&
                    strcasecmp(text, "discrete") != 0 &&
                    strcasecmp(text, "di") != 0 &&
                    strcasecmp(text, "input") != 0 &&
                    strcasecmp(text, "ir") != 0) {
                return false;
            }
            strlcpy(request->function, text, sizeof(request->function));
            break;
        case 6:
            if (!parse_unsigned(text, UINT16_MAX, &number)) return false;
            request->address = (uint16_t)number;
            break;
        case 7:
            if (strcasecmp(text, "u16") != 0 &&
                    strcasecmp(text, "s16") != 0 &&
                    strcasecmp(text, "int16") != 0 &&
                    strcasecmp(text, "u32") != 0 &&
                    strcasecmp(text, "uint32") != 0 &&
                    strcasecmp(text, "s32") != 0 &&
                    strcasecmp(text, "int32") != 0 &&
                    strcasecmp(text, "f32") != 0 &&
                    strcasecmp(text, "float") != 0) {
                return false;
            }
            strlcpy(request->datatype, text, sizeof(request->datatype));
            break;
        case 8:
            if (!parse_scale(text, &request->scale)) return false;
            break;
        case 9:
            if (strcmp(text, "0") != 0 && strcmp(text, "1") != 0) return false;
            request->writable = strcmp(text, "1") == 0;
            break;
        case 10: strlcpy(request->name, text, sizeof(request->name)); break;
        default: strlcpy(request->room, text, sizeof(request->room)); break;
        }
    }
    return true;
}

static void show_add_field_error(struct app_panel *panel, const char *detail)
{
    panel->add_field_error = true;
    panel->add_failed = false;
    remember_error(panel, gsp_panel_add_input_box_set_visible(panel->ui, false));
    remember_error(panel, gsp_panel_add_input_box_face_set_visible(panel->ui, false));
    set_add_keyboard_visible(panel, false);
    remember_error(panel, gsp_panel_add_result_title_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ?
                       "填写无效" : "Invalid value"));
    remember_error(panel, gsp_panel_add_result_title_light_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ?
                       "填写无效" : "Invalid value"));
    remember_error(panel, gsp_panel_add_result_meta_set_text(
                       panel->ui, detail));
    remember_error(panel, gsp_panel_add_result_meta_light_set_text(
                       panel->ui, detail));
    remember_error(panel, gsp_panel_add_cancel_text_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ? "重试" : "Retry"));
    remember_error(panel, gsp_panel_add_cancel_text_light_set_text(
                       panel->ui,
                       panel->lang == PANEL_LANG_ZH ? "重试" : "Retry"));
    set_add_status_visible(panel, true, true);
}

static void submit_add_field(struct app_panel *panel, const char *text)
{
    if (panel->add_commissioning) {
        return;
    }
    if (!store_add_field(panel, text ? text : "")) {
        const bool matter_code =
            panel->add_request.protocol == APP_HUB_PROTOCOL_MATTER &&
            panel->add_step == 0;
        show_add_field_error(panel,
                             panel->lang == PANEL_LANG_ZH ?
                             (matter_code ? "入网码无效" : "请填写有效内容") :
                             (matter_code ? "Invalid pairing code" :
                              "Enter a valid value"));
        return;
    }
    ++panel->add_step;
    if (panel->add_step < add_field_count(panel->add_request.protocol)) {
        render_add_field(panel);
        return;
    }
    const app_hub_result_t result =
        app_hub_add_device(panel->hub, &panel->add_request);
    if (result != APP_HUB_OK) {
        (void)app_hub_add_snapshot(panel->hub, &panel->add_snapshot);
        if (panel->add_snapshot.state == APP_HUB_ADD_FAILED ||
                panel->add_snapshot.state == APP_HUB_ADD_PREPARING ||
                panel->add_snapshot.state == APP_HUB_ADD_WAITING) {
            panel->add_commissioning = true;
            freeze_add_inputs(panel);
            render_add_page(panel);
        } else {
            show_add_field_error(
                panel,
                result == APP_HUB_ERR_BUSY ?
                (panel->lang == PANEL_LANG_ZH ?
                 "请求队列繁忙，请重试" : "Request queue is busy. Try again.") :
                copy_text(panel, COPY_ADD_FAILED));
        }
        return;
    }
    panel->add_commissioning = true;
    panel->add_failed = false;
    freeze_add_inputs(panel);
    set_add_status_visible(panel, true, true);
    render_add_page(panel);
}

static void panel_event(esp_gsp_handle_t ui, const esp_gsp_event_t *event,
                        void *ctx)
{
    struct app_panel *panel = ctx;
    if (gsp_panel_event_is_reboot_cancel(event)) {
        panel->reboot_confirm_open = false;
        remember_error(panel, gsp_panel_reboot_confirm_set_visible(ui, false));
        return;
    }
    if (gsp_panel_event_is_reboot_ok(event)) {
        panel->reboot_confirm_open = false;
        remember_error(panel, gsp_panel_reboot_confirm_set_visible(ui, false));
        if (app_hub_system_reboot(panel->hub) != APP_HUB_OK) {
            remember_error(panel, gsp_panel_system_summary_set_text(
                               ui, "Restart request failed"));
            remember_error(panel, gsp_panel_system_summary_light_set_text(
                               ui, "Restart request failed"));
        } else {
            remember_error(panel, gsp_panel_system_summary_set_text(
                               ui, "Restarting MiniHub..."));
            remember_error(panel, gsp_panel_system_summary_light_set_text(
                               ui, "Restarting MiniHub..."));
        }
        return;
    }
    if (overlay_open(panel) &&
            (gsp_panel_event_is_wifi(event) ||
             gsp_panel_event_is_refresh(event) ||
             gsp_panel_event_is_previous(event) ||
             gsp_panel_event_is_next(event))) {
        return;
    }
    if (gsp_panel_event_is_tab_devices(event) ||
            gsp_panel_event_is_tab_scenes(event) ||
            gsp_panel_event_is_tab_automation(event) ||
            gsp_panel_event_is_tab_settings(event)) {
        if (overlay_open(panel)) {
            return;
        }
    }
    if (gsp_panel_event_is_tab_devices(event)) {
        panel->scene_building = false;
        panel->rule_building = false;
        panel->rule_save_needed = false;
        show_tab(panel, PANEL_TAB_DEVICES);
        return;
    }
    if (gsp_panel_event_is_tab_scenes(event)) {
        panel->rule_building = false;
        panel->rule_save_needed = false;
        show_tab(panel, PANEL_TAB_SCENES);
        render_scenes_tab(panel);
        return;
    }
    if (gsp_panel_event_is_tab_automation(event)) {
        panel->scene_building = false;
        if (panel->rule_building) {
            panel->rule_building = false;
            panel->rule_save_needed = false;
        }
        show_tab(panel, PANEL_TAB_AUTOMATION);
        render_rules_tab(panel);
        return;
    }
    if (gsp_panel_event_is_tab_settings(event)) {
        panel->scene_building = false;
        panel->rule_building = false;
        panel->rule_save_needed = false;
        show_tab(panel, PANEL_TAB_SETTINGS);
        render_system_tab(panel);
        return;
    }
    if (gsp_panel_event_is_scene_create(event)) {
        if (!panel->scene_building) {
            refresh_home_snapshot(panel);
            if (!can_create_scene(&panel->snapshot)) {
                remember_error(panel, gsp_panel_scenes_status_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "请先添加可控制设备" :
                                   "Add a controllable device first"));
                remember_error(panel, gsp_panel_scenes_status_light_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "请先添加可控制设备" :
                                   "Add a controllable device first"));
                return;
            }
            begin_scene_builder(panel);
            return;
        }
        const char *point_ids[APP_HUB_PAGE_SIZE] = {0};
        bool values[APP_HUB_PAGE_SIZE] = {0};
        size_t count = 0;
        for (uint8_t i = 0; i < panel->scene_member_count &&
                count < APP_HUB_PAGE_SIZE; ++i) {
            const int point_index = panel->scene_member_point[i];
            if (point_index < 0 ||
                    (size_t)point_index >= panel->snapshot.visible_points ||
                    panel->scene_member_action[i] == 0) {
                continue;
            }
            point_ids[count] =
                panel->snapshot.points[point_index].point_id;
            values[count] = panel->scene_member_action[i] == 2;
            ++count;
        }
        fill_scene_name(panel);
        if (count == 0 || app_hub_scene_create(
                panel->hub, panel->builder_name,
                point_ids, values, count) != APP_HUB_OK) {
            remember_error(panel, gsp_panel_scenes_status_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "请添加设备后选择开或关" :
                               "Add a device and set On or Off"));
            remember_error(panel, gsp_panel_scenes_status_light_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "请添加设备后选择开或关" :
                               "Add a device and set On or Off"));
        } else {
            panel->scene_building = false;
            hide_scene_slots(panel);
            remember_error(panel, gsp_panel_scene_create_text_set_text(
                               ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "创建" : "Create"));
            render_scenes_tab(panel);
        }
        return;
    }
    if (gsp_panel_event_is_automation_create(event)) {
        if (!panel->rule_building) {
            (void)app_hub_snapshot(panel->hub, 0, &panel->snapshot);
            if (!can_create_rule(&panel->snapshot)) {
                remember_error(panel, gsp_panel_automation_status_set_text(
                                   ui, rule_create_blocked_hint(
                                       panel, &panel->snapshot)));
                remember_error(panel, gsp_panel_automation_status_light_set_text(
                                   ui, rule_create_blocked_hint(
                                       panel, &panel->snapshot)));
                return;
            }
            begin_rule_builder(panel);
            return;
        }
        const app_hub_point_t *source =
            panel->rule_source >= 0 &&
            (size_t)panel->rule_source < panel->snapshot.visible_points ?
            &panel->snapshot.points[panel->rule_source] : NULL;
        const app_hub_point_t *target =
            panel->rule_target >= 0 &&
            (size_t)panel->rule_target < panel->snapshot.visible_points ?
            &panel->snapshot.points[panel->rule_target] : NULL;
        if (source && target && panel->rule_source_value[0] &&
                target->controllable) {
            fill_rule_name(panel, source, target);
        }
        if (!source || !target || panel->rule_source_value[0] == '\0' ||
                !target->controllable ||
                app_hub_rule_create(
                    panel->hub, panel->builder_name, source->point_id,
                    panel->rule_source_value, target->point_id,
                    panel->rule_target_on) != APP_HUB_OK) {
            panel->rule_save_needed = true;
            render_rules_tab(panel);
        } else {
            panel->rule_building = false;
            panel->rule_save_needed = false;
            remember_error(panel, gsp_panel_automation_create_text_set_text(
                               ui,
                               panel->lang == PANEL_LANG_ZH ?
                               "创建" : "Create"));
            remember_error(panel, gsp_panel_automation_status_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "正在创建常用自动化..." :
                               "Creating a simple automation..."));
            remember_error(panel, gsp_panel_automation_status_light_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "正在创建常用自动化..." :
                               "Creating a simple automation..."));
        }
        return;
    }
    if (gsp_panel_event_is_scene_0_delete(event) ||
            gsp_panel_event_is_scene_1_delete(event) ||
            gsp_panel_event_is_scene_2_delete(event) ||
            gsp_panel_event_is_scene_3_delete(event)) {
        const int delete_index =
            gsp_panel_event_is_scene_0_delete(event) ? 0 :
            gsp_panel_event_is_scene_1_delete(event) ? 1 :
            gsp_panel_event_is_scene_2_delete(event) ? 2 : 3;
        if (!panel->scene_building &&
                (size_t)delete_index < panel->scenes.visible) {
            if (app_hub_scene_delete(
                    panel->hub,
                    panel->scenes.scenes[delete_index].id) != APP_HUB_OK) {
                remember_error(panel, gsp_panel_scenes_status_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "无法删除场景" : "Could not delete scene"));
                remember_error(panel, gsp_panel_scenes_status_light_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "无法删除场景" : "Could not delete scene"));
            } else {
                remember_error(panel, gsp_panel_scenes_status_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "正在删除场景..." : "Deleting scene..."));
                remember_error(panel, gsp_panel_scenes_status_light_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "正在删除场景..." : "Deleting scene..."));
            }
        }
        return;
    }
    if (gsp_panel_event_is_rule_0_delete(event) ||
            gsp_panel_event_is_rule_1_delete(event) ||
            gsp_panel_event_is_rule_2_delete(event) ||
            gsp_panel_event_is_rule_3_delete(event)) {
        const int delete_index =
            gsp_panel_event_is_rule_0_delete(event) ? 0 :
            gsp_panel_event_is_rule_1_delete(event) ? 1 :
            gsp_panel_event_is_rule_2_delete(event) ? 2 : 3;
        if (!panel->rule_building &&
                (size_t)delete_index < panel->rules.visible) {
            if (app_hub_rule_delete(
                    panel->hub,
                    panel->rules.rules[delete_index].id) != APP_HUB_OK) {
                remember_error(panel, gsp_panel_automation_status_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "无法删除自动化" : "Could not delete"));
                remember_error(panel, gsp_panel_automation_status_light_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "无法删除自动化" : "Could not delete"));
            } else {
                remember_error(panel, gsp_panel_automation_status_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "正在删除自动化..." :
                                   "Deleting automation..."));
                remember_error(panel, gsp_panel_automation_status_light_set_text(
                                   ui, panel->lang == PANEL_LANG_ZH ?
                                   "正在删除自动化..." :
                                   "Deleting automation..."));
            }
        }
        return;
    }
    const int scene_index = selected_scene(event);
    if (panel->scene_building && scene_index >= 0) {
        int unused[APP_HUB_PAGE_SIZE];
        const size_t unused_count =
            collect_unused_scene_points(panel, unused, APP_HUB_PAGE_SIZE);
        if (panel->scene_picking) {
            if ((size_t)scene_index < unused_count &&
                    panel->scene_member_count < APP_HUB_PAGE_SIZE) {
                panel->scene_member_point[panel->scene_member_count] =
                    (int8_t)unused[scene_index];
                panel->scene_member_action[panel->scene_member_count] = 0;
                ++panel->scene_member_count;
                panel->scene_picking = false;
                panel->scene_empty_notice = false;
                render_scenes_tab(panel);
            }
            return;
        }
        if ((uint8_t)scene_index < panel->scene_member_count) {
            const uint8_t current = panel->scene_member_action[scene_index];
            panel->scene_member_action[scene_index] =
                current == 0U ? 2U : (current == 2U ? 1U : 0U);
            render_scenes_tab(panel);
            return;
        }
        if ((uint8_t)scene_index == panel->scene_member_count &&
                panel->scene_member_count < APP_HUB_PAGE_SIZE) {
            if (unused_count == 0) {
                panel->scene_empty_notice = true;
                panel->scene_picking = false;
                render_scenes_tab(panel);
                return;
            }
            panel->scene_picking = true;
            panel->scene_empty_notice = false;
            render_scenes_tab(panel);
            return;
        }
    }
    if (scene_index >= 0 && (size_t)scene_index < panel->scenes.visible) {
        const app_hub_scene_t *scene = &panel->scenes.scenes[scene_index];
        bool all_match = scene->action_count > 0;
        bool any_offline = false;
        for (uint8_t i = 0; i < scene->action_count; ++i) {
            const app_hub_point_t *point =
                find_snapshot_point(&panel->snapshot, scene->action_points[i]);
            if (!point) {
                all_match = false;
                continue;
            }
            if (!point->online) {
                any_offline = true;
            }
            if (point->on != scene->action_on[i]) {
                all_match = false;
            }
        }
        if (all_match) {
            remember_error(panel, gsp_panel_scenes_status_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "已是这个状态" : "Already in this state"));
            remember_error(panel, gsp_panel_scenes_status_light_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "已是这个状态" : "Already in this state"));
            return;
        }
        if (app_hub_scene_invoke(panel->hub, scene->id) != APP_HUB_OK) {
            remember_error(panel, gsp_panel_scenes_status_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "无法执行场景" : "Could not start scene"));
            remember_error(panel, gsp_panel_scenes_status_light_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "无法执行场景" : "Could not start scene"));
        } else {
            remember_error(panel, gsp_panel_scenes_status_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               (any_offline ? "设备离线，灯可能失败" :
                                "正在执行场景...") :
                               (any_offline ?
                                "Offline; light may not change" :
                                "Starting scene...")));
            remember_error(panel, gsp_panel_scenes_status_light_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               (any_offline ? "设备离线，灯可能失败" :
                                "正在执行场景...") :
                               (any_offline ?
                                "Offline; light may not change" :
                                "Starting scene...")));
        }
        return;
    }
    const int rule_index = selected_rule(event);
    if (panel->rule_building && rule_index >= 0 &&
            (size_t)rule_index < panel->snapshot.visible_points) {
        const app_hub_point_t *point = &panel->snapshot.points[rule_index];
        if (panel->rule_source < 0) {
            capture_rule_source(panel, (int8_t)rule_index);
        } else if (rule_index == panel->rule_source) {
            if (is_onboard_imu(point)) {
                strlcpy(panel->rule_source_value,
                        next_imu_trigger(panel->rule_source_value),
                        sizeof(panel->rule_source_value));
            } else if (point->controllable) {
                strlcpy(panel->rule_source_value,
                        strcmp(panel->rule_source_value, "on") == 0 ?
                        "off" : "on",
                        sizeof(panel->rule_source_value));
            }
        } else if (point->controllable) {
            panel->rule_save_needed = false;
            if (panel->rule_target == rule_index) {
                panel->rule_target_on = !panel->rule_target_on;
            } else {
                panel->rule_target = (int8_t)rule_index;
                panel->rule_target_on = !point->on;
            }
        }
        render_rules_tab(panel);
        return;
    }
    if (rule_index >= 0 && (size_t)rule_index < panel->rules.visible) {
        const app_hub_rule_t *rule = &panel->rules.rules[rule_index];
        if (app_hub_rule_set_enabled(panel->hub, rule->id,
                                     !rule->enabled) != APP_HUB_OK) {
            remember_error(panel, gsp_panel_automation_status_set_text(
                               ui, "Could not update automation"));
            remember_error(panel, gsp_panel_automation_status_light_set_text(
                               ui, "Could not update automation"));
        } else {
            remember_error(panel, gsp_panel_automation_status_set_text(
                               ui, "Saving automation..."));
            remember_error(panel, gsp_panel_automation_status_light_set_text(
                               ui, "Saving automation..."));
        }
        return;
    }
    if (gsp_panel_event_is_system_wifi(event)) {
        panel->wifi_from_settings = true;
        open_wifi_page(panel);
        return;
    }
    if (gsp_panel_event_is_system_role(event)) {
        panel->role_from_settings = true;
        open_role_page(panel);
        return;
    }
    if (gsp_panel_event_is_system_language(event)) {
        panel->lang_from_settings = true;
        open_lang_page(panel);
        return;
    }
    if (gsp_panel_event_is_system_theme(event)) {
        panel->theme = panel->theme == PANEL_THEME_DARK ?
                       PANEL_THEME_LIGHT : PANEL_THEME_DARK;
        save_theme_preference(panel->theme);
        apply_theme(panel);
        return;
    }
    if (gsp_panel_event_is_system_reboot(event)) {
        show_reboot_confirm(panel);
        return;
    }
    if (gsp_panel_event_is_lang_en(event)) {
        panel->lang = PANEL_LANG_EN;
        apply_i18n(panel);
        panel->rendered = false;
        panel->scenes.generation = 0;
        panel->rules.generation = 0;
        render_active_tab(panel);
        return;
    }
    if (gsp_panel_event_is_lang_zh(event)) {
        panel->lang = PANEL_LANG_ZH;
        apply_i18n(panel);
        panel->rendered = false;
        panel->scenes.generation = 0;
        panel->rules.generation = 0;
        render_active_tab(panel);
        return;
    }
    if (gsp_panel_event_is_lang_continue(event)) {
        after_language(panel);
        return;
    }
    if (gsp_panel_event_is_lang_back(event)) {
        open_settings_page(panel);
        return;
    }
    if (gsp_panel_event_is_wifi(event)) {
        open_settings_page(panel);
        return;
    }
    if (gsp_panel_event_is_wifi_back(event)) {
        panel->wifi_scan_pending = false;
        panel->wifi_prompted = false;
        if (panel->wifi_from_settings) {
            panel->wifi_from_settings = false;
            open_settings_page(panel);
        } else {
            open_lang_page(panel);
        }
        return;
    }
    if (gsp_panel_event_is_role_back(event)) {
        if (panel->role_from_settings) {
            panel->role_from_settings = false;
            open_settings_page(panel);
        } else {
            open_wifi_page(panel);
        }
        return;
    }
    if (gsp_panel_event_is_add_protocol_matter(event)) {
        select_add_protocol(panel, APP_HUB_PROTOCOL_MATTER);
        return;
    }
    if (gsp_panel_event_is_add_protocol_zigbee(event)) {
        select_add_protocol(panel, APP_HUB_PROTOCOL_ZIGBEE);
        return;
    }
    if (gsp_panel_event_is_add_protocol_mesh(event)) {
        select_add_protocol(panel, APP_HUB_PROTOCOL_BLE_MESH);
        return;
    }
    if (gsp_panel_event_is_add_protocol_mqtt(event)) {
        select_add_protocol(panel, APP_HUB_PROTOCOL_MQTT);
        return;
    }
    if (gsp_panel_event_is_add_protocol_modbus(event)) {
        select_add_protocol(panel, APP_HUB_PROTOCOL_MODBUS_TCP);
        return;
    }
    if (gsp_panel_event_is_add_back(event)) {
        if (panel->builder_name_mode != 0) {
            const panel_tab_t tab = panel->builder_name_mode == 1 ?
                                    PANEL_TAB_SCENES :
                                    PANEL_TAB_AUTOMATION;
            panel->builder_name_mode = 0;
            show_tab(panel, tab);
            render_active_tab(panel);
            return;
        }
        if (panel->add_commissioning) {
            (void)app_hub_add_cancel(panel->hub);
        } else if (panel->add_protocol_selected) {
            open_add_page(panel);
            return;
        }
        panel->add_commissioning = false;
        panel->add_success_ticks = 0;
        show_home_page(panel);
        return;
    }
    if (gsp_panel_event_is_add_cancel(event)) {
        if (panel->add_commissioning) {
            (void)app_hub_add_cancel(panel->hub);
            panel->add_commissioning = false;
            panel->add_failed = false;
            panel->add_field_error = false;
            panel->add_success_ticks = 0;
            show_home_page(panel);
            return;
        }
        if (panel->add_field_error) {
            panel->add_field_error = false;
            render_add_field(panel);
            return;
        }
        if (panel->add_failed) {
            if (app_hub_add_device(panel->hub,
                                   &panel->add_request) != APP_HUB_OK) {
                remember_error(panel, gsp_panel_add_result_meta_set_text(
                                   ui, copy_text(panel, COPY_ADD_FAILED)));
                remember_error(panel, gsp_panel_add_result_meta_light_set_text(
                                   ui, copy_text(panel, COPY_ADD_FAILED)));
                return;
            }
            panel->add_failed = false;
            panel->add_commissioning = true;
            freeze_add_inputs(panel);
            remember_error(panel, gsp_panel_add_cancel_text_set_text(
                               ui, copy_text(panel, COPY_CANCEL_PAIRING)));
            remember_error(panel, gsp_panel_add_cancel_text_light_set_text(
                               ui, copy_text(panel, COPY_CANCEL_PAIRING)));
            render_add_page(panel);
            return;
        }
        (void)app_hub_add_cancel(panel->hub);
        panel->add_commissioning = false;
        panel->add_success_ticks = 0;
        show_home_page(panel);
        return;
    }
    if (gsp_panel_event_is_add_input_box(event)) {
        if (panel->add_open && !panel->add_commissioning &&
                (panel->add_protocol_selected || panel->builder_name_mode != 0)) {
            attach_add_keyboard(panel);
        }
        return;
    }
    if (gsp_panel_event_is_add_keyboard_key(event)) {
        if (panel->add_commissioning) {
            return;
        }
        char text[APP_HUB_TOPIC_LEN] = {0};
        remember_error(panel, esp_gsp_keyboard_text(ui, text, sizeof(text)));
        if (panel->builder_name_mode != 0) {
            /* An untouched compiled keyboard reports its OK key token. Keep
             * the visible localized default instead of naming the item "ok". */
            const char *name = strcmp(text, "ok") == 0 ?
                               panel->builder_name : text;
            if (!name[0]) {
                remember_error(panel, gsp_panel_add_meta_set_text(
                                   ui,
                                   panel->lang == PANEL_LANG_ZH ?
                                   "名称不能为空" :
                                   "Name is required"));
                remember_error(panel, gsp_panel_add_meta_light_set_text(
                                   ui,
                                   panel->lang == PANEL_LANG_ZH ?
                                   "名称不能为空" :
                                   "Name is required"));
                return;
            }
            strlcpy(panel->builder_name, name,
                    sizeof(panel->builder_name));
            const uint8_t mode = panel->builder_name_mode;
            panel->builder_name_mode = 0;
            if (mode == 1) {
                panel->scene_building = true;
                panel->scene_picking = false;
                panel->scene_empty_notice = false;
                panel->scene_member_count = 0;
                memset(panel->scene_member_point, -1,
                       sizeof(panel->scene_member_point));
                memset(panel->scene_member_action, 0,
                       sizeof(panel->scene_member_action));
                show_tab(panel, PANEL_TAB_SCENES);
                render_scenes_tab(panel);
            } else {
                panel->rule_building = true;
                panel->rule_save_needed = false;
                panel->rule_source = -1;
                panel->rule_target = -1;
                panel->rule_target_on = false;
                panel->rule_source_value[0] = '\0';
                (void)app_hub_snapshot(panel->hub, 0, &panel->snapshot);
                show_tab(panel, PANEL_TAB_AUTOMATION);
                render_rules_tab(panel);
            }
            return;
        }
        submit_add_field(panel, text);
        return;
    }
    if (gsp_panel_event_is_role_hub(event)) {
        if (app_hub_set_role(panel->hub, APP_HUB_ROLE_HUB, NULL) !=
                APP_HUB_OK) {
            remember_error(panel, gsp_panel_role_meta_set_text(
                               ui, "Could not save Hub mode"));
            remember_error(panel, gsp_panel_role_meta_light_set_text(
                               ui, "Could not save Hub mode"));
            return;
        }
        if (panel->role_from_settings) {
            open_settings_page(panel);
        } else {
            close_role_page(panel);
            show_home_page(panel);
        }
        return;
    }
    if (gsp_panel_event_is_role_web(event)) {
        if (app_hub_set_role(panel->hub, APP_HUB_ROLE_WEB, NULL) != APP_HUB_OK) {
            remember_error(panel, gsp_panel_role_meta_set_text(
                               ui, "Could not save Panel mode"));
            remember_error(panel, gsp_panel_role_meta_light_set_text(
                               ui, "Could not save Panel mode"));
            return;
        }
        panel->snapshot.role = APP_HUB_ROLE_WEB;
        panel->snapshot.url_needed = true;
        open_token_page_with_back(panel, panel->role_from_settings);
        return;
    }
    if (gsp_panel_event_is_token_back(event)) {
        close_token_page(panel);
        if (panel->role_from_settings) {
            open_settings_page(panel);
        } else if (!panel->snapshot.wifi_ready) {
            open_wifi_page(panel);
        } else if (onboarding(panel) || !panel->snapshot.connected) {
            open_role_page(panel);
        }
        return;
    }
    if (gsp_panel_event_is_token_keyboard_key(event)) {
        char text[APP_HUB_TOKEN_LEN] = {0};
        remember_error(panel, esp_gsp_keyboard_text(ui, text, sizeof(text)));
        if (panel->token_url_step) {
            const app_hub_result_t result = app_hub_set_url(panel->hub, text);
            if (result == APP_HUB_ERR_INVALID_ARG) {
                remember_error(panel, gsp_panel_token_meta_set_text(
                                   ui, "Hub URL required"));
                remember_error(panel, gsp_panel_token_meta_light_set_text(
                                   ui, "Hub URL required"));
            } else if (result != APP_HUB_OK) {
                remember_error(panel, gsp_panel_token_meta_set_text(
                                   ui, "URL queue full"));
                remember_error(panel, gsp_panel_token_meta_light_set_text(
                                   ui, "URL queue full"));
            } else {
                panel->token_url_step = false;
                remember_error(panel, gsp_panel_token_set_text(ui, ""));
                remember_error(panel, gsp_panel_token_title_set_text(
                                   ui, copy_text(panel, COPY_TOKEN)));
                remember_error(panel, gsp_panel_token_title_light_set_text(
                                   ui, copy_text(panel, COPY_TOKEN)));
                remember_error(panel, gsp_panel_token_meta_set_text(
                                   ui, copy_text(panel, COPY_TOKEN_META)));
                remember_error(panel, gsp_panel_token_meta_light_set_text(
                                   ui, copy_text(panel, COPY_TOKEN_META)));
            }
            return;
        }
        const app_hub_result_t result = app_hub_set_token(panel->hub, text);
        if (result != APP_HUB_OK) {
            remember_error(panel, gsp_panel_token_meta_set_text(
                               ui, "Token queue full"));
            remember_error(panel, gsp_panel_token_meta_light_set_text(
                               ui, "Token queue full"));
        } else {
            remember_error(panel, gsp_panel_token_meta_set_text(
                               ui, "Connecting to MiniHub"));
            remember_error(panel, gsp_panel_token_meta_light_set_text(
                               ui, "Connecting to MiniHub"));
        }
        return;
    }
    if (gsp_panel_event_is_wifi_password_back(event)) {
        PANEL_LOGI("Wi-Fi password back event");
        panel->wifi_connect_submitted = false;
        panel->wifi_cursor_delay_ticks = 0;
        panel->wifi_error_dismissed = true;
        panel->wifi_ssid[0] = '\0';
        set_wifi_password_error(panel, false);
        show_wifi_page(panel, true);
        return;
    }
    if (gsp_panel_event_is_wifi_keyboard_key(event) ||
            gsp_panel_event_is_wifi_keyboard_light_key(event)) {
        char password[APP_HUB_WIFI_PASSWORD_LEN] = {0};
        remember_error(panel, esp_gsp_keyboard_text(ui, password,
                                                    sizeof(password)));
        connect_selected_wifi(panel, password);
        return;
    }
    if (gsp_panel_event_is_wifi_list(event)) {
        select_wifi_ap(panel, event->item);
        return;
    }
    if (gsp_panel_event_is_detail_back(event)) {
        close_detail(panel);
        return;
    }
    if (gsp_panel_event_is_detail_reconnect(event)) {
        request_device_reconnect(panel);
        return;
    }
    if (gsp_panel_event_is_detail_delete(event)) {
        if (panel->detail_point_id[0] != '\0') {
            (void)app_hub_delete_point(panel->hub, panel->detail_point_id,
                                       panel->detail_node_id);
        }
        close_detail(panel);
        return;
    }
    if (gsp_panel_event_is_detail_power(event)) {
        const app_hub_point_t *point =
            find_detail_point(panel, &panel->snapshot);
        if (point && !point->online) {
            request_device_reconnect(panel);
            return;
        }
        if (app_hub_toggle(panel->hub, panel->detail_point_id) != APP_HUB_OK) {
            remember_error(panel, gsp_panel_detail_meta_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "命令队列已满" : "Command queue full"));
            remember_error(panel, gsp_panel_detail_meta_light_set_text(
                               ui, panel->lang == PANEL_LANG_ZH ?
                               "命令队列已满" : "Command queue full"));
        }
        return;
    }
    if (gsp_panel_event_is_detail_brightness(event)) {
        detail_parameter_event(panel, "brightness", (int32_t)event->arg,
                               gsp_panel_detail_brightness_value_set_text);
        return;
    }
    if (gsp_panel_event_is_detail_hue(event)) {
        detail_parameter_event(panel, "hue", (int32_t)event->arg,
                               gsp_panel_detail_hue_value_set_text);
        return;
    }
    if (gsp_panel_event_is_detail_saturation(event)) {
        detail_parameter_event(panel, "saturation", (int32_t)event->arg,
                               gsp_panel_detail_saturation_value_set_text);
        return;
    }
    if (gsp_panel_event_is_detail_cct(event)) {
        detail_parameter_event(panel, "cct", (int32_t)event->arg,
                               gsp_panel_detail_cct_value_set_text);
        return;
    }
    if (gsp_panel_event_is_previous(event)) {
        if (panel->page > 0) {
            --panel->page;
        }
        update_ui(panel);
        return;
    }
    if (gsp_panel_event_is_next(event)) {
        if (panel->page + 1 < panel->snapshot.page_count) {
            ++panel->page;
        }
        update_ui(panel);
        return;
    }
    if (gsp_panel_event_is_refresh(event)) {
        return;
    }

    const int card = selected_card(event);
    if (card >= 0 && overlay_open(panel)) {
        return;
    }
    if (card >= 0 && card == add_card_slot(&panel->snapshot)) {
        open_add_page(panel);
        return;
    }
    if (card < 0 || (size_t)card >= panel->snapshot.visible_points) {
        return;
    }
    const app_hub_point_t *point = &panel->snapshot.points[card];
    if (point->commissioning || point->commission_failed ||
            !point->controllable || is_light_point(point)) {
        panel->detail_open = true;
        snprintf(panel->detail_point_id, sizeof(panel->detail_point_id), "%s",
                 point->point_id);
        render_detail(panel, point);
        set_detail_shell(panel, true);
        return;
    }
    (void)app_hub_toggle(panel->hub, point->point_id);
}

esp_gsp_err_t app_panel_create(esp_gsp_handle_t ui, app_hub_handle_t hub,
                               app_panel_handle_t *out_handle)
{
    if (!ui || !hub || !out_handle) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;
    struct app_panel *panel = calloc(1, sizeof(*panel));
    if (!panel) {
        return ESP_GSP_ERR_NO_MEM;
    }
    panel->ui = ui;
    panel->hub = hub;
    panel->lang = PANEL_LANG_EN;
    panel->theme = load_theme_preference();
    panel->wifi_list = ESP_GSP_LIST_NONE;
    panel->wifi_list_light = ESP_GSP_LIST_NONE;
    remember_error(panel, esp_gsp_set_cursor(ui, ESP_GSP_NO_CURSOR));
    panel->wifi_list = gsp_panel_wifi_list_bind(ui, wifi_list_row, panel);
    panel->wifi_list_light = gsp_panel_wifi_list_light_bind(ui, wifi_list_row,
                                                           panel);
    (void)app_hub_snapshot(hub, 0, &panel->snapshot);
    set_home_interactive(panel, false);
    apply_i18n(panel);
    hide_blocking_overlays(panel);
#if !defined(ESP_PLATFORM)
    if (getenv("MINIHUB_PANEL_OPEN_SETTINGS") != NULL) {
        show_tab(panel, PANEL_TAB_SETTINGS);
        panel->system.generation = 0;
        render_system_tab(panel);
    } else if (getenv("MINIHUB_PANEL_OPEN_ADD") != NULL) {
        open_add_page(panel);
        select_add_protocol(panel, APP_HUB_PROTOCOL_MQTT);
    } else if (getenv("MINIHUB_PANEL_OPEN_WIFI") != NULL) {
        open_wifi_page(panel);
    } else if (getenv("MINIHUB_PANEL_OPEN_WIFI_PASSWORD") != NULL) {
        app_hub_wifi_ap_t ap = {
            .rssi = -50,
            .secured = true,
        };
        strlcpy(ap.ssid, "Office", sizeof(ap.ssid));
        panel->theme = getenv("MINIHUB_PANEL_THEME_DARK") != NULL ?
                       PANEL_THEME_DARK : PANEL_THEME_LIGHT;
        apply_theme(panel);
        open_wifi_page(panel);
        open_wifi_password(panel, &ap);
    } else
#endif
    if (onboarding(panel)) {
        open_welcome_page(panel);
    } else if (panel->snapshot.wifi_needs_setup) {
        open_wifi_page(panel);
    } else if (panel->snapshot.url_needed || panel->snapshot.auth_needed) {
        open_token_page(panel);
    } else {
        show_home_page(panel);
    }
    update_ui(panel);
    panel->refresh_timer = esp_gsp_timer_create(ui, PANEL_REFRESH_MS,
                                                refresh_tick, panel);
    if (!panel->refresh_timer) {
        free(panel);
        return ESP_GSP_ERR_NO_MEM;
    }
    esp_gsp_err_t err = esp_gsp_on_component_event(
                            ui, device_swipe_event, panel);
    if (err != ESP_GSP_OK) {
        esp_gsp_timer_delete(ui, panel->refresh_timer);
        free(panel);
        return err;
    }
    err = esp_gsp_on_event(ui, panel_event, panel);
    if (err != ESP_GSP_OK) {
        (void)esp_gsp_on_component_event(ui, NULL, NULL);
        esp_gsp_timer_delete(ui, panel->refresh_timer);
        free(panel);
        return err;
    }
    *out_handle = panel;
    return ESP_GSP_OK;
}
