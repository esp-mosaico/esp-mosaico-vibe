// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Start MiniHub Host on this Mosaico app. Safe to call once after NVS. */
esp_err_t app_minihub_start(void);

/**
 * Load or create this device's MiniHub REST token in the official NVS
 * namespace. Call before minihub_auth_init(), and never log the result.
 */
esp_err_t app_minihub_prepare_local_token(char *out_token, size_t out_size);

#ifdef __cplusplus
}
#endif
