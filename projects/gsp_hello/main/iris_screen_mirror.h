// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "esp_err.h"
#include "esp_gsp.h"

/** Register the RGB565 screen backend without allocating full-frame storage. */
esp_err_t iris_screen_mirror_init(void);

/** Attach the running GSP UI used to force a coherent first mirrored frame. */
esp_err_t iris_screen_mirror_attach(esp_gsp_handle_t ui);
