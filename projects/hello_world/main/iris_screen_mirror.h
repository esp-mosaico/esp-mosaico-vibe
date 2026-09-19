// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "esp_err.h"
#include "esp_gsp.h"

/** Register before GSP starts; retain its live shadow and coverage in PSRAM. */
esp_err_t iris_screen_mirror_init(void);

/** Attach the running UI. Capture waits until the first frame covers the panel. */
esp_err_t iris_screen_mirror_attach(esp_gsp_handle_t ui);
