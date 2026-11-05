// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Portable logical-to-physical viewport geometry shared by rendering and
 * touch input. A bar area is NOT interactive. */
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t logical_width, logical_height;
    uint16_t lcd_width, lcd_height;
    uint16_t view_x, view_y, view_width, view_height;
} box3_viewport_t;

static inline bool box3_viewport_init(
    box3_viewport_t *out, uint16_t logical_w, uint16_t logical_h,
    uint16_t lcd_w, uint16_t lcd_h)
{
    if (!out || !logical_w || !logical_h || !lcd_w || !lcd_h) return false;
    box3_viewport_t view = {
        .logical_width = logical_w, .logical_height = logical_h,
        .lcd_width = lcd_w, .lcd_height = lcd_h,
    };
    if ((uint32_t)logical_w * lcd_h <= (uint32_t)logical_h * lcd_w) {
        view.view_height = lcd_h;
        view.view_width = (uint16_t)((uint32_t)logical_w * lcd_h / logical_h);
    } else {
        view.view_width = lcd_w;
        view.view_height = (uint16_t)((uint32_t)logical_h * lcd_w / logical_w);
    }
    if (!view.view_width) view.view_width = 1;
    if (!view.view_height) view.view_height = 1;
    view.view_x = (lcd_w - view.view_width) / 2;
    view.view_y = (lcd_h - view.view_height) / 2;
    *out = view;
    return true;
}

static inline bool box3_viewport_map_touch(
    const box3_viewport_t *view, int32_t *x, int32_t *y)
{
    if (!view || !x || !y || !view->view_width || !view->view_height)
        return false;
    int32_t px = *x - view->view_x;
    int32_t py = *y - view->view_y;
    if (px < 0 || py < 0 ||
            px >= view->view_width || py >= view->view_height) return false;
    *x = (int32_t)((uint32_t)px * view->logical_width / view->view_width);
    *y = (int32_t)((uint32_t)py * view->logical_height / view->view_height);
    return true;
}
