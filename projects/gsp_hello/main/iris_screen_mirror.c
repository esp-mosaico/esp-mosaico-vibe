// SPDX-License-Identifier: Apache-2.0

#include "iris_screen_mirror.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bsp/esp_mosaico.h"
#include "esp_display_present.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define SCREEN_BYTES_PER_PIXEL 2U
#define SCREEN_STRIDE ((size_t)BSP_LCD_H_RES * SCREEN_BYTES_PER_PIXEL)
#define SCREEN_FRAME_BYTES (SCREEN_STRIDE * BSP_LCD_V_RES)

typedef struct {
    uint8_t *shadow;
    uint8_t *capture;
    SemaphoreHandle_t lock;
    StaticSemaphore_t lock_storage;
    bool capture_ready;
} screen_mirror_t;

static const char *TAG = "iris_screen";
static screen_mirror_t s_mirror;

static esp_err_t snapshot_frame(screen_mirror_t *mirror)
{
    if (xSemaphoreTake(mirror->lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    memcpy(mirror->capture, mirror->shadow, SCREEN_FRAME_BYTES);
    xSemaphoreGive(mirror->lock);
    mirror->capture_ready = true;
    return ESP_OK;
}

static esp_err_t screen_begin(const esp_iris_media_desc_t *requested,
                              esp_iris_media_desc_t *actual,
                              uint32_t *total_size, void *user_ctx)
{
    (void)requested;
    screen_mirror_t *mirror = user_ctx;
    if (mirror == NULL || mirror->shadow == NULL || mirror->lock == NULL ||
        actual == NULL || total_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (mirror->capture != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    mirror->capture = heap_caps_malloc(
        SCREEN_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (mirror->capture == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = snapshot_frame(mirror);
    if (err != ESP_OK) {
        heap_caps_free(mirror->capture);
        mirror->capture = NULL;
        return err;
    }

    *actual = (esp_iris_media_desc_t) {
        .x = 0,
        .y = 0,
        .width = BSP_LCD_H_RES,
        .height = BSP_LCD_V_RES,
        .stride = SCREEN_STRIDE,
        .format = ESP_IRIS_PIXEL_FORMAT_RGB565,
        .quality = 0,
    };
    *total_size = SCREEN_FRAME_BYTES;
    return ESP_OK;
}

static esp_err_t screen_read(uint32_t offset, uint8_t *out, size_t capacity,
                             size_t *out_size, void *user_ctx)
{
    screen_mirror_t *mirror = user_ctx;
    if (mirror == NULL || mirror->capture == NULL || out == NULL ||
        out_size == NULL || capacity == 0 || offset >= SCREEN_FRAME_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }

    if (offset == 0 && !mirror->capture_ready) {
        esp_err_t err = snapshot_frame(mirror);
        if (err != ESP_OK) {
            return err;
        }
    } else if (!mirror->capture_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t size = SCREEN_FRAME_BYTES - offset;
    if (size > capacity) {
        size = capacity;
    }
    memcpy(out, mirror->capture + offset, size);
    *out_size = size;
    if (offset + size == SCREEN_FRAME_BYTES) {
        mirror->capture_ready = false;
    }
    return ESP_OK;
}

static void screen_end(void *user_ctx)
{
    screen_mirror_t *mirror = user_ctx;
    if (mirror == NULL) {
        return;
    }
    heap_caps_free(mirror->capture);
    mirror->capture = NULL;
    mirror->capture_ready = false;
}

esp_err_t iris_screen_mirror_init(void)
{
    if (s_mirror.shadow != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_mirror.shadow = heap_caps_calloc(
        1, SCREEN_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_mirror.shadow == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_mirror.lock = xSemaphoreCreateMutexStatic(&s_mirror.lock_storage);
    if (s_mirror.lock == NULL) {
        heap_caps_free(s_mirror.shadow);
        s_mirror.shadow = NULL;
        return ESP_ERR_NO_MEM;
    }

    const esp_iris_screen_backend_t backend = {
        .begin = screen_begin,
        .read = screen_read,
        .end = screen_end,
        .user_ctx = &s_mirror,
    };
    esp_err_t err = esp_iris_screen_register(&backend);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_mirror.lock);
        s_mirror.lock = NULL;
        heap_caps_free(s_mirror.shadow);
        s_mirror.shadow = NULL;
        return err;
    }
    ESP_LOGI(TAG, "Registered %dx%d RGB565 GSP screen backend (%u bytes PSRAM)",
             BSP_LCD_H_RES, BSP_LCD_V_RES, (unsigned)SCREEN_FRAME_BYTES);
    return ESP_OK;
}

/* esp_display_present swaps RGB565 bytes in-place for this panel.  The linker
 * wrapper observes each GSP partition before that transport conversion, so
 * ESP-Iris always receives the documented little-endian RGB565 surface. */
esp_err_t __real_esp_display_presenter_submit_buffer(
    esp_display_presenter_t *presenter,
    const esp_display_presenter_buffer_t *buffer,
    const esp_display_present_area_t *area,
    size_t stride_bytes);

esp_err_t __wrap_esp_display_presenter_submit_buffer(
    esp_display_presenter_t *presenter,
    const esp_display_presenter_buffer_t *buffer,
    const esp_display_present_area_t *area,
    size_t stride_bytes)
{
    if (s_mirror.shadow != NULL && s_mirror.lock != NULL && buffer != NULL &&
        buffer->surface.pixels != NULL && area != NULL && area->x1 >= 0 &&
        area->y1 >= 0 && area->x2 >= area->x1 && area->y2 >= area->y1 &&
        area->x2 < BSP_LCD_H_RES && area->y2 < BSP_LCD_V_RES) {
        const size_t width = (size_t)(area->x2 - area->x1 + 1);
        const size_t height = (size_t)(area->y2 - area->y1 + 1);
        const size_t row_bytes = width * SCREEN_BYTES_PER_PIXEL;
        if (buffer->surface.pixel_format ==
                ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565 &&
            stride_bytes >= row_bytes &&
            height <= buffer->capacity_bytes / stride_bytes &&
            xSemaphoreTake(s_mirror.lock, portMAX_DELAY) == pdTRUE) {
            const uint8_t *source = buffer->surface.pixels;
            uint8_t *destination = s_mirror.shadow +
                (size_t)area->y1 * SCREEN_STRIDE +
                (size_t)area->x1 * SCREEN_BYTES_PER_PIXEL;
            for (size_t row = 0; row < height; ++row) {
                memcpy(destination + row * SCREEN_STRIDE,
                       source + row * stride_bytes, row_bytes);
            }
            xSemaphoreGive(s_mirror.lock);
        }
    }
    return __real_esp_display_presenter_submit_buffer(
        presenter, buffer, area, stride_bytes);
}
