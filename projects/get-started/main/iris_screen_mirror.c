// SPDX-License-Identifier: Apache-2.0

#include "iris_screen_mirror.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bsp/esp_mosaico.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "lvgl.h"

typedef struct {
    uint8_t *frame;
    uint32_t total_size;
    uint16_t width;
    uint16_t height;
    uint32_t stride;
    bool frame_ready;
} screen_capture_t;

static const char *TAG = "iris_screen";
static screen_capture_t s_capture;

static esp_err_t capture_frame(screen_capture_t *capture)
{
    if (!bsp_display_lock(-1)) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t result = ESP_OK;
    lv_display_t *display = bsp_display_get();
    lv_draw_buf_t *source = display != NULL
                                ? lv_display_get_buf_active(display)
                                : NULL;
    if (source == NULL || source->data == NULL) {
        result = ESP_ERR_INVALID_STATE;
        goto done;
    }

    const int32_t width = lv_display_get_horizontal_resolution(display);
    const int32_t height = lv_display_get_vertical_resolution(display);
    if (width <= 0 || height <= 0 || width > UINT16_MAX ||
        height > UINT16_MAX ||
        lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB565 ||
        source->header.w != (uint32_t)width ||
        source->header.h != (uint32_t)height) {
        result = ESP_ERR_NOT_SUPPORTED;
        goto done;
    }

    const uint64_t total_size =
        (uint64_t)source->header.stride * source->header.h;
    if (total_size == 0 || (total_size & 1U) != 0 ||
        total_size > UINT32_MAX ||
        total_size > source->data_size) {
        result = ESP_ERR_INVALID_SIZE;
        goto done;
    }

    if (capture->frame == NULL || capture->total_size != total_size) {
        uint8_t *new_frame = heap_caps_malloc(
            (size_t)total_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (new_frame == NULL) {
            result = ESP_ERR_NO_MEM;
            goto done;
        }
        heap_caps_free(capture->frame);
        capture->frame = new_frame;
    }

    const uint8_t *source_bytes = source->data;
    for (size_t offset = 0; offset < (size_t)total_size; offset += 2) {
        capture->frame[offset] = source_bytes[offset + 1];
        capture->frame[offset + 1] = source_bytes[offset];
    }
    capture->total_size = (uint32_t)total_size;
    capture->width = (uint16_t)width;
    capture->height = (uint16_t)height;
    capture->stride = source->header.stride;
    capture->frame_ready = true;

done:
    bsp_display_unlock();
    return result;
}

static void release_capture(screen_capture_t *capture)
{
    heap_caps_free(capture->frame);
    *capture = (screen_capture_t) {0};
}

static esp_err_t screen_begin(const esp_iris_media_desc_t *requested,
                              esp_iris_media_desc_t *actual,
                              uint32_t *total_size, void *user_ctx)
{
    (void)requested;
    screen_capture_t *capture = user_ctx;
    if (capture == NULL || actual == NULL || total_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (capture->frame != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = capture_frame(capture);
    if (err != ESP_OK) {
        release_capture(capture);
        return err;
    }

    *actual = (esp_iris_media_desc_t) {
        .x = 0,
        .y = 0,
        .width = capture->width,
        .height = capture->height,
        .stride = capture->stride,
        .format = ESP_IRIS_PIXEL_FORMAT_RGB565,
        .quality = 0,
    };
    *total_size = capture->total_size;
    return ESP_OK;
}

static esp_err_t screen_read(uint32_t offset, uint8_t *out, size_t capacity,
                             size_t *out_size, void *user_ctx)
{
    screen_capture_t *capture = user_ctx;
    if (capture == NULL || capture->frame == NULL || out == NULL ||
        out_size == NULL || capacity == 0 || offset >= capture->total_size) {
        return ESP_ERR_INVALID_ARG;
    }

    if (offset == 0 && !capture->frame_ready) {
        esp_err_t err = capture_frame(capture);
        if (err != ESP_OK) {
            return err;
        }
    } else if (!capture->frame_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t size = capture->total_size - offset;
    if (size > capacity) {
        size = capacity;
    }
    memcpy(out, capture->frame + offset, size);
    *out_size = size;

    if (offset + size == capture->total_size) {
        capture->frame_ready = false;
    }
    return ESP_OK;
}

static void screen_end(void *user_ctx)
{
    screen_capture_t *capture = user_ctx;
    if (capture != NULL) {
        release_capture(capture);
    }
}

esp_err_t iris_screen_mirror_register(void)
{
    if (bsp_display_get() == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_iris_screen_backend_t backend = {
        .begin = screen_begin,
        .read = screen_read,
        .end = screen_end,
        .user_ctx = &s_capture,
    };
    esp_err_t err = esp_iris_screen_register(&backend);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Registered %dx%d RGB565 screen backend",
                 BSP_LCD_H_RES, BSP_LCD_V_RES);
    }
    return err;
}
