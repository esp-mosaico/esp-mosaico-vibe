# Workspace and dependency migration

[简体中文](workspace-migration_CN.md) | [Documentation index](README.md)

Move or clone the whole workspace, including each user application's independent
Git repository and the pinned submodules. Hello World and blank projects reference
utils, BSP and cached Engine tooling relatively. New games resolve the runtime
from the Registry without a source override. Run `python mosaico.py game prepare`
after cloning; see [game migration](game-development.md#registry-runtime-and-pinned-development-tools)
for existing apps with source overrides. Build caches contain absolute paths;
configure fresh build directories after moving the workspace or changing the
integration layout. This workspace does not provide standalone application export.

## Current source layout

| Owner | Path |
| --- | --- |
| Product launcher, templates and build runner | `submodule/esp-mosaico-utils/mosaico-tools/` |
| Retained firmware and application adapter | `submodule/esp-mosaico-utils/esp-mosaico-vibe-mode/` |
| Vibe Mode firmware project | `esp-mosaico-vibe-mode/firmware/vibe-mode/` inside utils |
| Shared GSP acquisition and Ideas resources | `submodule/esp-mosaico-utils/shared/` |
| Product board/peripheral examples | `submodule/esp-mosaico-bsp/` |
| Game runtime | Registry `espressif2022/raylib-lite-engine` `0.1.1` |
| Complete games, native Board adapters and Host | Verified source cache reported by `game prepare` |

The old `esp-mosaico-recovery/` and `firmware/recovery/` source locations and legacy
forwarding tool entry points were removed. Update CMake includes, component paths,
automation and fixtures to the public [application integration contract](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/docs/application-integration.md).
Recovery component names, command names, protocol fields and ABI identifiers remain.
Do not replace every occurrence of `recovery` mechanically.

## Workspace configuration and applications

The workspace `.mosaico.json` uses `schema_version: 2`. Recreate/check its paths
against the committed configuration; the CLI rejects schema 1. Template descriptors
still use schema 1, which is independent. Keep project/device selections explicit;
creation does not change the default project.

New applications initialize Git on `main` and include shared quality configuration.
Existing applications are not rewritten automatically. Preserve their local changes
and integrate the current component structure and checks deliberately. Hello World
puts its scene/Host adapter in `components/app_hello/` and display policy in
`components/app_display/`. The blank game separates model and runtime components.
Engine example copies need the Iris wrapper before product installation; see
[game development](game-development.md).

## Device and Gateway compatibility

0.2 uses Gateway API 2, Iris wire version 2, Recovery ABI 2, the
`mosaico-retained-test-2m-v2` layout and `esp-iris-system-update/0.2` bundles.
The retained partition is `vibe_mode` (`app/test`); the application partition is
`main_app` (`app/ota_0`). Old bundles cannot be migrated by changing a version string.
Old 0.1 devices/Gateways require their original environment to preserve evidence
and finish operations. Host state is isolated under `esp-mosaico/0.2`; old accounts
and resumable upload state are not automatically imported.

Before base restoration, preserve logs, valid Core Dumps and required data, release
the old owner through its normal workflow, and follow [update selection](mosaico-cli.md#select-an-update-method).
Use `mosaico.py recover` for an unverified/incompatible base, then verify live identity,
Vibe Mode readiness and the intended application across reboots. Do not erase flash
or overwrite retained identity/credentials to bypass migration checks.
For existing 0.2 Gateway history, follow [project storage](project-gateway.md#project-history-and-evidence),
including independent state/credential backups before database upgrades.

The exact ESP-IDF revision and Python/target requirements remain those in
[AGENTS.md](../AGENTS.md). Source updates do not automatically replace reviewed
prebuilt firmware or update a connected device.
