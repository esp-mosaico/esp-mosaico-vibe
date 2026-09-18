# Abyss 360

A shared-source RGB565 underwater panorama for ESP-Mosaico and the browser Host
simulator. Drag horizontally to turn through the complete 360-degree scene and
drag vertically to look toward the surface or seabed. After three idle seconds,
the camera slowly resumes its automatic tour.

The example uses the current raylib-lite 2D surface to build a deterministic
2.5D scene: perspective seabed bands, a yaw-relative grid, depth-sorted fish,
rays, jellyfish, coral, bubbles and caustic light. It does not depend on
OpenGL, Camera3D, shaders, PNG decoding or a project-specific PC host.

```sh
python3 mosaico.py game sim projects/underwater_360
python3 mosaico.py game sim projects/underwater_360 --headless --frames 300
python3 mosaico.py game build projects/underwater_360
```

The Host and device compile the same `underwater_world.c` and
`underwater_view.c`. The browser only presents the RGB565 framebuffer and
forwards pointer input; the device submits that framebuffer to its GSP Canvas.

## Device performance baseline

Baseline captured on 2026-09-18 from an ESP-Mosaico ESP32-S31 at 480x480,
using the default `FX LIVING` setting and a 30 Hz target. The flashed sources
were main repository commit `2b9b0f1` and raylib-lite-engine commit `763c169`.

Each scene was selected through the ESP-Iris pointer RPC and observed for at
least 14 seconds with `mosaico.py monitor`. The initial JPEG/atlas loading
samples after a scene change were excluded; the ranges below are steady-state
device measurements during the normal idle camera tour.

| Scene | Logic rate | Display rate | Render time |
| --- | ---: | ---: | ---: |
| Jungle | 29.6-30.9 Hz | 26.6-27.0 FPS | 36.3-36.5 ms |
| Aurora | 29.9 Hz | 16.4 FPS | 59.4-59.7 ms |
| Sunrise | 29.2-29.4 Hz | 11.8-12.5 FPS | 77.6-84.0 ms |
| Ocean | 26.9-28.0 Hz | 9.0-9.4 FPS | 102.5-105.6 ms |

All retained samples reported `dropped=0`, `busy=0`, `superseded=0`,
`errors=0`, and `overflow=0`. Use the steady-state display rate and render
time as the primary comparison values for later rendering optimizations.
