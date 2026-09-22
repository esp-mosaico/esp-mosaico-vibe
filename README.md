# ESP-Mosaico Vibe

[简体中文](README_CN.md)

A get-started workspace for developing ESP-Mosaico applications with an AI coding
agent. The repository supplies the CLI entry, workspace settings, pinned
submodules, documentation and Agent guidance. Applications are created on demand;
a fresh checkout has no `projects/` or `components/` directory.

## Create an application

Python 3.8 or newer is sufficient for creation; ESP-IDF, a board, BSP and the game
engine are not required yet.

```sh
git submodule update --init submodule/esp-mosaico-utils
python mosaico.py project init my_app
```

This creates `projects/my_app` from the utils-owned Hello World template without
changing the default project. `--dry-run` writes nothing; existing targets are
never overwritten. See [project creation](docs/project-init.zh-CN.md).

## Preview and install

```sh
git submodule update --init submodule/esp-mosaico-bsp
# Prepare a compatible ESP-IDF environment and build the generated application.
python submodule/esp-mosaico-utils/mosaico-tools/skills/idf-low-noise-build/scripts/idf_low_noise_build.py --project projects/my_app doctor
python submodule/esp-mosaico-utils/mosaico-tools/skills/idf-low-noise-build/scripts/idf_low_noise_build.py --project projects/my_app build
python mosaico.py project sim --project projects/my_app --interactive
```

GSP preview uses the same portable C UI and GSP 1.4.0 scene as the device.
For the first installation on a blank or unverified board, run `python mosaico.py recover`,
then `python mosaico.py iris system-update --project projects/my_app`.
Use `iris app-update` only for code changes with the same complete partition table
and resources. Device operations always go through the product CLI.

## Workspace ownership

| Repository | Maintains |
| --- | --- |
| This workspace | Entry, configuration, Agent workflows, consumer integration checks |
| [utils](submodule/esp-mosaico-utils) | CLI, Recovery, shared application components and Hello World template |
| [BSP](submodule/esp-mosaico-bsp) | Board support and complete examples, including games |
| [Raylib Lite Engine](submodule/raylib-lite-engine) | Game runtime, renderer, assets and Host simulator |

Games are created from BSP `examples/`; see the [game entry](docs/game-development.zh-CN.md).
Initialize the engine only when developing games. Generated applications use
relative references: move or clone the whole workspace, initialize its pinned
dependencies and rebuild. Single-application export and old workspace path
compatibility are outside this layout. See [migration notes](docs/workspace-migration.zh-CN.md).

Start with the [documentation index](docs/README.md). Agents follow [AGENTS.md](AGENTS.md)
and the [skill index](.agents/skills/README.md). For ongoing observation,
`python mosaico.py iris run --project projects/my_app` prints the Gateway Web
workbench URL; see the [Gateway guide](docs/project-gateway.zh-CN.md).
