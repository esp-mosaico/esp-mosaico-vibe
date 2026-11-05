// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>
#include "esp_display_present.h"
#include "raylib_lite_video.h"

typedef struct mosaico_video mosaico_video_t;

raylib_lite_result_t mosaico_video_open(
    esp_display_presenter_t *presenter, uint16_t width, uint16_t height,
    mosaico_video_t **out);
raylib_lite_video_backend_t mosaico_video_backend(mosaico_video_t *video);
raylib_lite_result_t mosaico_video_close(
    mosaico_video_t *video, uint32_t timeout_ms);
