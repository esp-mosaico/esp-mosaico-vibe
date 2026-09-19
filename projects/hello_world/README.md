# ESP-Mosaico Hello World

The application shows `Hello World!` in the center of the 480x480 display and
logs the same message every five seconds. It is the reference normal
application: new projects should preserve its immutable Recovery partition prefix,
the shared normal-application configuration, and `esp_mosaico_app_recovery` integration.

Install it from the repository root after the device Recovery image has been
initialized and verified. Prefer a System Update for a new application or a
changed partition layout; this bundle preserves Recovery and its bootloader:

```sh
python mosaico.py iris system-update --project projects/hello_world
```

For code-only updates with a full partition table identical to the device:

```sh
python mosaico.py iris app-update --project projects/hello_world
```

Use `python mosaico.py iris logs --timeout 20` to observe the periodic log. USB
and firmware operations remain owned by ESP-Iris; do not open the serial port
directly.
