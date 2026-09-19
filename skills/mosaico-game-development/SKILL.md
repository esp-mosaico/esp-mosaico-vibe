---
name: mosaico-game-development
description: Create, extend, debug, test, package, or install 2D games on ESP-Mosaico with the in-tree Raylib-compatible Game SDK. Use for gameplay, RGB565 rendering, Atlas/Tiled assets, game audio, Host replay, touch input, performance work, and recovery-safe device deployment; do not use for non-game BSP examples.
---

# Mosaico Game Development

Build applications under `projects/<name>` and treat `projects/hello_world` as the recovery/reference project, never as the game implementation target.

## Start with the correct layer

Read [`docs/game-platform.md`](../../docs/game-platform.md), then inspect the closest reference project:

- `raylib_shooter` for a small code-drawn game;
- `tower_defense` for Atlas, Tiled, audio, and Host replay;
- `sky_hop` for platform physics, scrolling, generated art, and audio cues.

Keep gameplay state and `update()` logic in C files that compile without ESP-IDF.
Device firmware uses a thin `main.c` that calls `mosaico_game_app_run()`; put
board-specific assets, zones, audio, and callbacks in `<game>_app.c`. Do not
reimplement boot, touch, or the GSP loop in each game. Do not invent component
APIs; inspect component headers and BSP examples first.

For local simulation, add `game.sim.json` and keep the Raylib renderer in a
shared `<game>_view.c`; compile it in both device `main/CMakeLists.txt` and the
Host manifest. Use `game_module.c` only for Host lifecycle and input mapping.
Do not create a Python, HTML Canvas, or separate `host_adapter.c` renderer for
new games. Run `python3 mosaico.py game sim projects/<name>` for the standard
Host RGB565 preview and add `--headless --scenario <json>` for deterministic
input and state checks. Game projects do not add GSP `pc/` backends; validate
the device Canvas presenter with a device build, ESP-Iris logs and screenshots.

For component selection and supported API details, read [references/tech-stack.md](references/tech-stack.md). For assets or audio work, also read [references/content-pipeline.md](references/content-pipeline.md). For physical touch, gestures, or simultaneous controls, read [references/touch-input.md](references/touch-input.md).

## Implement

Use the shared CMake integration:

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/raylib_lite_engine.cmake")
mosaico_game_sdk_configure_gsp_compiler()
mosaico_game_sdk_add_components(RAYLIB AUDIO TILEMAP)
```

Request only used capabilities. Preserve a fixed update rate, bounded object pools, framebuffer clipping, and deterministic state where practical. Do not allocate, parse files, or block in the frame hot path.

Use `mosaico_raylib_fast.h` only for its implemented compatibility surface in
[`docs/raylib-api.md`](../../docs/raylib-api.md). Unsupported Raylib APIs must
not silently use the generic software-OpenGL path. Prefer Atlas drawing for
production art and code primitives for diagnostics or simple UI.

Do not equate Raylib mouse compatibility with multi-touch support. Preserve up to two physical contacts and their track IDs through the device input layer; derive the primary mouse contact only after gesture/action mapping.

## Preserve device recovery

Every normal game build must:

- set `CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY=y` and leave `CONFIG_ESP_IRIS_OTA` unset;
- compile and call `iris_ota_support_start()` before renderer startup
  (`mosaico_game_app_run()` already does this);
- register system inventory via `mosaico_game_iris` and mark healthy only after
  the first frame succeeds;
- retain the factory-compatible partition layout and `game_assets` partition when assets are used.

Never flash with raw ESP-IDF or ESP-Iris write commands. Query the live Device ID with `python mosaico.py iris list`, run `python mosaico.py recover` before the first install on a blank or unverified device, and install only with `python mosaico.py iris app-update --project ...`.

For boot loops, missing crash logs, failed OTA/system updates, or Recovery fallback, follow [`esp-iris-device-debugging`](../esp-iris-device-debugging/SKILL.md) before another write. If a system update containing `game_assets` is interrupted, treat that partition as absent or partial until verified. Do not install an asset-dependent application by app-only OTA unless the exact assets are verified or the application has a tested embedded fallback.

Do not make required resource mounting an unexplained `ESP_ERROR_CHECK`. Log the partition label, expected content/version, mount error, and selected fallback before aborting or marking the application healthy.

## Verify proportionally

At minimum:

1. Compile the pure gameplay model with the Host C compiler using `-Wall -Wextra -Werror` and test movement/state transitions.
2. Run `python3 -m unittest discover -s tests/game_sdk -v`.
3. Run `python3 mosaico.py game sim projects/<name> --headless`.
4. Build with `python3 mosaico.py game build projects/<name>`.
5. When device work is requested, verify the same Device ID returns from Recovery to normal with a new Boot ID and healthy application.
6. Capture a screenshot and inspect actual sprite scale, alpha, labels, and clipping. For audio, confirm readiness/errors from device logs and exercise each cue when feasible.

Do not claim device or audio success from a successful build alone.
