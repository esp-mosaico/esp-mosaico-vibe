// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "raylib_lite_input.h"
#include "raylib_lite_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct raylib_lite_example_board raylib_lite_example_board_t;

typedef struct {
    uint8_t touch_points;
    bool enable_imu;
    uint16_t imu_sample_ms;
    uint16_t drawbuf_lines;
    uint8_t drawbuf_count;
    /* Game-owned logical surface; Boards may scale it to the physical LCD. */
    uint16_t logical_width;
    uint16_t logical_height;
} raylib_lite_example_board_config_t;

esp_err_t raylib_lite_example_board_create(
    const raylib_lite_example_board_config_t *config,
    raylib_lite_example_board_t **out_board);
const raylib_lite_platform_t *raylib_lite_example_board_services(
    raylib_lite_example_board_t *board);
raylib_lite_input_queue_t *raylib_lite_example_board_input(
    raylib_lite_example_board_t *board);
esp_err_t raylib_lite_example_board_start_input(
    raylib_lite_example_board_t *board);
esp_err_t raylib_lite_example_board_stop(
    raylib_lite_example_board_t *board, uint32_t timeout_ms);
esp_err_t raylib_lite_example_board_destroy(
    raylib_lite_example_board_t *board, uint32_t timeout_ms);
esp_err_t raylib_lite_example_board_retry_cleanup(
    raylib_lite_example_board_t *board, uint32_t timeout_ms);

/* Optional singleton haptic capability for native example glue. Strength is 0..100; zero stops the motor. */
esp_err_t raylib_lite_example_board_haptic_init(void);
esp_err_t raylib_lite_example_board_haptic_set(uint8_t strength);

#ifdef __cplusplus
}
#endif
