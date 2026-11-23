# Game creation and simulation

[简体中文](game-development_CN.md) | [Documentation index](README.md)

Prefer **Raylib Lite Engine** (`submodule/raylib-lite-engine`) for games. It owns
complete reference games, runtime, rendering, assets, behavior tests and Host
simulation. BSP owns product board support and peripheral examples. Native engine
examples use their own Board Manager adapters; a native build alone does not add
Iris or the retained Vibe Mode layout.

## Start with a blank game

Use Python 3.10+ and Git 2.28+. Initialize the workspace dependencies:

```sh
git submodule update --init submodule/esp-mosaico-utils submodule/esp-mosaico-bsp submodule/raylib-lite-engine
python mosaico.py game create my_game
python mosaico.py game sim --project projects/my_game
python mosaico.py game sim --project projects/my_game --headless --frames 120
python mosaico.py game build --project projects/my_game
python mosaico.py iris system-update --project projects/my_game
```

The default `game build` compiles the firmware. For this Iris-ready project,
`iris system-update --project` builds the `.irisfw` bundle and installs it.

The default `blank` template starts with a black 480×480 RGB565 surface. It
includes Host/device entry points, Iris services and the retained layout, with
no example gameplay, GSP canvas placeholder, atlas, audio or game resource partition.
Firmware uses the workspace's pinned ESP-IDF; provision a blank or unverified
board first as described in [update selection](mosaico-cli.md#select-an-update-method).

| Path | Responsibility |
| --- | --- |
| `components/app_game/` | Pure C model and gameplay state |
| `components/app_game_runtime/` | Engine lifecycle/input adapter, drawing and JSON |
| `main/game_entry.c` | Select the game module |
| `game.sim.json` | Sources shared by Host and device |

Dependencies flow `main -> app_game_runtime -> app_game`. The model does not
depend on drawing or hardware. Host state includes `phase`, `tick`, pointer
coordinates/pressed state and `state_hash`. Pause, resume and reset work before
adding gameplay; use `--state-output state.json` to save a headless run's state.

## Start with an engine example

`game create/new --template` offers `blank`, `shooter`, `sky-hop`,
`tower-defense`, `last-zone`, `living-worlds`, and `neon-rift-rally`.
On first use, the CLI uses the configured ESP-IDF to download a released
example from the Component Registry and then copies it into a new project.
Use `python mosaico.py game prepare --example sky-hop` to populate the cache
before creating a project. This workspace's `dependencies.raylib` setting
selects local Engine sources for the Host and the blank template.

```sh
python mosaico.py game create my_platformer --template sky-hop
python mosaico.py game sim --project projects/my_platformer
python mosaico.py game build --project projects/my_platformer --target iris
```

Install from the game project directory so the CLI selects its Gateway. Use the
absolute bundle path printed by the build.

```sh
(
    cd projects/my_platformer &&
    python ../../mosaico.py iris system-update \
        --bundle "/absolute/path/to/my_platformer-system-update.irisfw"
)
```

`game build` defaults to `--target native`, which builds the source as supplied.
`--target iris` adds product services and compatible partitions in a generated
project under the workspace run directory, then emits a System Update bundle.
It also accepts a named Registry example, such as `game build sky_hop --target iris`.
Build does not install firmware. Native and Iris outputs use separate directories.
The wrapper references `main/` and optional `components/`. It generates its own
Board Manager configuration, partition table and top-level CMake project. Declare
resources in component CMake files. See the [integration contract](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/docs/application-integration.md#user-owned-raylib-lite-games).

All creators support `--dry-run`, refuse existing targets and initialize an
independent Git repository plus quality configuration. See [project creation](project-init.md).

## Board Manager configuration

`mosaico.py game build` generates missing `components/gen_bmgr_codes` before
configuring the application. The blank template uses the selected Engine's
`esp_mosaico` profile. Registry examples use their bundled Board profile. The
Iris wrapper has its own generated configuration. Product builds validate and
preserve existing configuration. If the CLI reports an incomplete or mismatched
component, back it up outside `components/` before retrying the build. The
`game build --target iris` console output shows the generated bundle path.

## Validate behavior

The Host needs a C compiler and Pillow; install NumPy when using the engine's
texture/asset conversion tools. Interactive simulation uses the same C model and
RGB565 drawing as the device. Check animation, pointer feedback, collisions,
scoring, win/loss and restart behavior as relevant. Replay recorded input
headlessly for repeatable checks; inspect visuals and re-run affected flows after
fixes. A successful launch or fixed frame count does not establish gameplay quality.
Scene, UI, save and audio engine modules are not all covered by the current Host.
Validate physical controls, touch, display, audio and performance on hardware;
Host timing is not device FPS.

- [Engine examples and supported boards](https://github.com/espressif2022/raylib-lite-engine/blob/main/examples/README.md)
- [Sky Hop](https://github.com/espressif2022/raylib-lite-engine/blob/main/examples/sky_hop/README.md)
- [Tower Defense](https://github.com/espressif2022/raylib-lite-engine/blob/main/examples/tower_defense/README.md)
- [Raylib Shooter](https://github.com/espressif2022/raylib-lite-engine/blob/main/examples/raylib_shooter/README.md)
- [Game development guide](https://github.com/espressif2022/raylib-lite-engine/blob/main/docs/game-development.EN.md)
- [ESP-Mosaico Board Manager adapter](https://github.com/espressif2022/raylib-lite-engine/blob/main/examples/boards/esp-mosaico/README.md)

In this workspace, install and diagnose devices through `mosaico.py`; standalone
example flash/monitor instructions do not replace the product workflow.
