---
name: gsp-sim
description: >
  Preview ESP-GSP apps on the PC with sim_bridge (default) plus the
  standalone sim matching espressif/esp-gsp 1.4.0 from the Component Registry.
  Use when authoring or debugging GSP JSON/UI or portable C UI logic
  for ESP-Mosaico before flashing.
---

# ESP-GSP host simulation

Use this skill when the application UI is **GSP**, not LVGL. Factory remains
LVGL unless the task explicitly switches it.

**Default path is `sim_bridge`.** Scene-only `sim --bundle` does not run
application timers or bind writes, so widgets that depend on C (progress
bars, lists, text updates) stay frozen at JSON defaults.

## Pin

- Runtime: **espressif/esp-gsp 1.4.0** from the ESP Component Registry
- Bridge: `managed_components/espressif__esp-gsp/tools/sim_bridge`
- Compiler: standalone `gspc` from `.gspc_version` (fetched by `fetch_gspc.py`)
- Simulator: standalone `sim` matching the component version (`GSP_SIM_EXECUTABLE`)

## First-time setup

The first `sim_bridge` preview or firmware build needs
`espressif/esp-gsp==1.4.0` in the application's `managed_components/`.
From the application directory (for example `projects/hello_world`):

```sh
idf.py reconfigure
```

This downloads `managed_components/espressif__esp-gsp/` from the ESP
Component Registry (gitignored). After that, `run.py` can find
`sim_bridge` without `ESP_GSP_COMPONENT_DIR`. Repeat in each new GSP
project. Scene-only `--dump-ppm` / `--scene-only` does not need this.

## Run

From the vibe repository root. `run.py` uses `sim_bridge` whenever the
project has `pc/CMakeLists.txt` (hello_world does):

```sh
python3 tools/gsp-sim/run.py --interactive
python3 tools/gsp-sim/run.py projects/<name>/ui/main.json --interactive
python3 tools/gsp-sim/run.py --headless
```

The default scene is `projects/hello_world/ui/main.json`. Interactive mode
opens the official local browser preview (listen URL is printed as
`Preview: http://127.0.0.1:<port>/`).

Call the component runner directly when you need extra bridge flags:

```sh
python3 projects/hello_world/managed_components/espressif__esp-gsp/tools/sim_bridge/run.py \
  --project projects/hello_world/pc \
  --component-dir projects/hello_world/managed_components/espressif__esp-gsp \
  --gspc "$(python3 tools/gsp-sim/fetch_gspc.py)" \
  --host "$(python3 tools/gsp-sim/fetch_gspc.py --sim)"
```

### Scene-only fallback

Use only for a static screenshot or extra `sim` flags (`--tap`, `--drag`).
This packs JSON and runs `sim --bundle` with **no** native backend:

```sh
python3 tools/gsp-sim/run.py --headless --dump-ppm /tmp/hello-world.ppm
python3 tools/gsp-sim/run.py --interactive --scene-only
python3 tools/gsp-sim/run.py projects/<name>/ui/main.json --interactive -- --drag 80 360 400 360
```

`--dump-ppm`, `--scene-only`, `--frames`, an explicit `--fps`, a `.gspb` input,
or extra flags after `--` all skip `sim_bridge`.

## How sim_bridge works

1. `gspc pack` builds a deployable GSPB and generated `bundle_gsp.h`.
2. CMake compiles portable UI sources + `pc/platform_pc.c` + the bridge
   into a native Backend (`gsp_add_backend`).
3. Official `sim` starts with `--backend-enable --backend-required`.
4. Backend connects over loopback TCP, runs `gsp_bridge_app_init`, and
   pumps timers/callbacks via `gsp_sim_bridge_poll`.
5. Bind/component writes (`gsp_hello_count_set_text`, …) update the live
   preview. Rendering stays in `sim`.

Keep display/touch/FreeRTOS/Iris out of the Backend. Share one portable
UI file between device and PC, as `projects/hello_world/main/hello_ui.c`
does. `app_main` only does hardware + `hello_ui_init`; `platform_pc.c`
only implements:

```c
esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui);
void gsp_bridge_app_deinit(esp_gsp_handle_t ui);
```

Init must return. A blocking loop prevents event dispatch.

New GSP apps copy `projects/hello_world/pc/` and point `gsp_add_backend`
`SOURCES` / `SCENES` at that project's portable `.c` and `ui/main.json`.

## Authoring rules

- Scene size **480×480**, RGB565, matching the CO5300 panel.
- Keep JSON under the application, typically `projects/<name>/ui/`.
- Start from `projects/hello_world` for a sim + flash Hello World.
- Put bind/timer/list/canvas logic in portable C (no IDF headers).
- Always add `pc/CMakeLists.txt` so `run.py` defaults to `sim_bridge`.
- Firmware depends on `espressif/esp-gsp` `==1.4.0` via `idf_component.yml`.
- First `sim_bridge` run or firmware build in a project: `idf.py reconfigure`
  in that project directory to pull the component.
- Do not import Mosaic claw hub, Lua runtime, or HTML review site into vibe.
- `mosaico.py` is unchanged: device install still goes through Recovery/OTA.

## Acceptance

1. `run.py --interactive` (or `--headless` without `--dump-ppm`) builds the
   Backend and prints `Preview:` / keeps the process running. Widgets
   driven by C callbacks or timers must change, not stay at JSON defaults.
   In Hello World, click Say hello and verify the count increases.
2. `run.py --headless --dump-ppm` stays scene-only, exits 0, and writes a
   480×480 PPM.
3. The same scene JSON is what firmware will pack with the pinned ESP-GSP.
4. True-device validation uses `python mosaico.py iris system-update` for a
   first install or changed UI assets, after Recovery. Use `iris app-update`
   only for code-only changes with matching partitions and UI assets.
