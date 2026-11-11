# 工作区与依赖迁移

[English](workspace-migration.md) | [返回索引](README_CN.md)

移动或克隆整个工作区，包括各用户应用的独立 Git 仓库和固定版本子模组。生成工程
中的 Hello World 与空白模板通过相对路径引用依赖；引擎示例副本当前在
`main/idf_component.yml` 中写入绝对的引擎 `override_path`，移动后须更新该引用。
构建缓存含绝对路径，移动工作区或改变集成布局
后使用新的构建目录重新配置。本工作区不提供单应用独立导出。

## 当前源码布局

| 所属内容 | 路径 |
| --- | --- |
| 产品入口、模板、应用适配组件、公开 Recovery ABI 与 bootloader 集成 | `submodule/esp-mosaico-utils/mosaico-tools/` |
| 已审查的 Vibe Mode 预构建包 | `submodule/esp-mosaico-utils/firmware/recovery/` |
| 共享 GSP 获取与 Ideas 资源工具 | `submodule/esp-mosaico-utils/shared/` |
| 产品板级支持与外设示例 | `submodule/esp-mosaico-bsp/` |
| 完整游戏、原生 Board 适配器与 Host | `submodule/raylib-lite-engine/` |

当前 Utils 版本不包含 Vibe Mode 固件源码工程；`firmware/recovery/` 存放已审查的预构建包。
旧 `esp-mosaico-recovery/` 源码目录及转发工具入口已移除。根据
[公共集成契约](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/docs/application-integration.md)
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
