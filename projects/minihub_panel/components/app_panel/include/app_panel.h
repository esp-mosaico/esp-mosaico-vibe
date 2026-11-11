// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "app_hub.h"
#include "esp_gsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct app_panel *app_panel_handle_t;

/**
 * Create the process-lifetime panel controller.
 *
 * The controller borrows both handles. GSP dispatch, the UI timer, and hub must
 * remain alive for the controller lifetime.
 */
esp_gsp_err_t app_panel_create(esp_gsp_handle_t ui, app_hub_handle_t hub,
                               app_panel_handle_t *out_handle);

#ifdef __cplusplus
}
#endif
