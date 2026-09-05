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
It does not change the partition tables of other projects.

An existing device whose current table still has the former 13 MiB `ota_0`
needs two transactions because Recovery cannot write a data partition that is
not present in its current table. Build the `system-update-bundle` target, apply
`build/gsp_hello-layout-migration.irisfw`, then apply the complete project
bundle. The intermediate application keeps ESP-Iris active while `ui_apps` is
still empty. This migration is project-owned; it does not require changes to
the shared `mosaico.py` CLI or Recovery firmware.

The build fetches a standalone `gspc` if `GSPC_EXECUTABLE` is unset and writes
`build/ui_apps.bin`. The application keeps the enter-Recovery RPC active before
loading that image; it does not include an OTA writer.
