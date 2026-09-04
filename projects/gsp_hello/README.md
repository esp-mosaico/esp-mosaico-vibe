# GSP Hello World

Minimal ESP-GSP 1.1.0 application for ESP-Mosaico. The same scene is used by
the PC host simulator and the firmware that `mosaico.py install` writes to
`ota_0`.

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

Recovery must already be on the device (`python mosaico.py recover`). Then:

```sh
python mosaico.py install --project projects/gsp_hello
```

The build fetches a standalone `gspc` if `GSPC_EXECUTABLE` is unset. The
application keeps the enter-Recovery RPC; it does not include an OTA writer.
