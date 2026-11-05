// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platform_esp_audio_service platform_esp_audio_service_t;

typedef raylib_lite_result_t (*platform_esp_audio_pull_fn)(
    void *context, int16_t *out_frames, size_t frame_count);
typedef bool (*platform_audio_should_stop_fn)(void *context);

typedef struct {
    uint32_t write_errors;
    uint32_t underruns;
} platform_esp_audio_stats_t;

/* Pure short-write helper, also used by Host tests. max_no_progress bounds
 * repeated OK+0/BUSY/TIMEOUT results; the caller owns any delay policy. */
raylib_lite_result_t platform_audio_write_all(
    const raylib_lite_audio_backend_t *backend, const int16_t *frames,
    size_t frame_count, uint32_t timeout_ms, uint32_t max_no_progress,
    platform_audio_should_stop_fn should_stop, void *stop_context,
    size_t *out_written);

/* Exactly one selected native Board supplies the PCM codec backend.
 * It owns I2S/codec lifecycle and accepts the RLE 24 kHz mono S16 contract. */
raylib_lite_audio_backend_t platform_esp_audio_board_backend(void);

/* The worker's READY and DONE bits can race in either order. No signal at
 * the bounded deadline is TIMEOUT, never an implicit successful startup. */
static inline raylib_lite_result_t platform_audio_start_status(bool ready, bool done)
{
    if (done) return RAYLIB_LITE_NOT_READY;
    return ready ? RAYLIB_LITE_OK : RAYLIB_LITE_TIMEOUT;
}

/* Idempotent Native Game shutdown barrier. TIMEOUT keeps the Worker, Mixer,
 * codec and asset leases alive; callers must retry before Board teardown. */
raylib_lite_result_t raylib_lite_game_audio_shutdown(uint32_t timeout_ms);

raylib_lite_result_t platform_esp_audio_service_create(
    platform_esp_audio_service_t **out_service);
raylib_lite_result_t platform_esp_audio_service_start(
    platform_esp_audio_service_t *service,
    platform_esp_audio_pull_fn pull, void *pull_context);
/* A codec write already executing in esp_codec_dev cannot be cancelled by its
 * current API. Stop first joins the Worker, then releases the backend on the
 * calling task. Either join timeout or backend stop failure retains the
 * service and codec state for retry; only OK permits destroy/Board teardown. */
raylib_lite_result_t platform_esp_audio_service_stop(
    platform_esp_audio_service_t *service, uint32_t timeout_ms);
raylib_lite_result_t platform_esp_audio_service_destroy(
    platform_esp_audio_service_t *service, uint32_t timeout_ms);
bool platform_esp_audio_service_ready(platform_esp_audio_service_t *service);
void platform_esp_audio_service_get_stats(
    platform_esp_audio_service_t *service, platform_esp_audio_stats_t *out_stats);

/* These callbacks let the portable mixer serialize game-thread controls with
 * the platform worker without including FreeRTOS types in the mixer. */
void platform_esp_audio_service_lock(void *service);
void platform_esp_audio_service_unlock(void *service);

#ifdef __cplusplus
}
#endif
