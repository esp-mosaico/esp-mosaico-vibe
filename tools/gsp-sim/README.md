# ESP-GSP host simulator

PC preview for GSP applications on ESP-Mosaico. By default it uses
`managed_components/espressif__esp-gsp/tools/sim_bridge`: pack the scene,
build a native Backend from portable UI C, and run the standalone **sim**
with a Backend channel so timers and bind writes match the device.

`--dump-ppm`, `--scene-only`, `--frames`, an explicit `--fps`, or extra `sim`
flags after `--` fall back to scene-only `sim --bundle` (no application C).
This wrapper does not include Mosaic claw hub, Lua apps, or HTML review tooling.

## Prerequisites

- Linux x86_64 (or another host published in the sim/gspc manifests)
- Network once, to fetch standalone `gspc` and `sim`
- `espressif/esp-gsp==1.4.0` pulled by the component manager
  (`idf.py reconfigure` in the application, or set `ESP_GSP_COMPONENT_DIR`)

## Run the GSP Hello World demo

Headless smoke (writes a PPM):

```sh
python3 tools/gsp-sim/run.py --headless --dump-ppm /tmp/hello-world.ppm
```

Interactive preview (opens the official local browser canvas):

```sh
python3 tools/gsp-sim/run.py --interactive
```

The default scene is [`projects/hello_world/ui/main.json`](../../projects/hello_world/ui/main.json).
Preview another scene:

```sh
python3 tools/gsp-sim/run.py projects/<name>/ui/main.json --interactive
```

Pass extra simulator flags after `--`, for example `-- --drag 80 360 400 360`.
Pass a precompiled `.gspb` to skip `gspc`. Set `GSPC_EXECUTABLE` or
`GSP_SIM_EXECUTABLE` to skip the download cache.

## New GSP projects

Keep scene JSON under the application, typically `projects/<name>/ui/`.
The reference demo is [`projects/hello_world`](../../projects/hello_world).
Author at **480×480 RGB565** to match the CO5300 panel. Firmware should depend
on `espressif/esp-gsp` `==1.4.0` from the ESP Component Registry.
New apps need `pc/CMakeLists.txt` so `run.py` can default to `sim_bridge`.
The utilities-owned Recovery firmware also uses GSP, but remains an internal
recovery resource rather than an application template.

## Upgrading an existing build to GSP 1.4

GSP 1.4.0 requires GSPC 0.5.0 and simulator 1.4.0. An existing CMake cache may
still point at GSPC 0.3.0. In the verified ESP-IDF environment, run from the
workspace root:

```sh
idf.py -C projects/hello_world \
  -D "GSPC_EXECUTABLE=$(python3 tools/gsp-sim/fetch_gspc.py --pinned)" reconfigure
```

Use the corresponding project path for other applications or Recovery. The
`--pinned` bootstrap selects the new workspace tool version before the component
manager replaces the old managed component. Explicit `GSPC_EXECUTABLE` and
`GSP_SIM_EXECUTABLE` environment overrides still take precedence; update any
such overrides to the matching versions. Rebuild firmware and UI resources
together, and use a System Update when installing a newly compiled UI bundle.
