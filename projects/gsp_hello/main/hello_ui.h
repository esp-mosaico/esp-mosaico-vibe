// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "esp_gsp.h"

typedef struct {
    int32_t load;
    void *timer;
    esp_gsp_err_t last_error;
} hello_ui_t;

esp_gsp_err_t hello_ui_init(esp_gsp_handle_t ui, hello_ui_t *state);
void hello_ui_deinit(esp_gsp_handle_t ui, hello_ui_t *state);
