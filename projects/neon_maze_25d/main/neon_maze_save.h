// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t neon_maze_save_load(uint32_t *best_ticks);
esp_err_t neon_maze_save_best(uint32_t best_ticks);
esp_err_t neon_maze_save_flush(void);

#ifdef __cplusplus
}
#endif
