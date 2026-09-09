#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint32_t stride;
    uint16_t format;
    uint16_t quality;
} esp_iris_media_desc_t;

typedef struct {
    esp_err_t (*begin)(const esp_iris_media_desc_t *requested,
                       esp_iris_media_desc_t *actual, uint32_t *total_size,
                       void *user_ctx);
    esp_err_t (*read)(uint32_t offset, uint8_t *out, size_t capacity,
                      size_t *out_size, void *user_ctx);
    void (*end)(void *user_ctx);
    void *user_ctx;
} esp_iris_screen_backend_t;

#define ESP_IRIS_PIXEL_FORMAT_RGB565 1U

esp_err_t esp_iris_screen_register(const esp_iris_screen_backend_t *backend);
