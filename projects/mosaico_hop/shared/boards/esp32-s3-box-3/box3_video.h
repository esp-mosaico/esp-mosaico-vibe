// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "raylib_lite_video.h"

typedef struct box3_video box3_video_t;

raylib_lite_result_t box3_video_open(
    esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
    uint16_t logical_width, uint16_t logical_height, bool swap_bytes,
    box3_video_t **out);
raylib_lite_video_backend_t box3_video_backend(box3_video_t *video);
raylib_lite_result_t box3_video_close(box3_video_t *video, uint32_t timeout_ms);
/* False when the physical touch falls within a letterbox bar. */
bool box3_video_map_touch(const box3_video_t *video, int32_t *x, int32_t *y);
