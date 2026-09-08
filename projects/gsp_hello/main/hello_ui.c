// SPDX-License-Identifier: Apache-2.0

#include "hello_ui.h"

#include "bundle_gsp.h"

static void feed_load(esp_gsp_handle_t ui, void *ctx)
{
    hello_ui_t *state = ctx;
    state->load = (state->load + 5) % 101;
    state->last_error = gsp_hello_load_set_value(ui, state->load);
}

esp_gsp_err_t hello_ui_init(esp_gsp_handle_t ui, hello_ui_t *state)
{
    if (!ui || !state) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    state->load = 0;
    state->timer = NULL;
    state->last_error = gsp_hello_load_set_value(ui, 0);
    if (state->last_error != ESP_GSP_OK) {
        return state->last_error;
    }
    state->timer = esp_gsp_timer_create(ui, 250, feed_load, state);
    return state->timer ? ESP_GSP_OK : ESP_GSP_ERR_NO_MEM;
}

void hello_ui_deinit(esp_gsp_handle_t ui, hello_ui_t *state)
{
    if (state && state->timer) {
        (void)esp_gsp_timer_delete(ui, state->timer);
        state->timer = NULL;
    }
}
