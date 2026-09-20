# Technical stack and routing

## Core platform

- ESP-IDF matching the selected application manifest, target `esp32s31`;
  check its `sdkconfig.defaults` for the FreeRTOS tick rate.
- ESP-Mosaico BSP for display, touch, power and ES8311/I2S audio.
- ESP-GSP 1.4.0 Canvas for presenting the 480×480 RGB565 framebuffer.
- ESP-Iris for lifecycle, system inventory, screen/input RPC, logs, health and recovery-first update.
- Raylib 6.0-compatible public types and selected 2D calls; the embedded fast layer is intentionally not full Raylib.

Check the selected application's `main/idf_component.yml`; the normal-app
reference is `projects/hello_world/main/idf_component.yml`. Resolve the host
environment according to repository `AGENTS.md`, and check Recovery's separate
manifest when working on Recovery.

The CST92xx touch hardware can report two contacts, but support is end-to-end:
the BSP driver, `CONFIG_ESP_LCD_TOUCH_MAX_POINTS`, the requested point-array
length, the platform event schema, and the game mapping must all preserve two
points. See [touch-input.md](touch-input.md).

## Component selection

| Capability | Components | Use when |
|---|---|---|
| Core | `mosaico_game`, `mosaico_game_input`, `mosaico_game_debug` | Every game |
| Raylib 2D | `mosaico_raylib_fast`, `mosaico_raylib_port`, `mosaico_game_2d`, `mosaico_game_assets` | Drawing through the compatible Raylib surface |
| Assets | `mosaico_game_assets` | Reading packaged content from `game_assets` |
| Tilemap | `mosaico_game_tilemap` | Finite orthogonal Tiled maps, collision and object/path queries |
| Audio | `mosaico_game_audio` | 8 SFX voices and one looping BGM stream |

Declare these via `mosaico_game_sdk_add_components`; do not enumerate their paths in each project.

## Raylib fast surface

Supported calls are declared in `submodule/raylib-lite-engine/components/mosaico_raylib_fast/include/mosaico_raylib_fast.h`. Inspect that header before choosing an API. It currently covers window/frame boundaries, Camera2D world/screen transforms, clear, pixel, line, circle, rectangle, rectangle outline, triangle, bitmap text, text measure/format, textures, `DrawTexture*` variants, and common rectangle/circle/point collision queries.

Keep gameplay coordinates in world space and convert to screen space at render time. Clip before expensive draw loops. A full framebuffer is 450 KiB; avoid extra full-screen copies.

## Model and runtime split

The Host-testable model may use standard C headers and math but should not include ESP-IDF, Raylib, BSP or FreeRTOS headers. Express input as game-level commands or simple coordinates. Device `main.c` calls `mosaico_game_app_run()`; the shared runner owns input
and frame scheduling, while project callbacks update the model and render the
shared view.

Use fixed arrays/object pools for enemies, projectiles and effects. A stable state hash is useful for deterministic replay and regression tests.

## Device startup order

Follow the management-before-render sequence in the engine
[component lifecycle](../../../../submodule/raylib-lite-engine/components/README.md#configuration-and-lifecycle). Screen mirror callbacks may return invalid state before the first framebuffer; that is acceptable. Marking healthy before a successful first frame is not acceptable.
