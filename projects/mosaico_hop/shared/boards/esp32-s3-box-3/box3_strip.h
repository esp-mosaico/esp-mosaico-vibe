// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Pure nearest-neighbor RGB565 strip extraction for the BOX-3 SPI presenter.
 * Kept independent from ESP-IDF so Host tests check the actual pixel path. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "box3_viewport.h"

static inline void box3_fill_strip_pixels(
    const box3_viewport_t *view, const uint16_t *frame, uint16_t *strip,
    uint16_t top, uint16_t count, bool swap_bytes)
{
    const uint32_t logical_w = view->logical_width;
    const uint32_t logical_h = view->logical_height;
    for (uint32_t row = 0; row < count; row++) {
        uint32_t y = (uint32_t)top + row;
        uint16_t *dst = strip + row * view->lcd_width;
        if (y < view->view_y ||
                y >= (uint32_t)view->view_y + view->view_height) {
            memset(dst, 0, view->lcd_width * sizeof(uint16_t));
            continue;
        }
        uint32_t src_y = ((y - view->view_y) * logical_h) / view->view_height;
        const uint16_t *src = frame + src_y * logical_w;
        for (uint32_t x = 0; x < view->lcd_width; x++) {
            uint16_t color = 0;
            if (x >= view->view_x &&
                    x < (uint32_t)view->view_x + view->view_width) {
                uint32_t src_x = ((x - view->view_x) * logical_w) / view->view_width;
                color = src[src_x];
                if (swap_bytes)
                    color = (uint16_t)((color << 8) | (color >> 8));
            }
            dst[x] = color;
        }
    }
}
