---
name: gsp-sim
description: >
  Preview ESP-GSP apps on the PC with sim_bridge (default) plus the
  standalone sim matching submodule/esp-gsp (espressif/esp-gsp 1.2.0).
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

- Runtime: `submodule/esp-gsp` = **espressif/esp-gsp 1.2.0**
- Bridge: `submodule/esp-gsp/tools/sim_bridge`
- Compiler: standalone `gspc` from `.gspc_version` (fetched by `fetch_gspc.py`)
- Simulator: standalone `sim` matching the component version (`GSP_SIM_EXECUTABLE`)

## Run

From the vibe repository root. `run.py` uses `sim_bridge` whenever the
project has `pc/CMakeLists.txt` (gsp_hello does):

```sh
python3 tools/gsp-sim/run.py --interactive
python3 tools/gsp-sim/run.py projects/<name>/ui/main.json --interactive
python3 tools/gsp-sim/run.py --headless
```

The default scene is `projects/gsp_hello/ui/main.json`. Interactive mode
opens the official local browser preview (listen URL is printed as
`Preview: http://127.0.0.1:<port>/`).

Call the component runner directly when you need extra bridge flags:

```sh
python3 submodule/esp-gsp/tools/sim_bridge/run.py \
  --project projects/gsp_hello/pc \
  --component-dir submodule/esp-gsp \
  --gspc "$(python3 tools/gsp-sim/fetch_gspc.py)" \
  --host "$(python3 tools/gsp-sim/fetch_gspc.py --sim)"
```

### Scene-only fallback

Use only for a static screenshot or extra `sim` flags (`--tap`, `--drag`).
This packs JSON and runs `sim --bundle` with **no** native backend:

```sh
python3 tools/gsp-sim/run.py --headless --dump-ppm /tmp/gsp-hello.ppm
python3 tools/gsp-sim/run.py --interactive --scene-only
python3 tools/gsp-sim/run.py projects/<name>/ui/main.json --interactive -- --drag 80 360 400 360
```

`--dump-ppm`, `--scene-only`, a `.gspb` input, or extra flags after `--`
all skip `sim_bridge`.

## How sim_bridge works

1. `gspc pack` builds a deployable GSPB and generated `bundle_gsp.h`.
2. CMake compiles portable UI sources + `pc/platform_pc.c` + the bridge
   into a native Backend (`gsp_add_backend`).
3. Official `sim` starts with `--backend-enable --backend-required`.
4. Backend connects over loopback TCP, runs `gsp_bridge_app_init`, and
   pumps timers/callbacks via `gsp_sim_bridge_poll`.
5. Bind/component writes (`gsp_hello_load_set_value`, …) update the live
   preview. Rendering stays in `sim`.

Keep display/touch/FreeRTOS/Iris out of the Backend. Share one portable
UI file between device and PC, as `projects/gsp_hello/main/hello_ui.c`
does. `app_main` only does hardware + `hello_ui_init`; `platform_pc.c`
only implements:

```c
esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui);
void gsp_bridge_app_deinit(esp_gsp_handle_t ui);
```

Init must return. A blocking loop prevents event dispatch.

New GSP apps copy `projects/gsp_hello/pc/` and point `gsp_add_backend`
`SOURCES` / `SCENES` at that project's portable `.c` and `ui/main.json`.

## Authoring rules

- Scene size **480×480**, RGB565, matching the CO5300 panel.
- Keep JSON under the application, typically `projects/<name>/ui/`.
- Start from `projects/gsp_hello` for a sim + flash Hello World.
- Put bind/timer/list/canvas logic in portable C (no IDF headers).
- Always add `pc/CMakeLists.txt` so `run.py` defaults to `sim_bridge`.
- Firmware depends on `espressif/esp-gsp` `==1.2.0` or
  `override_path: ../../../submodule/esp-gsp`.
- Do not import Mosaic claw hub, Lua runtime, or HTML review site into vibe.
- `mosaico.py` is unchanged: device install still goes through Recovery/OTA.

## Acceptance

1. `run.py --interactive` (or `--headless` without `--dump-ppm`) builds the
   Backend and prints `Preview:` / keeps the process running. Widgets
   driven by C timers or bind writes must change, not stay at JSON defaults.
2. `run.py --headless --dump-ppm` stays scene-only, exits 0, and writes a
   480×480 PPM.
3. The same scene JSON is what firmware will pack with the pinned ESP-GSP.
4. True-device validation still uses `python mosaico.py install` after Recovery.
