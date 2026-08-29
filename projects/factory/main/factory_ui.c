#include "factory_ui.h"

#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"

#include "bsp/esp_mosaico.h"
#include "esp_check.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#if CONFIG_GET_STARTED_RECOVERY
#include "esp_iris_system_update.h"
#include "esp_wifi.h"
#include "factory_network.h"
#endif

#define COLOR_PAPER   lv_color_hex(0xF6F6F3)
#define COLOR_TEXT    lv_color_hex(0x101010)
#define COLOR_MUTED   lv_color_hex(0x777777)
#define COLOR_LINE    lv_color_hex(0xD9DCE0)
#define COLOR_SURFACE lv_color_hex(0xFAFAFA)
#define COLOR_ORANGE  lv_color_hex(0xFF4C01)
#define COLOR_GREEN   lv_color_hex(0x2E7D32)
#define COLOR_RED     lv_color_hex(0xC62828)

static const char *TAG = "factory_ui";

static lv_obj_t *label_create(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

#if CONFIG_GET_STARTED_RECOVERY
static lv_obj_t *box_create(lv_obj_t *parent, int32_t width, int32_t height,
                            lv_color_t color, int32_t radius)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_bg_color(box, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(box, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}
#endif

static void screen_prepare(lv_obj_t *screen)
{
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, COLOR_PAPER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

#if CONFIG_GET_STARTED_RECOVERY

typedef enum {
    FACTORY_PAGE_READY = 0,
    FACTORY_PAGE_WIFI,
    FACTORY_PAGE_PAIRING,
    FACTORY_PAGE_PASSWORD,
    FACTORY_PAGE_UPDATE,
    FACTORY_PAGE_RESULT,
} factory_page_t;

typedef struct {
    lv_display_t *display;
    factory_page_t page;
    lv_obj_t *ready_screen;
    lv_obj_t *wifi_screen;
    lv_obj_t *pairing_screen;
    lv_obj_t *password_screen;
    lv_obj_t *update_screen;
    lv_obj_t *result_screen;
    lv_obj_t *usb_dot;
    lv_obj_t *usb_value;
    lv_obj_t *network_dot;
    lv_obj_t *network_value;
    lv_obj_t *address;
    lv_obj_t *wifi_connection;
    lv_obj_t *wifi_connection_detail;
    lv_obj_t *wifi_list;
    lv_obj_t *wifi_scan_status;
    lv_obj_t *pairing_endpoint;
    lv_obj_t *pairing_token;
    lv_obj_t *password_title;
    lv_obj_t *password_input;
    lv_obj_t *password_toggle;
    lv_obj_t *update_title;
    lv_obj_t *update_detail;
    lv_obj_t *update_percent;
    lv_obj_t *update_bar;
    lv_obj_t *update_owner;
    lv_obj_t *result_mark;
    lv_obj_t *result_title;
    lv_obj_t *result_detail;
    char selected_ssid[FACTORY_NETWORK_SSID_BYTES];
    char ap_ssids[FACTORY_NETWORK_MAX_RESULTS][FACTORY_NETWORK_SSID_BYTES];
    uint32_t scan_generation;
    esp_iris_system_update_phase_t update_phase;
} factory_ui_context_t;

static factory_ui_context_t s_ui;

static lv_obj_t *button_create(lv_obj_t *parent, const char *text,
                               int32_t width, int32_t height, bool primary)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_radius(button, height / 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, primary ? 0 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, COLOR_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, primary ? COLOR_TEXT : COLOR_SURFACE,
                              LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_t *label = label_create(button, text, &lv_font_montserrat_14,
                                   primary ? COLOR_PAPER : COLOR_TEXT);
    lv_obj_center(label);
    return button;
}

static void header_create(lv_obj_t *screen, const char *section,
                          const char *title, bool back);
static void show_page(factory_page_t page);

static void page_event(lv_event_t *event)
{
    show_page((factory_page_t)(intptr_t)lv_event_get_user_data(event));
}

static void scan_event(lv_event_t *event)
{
    (void)event;
    (void)factory_network_request_scan();
}

static void forget_event(lv_event_t *event)
{
    (void)event;
    (void)factory_network_forget();
}

static void password_submit(void)
{
    const char *password = lv_textarea_get_text(s_ui.password_input);
    const esp_err_t err = factory_network_connect(s_ui.selected_ssid, password);
    if (err == ESP_OK) {
        show_page(FACTORY_PAGE_READY);
    } else {
        lv_label_set_text(s_ui.password_title, "Check password and try again");
        lv_obj_set_style_text_color(s_ui.password_title, COLOR_RED,
                                    LV_PART_MAIN);
    }
}

static void keyboard_event(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY) {
        password_submit();
    } else if (code == LV_EVENT_CANCEL) {
        show_page(FACTORY_PAGE_WIFI);
    }
}

static void password_toggle_event(lv_event_t *event)
{
    (void)event;
    const bool hidden = lv_textarea_get_password_mode(s_ui.password_input);
    lv_textarea_set_password_mode(s_ui.password_input, !hidden);
    lv_label_set_text(lv_obj_get_child(s_ui.password_toggle, 0),
                      hidden ? "Hide" : "Show");
}

static void network_select_event(lv_event_t *event)
{
    const char *ssid = lv_event_get_user_data(event);
    if (ssid == NULL || ssid[0] == '\0') {
        return;
    }
    strlcpy(s_ui.selected_ssid, ssid, sizeof(s_ui.selected_ssid));
    lv_label_set_text_fmt(s_ui.password_title, "Join %s", ssid);
    lv_obj_set_style_text_color(s_ui.password_title, COLOR_TEXT, LV_PART_MAIN);
    lv_textarea_set_text(s_ui.password_input, "");
    show_page(FACTORY_PAGE_PASSWORD);
}

static void header_create(lv_obj_t *screen, const char *section,
                          const char *title, bool back)
{
    if (back) {
        lv_obj_t *button = lv_button_create(screen);
        lv_obj_set_size(button, 48, 48);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT, 22, 21);
        lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(button, page_event, LV_EVENT_CLICKED,
                            (void *)(intptr_t)FACTORY_PAGE_READY);
        lv_obj_t *arrow = label_create(button, LV_SYMBOL_LEFT,
                                       &lv_font_montserrat_22, COLOR_TEXT);
        lv_obj_center(arrow);
    } else {
        lv_obj_t *brand = label_create(screen, "ESP-", &lv_font_montserrat_14,
                                       COLOR_TEXT);
        lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 40, 36);
        lv_obj_t *mosaico = label_create(screen, "MOSAICO",
                                         &lv_font_montserrat_14, COLOR_ORANGE);
        lv_obj_align_to(mosaico, brand, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    }
    lv_obj_t *eyebrow = label_create(screen, section,
                                     &lv_font_montserrat_12, COLOR_ORANGE);
    lv_obj_set_style_text_letter_space(eyebrow, 1, LV_PART_MAIN);
    lv_obj_align(eyebrow, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_t *heading = label_create(screen, title, &lv_font_montserrat_22,
                                     COLOR_TEXT);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 43);
}

static lv_obj_t *status_row_create(lv_obj_t *parent, int32_t y,
                                   const char *name, lv_obj_t **dot,
                                   lv_obj_t **value)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, 360, 45);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, y == 0 ? 0 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, COLOR_LINE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    *dot = box_create(row, 9, 9, COLOR_MUTED, LV_RADIUS_CIRCLE);
    lv_obj_align(*dot, LV_ALIGN_LEFT_MID, 17, 0);
    lv_obj_t *label = label_create(row, name, &lv_font_montserrat_14,
                                   COLOR_TEXT);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 39, 0);
    *value = label_create(row, "Starting", &lv_font_montserrat_12,
                          COLOR_MUTED);
    lv_obj_align(*value, LV_ALIGN_RIGHT_MID, -17, 0);
    return row;
}

static void ready_screen_create(void)
{
    s_ui.ready_screen = lv_obj_create(NULL);
    screen_prepare(s_ui.ready_screen);
    header_create(s_ui.ready_screen, "", "", false);

    lv_obj_t *badge = box_create(s_ui.ready_screen, 104, 30, COLOR_TEXT, 15);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -40, 28);
    lv_obj_t *badge_label = label_create(badge, "RECOVERY",
                                         &lv_font_montserrat_12, COLOR_PAPER);
    lv_obj_center(badge_label);

    lv_obj_t *arc = lv_arc_create(s_ui.ready_screen);
    lv_obj_set_size(arc, 82, 82);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 80);
    lv_arc_set_rotation(arc, 120);
    lv_arc_set_bg_angles(arc, 0, 300);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 84);
    lv_obj_set_style_arc_color(arc, COLOR_LINE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *core = box_create(s_ui.ready_screen, 30, 30, COLOR_TEXT, 8);
    lv_obj_align_to(core, arc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *core_dot = box_create(core, 8, 8, COLOR_ORANGE, 3);
    lv_obj_center(core_dot);

    lv_obj_t *title = label_create(s_ui.ready_screen, "Recovery Mode",
                                   &lv_font_montserrat_32, COLOR_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 177);
    lv_obj_t *subtitle = label_create(
        s_ui.ready_screen, "Firmware update service is ready",
        &lv_font_montserrat_14, lv_color_hex(0x383B40));
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 217);

    lv_obj_t *panel = box_create(s_ui.ready_screen, 360, 90,
                                 COLOR_SURFACE, 18);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, COLOR_LINE, LV_PART_MAIN);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 250);
    status_row_create(panel, 0, "USB", &s_ui.usb_dot, &s_ui.usb_value);
    status_row_create(panel, 45, "Network", &s_ui.network_dot,
                      &s_ui.network_value);

    s_ui.address = label_create(s_ui.ready_screen,
                                "Network setup not completed",
                                &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_set_width(s_ui.address, 400);
    lv_obj_set_style_text_align(s_ui.address, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);
    lv_obj_align(s_ui.address, LV_ALIGN_TOP_MID, 0, 351);

    lv_obj_t *settings = button_create(s_ui.ready_screen, "Wi-Fi", 174, 44,
                                       true);
    lv_obj_align(settings, LV_ALIGN_TOP_LEFT, 60, 383);
    lv_obj_add_event_cb(settings, page_event, LV_EVENT_CLICKED,
                        (void *)(intptr_t)FACTORY_PAGE_WIFI);
    lv_obj_t *pairing = button_create(s_ui.ready_screen, "TCP pairing", 174,
                                      44, false);
    lv_obj_align(pairing, LV_ALIGN_TOP_RIGHT, -60, 383);
    lv_obj_add_event_cb(pairing, page_event, LV_EVENT_CLICKED,
                        (void *)(intptr_t)FACTORY_PAGE_PAIRING);
    lv_obj_t *footer = label_create(
        s_ui.ready_screen, "USB OTA remains available while offline",
        &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void pairing_screen_create(void)
{
    s_ui.pairing_screen = lv_obj_create(NULL);
    screen_prepare(s_ui.pairing_screen);
    header_create(s_ui.pairing_screen, "RECOVERY", "TCP pairing", true);

    lv_obj_t *card = box_create(s_ui.pairing_screen, 368, 116, COLOR_TEXT, 18);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 91);
    lv_obj_t *endpoint_label = label_create(card, "DISCOVERABLE ENDPOINT",
                                            &lv_font_montserrat_12,
                                            COLOR_ORANGE);
    lv_obj_align(endpoint_label, LV_ALIGN_TOP_LEFT, 18, 17);
    s_ui.pairing_endpoint = label_create(card, "Connect Wi-Fi first",
                                         &lv_font_montserrat_14, COLOR_PAPER);
    lv_obj_set_width(s_ui.pairing_endpoint, 332);
    lv_obj_set_style_text_align(s_ui.pairing_endpoint, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);
    lv_obj_align(s_ui.pairing_endpoint, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_t *port = label_create(card, "", &lv_font_montserrat_12,
                                  lv_color_hex(0xC7C7C2));
    lv_label_set_text_fmt(port, "mDNS service _esp-iris._tcp - port %u",
                          CONFIG_ESP_IRIS_TCP_PORT);
    lv_obj_align(port, LV_ALIGN_BOTTOM_MID, 0, -18);

    lv_obj_t *token_label = label_create(s_ui.pairing_screen,
                                         "PAIRING TOKEN",
                                         &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_align(token_label, LV_ALIGN_TOP_LEFT, 57, 232);
    lv_obj_t *token_card = box_create(s_ui.pairing_screen, 368, 106,
                                      COLOR_SURFACE, 16);
    lv_obj_set_style_border_width(token_card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(token_card, COLOR_LINE, LV_PART_MAIN);
    lv_obj_align(token_card, LV_ALIGN_TOP_MID, 0, 256);
    s_ui.pairing_token = label_create(token_card, "Available after startup",
                                      &lv_font_montserrat_14, COLOR_TEXT);
    lv_obj_set_width(s_ui.pairing_token, 328);
    lv_obj_set_style_text_align(s_ui.pairing_token, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);
    lv_obj_center(s_ui.pairing_token);

    lv_obj_t *help = label_create(
        s_ui.pairing_screen,
        "Enter this token once in the Gateway. It never crosses the TCP link.",
        &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_set_width(help, 368);
    lv_obj_set_style_text_align(help, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(help, LV_ALIGN_TOP_MID, 0, 382);
}

static void pairing_screen_update(void)
{
    factory_network_snapshot_t network = {0};
    if (factory_network_get_snapshot(&network) == ESP_OK &&
        network.state == FACTORY_NETWORK_CONNECTED) {
        lv_label_set_text_fmt(s_ui.pairing_endpoint, "%s.local\n%s:%u",
                              network.hostname, network.ip,
                              CONFIG_ESP_IRIS_TCP_PORT);
    } else {
        lv_label_set_text(s_ui.pairing_endpoint, "Connect Wi-Fi first");
    }

    char token[65] = {0};
    if (esp_iris_pairing_token_get(token) == ESP_OK) {
        char wrapped[66];
        memcpy(wrapped, token, 32);
        wrapped[32] = '\n';
        memcpy(wrapped + 33, token + 32, 32);
        wrapped[65] = '\0';
        lv_label_set_text(s_ui.pairing_token, wrapped);
    } else {
        lv_label_set_text(s_ui.pairing_token, "Pairing token unavailable");
    }
}

static void wifi_screen_create(void)
{
    s_ui.wifi_screen = lv_obj_create(NULL);
    screen_prepare(s_ui.wifi_screen);
    header_create(s_ui.wifi_screen, "RECOVERY", "Wi-Fi", true);
    lv_obj_t *rescan = button_create(s_ui.wifi_screen, "Rescan", 82, 36,
                                     false);
    lv_obj_align(rescan, LV_ALIGN_TOP_RIGHT, -26, 27);
    lv_obj_add_event_cb(rescan, scan_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *connected = box_create(s_ui.wifi_screen, 368, 76,
                                     COLOR_TEXT, 17);
    lv_obj_align(connected, LV_ALIGN_TOP_MID, 0, 83);
    lv_obj_t *kicker = label_create(connected, "NETWORK",
                                    &lv_font_montserrat_12, COLOR_ORANGE);
    lv_obj_align(kicker, LV_ALIGN_TOP_LEFT, 17, 12);
    s_ui.wifi_connection = label_create(connected, "Not connected",
                                        &lv_font_montserrat_14, COLOR_PAPER);
    lv_obj_align(s_ui.wifi_connection, LV_ALIGN_TOP_LEFT, 17, 31);
    s_ui.wifi_connection_detail = label_create(
        connected, "Choose a network below", &lv_font_montserrat_12,
        lv_color_hex(0xC7C7C2));
    lv_obj_align(s_ui.wifi_connection_detail, LV_ALIGN_TOP_LEFT, 17, 50);
    lv_obj_t *forget = lv_button_create(connected);
    lv_obj_set_size(forget, 70, 34);
    lv_obj_align(forget, LV_ALIGN_RIGHT_MID, -13, 0);
    lv_obj_set_style_radius(forget, 17, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(forget, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(forget, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(forget, lv_color_hex(0x5D5D5A),
                                  LV_PART_MAIN);
    lv_obj_set_style_shadow_width(forget, 0, LV_PART_MAIN);
    lv_obj_t *forget_label = label_create(forget, "Forget",
                                          &lv_font_montserrat_12, COLOR_PAPER);
    lv_obj_center(forget_label);
    lv_obj_add_event_cb(forget, forget_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *list_title = label_create(s_ui.wifi_screen,
                                        "Available networks",
                                        &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_align(list_title, LV_ALIGN_TOP_LEFT, 57, 174);
    s_ui.wifi_scan_status = label_create(s_ui.wifi_screen, "Scanning...",
                                         &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_align(s_ui.wifi_scan_status, LV_ALIGN_TOP_RIGHT, -57, 174);
    s_ui.wifi_list = lv_obj_create(s_ui.wifi_screen);
    lv_obj_set_size(s_ui.wifi_list, 368, 235);
    lv_obj_align(s_ui.wifi_list, LV_ALIGN_TOP_MID, 0, 197);
    lv_obj_set_flex_flow(s_ui.wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_ui.wifi_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_ui.wifi_list, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ui.wifi_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_ui.wifi_list, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(s_ui.wifi_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_t *usb_note = label_create(
        s_ui.wifi_screen, "USB OTA remains available while offline",
        &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_align(usb_note, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void password_screen_create(void)
{
    s_ui.password_screen = lv_obj_create(NULL);
    screen_prepare(s_ui.password_screen);
    header_create(s_ui.password_screen, "JOIN NETWORK", "", true);
    s_ui.password_title = label_create(s_ui.password_screen, "Join network",
                                       &lv_font_montserrat_22, COLOR_TEXT);
    lv_obj_align(s_ui.password_title, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_t *field_label = label_create(s_ui.password_screen, "Password",
                                         &lv_font_montserrat_12, COLOR_TEXT);
    lv_obj_align(field_label, LV_ALIGN_TOP_LEFT, 42, 90);
    s_ui.password_input = lv_textarea_create(s_ui.password_screen);
    lv_obj_set_size(s_ui.password_input, 396, 52);
    lv_obj_align(s_ui.password_input, LV_ALIGN_TOP_MID, 0, 109);
    lv_textarea_set_one_line(s_ui.password_input, true);
    lv_textarea_set_password_mode(s_ui.password_input, true);
    lv_textarea_set_max_length(s_ui.password_input, 64);
    lv_obj_set_style_radius(s_ui.password_input, 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_ui.password_input, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_ui.password_input, COLOR_LINE,
                                  LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.password_input, COLOR_SURFACE,
                              LV_PART_MAIN);
    lv_obj_set_style_text_font(s_ui.password_input, &lv_font_montserrat_14,
                               LV_PART_MAIN);
    s_ui.password_toggle = button_create(s_ui.password_screen, "Show", 70, 34,
                                         false);
    lv_obj_align(s_ui.password_toggle, LV_ALIGN_TOP_RIGHT, -50, 118);
    lv_obj_add_event_cb(s_ui.password_toggle, password_toggle_event,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_t *help = label_create(
        s_ui.password_screen,
        "8-63 characters or 64 hex - recovery only",
        &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_align(help, LV_ALIGN_TOP_LEFT, 42, 168);

    lv_obj_t *keyboard = lv_keyboard_create(s_ui.password_screen);
    lv_obj_set_size(keyboard, 480, 275);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(keyboard, s_ui.password_input);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xE3E4E2), LV_PART_MAIN);
    lv_obj_set_style_border_width(keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(keyboard, COLOR_SURFACE, LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_12,
                               LV_PART_ITEMS);
    lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_ALL, NULL);
}

static void update_screen_create(void)
{
    s_ui.update_screen = lv_obj_create(NULL);
    screen_prepare(s_ui.update_screen);
    header_create(s_ui.update_screen, "", "", false);
    lv_obj_t *badge = box_create(s_ui.update_screen, 132, 30,
                                 COLOR_ORANGE, 15);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -40, 28);
    lv_obj_t *badge_label = label_create(badge, "SYSTEM UPDATE",
                                         &lv_font_montserrat_12, COLOR_PAPER);
    lv_obj_center(badge_label);
    lv_obj_t *icon = box_create(s_ui.update_screen, 74, 74, COLOR_TEXT, 22);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 89);
    lv_obj_t *arrow = label_create(icon, LV_SYMBOL_DOWNLOAD,
                                   &lv_font_montserrat_32, COLOR_ORANGE);
    lv_obj_center(arrow);
    s_ui.update_title = label_create(s_ui.update_screen, "Updating system",
                                     &lv_font_montserrat_32, COLOR_TEXT);
    lv_obj_align(s_ui.update_title, LV_ALIGN_TOP_MID, 0, 178);
    s_ui.update_detail = label_create(s_ui.update_screen, "Preparing update",
                                      &lv_font_montserrat_14, COLOR_MUTED);
    lv_obj_align(s_ui.update_detail, LV_ALIGN_TOP_MID, 0, 220);
    s_ui.update_percent = label_create(s_ui.update_screen, "0%",
                                       &lv_font_montserrat_22, COLOR_TEXT);
    lv_obj_align(s_ui.update_percent, LV_ALIGN_TOP_LEFT, 56, 264);
    s_ui.update_bar = lv_bar_create(s_ui.update_screen);
    lv_obj_set_size(s_ui.update_bar, 368, 9);
    lv_obj_align(s_ui.update_bar, LV_ALIGN_TOP_MID, 0, 298);
    lv_bar_set_range(s_ui.update_bar, 0, 1000);
    lv_bar_set_value(s_ui.update_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ui.update_bar, COLOR_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.update_bar, COLOR_ORANGE,
                              LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_ui.update_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ui.update_bar, 5, LV_PART_INDICATOR);
    lv_obj_t *owner_label = label_create(s_ui.update_screen, "Session owner",
                                         &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_align(owner_label, LV_ALIGN_TOP_LEFT, 56, 329);
    s_ui.update_owner = label_create(s_ui.update_screen, "Negotiating",
                                     &lv_font_montserrat_12, COLOR_TEXT);
    lv_obj_align(s_ui.update_owner, LV_ALIGN_TOP_RIGHT, -56, 329);
    lv_obj_t *verified_label = label_create(s_ui.update_screen, "Manifest",
                                            &lv_font_montserrat_12,
                                            COLOR_MUTED);
    lv_obj_align(verified_label, LV_ALIGN_TOP_LEFT, 56, 365);
    lv_obj_t *verified = label_create(s_ui.update_screen, "Verified",
                                      &lv_font_montserrat_12, COLOR_GREEN);
    lv_obj_align(verified, LV_ALIGN_TOP_RIGHT, -56, 365);
    lv_obj_t *warning = label_create(
        s_ui.update_screen,
        "Keep power connected. The device restarts automatically.",
        &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_align(warning, LV_ALIGN_BOTTOM_MID, 0, -42);
}

static void result_screen_create(void)
{
    s_ui.result_screen = lv_obj_create(NULL);
    screen_prepare(s_ui.result_screen);
    lv_obj_t *badge = box_create(s_ui.result_screen, 104, 30,
                                 COLOR_TEXT, 15);
    lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_t *badge_label = label_create(badge, "RECOVERY",
                                         &lv_font_montserrat_12, COLOR_PAPER);
    lv_obj_center(badge_label);
    s_ui.result_mark = box_create(s_ui.result_screen, 74, 74,
                                  COLOR_GREEN, LV_RADIUS_CIRCLE);
    lv_obj_align(s_ui.result_mark, LV_ALIGN_TOP_MID, 0, 86);
    lv_obj_t *check = label_create(s_ui.result_mark, LV_SYMBOL_OK,
                                   &lv_font_montserrat_32, COLOR_PAPER);
    lv_obj_center(check);
    s_ui.result_title = label_create(s_ui.result_screen, "Update complete",
                                     &lv_font_montserrat_32, COLOR_TEXT);
    lv_obj_align(s_ui.result_title, LV_ALIGN_TOP_MID, 0, 181);
    s_ui.result_detail = label_create(
        s_ui.result_screen, "System images verified and committed",
        &lv_font_montserrat_14, COLOR_MUTED);
    lv_obj_align(s_ui.result_detail, LV_ALIGN_TOP_MID, 0, 225);
    lv_obj_t *restart = box_create(s_ui.result_screen, 368, 48,
                                   COLOR_SURFACE, 24);
    lv_obj_set_style_border_width(restart, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(restart, COLOR_LINE, LV_PART_MAIN);
    lv_obj_align(restart, LV_ALIGN_TOP_MID, 0, 302);
    lv_obj_t *restart_label = label_create(
        restart, "Restarting into the application...",
        &lv_font_montserrat_14, COLOR_TEXT);
    lv_obj_center(restart_label);
    lv_obj_t *footer = label_create(s_ui.result_screen,
                                    "ESP-Iris recovery verification",
                                    &lv_font_montserrat_12, COLOR_MUTED);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -28);
}

static void show_page(factory_page_t page)
{
    s_ui.page = page;
    lv_obj_t *screen = s_ui.ready_screen;
    if (page == FACTORY_PAGE_WIFI) {
        screen = s_ui.wifi_screen;
        (void)factory_network_request_scan();
    } else if (page == FACTORY_PAGE_PAIRING) {
        screen = s_ui.pairing_screen;
        pairing_screen_update();
    } else if (page == FACTORY_PAGE_PASSWORD) {
        screen = s_ui.password_screen;
    } else if (page == FACTORY_PAGE_UPDATE) {
        screen = s_ui.update_screen;
    } else if (page == FACTORY_PAGE_RESULT) {
        screen = s_ui.result_screen;
    }
    lv_screen_load(screen);
}

static void wifi_list_rebuild(const factory_network_snapshot_t *network)
{
    lv_obj_clean(s_ui.wifi_list);
    for (size_t i = 0; i < network->ap_count; ++i) {
        strlcpy(s_ui.ap_ssids[i], network->aps[i].ssid,
                sizeof(s_ui.ap_ssids[i]));
        lv_obj_t *row = lv_button_create(s_ui.wifi_list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 56);
        lv_obj_set_flex_grow(row, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM,
                                     LV_PART_MAIN);
        lv_obj_set_style_border_color(row, COLOR_LINE, LV_PART_MAIN);
        lv_obj_t *name = label_create(row, network->aps[i].ssid,
                                      &lv_font_montserrat_14, COLOR_TEXT);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 8, -8);
        const char *security = network->aps[i].authmode == WIFI_AUTH_OPEN
            ? "Open" : "Secured";
        lv_obj_t *detail = label_create(row, "", &lv_font_montserrat_12,
                                        COLOR_MUTED);
        lv_label_set_text_fmt(detail, "%s - %d dBm", security,
                              network->aps[i].rssi);
        lv_obj_align(detail, LV_ALIGN_LEFT_MID, 8, 10);
        lv_obj_t *arrow = label_create(row, LV_SYMBOL_RIGHT,
                                       &lv_font_montserrat_14, COLOR_MUTED);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_add_event_cb(row, network_select_event, LV_EVENT_CLICKED,
                            s_ui.ap_ssids[i]);
    }
    lv_label_set_text_fmt(s_ui.wifi_scan_status, "%u found",
                          (unsigned)network->ap_count);
}

static void link_status_update(const factory_network_snapshot_t *network,
                               const esp_iris_status_t *iris)
{
    const bool session = iris->session_ready;
    const bool usb_owner = session &&
        iris->transport == ESP_IRIS_TRANSPORT_KIND_USB;
    const bool tcp_owner = session &&
        iris->transport == ESP_IRIS_TRANSPORT_KIND_TCP;
    lv_label_set_text(s_ui.usb_value,
                      usb_owner ? "Active" : session ? "Paused" : "Available");
    lv_obj_set_style_bg_color(s_ui.usb_dot,
                              usb_owner ? COLOR_ORANGE : COLOR_GREEN,
                              LV_PART_MAIN);

    if (network->state == FACTORY_NETWORK_CONNECTED) {
        lv_label_set_text(s_ui.network_value,
                          tcp_owner ? "Active" : session ? "Paused" : "Available");
        lv_obj_set_style_bg_color(s_ui.network_dot,
                                  tcp_owner ? COLOR_ORANGE : COLOR_GREEN,
                                  LV_PART_MAIN);
        lv_label_set_text_fmt(s_ui.address, "%s.local - %s:%u",
                              network->hostname, network->ip,
                              CONFIG_ESP_IRIS_TCP_PORT);
    } else if (network->state == FACTORY_NETWORK_CONNECTING) {
        lv_label_set_text(s_ui.network_value, "Connecting");
        lv_obj_set_style_bg_color(s_ui.network_dot, COLOR_ORANGE,
                                  LV_PART_MAIN);
        lv_label_set_text(s_ui.address, "Connecting to saved Wi-Fi");
    } else {
        lv_label_set_text(s_ui.network_value, "Offline");
        lv_obj_set_style_bg_color(s_ui.network_dot, COLOR_MUTED,
                                  LV_PART_MAIN);
        lv_label_set_text(s_ui.address, "Network setup not completed");
    }
}

static void network_ui_update(const factory_network_snapshot_t *network)
{
    if (network->state == FACTORY_NETWORK_CONNECTED) {
        lv_label_set_text(s_ui.wifi_connection, network->ssid);
        lv_label_set_text_fmt(s_ui.wifi_connection_detail, "%s - %s.local",
                              network->ip, network->hostname);
    } else if (network->state == FACTORY_NETWORK_CONNECTING) {
        lv_label_set_text(s_ui.wifi_connection, network->ssid);
        lv_label_set_text(s_ui.wifi_connection_detail, "Connecting...");
    } else if (network->state == FACTORY_NETWORK_FAILED) {
        lv_label_set_text(s_ui.wifi_connection, "Connection failed");
        lv_label_set_text(s_ui.wifi_connection_detail,
                          "Choose the network and retry");
    } else {
        lv_label_set_text(s_ui.wifi_connection, "Not connected");
        lv_label_set_text(s_ui.wifi_connection_detail,
                          "Choose a network below");
    }
    if (network->scanning) {
        lv_label_set_text(s_ui.wifi_scan_status, "Scanning...");
    }
    if (network->scan_generation != s_ui.scan_generation) {
        s_ui.scan_generation = network->scan_generation;
        wifi_list_rebuild(network);
    }
}

static const char *transport_name(esp_iris_transport_kind_t transport)
{
    if (transport == ESP_IRIS_TRANSPORT_KIND_USB) {
        return "USB";
    }
    if (transport == ESP_IRIS_TRANSPORT_KIND_TCP) {
        return "TCP";
    }
    return "Negotiating";
}

static void system_update_ui_update(const esp_iris_status_t *iris)
{
    esp_iris_system_update_status_t update;
    if (esp_iris_system_update_get_status(&update) != ESP_OK) {
        return;
    }
    if (update.phase == ESP_IRIS_SYSTEM_UPDATE_PHASE_IDLE) {
        return;
    }
    if (update.phase == ESP_IRIS_SYSTEM_UPDATE_PHASE_COMMITTED) {
        lv_obj_set_style_bg_color(s_ui.result_mark, COLOR_GREEN, LV_PART_MAIN);
        lv_label_set_text(s_ui.result_title, "Update complete");
        lv_label_set_text(s_ui.result_detail,
                          "System images verified and committed");
        if (s_ui.page != FACTORY_PAGE_RESULT) {
            show_page(FACTORY_PAGE_RESULT);
        }
        s_ui.update_phase = update.phase;
        return;
    }
    if (update.phase == ESP_IRIS_SYSTEM_UPDATE_PHASE_FAILED ||
        update.phase == ESP_IRIS_SYSTEM_UPDATE_PHASE_CANCELLED) {
        lv_obj_set_style_bg_color(s_ui.result_mark, COLOR_RED, LV_PART_MAIN);
        lv_label_set_text(s_ui.result_title, "Update failed");
        lv_label_set_text_fmt(s_ui.result_detail, "Error 0x%08x - use USB to retry",
                              (unsigned)update.result);
        if (s_ui.page != FACTORY_PAGE_RESULT) {
            show_page(FACTORY_PAGE_RESULT);
        }
        s_ui.update_phase = update.phase;
        return;
    }

    const uint16_t progress = update.component_size > 0
        ? (uint16_t)(((uint64_t)update.component_received * 1000U) /
                     update.component_size)
        : 0;
    const char *detail = "Validating signed update";
    if (update.phase == ESP_IRIS_SYSTEM_UPDATE_PHASE_RECEIVING) {
        detail = "Receiving system component";
    } else if (update.phase ==
               ESP_IRIS_SYSTEM_UPDATE_PHASE_COMPONENT_VERIFIED) {
        detail = "Component verified";
    } else if (update.phase == ESP_IRIS_SYSTEM_UPDATE_PHASE_COMMITTING) {
        detail = "Committing protected system images";
    }
    lv_label_set_text(s_ui.update_detail, detail);
    lv_label_set_text_fmt(s_ui.update_percent, "%u%%", progress / 10U);
    lv_bar_set_value(s_ui.update_bar, progress, LV_ANIM_OFF);
    lv_label_set_text(s_ui.update_owner, transport_name(iris->transport));
    if (s_ui.page != FACTORY_PAGE_UPDATE) {
        show_page(FACTORY_PAGE_UPDATE);
    }
    s_ui.update_phase = update.phase;
}

static void ui_status_task(void *arg)
{
    (void)arg;
    while (true) {
        factory_network_snapshot_t network = {0};
        esp_iris_status_t iris = {0};
        const bool have_network =
            factory_network_get_snapshot(&network) == ESP_OK;
        const bool have_iris = esp_iris_get_status(&iris) == ESP_OK;
        if (have_iris && bsp_display_lock(100)) {
            link_status_update(&network, &iris);
            if (have_network) {
                network_ui_update(&network);
            }
            system_update_ui_update(&iris);
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

#endif

esp_err_t factory_ui_start(void)
{
    lv_display_t *display = bsp_display_start();
    ESP_RETURN_ON_FALSE(display, ESP_FAIL, TAG, "start display");
    ESP_RETURN_ON_FALSE(bsp_display_lock(-1), ESP_FAIL, TAG, "lock display");

#if CONFIG_GET_STARTED_RECOVERY
    s_ui.display = display;
    ready_screen_create();
    wifi_screen_create();
    pairing_screen_create();
    password_screen_create();
    update_screen_create();
    result_screen_create();
    show_page(FACTORY_PAGE_READY);
#else
    lv_obj_t *screen = lv_display_get_screen_active(display);
    screen_prepare(screen);
    lv_obj_t *title = label_create(screen, "ESP-MOSAICO",
                                   &lv_font_montserrat_48, COLOR_TEXT);
    lv_obj_center(title);
#endif
    bsp_display_unlock();

#if CONFIG_GET_STARTED_RECOVERY
    ESP_RETURN_ON_FALSE(xTaskCreate(ui_status_task, "factory_ui", 4096, NULL,
                                    4, NULL) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "start UI status task");
#endif
    ESP_LOGI(TAG, "factory UI started at %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    return ESP_OK;
}
