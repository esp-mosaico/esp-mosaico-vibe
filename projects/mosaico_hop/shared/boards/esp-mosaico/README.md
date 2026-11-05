# ESP-Mosaico Board Manager adapter

This application-side component supplies display, touch, IMU, haptics and audio
for native Raylib Lite examples through ESP Board Manager 0.7.x. It supports
Mosaico hardware revisions v1.0, v1.1 and v1.2, selected from eFuse before
hardware initialization. It does not depend on the product BSP, Iris or Recovery.

## Build

From a Game project directory, with ESP-IDF supporting the preview ESP32-S31 target:

```sh
cd examples/mosaico_hop
idf.py bmgr -c ../boards/esp-mosaico/bmgr -b esp_mosaico
idf.py --preview -B /tmp/sky-hop-native -DIDF_TARGET=esp32s31 build
```

The [Board Manager profile](bmgr/esp_mosaico/board_info.yaml) and peripheral configuration live
in `examples/boards/esp-mosaico/bmgr/esp_mosaico`. The standard `bmgr`
command generates `components/gen_bmgr_codes` for that Game project. Re-run it
with the selected profile when changing Boards. Generated files are ignored by
Git. The old `RAYLIB_LITE_BSP_DIR` override is no longer used.

The package uses the underscore-style Board Manager ID `esp_mosaico`. Its
hyphen-style ESP-IDF adapter component lives directly in this Board directory; it adapts generated hardware handles to the Raylib
Lite Board API without creating a second top-level Board directory.

The default partition table contains a standalone factory game. The example
prints logs to UART0. A product wrapper can supply another partition table.
Use a fresh build directory when switching Boards so the target and Kconfig
values are regenerated for the selected Board.

## Display and ownership

The Game surface remains 480x480 RGB565; other logical dimensions return
`ESP_ERR_NOT_SUPPORTED`. Board Manager owns the CO5300 panel, panel IO, CST9220
touch, shared buses, power, ES8311 DAC, motor PWM and optional BMI270 device.
The existing `esp_display_present` double-buffered staging and
`mosaico_video` asynchronous strip worker remain the video backend.
Touch coordinates keep the existing unrotated BSP convention.

The panel factory consumes the retained bootloader's LP STORE15 marker. It
skips reset, Sleep Out, Display On and brightness changes only during initial
adoption. The driver's lifetime init table remains complete so later hardware
reset or deep-standby wake can restore the panel normally. No second splash
renderer or compatibility splash component is linked into the application.

Audio starts lazily on `InitAudioDevice()`; IMU starts only when requested by
the Game. Touch release events are retried when the input queue is full.
Cleanup joins audio and input workers, closes the strip backend, deletes the
presenter, then deinitializes Board Manager. Failed cleanup retains the Board
handle for retry. A successful destroy permits creating a new Board.
All Board lifecycle calls have one application-task owner; they are not
concurrent with rendering or each other.

## Vibe integration

Vibe's `mosaico.py game build mosaico_hop --target iris` supplies required Iris
application services and Recovery-compatible partitions. Its service component
starts Iris, attaches screenshot/input handlers, confirms health after the
first frame, and detaches before Board destruction. Hardware still belongs to
this Board Manager profile. Build and installation commands are maintained by Vibe.

See [build paths](../../../docs/build-matrix.EN.md) and
[Board porting](../../../docs/board-porting.EN.md). Host tests and successful
firmware builds do not replace device acceptance of startup-screen continuity,
input, sound, IMU, haptics and shutdown.
