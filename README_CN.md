# ESP-Mosaico Vibe

[English](README.md) | [中文](README_CN.md)

[![CI](https://github.com/esp-mosaico/esp-mosaico-vibe/actions/workflows/ci.yml/badge.svg)](https://github.com/esp-mosaico/esp-mosaico-vibe/actions/workflows/ci.yml)

面向 ESP-Mosaico 的 Agent 主导开发工作区。描述应用目标和预期行为，Agent 使用
参考工程、模拟器和 ESP-Iris 设备工具完成开发。用户负责定义目标、执行必要的
物理操作，并验收真机结果。

## 创建工程

以下命令均在工作区根目录执行。创建工程只需要 Python 3.8 或更新版本和工具
子模块，无需先安装 ESP-IDF 或连接设备：

```sh
git submodule update --init submodule/esp-mosaico-utils
python mosaico.py project init my_app
```

命令以 PC/设备共用的 [GSP Hello World](projects/hello_world/README.md) 为模板
创建 `projects/my_app`，
保留 Recovery 接入，拒绝覆盖已有目录，不更改默认工程。使用 `--dry-run` 预览
将生成的文件。

构建前初始化所需依赖，并解析满足应用 `main/idf_component.yml` 约束、支持
`esp32s31` 的 ESP-IDF 环境：

```sh
git submodule update --init --recursive submodule/esp-mosaico-bsp submodule/esp-mosaico-utils
python mosaico.py doctor
```

空白或未经验证的设备须先运行 `python mosaico.py recover`，确认 Recovery 就绪。
然后安装新应用并查看日志：

```sh
python mosaico.py iris system-update --project projects/my_app
python mosaico.py iris logs --project projects/my_app --timeout 20
```

后续仅修改代码且完整分区表一致时使用 `iris app-update`；新应用、分区布局或
外部资源变化使用 `iris system-update`。详见[工程初始化指南](docs/project-init.zh-CN.md)
与 [CLI 命令参考](docs/mosaico-cli.zh-CN.md)。

## 开发与观察

- 设备 UI：适合使用 GSP 时，从 [GSP Hello World](projects/hello_world/README.md)
  开始。[GSP 仿真工具](tools/gsp-sim/README.md) 使用 `espressif/esp-gsp` 1.4.0，
  支持 PC 与设备共享 UI 逻辑。
- 持续观察设备：执行 `python mosaico.py iris run --project projects/my_app`，
  打开输出中的 Gateway Web 工作台地址。会话和设备归属见
  [Gateway 指南](docs/project-gateway.zh-CN.md)。

设备操作统一通过 `mosaico.py`。保留 Recovery 路径，更新后验证设备身份、目标
固件和应用健康状态。恢复流程从 CLI 命令参考进入。

## 仓库结构

| 位置 | 职责 |
| --- | --- |
| `projects/` | 参考应用和独立的用户应用 |
| `components/`、`cmake/` | 工作区集成及普通应用 Recovery 契约 |
| `tools/gsp-sim/` | GSP 编译器与 PC 模拟器集成 |
| `tests/` | 主机检查；可烧录验收固件放在 `tests/firmware/` |
| `submodule/esp-mosaico-bsp/` | 板级支持与板级示例 |
| `submodule/esp-mosaico-utils/` | `mosaico-tools` 产品 CLI、Recovery 固件和 ESP-Iris |
| `submodule/raylib-lite-engine/` | 游戏运行时、Host 模拟器和资源工具 |
| `.mosaico.json` | 工作区路径和设备配置 |
| `.agents/skills/`、`.agents/tools/` | 纳入版本控制的 Agent 技能和辅助工具 |
| `.agents/analysis/` | 忽略的本地分析产物 |
| `docs/` | 面向开发者的指南和参考文档 |

只初始化当前任务所需的子模块。Agent 遵循 [AGENTS.md](AGENTS.md)，从
[技能索引](.agents/skills/README.md)选择工作流。

## 文档入口

[文档索引](docs/README.md)汇总工程创建、设备操作和组件参考。
详细指南目前以中文维护。主机检查和固件 CI 见[持续集成指南](docs/ci.zh-CN.md)。
