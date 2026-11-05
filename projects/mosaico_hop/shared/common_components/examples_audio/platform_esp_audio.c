// SPDX-License-Identifier: Apache-2.0
#include "platform_esp_audio.h"

#include <stdatomic.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define AUDIO_CHUNK_FRAMES 240U
#define AUDIO_DONE_BIT BIT0
#define AUDIO_READY_BIT BIT1
#define AUDIO_START_WAIT_MS 3000U
#define AUDIO_WRITE_TIMEOUT_MS 20U

struct platform_esp_audio_service {
    SemaphoreHandle_t mutex;
    EventGroupHandle_t events;
    TaskHandle_t task;
    platform_esp_audio_pull_fn pull;
    void *pull_context;
    atomic_bool stopping;
    bool ready;
    /* The Worker can exit while a partially initialized backend still needs
     * cleanup. Only a successful backend.stop() clears this obligation. */
    bool backend_cleanup_pending;
    platform_esp_audio_stats_t stats;
    raylib_lite_audio_backend_t backend;
};

static bool service_should_stop(void *context)
{
    platform_esp_audio_service_t *service = context;
    return atomic_load_explicit(&service->stopping, memory_order_acquire);
}

static void audio_worker(void *argument)
{
    platform_esp_audio_service_t *service = argument;
    raylib_lite_audio_format_t format = RAYLIB_LITE_AUDIO_FORMAT_DEFAULT();
    if (service->backend.start(service->backend.context, &format) != RAYLIB_LITE_OK) {
        xSemaphoreTake(service->mutex, portMAX_DELAY);
        service->ready = false;
        xSemaphoreGive(service->mutex);
        xEventGroupSetBits(service->events, AUDIO_DONE_BIT);
        vTaskSuspend(NULL);
        return; /* Unreachable: stop owns deletion after observing DONE. */
    }
    xSemaphoreTake(service->mutex, portMAX_DELAY);
    service->ready = true;
    xSemaphoreGive(service->mutex);
    xEventGroupSetBits(service->events, AUDIO_READY_BIT);
    int16_t output[AUDIO_CHUNK_FRAMES];
    while (!atomic_load_explicit(&service->stopping, memory_order_acquire)) {
        if (service->pull(service->pull_context, output, AUDIO_CHUNK_FRAMES) !=
            RAYLIB_LITE_OK) break;
        size_t written = 0;
        raylib_lite_result_t result = platform_audio_write_all(
            &service->backend, output, AUDIO_CHUNK_FRAMES,
            AUDIO_WRITE_TIMEOUT_MS, 2, service_should_stop, service, &written);
        if (result == RAYLIB_LITE_BUSY || result == RAYLIB_LITE_TIMEOUT) {
            xSemaphoreTake(service->mutex, portMAX_DELAY);
            ++service->stats.underruns;
            xSemaphoreGive(service->mutex);
            (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
            continue;
        }
        if (result != RAYLIB_LITE_OK) {
            if (!atomic_load_explicit(&service->stopping, memory_order_acquire)) {
                xSemaphoreTake(service->mutex, portMAX_DELAY);
                ++service->stats.write_errors;
                xSemaphoreGive(service->mutex);
            }
            break;
        }
    }
    xSemaphoreTake(service->mutex, portMAX_DELAY);
    service->ready = false;
    xSemaphoreGive(service->mutex);
    /* DONE means no more PCM access, not that codec resources were released.
     * The joining caller owns backend.stop() and can retry a failure. */
    xEventGroupSetBits(service->events, AUDIO_DONE_BIT);
    vTaskSuspend(NULL);
}

raylib_lite_result_t platform_esp_audio_service_create(
    platform_esp_audio_service_t **out_service)
{
    if (!out_service) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out_service = NULL;
    platform_esp_audio_service_t *service = calloc(1, sizeof(*service));
    if (!service) return RAYLIB_LITE_NO_MEMORY;
    service->mutex = xSemaphoreCreateMutex();
    service->events = xEventGroupCreate();
    if (!service->mutex || !service->events) {
        if (service->events) vEventGroupDelete(service->events);
        if (service->mutex) vSemaphoreDelete(service->mutex);
        free(service);
        return RAYLIB_LITE_NO_MEMORY;
    }
    /* The selected Board owns its codec/I2S setup. The worker and mixer are
     * common to all native example Boards. */
    service->backend = platform_esp_audio_board_backend();
    if (!service->backend.start || !service->backend.write || !service->backend.stop) {
        vEventGroupDelete(service->events);
        vSemaphoreDelete(service->mutex);
        free(service);
        return RAYLIB_LITE_NOT_SUPPORTED;
    }
    atomic_init(&service->stopping, false);
    *out_service = service;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_start(
    platform_esp_audio_service_t *service,
    platform_esp_audio_pull_fn pull, void *pull_context)
{
    if (!service || !pull) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (service->task || service->backend_cleanup_pending)
        return RAYLIB_LITE_INVALID_STATE;
    service->pull = pull;
    service->pull_context = pull_context;
    atomic_store_explicit(&service->stopping, false, memory_order_release);
    service->ready = false;
    xEventGroupClearBits(service->events, AUDIO_DONE_BIT | AUDIO_READY_BIT);
    /* Set before scheduling: backend.start() may already be running by the
     * time xTaskCreate returns, and even a failed start needs a stop attempt. */
    service->backend_cleanup_pending = true;
    if (xTaskCreate(audio_worker, "game_audio", 4096, service, 6,
                    &service->task) != pdPASS) {
        service->task = NULL;
        service->backend_cleanup_pending = false;
        return RAYLIB_LITE_NO_MEMORY;
    }
    /* Native Games commonly query IsAudioDeviceReady() immediately after
     * InitAudioDevice(). Wait until the codec has opened before returning;
     * otherwise such Games silently skip loading their music/SFX. A slow
     * backend is kept alive on timeout, so caller cleanup cannot free a
     * still-starting worker. */
    EventBits_t bits = xEventGroupWaitBits(service->events,
        AUDIO_READY_BIT | AUDIO_DONE_BIT, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(AUDIO_START_WAIT_MS));
    return platform_audio_start_status((bits & AUDIO_READY_BIT) != 0,
                                       (bits & AUDIO_DONE_BIT) != 0);
}

raylib_lite_result_t platform_esp_audio_service_stop(
    platform_esp_audio_service_t *service, uint32_t timeout_ms)
{
    if (!service) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (timeout_ms == RAYLIB_LITE_WAIT_FOREVER)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    if (service->task) {
        atomic_store_explicit(&service->stopping, true, memory_order_release);
        if (!(xEventGroupGetBits(service->events) & AUDIO_DONE_BIT))
            xTaskNotifyGive(service->task);

        TickType_t ticks = timeout_ms ? pdMS_TO_TICKS(timeout_ms) : 0;
        if (timeout_ms && ticks == 0) ticks = 1;
        EventBits_t bits = xEventGroupWaitBits(
            service->events, AUDIO_DONE_BIT, pdFALSE, pdTRUE, ticks);
        if (!(bits & AUDIO_DONE_BIT)) return RAYLIB_LITE_TIMEOUT;
        vTaskDelete(service->task);
        service->task = NULL;
    }

    /* No Worker can now touch the codec. A failed backend stop keeps the
     * service and backend handles for a later retry; DONE alone is not OK. */
    if (service->backend_cleanup_pending) {
        raylib_lite_result_t result =
            service->backend.stop(service->backend.context, timeout_ms);
        if (result != RAYLIB_LITE_OK) return result;
        service->backend_cleanup_pending = false;
    }
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_destroy(
    platform_esp_audio_service_t *service, uint32_t timeout_ms)
{
    if (!service) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_result_t result = platform_esp_audio_service_stop(
        service, timeout_ms);
    if (result != RAYLIB_LITE_OK) return result;
    vEventGroupDelete(service->events);
    vSemaphoreDelete(service->mutex);
    free(service);
    return RAYLIB_LITE_OK;
}

bool platform_esp_audio_service_ready(platform_esp_audio_service_t *service)
{
    if (!service) return false;
    xSemaphoreTake(service->mutex, portMAX_DELAY);
    bool ready = service->ready &&
        !atomic_load_explicit(&service->stopping, memory_order_acquire);
    xSemaphoreGive(service->mutex);
    return ready;
}

void platform_esp_audio_service_get_stats(
    platform_esp_audio_service_t *service, platform_esp_audio_stats_t *out_stats)
{
    if (!service || !out_stats) return;
    xSemaphoreTake(service->mutex, portMAX_DELAY);
    *out_stats = service->stats;
    xSemaphoreGive(service->mutex);
}

void platform_esp_audio_service_lock(void *opaque)
{
    platform_esp_audio_service_t *service = opaque;
    if (service) xSemaphoreTake(service->mutex, portMAX_DELAY);
}

void platform_esp_audio_service_unlock(void *opaque)
{
    platform_esp_audio_service_t *service = opaque;
    if (service) xSemaphoreGive(service->mutex);
}
