// SPDX-License-Identifier: Apache-2.0

#include "app_hub.h"
#include "app_panel.h"
#include "gsp_sim_bridge.h"

#include <stdio.h>

static struct {
    app_hub_handle_t hub;
    app_panel_handle_t panel;
} backend;

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    if (app_hub_start(&backend.hub) != APP_HUB_OK) {
        return ESP_GSP_ERR_NO_MEM;
    }
    return app_panel_create(ui, backend.hub, &backend.panel);
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    (void)ui;
    /* The simulator owns process lifetime; production has the same reboot-only
     * controller lifetime until an explicit quiescent shutdown API is added. */
}

esp_gsp_err_t esp_gsp_keyboard_text(esp_gsp_handle_t gsp, char *out_text,
                                    size_t capacity)
{
    (void)gsp;
    if (!out_text || capacity == 0) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    /* sim_bridge does not expose the framework edit buffer. Open APs still
     * connect; secured APs accept any non-empty password except "wrong". */
    snprintf(out_text, capacity, "%s", "ok");
    return ESP_GSP_OK;
}
