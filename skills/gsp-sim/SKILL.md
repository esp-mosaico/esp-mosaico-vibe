---
name: gsp-sim
description: >
  Preview ESP-GSP scenes on the PC with the standalone sim matching
  submodule/esp-gsp (espressif/esp-gsp 1.1.0). Use when authoring
  or debugging GSP JSON/UI for ESP-Mosaico before flashing.
---

# ESP-GSP host simulation

Use this skill when the application UI is **GSP**, not LVGL. Factory remains
LVGL unless the task explicitly switches it.

## Pin

- Runtime: `submodule/esp-gsp` = **espressif/esp-gsp 1.1.0**
- Compiler: standalone `gspc` from `.gspc_version` (fetched by `fetch_gspc.py`)
- Simulator: standalone `sim` matching the component version (`GSP_SIM_EXECUTABLE`)

## Run

From the vibe repository root:

```sh
python3 tools/gsp-sim/run.py --headless --dump-ppm /tmp/gsp-hello.ppm
python3 tools/gsp-sim/run.py --interactive
python3 tools/gsp-sim/run.py projects/<name>/ui/main.json --interactive
```

The default scene is `projects/gsp_hello/ui/main.json`. Headless dump is the
Agent-safe check. Interactive mode opens the official local browser preview.
`gsp_hello` attaches `projects/gsp_hello/sim_backend.py` so the load bar
ticks like firmware `esp_gsp_timer_create`. Extra simulator flags go after
`--` (`--tap`, `--drag`, `--wait`). `--no-backend` previews the static scene.

## Authoring rules

- Scene size **480×480**, RGB565, matching the CO5300 panel.
- Keep JSON under the application, typically `projects/<name>/ui/`.
- Start from `projects/gsp_hello` for a sim + flash Hello World.
- Firmware depends on `espressif/esp-gsp` `==1.1.0` or
  `override_path: ../../../submodule/esp-gsp`.
- Do not import Mosaic claw hub, Lua runtime, or HTML review site into vibe.
- `mosaico.py` is unchanged: device install still goes through Recovery/OTA.

## Acceptance

1. `run.py --headless --dump-ppm` exits 0 and writes a 480×480 PPM.
2. The same scene JSON is what firmware will pack with the pinned ESP-GSP.
3. True-device validation still uses `python mosaico.py install` after Recovery.
