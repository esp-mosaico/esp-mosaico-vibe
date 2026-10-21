// SPDX-License-Identifier: Apache-2.0

#include "gsp_sim_bridge.h"
#include "game_ui.h"

#include "gsp/gsp_component.h"

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    return game_ui_init(ui);
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    game_ui_deinit(ui);
}

/*
 * ESP-GSP 1.5.1 exposes the synchronized position helper in the device
 * runtime. sim_bridge currently exposes the generic property animation API,
 * so provide only the missing convenience wrapper on Host.
 */
esp_gsp_err_t esp_gsp_component_animate_position_to(
    esp_gsp_handle_t gsp, gsp_component_key_t component,
    int32_t x, int32_t y, uint32_t duration_ms, esp_gsp_ease_t ease)
{
    gsp_value_t value = {.type = GSP_VALUE_I32};
    value.data.i32 = x;
    esp_gsp_err_t err = esp_gsp_component_animate_property_to(
        gsp, component, GSP_PROPERTY_KEY_X, &value, duration_ms, ease);
    if (err != ESP_GSP_OK) {
        return err;
    }
    value.data.i32 = y;
    return esp_gsp_component_animate_property_to(
        gsp, component, GSP_PROPERTY_KEY_Y, &value, duration_ms, ease);
}
