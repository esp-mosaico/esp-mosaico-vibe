# ESP-Mosaico Hello World

ESP-GSP 1.2.0 welcome application for the 480×480 ESP-Mosaico display, with
warm white, black, and orange styling inspired by https://mosaico.espressif.com/.
Tap **Say hello** to increment the two-digit counter. The 100th tap reveals
orange and black animated confetti with “100 · Hello, Maker!” for about three
seconds, then resets to 00. Taps during the celebration are ignored. Restarting
also resets the counter. The counter demonstrates touch input, not telemetry.

This is the single Hello World reference, replacing the former LVGL application
and the separate `gsp_hello` project. The same native scene and portable C logic
run in the PC simulator and on the device. New applications should preserve the
immutable Recovery partition prefix, shared normal-application configuration,
and `esp_mosaico_app_recovery` integration. Recovery itself remains LVGL-based.

## Simulate

From the workspace root, after resolving dependencies with `idf.py reconfigure`
in the application directory:

```sh
python3 tools/gsp-sim/run.py --interactive projects/hello_world/ui/main.json
```

The native backend handles clicks and updates the count. For a static scene-only
capture (without the C backend):

```sh
python3 tools/gsp-sim/run.py --headless --dump-ppm /tmp/hello-world.ppm projects/hello_world/ui/main.json
```

## Install

For a blank or unverified device, initialize and verify retained Recovery first:

```sh
python mosaico.py recover
```

Use a System Update for the first install, migration from the former LVGL Hello
World, or any scene/font/image changes. It includes the app, `ui_apps`, and the
compatible partition table while preserving Recovery and its bootloader:

```sh
python mosaico.py iris system-update --project projects/hello_world
```

Only use an application-only update for C-only changes when the full partition
table matches the device and the matching UI bundle is already installed:

```sh
python mosaico.py iris app-update --project projects/hello_world
```

The build fetches a standalone `gspc` when `GSPC_EXECUTABLE` is unset and writes
`build/ui_apps.bin`. The app maps and validates the deployable GSPB from this
partition, rather than embedding it in `ota_0`. The enter-Recovery RPC starts
before loading the UI, so Recovery remains reachable if the UI image is invalid.
The normal app does not contain an OTA writer.

## Screen capture

The ESP-Iris screen backend observes display presents from the first GSP frame.
It retains a RGB565 shadow and initial-paint coverage bitmap in PSRAM (478.125
KiB). A mirror or screenshot request allocates one additional 450 KiB capture
frame in PSRAM and releases it on close, stop, or disconnect. Capture does not
pause GSP or fall back to internal RAM. Early captures wait for all pixels to
be painted. Device operations and observation use `mosaico.py iris`.

## Source layout

- `ui/main.json`: native labels, geometry, button, and counter text binding.
- `main/hello_ui.c`: shared click handling and counter lifecycle.
- `pc/`: native simulator backend, without ESP-IDF dependencies.
- `main/board_display.c`, `iris_screen_mirror.c`, `ui_bundle.c`: device adapters.
- `mosaico-template.json`: complete source/font copy and path relocation rules
  used by `python mosaico.py project init <name>`.

The bundled DejaVu regular and bold fonts use the license in `ui/fonts/LICENSE`.

After building the PC backend, `tests/hello_world_ui/run.py` exercises its real
label/arrow touch targets, ignored outside taps, a held contact, pressed corners, the hundred-tap celebration, animated confetti,
ignored celebration taps, timed reset, and shutdown through the simulator API. See the CI workflow for invocation.
