# Touch input on ESP-Mosaico

## Current hardware driver boundary

The board uses a CST9217/CST9220-family controller. The registry component
`78/esp_lcd_touch_cst92xx` version `0.1.0` supports up to two simultaneous
points and supplies track IDs through the standard `esp_lcd_touch` API. Set
`CONFIG_ESP_LCD_TOUCH_MAX_POINTS` to at least `2`.

The BSP now integrates this driver and keeps `bsp_touch_new()` as the stable
application-facing API. Game projects still need
`CONFIG_ESP_LCD_TOUCH_MAX_POINTS>=2` and must request two points; Sky Hop is the
reference implementation for preserving track IDs and simultaneous actions.

## Integration boundary

Maintain the registry component in the BSP submodule, not separately in every
game. Future driver updates must change the BSP include, I2C configuration,
constructor and dependency together without bypassing `bsp_touch_new()`.

The game platform input event must represent a bounded set of two contacts:

- stable track ID;
- screen-space x/y;
- down, move, and up lifecycle;
- point count for the current report;
- source timestamp or frame sequence when available.

Keep Raylib mouse compatibility as a projection of the primary contact. Do not
collapse both contacts into mouse state before gesture or game-action mapping.
Gameplay models should consume actions such as move, jump, aim, or pause unless
the game genuinely needs raw multi-touch contacts.

## Reading reports

Allocate and request two points, never one:

```c
esp_lcd_touch_point_data_t points[2] = {0};
uint8_t count = 0;

ESP_RETURN_ON_ERROR(esp_lcd_touch_read_data(touch), TAG, "read touch");
ESP_RETURN_ON_ERROR(
    esp_lcd_touch_get_data(touch, points, &count, 2), TAG, "get touch points");
```

Clamp unexpected counts and validate coordinates before queueing events. Track
contact identity by the driver's track ID rather than array order; controllers
may reorder contacts between reports.

## Acceptance tests

Verify on the physical board:

1. Each finger independently produces down, move, and up without a stuck point.
2. Adding or removing the second finger does not swap the first finger's logical
   action.
3. Crossing two fingers preserves track identity.
4. Empty and malformed reports do not stall later touches.
5. Two held controls work concurrently, for example move plus jump.
6. Single-touch games and ESP-Iris remote input retain their existing behavior.

Log point count and track IDs in a rate-limited diagnostic mode. Do not log every
touch report in production because it can perturb timing and flood ESP-Iris.
