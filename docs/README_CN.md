# ESP-Mosaico 工作区文档

[English](README.md)

- [设备模式：Vibe Mode 与 ROM 下载模式](device-modes_CN.md)
- [工程创建与选择](project-init_CN.md)
- [应用复用 Vibe Mode 设置](vibe-settings_CN.md)
- [编程 Agent 的 Espressif MCP 服务](mcp-servers_CN.md)
- [CLI 与设备操作](mosaico-cli_CN.md)
- [Gateway 与设备归属](project-gateway_CN.md)
- [游戏创建与仿真](game-development_CN.md)
- [仿真预览连接排查](simulator-preview_CN.md)
- [工作区、依赖与 0.2 迁移](workspace-migration_CN.md)

公共实现、API 和协议资料由所属仓库维护：

- [utils 组件边界](../submodule/esp-mosaico-utils/docs/component-boundaries.md)
- [应用公共接口](../submodule/esp-mosaico-utils/mosaico-tools/docs/application-integration.md)
- [Hello World 模板](../submodule/esp-mosaico-utils/mosaico-tools/templates/hello_world/README.md)
- [GSP 预览](../submodule/esp-mosaico-utils/mosaico-tools/tools/gsp-sim/README.md)
- [Vibe Mode 固件](../submodule/esp-mosaico-utils/esp-mosaico-vibe-mode/firmware/vibe-mode/README.md)
- [BSP 示例](../submodule/esp-mosaico-bsp/README.md#examples)
- [Raylib Lite Engine 源码参考（Registry 0.1.1）](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/README.md)

- [Ideas 文件发布](../submodule/esp-mosaico-utils/mosaico-tools/docs/ideas-file-release/README.zh-CN.md)
- [Gateway 存储与证据](../submodule/esp-mosaico-utils/ESP-Iris/components/esp_iris/tools/README.md#runtime-and-storage-model)
- [BSP 扩展模块：ToF、热成像、矩阵 LED 与 EEPROM 配置](../submodule/esp-mosaico-bsp/README.md#expansion-modules-and-provisioning)

utils/BSP 的本地实现文档随固定版本子模块获取。Raylib 使用 Registry 组件，
运行 `python mosaico.py game prepare` 获取固定版本 Host 工具与技术文档，
`game prepare --example <name>` 获取示例。版本对应的技术阅读入口见[游戏开发](game-development_CN.md)；
其他上游链接可能指向默认分支。
