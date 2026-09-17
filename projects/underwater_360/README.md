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
