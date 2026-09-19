# ESP-Mosaico Hello World

The application shows `Hello World!` in the center of the 480x480 display and
logs the same message every five seconds. It is the reference normal
application: new projects should preserve its partition layout,
`sdkconfig.application.defaults`, and `esp_mosaico_app_recovery` integration.

Install it from the repository root after the device Recovery image has been
initialized and verified:

```sh
python mosaico.py iris app-update --project projects/hello_world
```

To build and install an atomic System Update containing the application,
bootloader, and partition table, use:

```sh
python mosaico.py iris system-update --project projects/hello_world
```

Use `python mosaico.py iris logs --timeout 20` to observe the periodic log. USB
and firmware operations remain owned by ESP-Iris; do not open the serial port
directly.
