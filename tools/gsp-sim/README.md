# ESP-GSP host simulator

PC preview for GSP scenes on ESP-Mosaico. It packs JSON with the GSPC version
recorded in `submodule/esp-gsp/.gspc_version` and runs the matching standalone
**sim** binary from dl.espressif.com.

This wrapper is scene-only. It does not include Mosaic claw hub, Lua apps, or
HTML review tooling.

## Prerequisites

- Linux x86_64 (or another host published in the sim/gspc manifests)
- Network once, to fetch standalone `gspc` and `sim`
- Git submodule `submodule/esp-gsp` initialized

## Run the GSP Hello World demo

Headless smoke (writes a PPM):

```sh
python3 tools/gsp-sim/run.py --headless --dump-ppm /tmp/gsp-hello.ppm
```

Interactive preview (opens the official local browser canvas):

```sh
python3 tools/gsp-sim/run.py --interactive
```

The default scene is [`projects/gsp_hello/ui/main.json`](../../projects/gsp_hello/ui/main.json).
Preview another scene:

```sh
python3 tools/gsp-sim/run.py projects/<name>/ui/main.json --interactive
```

Pass extra simulator flags after `--`, for example `-- --drag 80 360 400 360`.
Pass a precompiled `.gspb` to skip `gspc`. Set `GSPC_EXECUTABLE` or
`GSP_SIM_EXECUTABLE` to skip the download cache.

## New GSP projects

Keep scene JSON under the application, typically `projects/<name>/ui/`.
The reference demo is [`projects/gsp_hello`](../../projects/gsp_hello).
Author at **480×480 RGB565** to match the CO5300 panel. Firmware should depend
on `espressif/esp-gsp` `==1.1.0` (or `override_path` to `submodule/esp-gsp`).
The tools-owned Recovery firmware remains LVGL-based and is not a GSP
application template.
