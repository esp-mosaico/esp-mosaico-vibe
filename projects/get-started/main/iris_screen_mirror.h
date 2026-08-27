// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "esp_err.h"

/** Register the active LVGL screen as the ESP-Iris screenshot/mirror source. */
esp_err_t iris_screen_mirror_register(void);
