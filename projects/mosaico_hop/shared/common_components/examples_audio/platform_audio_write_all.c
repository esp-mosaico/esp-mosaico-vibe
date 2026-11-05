// SPDX-License-Identifier: Apache-2.0
#include "platform_esp_audio.h"

raylib_lite_result_t platform_audio_write_all(
    const raylib_lite_audio_backend_t *backend, const int16_t *frames,
    size_t frame_count, uint32_t timeout_ms, uint32_t max_no_progress,
    platform_audio_should_stop_fn should_stop, void *stop_context,
    size_t *out_written)
{
    if (!backend || !backend->write || (!frames && frame_count) || !out_written) {
        return RAYLIB_LITE_INVALID_ARGUMENT;
    }
    *out_written = 0;
    uint32_t no_progress = 0;
    while (*out_written < frame_count) {
        if (should_stop && should_stop(stop_context)) return RAYLIB_LITE_NOT_READY;
        size_t written = 0;
        raylib_lite_result_t result = backend->write(
            backend->context, frames + *out_written,
            frame_count - *out_written, timeout_ms, &written);
        if (written > frame_count - *out_written) return RAYLIB_LITE_PLATFORM_ERROR;
        *out_written += written;
        if (written) no_progress = 0;
        else if (++no_progress > max_no_progress) {
            return result == RAYLIB_LITE_OK ? RAYLIB_LITE_BUSY : result;
        }
        if (result != RAYLIB_LITE_OK && result != RAYLIB_LITE_BUSY &&
            result != RAYLIB_LITE_TIMEOUT) return result;
    }
    return RAYLIB_LITE_OK;
}
