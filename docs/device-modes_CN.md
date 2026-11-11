# 设备模式

[English](device-modes.md) | [返回文档索引](README_CN.md)

**Vibe Mode** 是 ESP-Mosaico 内置的应用安装与设备维护模式。它运行 ESP-Iris，
提供应用更新、Wi-Fi 配置和 **Download Ideas**。Download Ideas 通过网络获取应用，
不会使芯片进入 ROM Download Mode。

| 模式 | 运行内容 | 使用场景 |
| --- | --- | --- |
| 正常应用 | 用户应用；集成并启动后可使用 ESP-Iris | 运行、调试和验收应用 |
| Vibe Mode | Flash 的 `vibe_mode`（`app/test`）分区中的保留固件，包含 ESP-Iris | 安装、更新应用和维护设备 |
| ROM Download Mode（ROM 下载模式） | 芯片内置的固件下载程序，不运行 ESP-Iris | 空白设备初始化、基础固件无法工作时恢复 |

Agent 可连接运行 ESP-Iris 的正常应用或 Vibe Mode。离线、握手失败不代表设备
处于 ROM 模式或固件损坏；先检查归属、活动操作及连接，遵循
[实时证据规则](project-gateway_CN.md#实时证据与下一步)。

## 选择操作

`python mosaico.py iris test enter-recovery` 从可达的正常应用进入已有
Vibe Mode，不安装固件。应用更新命令会自动管理这一切换。

`python mosaico.py recover` 写入基础固件，再验证 Vibe Mode 就绪。
它可从已连接设备开始执行，仅在流程要求时手动进入 ROM。初始化条件及应用
更新方式统一见[更新方式](mosaico-cli_CN.md#选择更新方式)，Wi-Fi 和 Download Ideas
命令见[CLI 参考](mosaico-cli_CN.md#命令职责)。

## 物理入口

- **Vibe Mode — AI 键（GPIO7）：** 已安装有效的 Mosaico bootloader 和保留固件时，
  按住 AI 键上电，待 Vibe Mode 启动后松开。bootloader 仅为本次启动选择
  `vibe_mode`（`app/test`），不修改 OTA 启动选择。
- **ROM Download Mode — Boot 键（GPIO61）：** 仅当 `recover` 要求手动进入时，
  先关机，按住 USB-C 接口左侧的 Boot 键，再保持按住并开机。
  进入 ROM 下载模式后松开 Boot 键，告知 Agent 物理操作完成，由 Agent
  核实 ROM 连接并继续 `recover`。此时 Vibe Mode 和 ESP-Iris 均不运行。

这是两个不同的按键和启动入口。通过 AI 键临时进入后，松开 AI 键再次重启会沿用
有效的原 OTA 启动选择。应用更新会选择并启动新应用，随后须验收身份、健康状态
和目标行为；ROM 恢复完成后，先确认 Vibe Mode 就绪，再安装目标应用。

## Ideas 与固件维护

Download Ideas 输入六位应用编号，保留开头的零；先查看名称、图标、描述和兼容
版本，再确认下载到 NAND。下载与设为开机应用是两个操作。My Ideas 管理本地
下载；About 提供需要确认的 Vibe Mode 在线固件更新。旧 Bridge 配对流程是独立
的维护接口。工作区使用的已审查基础固件包与 ROM 恢复流程见
[Utils 指南](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/README.md#rom-recovery-operations)。

0.2 产品使用 Recovery ABI 2 和保留 test 分区布局；普通应用位于 `main_app`
（`app/ota_0`）。当前固定的 Utils 版本包含已审查的 Vibe Mode 预构建包，不含固件源码工程。
旧设备或旧工作区使用当前工具前，先阅读[迁移说明](workspace-migration_CN.md)。
