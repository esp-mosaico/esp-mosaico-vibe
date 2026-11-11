// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_IMU_POINT_ID "mosaico.imu"

/**
 * Register the onboard BMI270 as a MiniHub sensor Point and start sampling.
 *
 * Call once after MiniHub core/rules are up. IMU hardware failure is logged
 * and returns an error without aborting the panel. The sampler task owns the
 * BMI270 handle for process lifetime.
 */
esp_err_t app_imu_start(void);

#ifdef __cplusplus
}
#endif
