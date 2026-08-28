# ESP-Mosaico Vibe

[English](README.md) | [中文](README_CN.md)

本仓库是为 ESP-Mosaico 定制的 **Agent 主导（Agent-Led）人机协同开发统一入口**。
它为 Agent 提供统一的工程能力与设备通道。

用户定义目标并验收实物。Agent 作为默认执行主体，持续推进到真机验证。
涉及授权、物理操作或高风险变更时，Agent 请求用户介入。

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

`idf.py flash` 是保底初始烧录／恢复手段，仅在 normal 和 recovery
模式下均没有可连接的 ESP-Iris 固件时使用。设备进入不可恢复的故障状态，且
normal USB 与 recovery USB 均不可用时，Agent 应先保存仍可获取的崩溃证据，
再提示开发者完成进入 ROM 下载模式所需的物理操作：

1. 将设备关机。
2. 按住位于 USB-C 接口左侧的 **Boot** 键。
3. 保持按住 **Boot** 键并开机。
4. 设备进入 ROM 下载模式后松开 **Boot** 键，并告知 Agent 物理操作已完成。

开发者只需完成上述按键和上电操作。之后由 Agent 检测并确认 ROM 下载
连接，执行经过确认的 `idf.py flash` 重新烧录流程，验证目标固件和产品
功能。ESP-Iris 恢复可连接后，Agent 应立即将后续设备操作交回 ESP-Iris
Gateway。

手动进入 ROM 下载模式仅用于最后恢复。不要仅为恢复连接而擦除整片 Flash，
也不应在未经用户明确授权时覆盖凭据、设备身份、recovery 数据或相关分区。

## 仓库结构

- `projects/`：开发者工程目录，新工程从 `projects/get-started` 开始。
- `skills/`：面向 Agent 和开发者的任务集成指南，详见
  [`skills/README.md`](skills/README.md)。
- `docs/`：面向用户的文档。
- `.agents/`：面向 Agent 的文档和工具。
- `AGENTS.md`：供编码 Agent 使用的简明路由与操作规则。

仓库的目标、架构、功能契约和适用边界见
[`docs/repository-specification.zh-CN.md`](docs/repository-specification.zh-CN.md)。
