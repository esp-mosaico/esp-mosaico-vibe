# GSP Hello World

Minimal ESP-GSP 1.1.0 application for ESP-Mosaico. The same scene is used by
the PC host simulator and packed as a deployable GSPB in the dedicated
`ui_apps` Flash partition. The application maps and validates that bundle at
startup instead of embedding it in `ota_0`.

Factory remains the LVGL Recovery template.

## Simulate

From the vibe repository root:

```sh
python3 tools/gsp-sim/run.py projects/gsp_hello/ui/main.json --headless --dump-ppm /tmp/gsp-hello.ppm
python3 tools/gsp-sim/run.py projects/gsp_hello/ui/main.json --interactive
```

`run.py` attaches `sim_backend.py` automatically. That script is the PC equivalent of
the firmware load timer: it connects to `sim --backend-listen` and updates the
`load` progress bind every 250 ms. Use `--no-backend` to preview the static scene.

## Flash

Recovery must already be on the device (`python mosaico.py recover`). A normal
application-only update can reuse the installed UI bundle:

```sh
python mosaico.py install --project projects/gsp_hello
```

To install a changed scene, font, or image, use a System Update containing the
application, `ui_apps`, bootloader, and partition table:

```sh
python mosaico.py system-update --project projects/gsp_hello
```

The command updates this project's partition table as part of the transaction.
Recovery 2.4.0 or newer validates the shared recovery-critical partitions,
then uses this target table to write `ota_0` and `ui_apps`. Therefore the same
single command works whether the device currently runs `hello_world` or
`gsp_hello`; no intermediate layout-migration bundle is needed.

The build fetches a standalone `gspc` if `GSPC_EXECUTABLE` is unset and writes
`build/ui_apps.bin`. The application keeps the enter-Recovery RPC active before
loading that image; it does not include an OTA writer.
