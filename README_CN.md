# ESP-Mosaico Vibe

[English](README.md) | [中文](README_CN.md)

本仓库是 ESP-Mosaico 开发版进行 vibe coding 的统一起点，为开发者和编码
Agent 提供工程创建、板级资料加载、设备开发、调试及恢复的标准入口。

## 创建工程

以 [`projects/get-started`](projects/get-started) 为参考工程，在 `projects/`
目录下为每个新应用创建独立目录。除非任务明确要求修改模板，否则不要直接把
`get-started` 改造成具体应用。

组件仓库和其他项目资料通过 Git 子模组提供。只加载或初始化当前任务所需的
子模组。实现功能前，先查看 [`skills/README.md`](skills/README.md)，并按需读取
相关的 `SKILL.md`，无需一次性加载全部资料。

## 开发、调试与恢复

日常日志、设备控制、OTA、崩溃证据和恢复默认通过 USB High-Speed 上的
**ESP-Iris Developer Gateway** 完成。

- 编码 Agent 只能使用 ESP-Iris 组件源码内提供的 CLI 操作设备。
- 开发者可以同时打开 Gateway Web 工作台，观察同一设备的日志、画面、Job、
  重启及 recovery 进度。
- CLI 与 Web 工作台共享同一个稳定的 Device ID、Boot ID 和结构化操作记录。
- Gateway 独占 USB 会话，并持久化结构化证据与原始日志；执行 OTA 前应先保存
  有效的 core dump。

ESP-Mosaico 只有一个 High-Speed USB 接口。normal 固件和 factory recovery
模板都会把该接口交给 ESP-Iris，因此 Gateway 在两种模式下都独占该会话。

### 保底重新烧录

`idf.py flash` 是由开发者执行的保底初始烧录／恢复手段，仅在 normal 和 recovery
模式下均没有可连接的 ESP-Iris 固件时使用。设备进入不可恢复的故障状态，且
normal USB 与 recovery USB 均不可用时，应建议开发者手动进入 ROM 下载模式并
重新烧录：

1. 将设备关机。
2. 按住位于 USB-C 接口左侧的 **Boot** 键。
3. 保持按住 **Boot** 键并开机。
4. 执行经过确认的重新烧录流程；ESP-Iris 恢复可连接后，设备操作重新交回
   ESP-Iris Gateway。

手动进入 ROM 下载模式是保底恢复策略，不是日常开发路径。重新烧录前应保存
仍可获取的崩溃证据，不要仅为恢复连接而擦除整片 Flash。

## 仓库结构

- `projects/`：开发者工程目录，新工程从 `projects/get-started` 开始。
- `skills/`：面向 Agent 和开发者的任务集成指南，详见
  [`skills/README.md`](skills/README.md)。
- `docs/`：面向用户的文档。
- `.agents/`：面向 Agent 的文档和工具。
- `AGENTS.md`：供编码 Agent 使用的简明路由与操作规则。
