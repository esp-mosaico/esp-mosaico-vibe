// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "esp_err.h"

/** Allocate the RGB565 shadow frame and register it with ESP-Iris. */
esp_err_t iris_screen_mirror_init(void);
