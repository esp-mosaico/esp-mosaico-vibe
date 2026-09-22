# ESP-Mosaico Vibe

[English](README.md) | [中文](README_CN.md)

[![CI](https://github.com/esp-mosaico/esp-mosaico-vibe/actions/workflows/ci.yml/badge.svg)](https://github.com/esp-mosaico/esp-mosaico-vibe/actions/workflows/ci.yml)

An Agent-led development workspace for ESP-Mosaico. Describe the application
and its expected behavior; the Agent develops it using the reference projects,
simulators, and ESP-Iris device tools. The user defines the goal, performs any
required physical actions, and accepts the result on the device.

## Start a project

Run these commands from the workspace root. Creating a project requires Python
3.8 or newer and the tools submodule, but no ESP-IDF installation or device:

```sh
git submodule update --init submodule/esp-mosaico-utils
python mosaico.py project init my_app
```

The command creates `projects/my_app` from the shared PC/device
[GSP Hello World](projects/hello_world/README.md) reference.
It preserves Recovery integration, refuses existing destinations, and leaves
the default project unchanged. Use `--dry-run` to preview the generated files.

Before building, initialize the required dependencies and resolve an ESP-IDF
installation that satisfies the application's `main/idf_component.yml` and
supports `esp32s31`:

```sh
git submodule update --init --recursive submodule/esp-mosaico-bsp submodule/esp-mosaico-utils
python mosaico.py doctor
```

On a blank or unverified device, run `python mosaico.py recover` first and verify
Recovery is ready. Then install the new application and inspect its logs:

```sh
python mosaico.py iris system-update --project projects/my_app
python mosaico.py iris logs --project projects/my_app --timeout 20
```

For subsequent code-only changes with an identical full partition table, use
`iris app-update`. New applications, changed layouts, and external resources
use `iris system-update`. See the [project guide](docs/project-init.zh-CN.md)
and [CLI reference](docs/mosaico-cli.zh-CN.md) for details.

## Develop and observe

- For device UI, start with [GSP Hello World](projects/hello_world/README.md) when
  GSP fits the application. [GSP simulation](tools/gsp-sim/README.md) uses
  `espressif/esp-gsp` 1.4.0 and supports shared PC/device UI logic.
- For Raylib-compatible games, start from the [game entry](docs/game-development.zh-CN.md)
  and follow [Raylib Lite Engine](submodule/raylib-lite-engine/README.md). This
  workspace does not keep in-tree game examples.
- For ongoing device observation, run `python mosaico.py iris run --project projects/my_app`
  and open the printed Gateway Web workbench URL. See the
  [Gateway guide](docs/project-gateway.zh-CN.md) for sessions and device ownership.

Use `mosaico.py` for device operations. Keep the retained Recovery path and
verify device identity, the intended firmware, and application health after
an update. Recovery procedures are linked from the CLI reference.

## Repository layout

| Location | Purpose |
| --- | --- |
| `projects/` | Reference applications and independent user applications |
| `components/`, `cmake/` | Workspace integration and normal-application Recovery contract |
| `tools/gsp-sim/` | GSP compiler and PC simulator integration |
| `tests/` | Host checks and flashable acceptance fixtures under `tests/firmware/` |
| `submodule/esp-mosaico-bsp/` | Board support and board examples |
| `submodule/esp-mosaico-utils/` | Product CLI in `mosaico-tools`, Recovery firmware, and ESP-Iris |
| `submodule/raylib-lite-engine/` | Game runtime, Host simulator, and asset tools |
| `.mosaico.json` | Workspace paths and supported-device configuration |
| `.agents/skills/`, `.agents/tools/` | Versioned Agent skills and auxiliary tools |
| `.agents/analysis/` | Ignored local analysis artifacts |
| `docs/` | Developer-facing guides and references |

Initialize only the submodules needed for the task. Agents follow
[AGENTS.md](AGENTS.md) and select workflows from the
[skill index](.agents/skills/README.md).

## Documentation

The [documentation index](docs/README.md) links project creation, device
operation, the [game entry](docs/game-development.zh-CN.md), and component
references. Detailed guides are currently in Chinese.
Host checks and firmware CI are described in the [CI guide](docs/ci.zh-CN.md).
