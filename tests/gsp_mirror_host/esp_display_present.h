#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct esp_display_presenter esp_display_presenter_t;

typedef struct {
    struct {
        const void *pixels;
        unsigned pixel_format;
    } surface;
    size_t capacity_bytes;
} esp_display_presenter_buffer_t;

typedef struct {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
} esp_display_present_area_t;

#define ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565 1U
