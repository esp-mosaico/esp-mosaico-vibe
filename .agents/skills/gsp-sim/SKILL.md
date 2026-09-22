---
name: gsp-sim
description: Preview generated GSP applications with the pinned GSP 1.4.0 sim_bridge and shared native C backend.
---

# GSP application preview

Read the [product preview guide](../../../submodule/esp-mosaico-utils/mosaico-tools/tools/gsp-sim/README.md).
Create an application with `python mosaico.py project init <name>`; the template
is owned by utils, and there is no pre-created Hello World in this workspace.

After the first application build/reconfigure resolves ESP-GSP 1.4.0:

```sh
python mosaico.py project sim --project projects/<name> --interactive
python mosaico.py project sim --project projects/<name> --headless --duration 3
```

The public command selects explicit --project, current application, valid user
default or the sole created app. No internal Recovery or template directory is
selected implicitly. Native sim_bridge executes the application's portable C UI;
keep board/FreeRTOS/Iris out of pc/. Preserve 480×480 RGB565 and the shared scene.

Use --scene-only or --dump-ppm PATH only for static rendering; they do not run C
callbacks. Validate clicks, held input, counters and timers using the native
backend before device work. Tool resolution and preview orchestration are owned
by mosaico-tools; do not copy them into the workspace or an application.

For device installation follow AGENTS.md: first install or changed resources
uses recover/system-update; code-only app-update requires a matching full layout.
