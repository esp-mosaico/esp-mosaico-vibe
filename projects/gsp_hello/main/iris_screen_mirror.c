// SPDX-License-Identifier: Apache-2.0

#include "iris_screen_mirror.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bsp/esp_mosaico.h"
#include "esp_display_present.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define SCREEN_BYTES_PER_PIXEL 2U
#define SCREEN_STRIDE ((size_t)BSP_LCD_H_RES * SCREEN_BYTES_PER_PIXEL)
#define SCREEN_FRAME_BYTES (SCREEN_STRIDE * BSP_LCD_V_RES)
#define SCREEN_PIXELS ((size_t)BSP_LCD_H_RES * BSP_LCD_V_RES)
#define COVERAGE_WORD_BITS 32U
#define COVERAGE_WORDS_PER_ROW \
    (((size_t)BSP_LCD_H_RES + COVERAGE_WORD_BITS - 1U) / COVERAGE_WORD_BITS)
#define COVERAGE_WORD_COUNT (COVERAGE_WORDS_PER_ROW * BSP_LCD_V_RES)
#define COVERAGE_BYTES (COVERAGE_WORD_COUNT * sizeof(uint32_t))
#define FULL_REPAINT_TIMEOUT_MS 2000U

_Static_assert(COVERAGE_BYTES <= SCREEN_FRAME_BYTES,
               "capture frame is too small for repaint coverage tracking");

typedef struct {
    esp_gsp_handle_t ui;
    uint8_t *shadow;
    uint8_t *capture;
    size_t covered_pixels;
    SemaphoreHandle_t lock;
    StaticSemaphore_t lock_storage;
    SemaphoreHandle_t frame_ready;
    StaticSemaphore_t frame_ready_storage;
    bool capture_ready;
    bool warming;
} screen_mirror_t;

static const char *TAG = "iris_screen";
static screen_mirror_t s_mirror;

static void release_frame_storage(screen_mirror_t *mirror)
{
    uint8_t *shadow = NULL;
    uint8_t *capture = NULL;
    if (mirror->lock != NULL &&
            xSemaphoreTake(mirror->lock, portMAX_DELAY) == pdTRUE) {
        shadow = mirror->shadow;
        capture = mirror->capture;
        mirror->shadow = NULL;
        mirror->capture = NULL;
        mirror->covered_pixels = 0;
        mirror->capture_ready = false;
        mirror->warming = false;
        xSemaphoreGive(mirror->lock);
    }
    heap_caps_free(capture);
    heap_caps_free(shadow);
}

static uint32_t coverage_mask(unsigned first, unsigned last)
{
    const uint32_t lower = UINT32_MAX << first;
    const uint32_t upper = last == 31U
        ? UINT32_MAX : (UINT32_C(1) << (last + 1U)) - 1U;
    return lower & upper;
}

static bool mark_coverage(screen_mirror_t *mirror,
                          const esp_display_present_area_t *area)
{
    const size_t first_word = (size_t)area->x1 / COVERAGE_WORD_BITS;
    const size_t last_word = (size_t)area->x2 / COVERAGE_WORD_BITS;
    for (int32_t y = area->y1; y <= area->y2; ++y) {
        uint32_t *row = (uint32_t *)mirror->capture +
            (size_t)y * COVERAGE_WORDS_PER_ROW;
        for (size_t word = first_word; word <= last_word; ++word) {
            const unsigned first = word == first_word
                ? (unsigned)area->x1 % COVERAGE_WORD_BITS : 0U;
            const unsigned last = word == last_word
                ? (unsigned)area->x2 % COVERAGE_WORD_BITS
                : COVERAGE_WORD_BITS - 1U;
            const uint32_t mask = coverage_mask(first, last);
            const uint32_t added = mask & ~row[word];
            row[word] |= mask;
            mirror->covered_pixels += (size_t)__builtin_popcount(added);
        }
    }
    if (mirror->covered_pixels < SCREEN_PIXELS) {
        return false;
    }
    mirror->warming = false;
    return true;
}

static esp_err_t snapshot_frame(screen_mirror_t *mirror)
{
    if (xSemaphoreTake(mirror->lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (mirror->capture == NULL || mirror->shadow == NULL) {
        xSemaphoreGive(mirror->lock);
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(mirror->capture, mirror->shadow, SCREEN_FRAME_BYTES);
    mirror->capture_ready = true;
    xSemaphoreGive(mirror->lock);
    return ESP_OK;
}

static esp_err_t screen_begin(const esp_iris_media_desc_t *requested,
                              esp_iris_media_desc_t *actual,
                              uint32_t *total_size, void *user_ctx)
{
    (void)requested;
    screen_mirror_t *mirror = user_ctx;
    if (mirror == NULL || mirror->lock == NULL || mirror->frame_ready == NULL ||
            actual == NULL || total_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(mirror->lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    esp_gsp_handle_t ui = mirror->ui;
    const bool unavailable = ui == NULL || mirror->shadow != NULL ||
        mirror->capture != NULL;
    xSemaphoreGive(mirror->lock);
    if (unavailable) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t *shadow = heap_caps_calloc(
        1, SCREEN_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *capture = heap_caps_malloc(
        SCREEN_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (shadow == NULL || capture == NULL) {
        heap_caps_free(capture);
        heap_caps_free(shadow);
        return ESP_ERR_NO_MEM;
    }
    memset(capture, 0, COVERAGE_BYTES);

    esp_gsp_esp_lcd_pause_t *pause = NULL;
    esp_err_t err = esp_gsp_esp_lcd_pause(
        ui, FULL_REPAINT_TIMEOUT_MS, &pause);
    if (err != ESP_OK) {
        heap_caps_free(capture);
        heap_caps_free(shadow);
        return err;
    }

    while (xSemaphoreTake(mirror->frame_ready, 0) == pdTRUE) {
    }
    if (xSemaphoreTake(mirror->lock, portMAX_DELAY) != pdTRUE) {
        esp_gsp_handle_t resumed = NULL;
        (void)esp_gsp_esp_lcd_resume_paused(pause, &resumed);
        heap_caps_free(capture);
        heap_caps_free(shadow);
        return ESP_ERR_TIMEOUT;
    }
    mirror->shadow = shadow;
    mirror->capture = capture;
    mirror->covered_pixels = 0;
    mirror->capture_ready = false;
    mirror->warming = true;
    xSemaphoreGive(mirror->lock);

    esp_gsp_handle_t resumed = NULL;
    err = esp_gsp_esp_lcd_resume_paused(pause, &resumed);
    if (err != ESP_OK) {
        release_frame_storage(mirror);
        return err;
    }
    if (xSemaphoreTake(mirror->lock, portMAX_DELAY) == pdTRUE) {
        mirror->ui = resumed;
        xSemaphoreGive(mirror->lock);
    }

    if (xSemaphoreTake(mirror->frame_ready,
                       pdMS_TO_TICKS(FULL_REPAINT_TIMEOUT_MS)) != pdTRUE) {
        release_frame_storage(mirror);
        return ESP_ERR_TIMEOUT;
    }
    err = snapshot_frame(mirror);
    if (err != ESP_OK) {
        release_frame_storage(mirror);
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
    release_frame_storage(mirror);
}

esp_err_t iris_screen_mirror_init(void)
{
    if (s_mirror.lock != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_mirror.lock = xSemaphoreCreateMutexStatic(&s_mirror.lock_storage);
    s_mirror.frame_ready = xSemaphoreCreateBinaryStatic(
        &s_mirror.frame_ready_storage);
    if (s_mirror.lock == NULL || s_mirror.frame_ready == NULL) {
        if (s_mirror.lock != NULL) {
            vSemaphoreDelete(s_mirror.lock);
        }
        if (s_mirror.frame_ready != NULL) {
            vSemaphoreDelete(s_mirror.frame_ready);
        }
        s_mirror.lock = NULL;
        s_mirror.frame_ready = NULL;
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
        vSemaphoreDelete(s_mirror.frame_ready);
        vSemaphoreDelete(s_mirror.lock);
        s_mirror.frame_ready = NULL;
        s_mirror.lock = NULL;
        return err;
    }
    ESP_LOGI(TAG,
             "Registered %dx%d RGB565 GSP screen backend (frames allocated on demand)",
             BSP_LCD_H_RES, BSP_LCD_V_RES);
    return ESP_OK;
}

esp_err_t iris_screen_mirror_attach(esp_gsp_handle_t ui)
{
    if (ui == NULL || s_mirror.lock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_mirror.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (s_mirror.shadow != NULL || s_mirror.capture != NULL) {
        xSemaphoreGive(s_mirror.lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_mirror.ui = ui;
    xSemaphoreGive(s_mirror.lock);
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
    if (s_mirror.lock != NULL && buffer != NULL &&
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
            bool frame_ready = false;
            if (s_mirror.shadow != NULL) {
                const uint8_t *source = buffer->surface.pixels;
                uint8_t *destination = s_mirror.shadow +
                    (size_t)area->y1 * SCREEN_STRIDE +
                    (size_t)area->x1 * SCREEN_BYTES_PER_PIXEL;
                for (size_t row = 0; row < height; ++row) {
                    memcpy(destination + row * SCREEN_STRIDE,
                           source + row * stride_bytes, row_bytes);
                }
                if (s_mirror.warming && s_mirror.capture != NULL) {
                    frame_ready = mark_coverage(&s_mirror, area);
                }
            }
            xSemaphoreGive(s_mirror.lock);
            if (frame_ready) {
                xSemaphoreGive(s_mirror.frame_ready);
            }
        }
    }
    return __real_esp_display_presenter_submit_buffer(
        presenter, buffer, area, stride_bytes);
}
