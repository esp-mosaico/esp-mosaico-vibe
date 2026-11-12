# 工作区与依赖迁移

[English](workspace-migration.md) | [返回索引](README_CN.md)

移动或克隆整个工作区，包括用户应用的独立 Git 仓库和固定的 utils/BSP 子模块。
游戏运行时改由 Registry 组件提供，Host 与 Board 工具运行 `python mosaico.py game prepare`
下载并校验。旧游戏的 `override_path` 和工具路径按[游戏迁移](game-development_CN.md#registry-运行时与固定版本开发工具)
更新。构建缓存含绝对路径，迁移工作区后使用新的构建目录；此布局不提供单应用独立导出。

## 当前源码布局

| 所属内容 | 路径 |
| --- | --- |
| 产品入口、模板与构建脚本 | `submodule/esp-mosaico-utils/mosaico-tools/` |
| 保留固件与应用适配组件 | `submodule/esp-mosaico-utils/esp-mosaico-vibe-mode/` |
| Vibe Mode 固件工程 | utils 内的 `esp-mosaico-vibe-mode/firmware/vibe-mode/` |
| 共享 GSP 获取与 Ideas 资源工具 | `submodule/esp-mosaico-utils/shared/` |
| 产品板级支持与外设示例 | `submodule/esp-mosaico-bsp/` |
| 游戏运行时 | Registry `espressif2022/raylib-lite-engine` `0.1.1` |
| Host、空白游戏启动器与 Board 工具 | `game prepare --json` 返回的已校验源码缓存 |
| 发布游戏及其自带启动器/Board 适配器 | `game prepare --example <name> --json` 返回的 Registry 示例缓存 |

旧 `esp-mosaico-recovery/`、`firmware/recovery/` 源码目录及转发工具入口已移除。
根据[公共集成契约](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/docs/application-integration.md)
更新 CMake include、组件路径、自动化脚本和测试夹具。Recovery 组件名、命令、协议字段
与 ABI 标识继续保留，不要机械替换所有 `recovery` 字符串。

## 工作区配置与应用

工作区 `.mosaico.json` 使用 `schema_version: 2`，应按已提交配置重新核对路径，
CLI 拒绝 schema 1。模板描述仍使用 schema 1，两者独立。显式选择项目和设备；
创建应用不会改变默认工程。

新工程初始化分支为 `main` 的 Git 仓库并附带共享质量配置，已有工程不会自动改写。
保留其本地修改，再按需要接入组件结构和检查。Hello World 的场景/Host 适配器位于
`components/app_hello/`，显示策略位于 `components/app_display/`；空白游戏拆分
模型与运行时组件。引擎示例副本安装到产品前需要 Iris 适配构建，见[游戏开发](game-development_CN.md)。

## 设备与 Gateway 兼容性

0.2 使用 Gateway API 2、Iris wire version 2、Recovery ABI 2、
`mosaico-retained-test-2m-v2` 布局和 `esp-iris-system-update/0.2` 更新包。
保留分区为 `vibe_mode`（`app/test`），应用分区为 `main_app`（`app/ota_0`）。
不能通过修改版本字符串迁移旧包。旧 0.1 设备/Gateway 须用原环境保存证据并结束操作；
主机状态隔离在 `esp-mosaico/0.2`，旧账户和续传记录不会自动导入。

恢复基础固件前，保存日志、有效 Core Dump 和必要数据，按原工作流释放旧归属，再按
[更新方式](mosaico-cli_CN.md#选择更新方式)操作。对未验证/不兼容基础固件使用
`mosaico.py recover`，随后核对实时身份、Vibe Mode 就绪及重启后的目标应用行为。
不得通过全片擦除或覆盖保留身份/凭据来绕过迁移检查。已有 0.2 Gateway 历史迁移见
[项目存储](project-gateway_CN.md#项目历史与证据)，数据库升级前须独立备份状态与凭据。

精确 ESP-IDF 提交及 Python/芯片目标要求仍以 [AGENTS.md](../AGENTS.md) 为准。
更新源码不会自动替换已审查预构建包，也不会更新已连接设备。
