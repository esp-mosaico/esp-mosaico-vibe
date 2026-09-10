// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>

#include "gsp_sim_bridge.h"
#include "hello_ui.h"

static hello_ui_t state;

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    return hello_ui_init(ui, &state);
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    hello_ui_deinit(ui, &state);
    fprintf(stderr, "hello_backend: load=%d, last_error=%d\n",
            (int)state.load, (int)state.last_error);
}
