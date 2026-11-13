# Mosaico game development entry

[简体中文](game-development_CN.md) | [Documentation index](README.md)

Prefer **Raylib Lite Engine** (`espressif2022/raylib-lite-engine`, pinned to `0.1.1`) for games. It owns
complete reference games, runtime, rendering, assets, behavior tests and Host
simulation. BSP owns product board support and peripheral examples. Native engine
examples use their own Board Manager adapters; a native build alone does not add
Iris or the retained Vibe Mode layout.

## Reading path and version

This page owns **creation, simulation, builds and installation in Mosaico**.
Engine documents below own game architecture and API details; the skill keeps
execution constraints rather than repeating the full workflow.

The runtime is pinned to `0.1.1`. Tools and these technical references use the
revision recorded in utils' `raylib-toolchain.json`. They remain usable without
a component update. Maintain documentation in its owning repository, not the tool cache.

| Task | Technical reference |
| --- | --- |
| Structure model/view/feedback/module code and declare `game.sim.json` | [Game development](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/docs/game-development.EN.md) |
| Choose rendering paths and design inputs, feedback and asset manifests | [Reusable designs](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/docs/reference-designs.EN.md) |
| Check threads, lifecycle, frame ownership, input, audio and saves | [Public API contract](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/API.md) |
| Choose public, low-level or Raylib-compatible interfaces | [API usage boundaries (Chinese)](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/docs/custom-api.CN.md) |
| Implement Scene lifecycle, hit testing, multiple pointers and focus | [Scene/UI contract (Chinese)](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/docs/scene-ui.CN.md) |
| Configure recorded input, state output and Host options | [Host reference](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/host/README.md) |
| Select a complete reference game | [Examples and board matrix](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/examples/README.md) |

These are architecture and API references, not a complete step-by-step tutorial.
Create a project below, then consult the relevant reference. Use the `mosaico.py`
commands on this page in place of the Engine's standalone CLI workflow;
installation and recovery follow the [product CLI](mosaico-cli.md).
Run `python mosaico.py game prepare --json` to locate the tool cache; offline,
read the same documents and `include/` headers there.

## Start with a blank game

Use Python 3.10+ and Git 2.28+. Initialize utils and BSP; the Engine is an ESP-IDF Component Registry dependency:

```sh
git submodule update --init submodule/esp-mosaico-utils submodule/esp-mosaico-bsp
python mosaico.py game prepare
python mosaico.py game create my_game
python mosaico.py game sim --project projects/my_game
python mosaico.py game sim --project projects/my_game --headless --frames 120
# First builds download dependencies and generate Board Manager configuration automatically.
python mosaico.py game build --project projects/my_game
python mosaico.py iris system-update --project projects/my_game
```

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

This table locates the blank template's code; use the technical references above
for reference-game model/view/feedback architecture. Read inherited brightness,
Wi-Fi and language preferences in the device adapter as described in
[settings reuse](vibe-settings.md).

## Registry runtime and pinned development tools

New games declare `espressif2022/raylib-lite-engine: "==0.1.1"` without
`override_path`. IDF downloads the firmware runtime into `managed_components/`.
The component does not ship the Host simulator. `game prepare` downloads the
matching Host and blank-game launcher/Board helpers from a pinned
source archive, verifies SHA-256, and caches it under the workspace run directory.
The version, revision and digest live in utils' `mosaico-tools/raylib-toolchain.json`.
The cached runtime sources used by Host match the Registry component's sources.
Subsequent runs reuse the verified cache offline; first use requires network access.
`game create`, `game sim` and `game build` prepare the cache automatically.
A blank `game create --dry-run` does not download it. `game prepare --json` reports
its exact path for automation and Board Manager generation.

For Engine development only, set `dependencies.raylib` in `.mosaico.json` to your
source checkout; an invalid explicit path is an error, never a fallback. This
selects Host and blank-game tooling, not Registry examples or the firmware runtime. To compile a modified
runtime, explicitly add `override_path` to the app's Engine manifest dependency.
Normal applications retain the exact Registry pin.

Existing games are not rewritten automatically. Remove their Engine
`override_path`, pin `==0.1.1`, point `RAYLIB_LITE_ENGINE_ROOT` at the prepared
toolchain (prefer the CLI's environment value), and regenerate Board Manager
files against that path. Do not erase old builds or source checkouts to migrate;
use a separate build directory if stale paths prevent reconfiguration.

## Start with an engine example

`game create/new` supports `shooter`, `sky-hop`, `tower-defense`, `last-zone`,
`living-worlds` and `neon-rift-rally`, plus `blank`. Examples are downloaded from
Registry version `0.1.1` with their private `shared/` launcher and Board, then
copied into the user project. The first download needs the pinned ESP-IDF
environment (`IDF_PATH`, or `--idf-path`); a cached example needs no SDK or network
for creation. Both blank and example creation dry runs do not download anything.

To download and locate an example independently, the upstream command is:

```sh
# Run in the directory that should contain last_zone_extraction/.
idf.py create-project-from-example "espressif2022/raylib-lite-engine=0.1.1:last_zone_extraction"
```

The workspace equivalent downloads into a versioned cache and reports its path:

```sh
python mosaico.py game prepare --example last_zone_extraction --json
```

`game build last_zone_extraction --target iris` and named-game simulation resolve
this Registry example cache, never a similarly named source-tree example.
Use explicit project paths to work on your own copies.

```sh
python mosaico.py game create my_platformer --template sky-hop
python mosaico.py game sim --project projects/my_platformer
python mosaico.py game build --project projects/my_platformer --target iris
# Use the exact bundle path printed by the build:
python mosaico.py iris system-update --project projects/my_platformer --bundle /path/to/my_platformer-system-update.irisfw
```

`game build` defaults to `--target native`, which builds the source as supplied.
`--target iris` adds product services and compatible partitions in a generated
project under the workspace run directory, then emits a System Update bundle.
It also accepts a named engine game, such as `game build sky_hop --target iris`.
Build does not install firmware. Native and Iris outputs use separate directories.
The wrapper references `main/` and optional `components/`; it does not use the
source's top-level partition table or custom top-level CMake logic. Declare
resources in component CMake files. See the [integration contract](https://github.com/esp-mosaico/esp-mosaico-utils/blob/ac32a5a79a46f24a3495cb17078529025d701d3a/mosaico-tools/docs/application-integration.md#user-owned-raylib-lite-games).

All creators support `--dry-run`, refuse existing targets and initialize an
independent Git repository plus quality configuration. See [project creation](project-init.md).

Named examples used by native `game build`, `game sim` and `game run` use the
shared Registry example cache as scratch space, including build/configuration
output. Run operations on the same named example serially. For concurrent or
editable work, create separate projects with `game create`; Iris builds use
separate wrapper directories.

## Automatic Board Manager preparation

`game build` generates missing `components/gen_bmgr_codes` before compiling. No
initial failed build or global `esp-bmgr-assist` installation is needed. Tools
resolve pinned Board Manager `0.7.5` in a separate minimal IDF project, run
`idf.py bmgr`, validate the output and then publish the component. Failures keep
complete logs and do not edit application sdkconfig.

Blank games use `examples/boards/esp-mosaico/bmgr` in the pinned tool cache;
Registry examples use their own `shared/boards/esp-mosaico/bmgr`. `--target iris`
generates an independent `esp_mosaico` configuration in the wrapper and excludes
the original game's `components/gen_bmgr_codes`. It does not change the original
Board selection, generated files or build configuration. First preparation needs
network access; complete existing configurations are reused.

Native builds preserve an existing Board selection. Iris product builds check
the Board, chip, configuration source and required files. Incomplete or mismatched
existing configurations fail explicitly instead of being overwritten. To regenerate,
preserve the old `gen_bmgr_codes` outside `components/` and repeat the build;
do the same after editing Board YAML.

Keep normal console output when building: this revision's successful
`game build --json` does not emit a result object or bundle path. Installation
remains a separate `mosaico.py` operation.

Default product game builds compile the application and resources without rebuilding
the retained base bootloader. Explicit bootloader builds and compatibility checks
remain available; partition offsets are unchanged.

## Simulator and device checks

Host needs a C compiler and Pillow; texture/asset conversion tools also need
NumPy. Run the shared C model and RGB565 drawing rather than separate browser
game logic. Exercise relevant input, scoring, collisions, pause and restart;
use recorded input for repeatable checks. A fixed frame count alone does not
establish interactive behavior.

Check actual browser clicks, keyboard input and coordinates at the displayed size.
See [preview troubleshooting](simulator-preview.md) for local/SSH connections.
Host does not cover every Scene, UI, save or audio module. Validate touch,
display, audio and performance through the [device evidence workflow](project-gateway.md#live-evidence-and-next-steps);
Host timings are not device FPS.
