# ESP-Mosaico workspace documentation

[简体中文](README_CN.md)

- [Device modes: Vibe Mode and ROM Download Mode](device-modes.md)
- [Project creation and selection](project-init.md)
- [Reusing Vibe Mode settings](vibe-settings.md)
- [Espressif MCP services for coding agents](mcp-servers.md)
- [CLI and device operations](mosaico-cli.md)
- [Gateways and device ownership](project-gateway.md)
- [Game creation and simulation](game-development.md)
- [Simulator preview troubleshooting](simulator-preview.md)
- [Workspace, dependency and 0.2 migration](workspace-migration.md)

Public implementations, APIs and protocol documentation are maintained by their
owning repositories:

- [Utils component boundaries](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/docs/component-boundaries.md)
- [Public application interfaces](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/docs/application-integration.md)
- [Hello World template](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/templates/hello_world/README.md)
- [GSP preview](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/tools/gsp-sim/README.md)
- [Vibe Mode firmware](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/esp-mosaico-vibe-mode/firmware/vibe-mode/README.md)
- [BSP examples](https://github.com/esp-mosaico/esp-mosaico-bsp/blob/master/README.md#examples)
- [Raylib Lite Engine source reference (Registry 0.1.1)](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/README.md)

- [Ideas file releases](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/docs/ideas-file-release/README.zh-CN.md)
- [Gateway storage and evidence](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/ESP-Iris/components/esp_iris/tools/README.md#runtime-and-storage-model)
- [BSP expansion modules: ToF, thermal camera, matrix LED and EEPROM provisioning](https://github.com/esp-mosaico/esp-mosaico-bsp/blob/master/README.md#expansion-modules-and-provisioning)

Initialize the pinned utils/BSP submodules for their local implementation docs.
Raylib uses a Registry component; obtain its pinned Host tools and technical docs
with `python mosaico.py game prepare`, and examples with
`game prepare --example <name>`. See the [game entry](game-development.md) for the
versioned technical reading path. Other upstream links may follow default branches.
