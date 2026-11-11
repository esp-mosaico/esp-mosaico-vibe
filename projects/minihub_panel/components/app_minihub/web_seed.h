// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "esp_err.h"

/** Copy factory Web console into /data/web when index.html is missing. */
esp_err_t app_minihub_seed_web(void);
