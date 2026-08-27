// SPDX-License-Identifier: Apache-2.0

#include "esp_err.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "bsp/esp_mosaico.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iris_ota_support.h"
#include "iris_screen_mirror.h"
#include "nvs_flash.h"

static const char *TAG = "get_started";

#if CONFIG_GET_STARTED_RECOVERY
#define DISPLAY_TITLE "Recovery Mode"
#else
#define DISPLAY_TITLE "ESP-MOSAICO"
#endif

#define COLOR_PAPER       lv_color_hex(0xF6F6F3)
#define COLOR_TEXT        lv_color_hex(0x101010)
#define COLOR_MUTED       lv_color_hex(0x777777)
#define COLOR_LINE        lv_color_hex(0xD9DCE0)
#define COLOR_SURFACE     lv_color_hex(0xFAFAFA)
#define COLOR_ORANGE      lv_color_hex(0xFF4C01)

#if CONFIG_GET_STARTED_RECOVERY
typedef enum {
    RECOVERY_CONNECTION_STARTING,
    RECOVERY_CONNECTION_WAITING,
    RECOVERY_CONNECTION_NEGOTIATING,
    RECOVERY_CONNECTION_READY,
    RECOVERY_CONNECTION_FAILED,
} recovery_connection_state_t;

typedef struct {
    const char *text;
    lv_color_t color;
} recovery_connection_style_t;

static lv_obj_t *s_recovery_status_dot;
static lv_obj_t *s_recovery_status_label;

static lv_obj_t *create_box(lv_obj_t *parent, int32_t width, int32_t height,
                            lv_color_t color, int32_t radius)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_bg_color(box, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(box, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return box;
}
#endif

static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text_static(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

#if CONFIG_GET_STARTED_RECOVERY
static recovery_connection_style_t recovery_connection_style(
    recovery_connection_state_t state)
{
    switch (state) {
    case RECOVERY_CONNECTION_WAITING:
        return (recovery_connection_style_t) {
            .text = "Waiting for ESP-Iris Gateway",
            .color = COLOR_MUTED,
        };
    case RECOVERY_CONNECTION_NEGOTIATING:
        return (recovery_connection_style_t) {
            .text = "Connecting to Gateway",
            .color = COLOR_ORANGE,
        };
    case RECOVERY_CONNECTION_READY:
        return (recovery_connection_style_t) {
            .text = "Gateway connected - Ready to update",
            .color = lv_color_hex(0x2E7D32),
        };
    case RECOVERY_CONNECTION_FAILED:
        return (recovery_connection_style_t) {
            .text = "Recovery service unavailable",
            .color = lv_color_hex(0xC62828),
        };
    case RECOVERY_CONNECTION_STARTING:
    default:
        return (recovery_connection_style_t) {
            .text = "Starting recovery service",
            .color = COLOR_MUTED,
        };
    }
}

static recovery_connection_state_t recovery_connection_from_status(
    const esp_iris_status_t *status)
{
    if (status == NULL || !status->started ||
        status->lifecycle == ESP_IRIS_LIFECYCLE_STOPPED ||
        status->lifecycle == ESP_IRIS_LIFECYCLE_STARTING ||
        status->lifecycle == ESP_IRIS_LIFECYCLE_STOPPING) {
        return RECOVERY_CONNECTION_STARTING;
    }
    if (status->lifecycle == ESP_IRIS_LIFECYCLE_FAILED) {
        return RECOVERY_CONNECTION_FAILED;
    }
    if (!status->link_connected) {
        return RECOVERY_CONNECTION_WAITING;
    }
    if (!status->session_ready) {
        return RECOVERY_CONNECTION_NEGOTIATING;
    }
    return RECOVERY_CONNECTION_READY;
}

static void update_recovery_connection_ui(recovery_connection_state_t state)
{
    const recovery_connection_style_t style =
        recovery_connection_style(state);
    if (!bsp_display_lock(-1)) {
        ESP_LOGW(TAG, "Failed to lock LVGL for recovery status update");
        return;
    }

    if (s_recovery_status_dot != NULL && s_recovery_status_label != NULL) {
        lv_obj_set_style_bg_color(s_recovery_status_dot, style.color,
                                  LV_PART_MAIN);
        lv_label_set_text(s_recovery_status_label, style.text);
    }
    bsp_display_unlock();
    ESP_LOGI(TAG, "Recovery connection state: %s", style.text);
}

static void recovery_connection_task(void *arg)
{
    (void)arg;
    recovery_connection_state_t previous = RECOVERY_CONNECTION_FAILED;
    bool have_previous = false;

    while (true) {
        esp_iris_status_t status;
        recovery_connection_state_t current = RECOVERY_CONNECTION_STARTING;
        if (esp_iris_get_status(&status) == ESP_OK) {
            current = recovery_connection_from_status(&status);
        }

        if (!have_previous || current != previous) {
            update_recovery_connection_ui(current);
            previous = current;
            have_previous = true;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

static void create_recovery_screen(lv_obj_t *screen)
{
    lv_obj_t *brand_esp = create_label(screen, "ESP-", &lv_font_montserrat_14,
                                       COLOR_TEXT);
    lv_obj_set_style_text_letter_space(brand_esp, 1, LV_PART_MAIN);
    lv_obj_align(brand_esp, LV_ALIGN_TOP_LEFT, 40, 36);

    lv_obj_t *brand_mosaico = create_label(
        screen, "MOSAICO", &lv_font_montserrat_14, COLOR_ORANGE);
    lv_obj_set_style_text_letter_space(brand_mosaico, 1, LV_PART_MAIN);
    lv_obj_align_to(brand_mosaico, brand_esp, LV_ALIGN_OUT_RIGHT_MID, 1, 0);

    lv_obj_t *badge = create_box(screen, 104, 30, COLOR_TEXT, 15);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -40, 28);
    lv_obj_t *badge_label = create_label(
        badge, "RECOVERY", &lv_font_montserrat_14, COLOR_PAPER);
    lv_obj_center(badge_label);

    lv_obj_t *recovery_arc = lv_arc_create(screen);
    lv_obj_set_size(recovery_arc, 112, 112);
    lv_obj_align(recovery_arc, LV_ALIGN_TOP_MID, 0, 91);
    lv_arc_set_rotation(recovery_arc, 120);
    lv_arc_set_bg_angles(recovery_arc, 0, 300);
    lv_arc_set_range(recovery_arc, 0, 100);
    lv_arc_set_value(recovery_arc, 84);
    lv_obj_set_style_arc_color(recovery_arc, COLOR_LINE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(recovery_arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(recovery_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(recovery_arc, COLOR_ORANGE,
                               LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(recovery_arc, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(recovery_arc, true, LV_PART_INDICATOR);
    lv_obj_remove_style(recovery_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(recovery_arc,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *module = create_box(screen, 38, 38, COLOR_TEXT, 9);
    lv_obj_align_to(module, recovery_arc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *module_core = create_box(module, 10, 10, COLOR_ORANGE, 3);
    lv_obj_center(module_core);

    lv_obj_t *title = create_label(screen, DISPLAY_TITLE,
                                   &lv_font_montserrat_48, COLOR_TEXT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 226);

    lv_obj_t *subtitle = create_label(
        screen, "Firmware update service is ready",
        &lv_font_montserrat_14, lv_color_hex(0x383B40));
    lv_obj_set_width(subtitle, LV_PCT(100));
    lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 293);

    lv_obj_t *status = create_box(screen, 360, 54, COLOR_SURFACE, 27);
    lv_obj_set_style_border_width(status, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(status, COLOR_LINE, LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 335);

    s_recovery_status_dot = create_box(status, 10, 10, COLOR_MUTED,
                                       LV_RADIUS_CIRCLE);
    lv_obj_align(s_recovery_status_dot, LV_ALIGN_LEFT_MID, 22, 0);
    s_recovery_status_label = create_label(
        status, "Starting recovery service", &lv_font_montserrat_14,
        COLOR_TEXT);
    lv_obj_align(s_recovery_status_label, LV_ALIGN_LEFT_MID, 46, 0);

    lv_obj_t *footer = create_label(
        screen, "Keep USB connected during recovery",
        &lv_font_montserrat_14, COLOR_MUTED);
    lv_obj_set_width(footer, LV_PCT(100));
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(footer, LV_ALIGN_TOP_MID, 0, 419);
}
#else
static void create_application_screen(lv_obj_t *screen)
{
    lv_obj_t *title = create_label(screen, DISPLAY_TITLE,
                                   &lv_font_montserrat_48, COLOR_TEXT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(title);
}
#endif

static esp_err_t show_display_title(void)
{
    lv_display_t *display = bsp_display_start();
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to start the ESP-Mosaico display");
        return ESP_FAIL;
    }

    if (!bsp_display_lock(-1)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return ESP_FAIL;
    }

    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, COLOR_PAPER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

#if CONFIG_GET_STARTED_RECOVERY
    create_recovery_screen(screen);
#else
    create_application_screen(screen);
#endif

    bsp_display_unlock();
    ESP_LOGI(TAG, "Displaying %s at %dx%d", DISPLAY_TITLE, BSP_LCD_H_RES,
             BSP_LCD_V_RES);
    return ESP_OK;
}

static void hello_world_task(void *arg)
{
    (void)arg;
    TickType_t next_wake = xTaskGetTickCount();

    while (true) {
        ESP_LOGI(TAG, "HELLO WORLD");
        vTaskDelayUntil(&next_wake, pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(show_display_title());
    ESP_ERROR_CHECK(iris_screen_mirror_register());
    iris_ota_support_start();

#if CONFIG_GET_STARTED_RECOVERY
    ESP_ERROR_CHECK(xTaskCreate(recovery_connection_task, "recovery_status",
                                2048, NULL, 4, NULL) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
#endif

    ESP_ERROR_CHECK(xTaskCreate(hello_world_task, "hello_world", 2048, NULL, 4,
                                NULL) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
}
