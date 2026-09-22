# ESP-Mosaico 开发文档

首次使用从根目录 [README](../README_CN.md) 的最短上手流程开始。本文档目录说明
开发者如何使用工作区；Agent 执行规则见 [AGENTS.md](../AGENTS.md)，可复用工作流见
[技能索引](../.agents/skills/README.md)。

## 按任务阅读

| 要完成的任务 | 文档 |
| --- | --- |
| 创建应用、选择参考工程、配置模板并首次安装 | [工程初始化](project-init.zh-CN.md) |
| 选择更新方式、查询命令或恢复设备 | [CLI 命令参考](mosaico-cli.zh-CN.md) |
| 打开工作台、理解会话、选择或转让设备、排查占用 | [Gateway 与设备归属](project-gateway.zh-CN.md) |
| 开发游戏、运行同源仿真与回放、验证真机 | [游戏开发](game-development.zh-CN.md) |
| 对 Sky Hop 做可重复的真机性能对比 | [Sky Hop 性能测试](sky-hop-performance.zh-CN.md) |
| 运行主机检查、了解固件构建矩阵 | [持续集成](ci.zh-CN.md) |

## 专项与组件资料

- UI 参考：[GSP Hello World](../projects/hello_world/README.md)、
  [GSP 仿真工具](../tools/gsp-sim/README.md)。
- 工具与恢复：[产品 CLI](../submodule/esp-mosaico-utils/mosaico-tools/README.md)、
  [Recovery 固件](../submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery/README.md)、
  [组件边界](../submodule/esp-mosaico-utils/docs/component-boundaries.md)。
- 游戏引擎：[概览](../submodule/raylib-lite-engine/README.md)、
  [组件生命周期](../submodule/raylib-lite-engine/components/README.md)、
  [Host runner 与回放格式](../submodule/raylib-lite-engine/host/run_game.py)。

组件链接依赖对应子模块已经初始化。API、协议和模板格式随所属组件维护；本目录
保留工作区集成与操作流程，引用组件文档，不复制其完整规格。

## 设计与验证记录

以下资料保留对应版本的设计与实测背景，不代替现行使用指南：

- Hello World：[设计概念](design/gsp-hello/concept-v1.md)、
  [实现与验证](design/gsp-hello/implementation.md)。
- Recovery Download Ideas：[延迟分析](download-ideas-pairing-latency-analysis.zh-CN.md)、
  [预取与压缩验证](download-ideas-prefetch-validation.zh-CN.md)、
  [ESP-61 二维码、页面与 OTA 进度实现计划](esp-61-recovery-ideas-plan.zh-CN.md)。
  当前实现、容量与真机回路见 [ESP-61 验证记录](esp-61-validation.zh-CN.md)。
- Recovery 容量：[固件大小分析与缩减实验](recovery-size-analysis.zh-CN.md)。

## 维护约定

新增用户文档时补充本索引，并为文档指定明确主题。中文专题使用
`<topic>.zh-CN.md`；中英文根 README 保持相同上手步骤和导航。
Gateway 生命周期与设备选择规则统一维护在 Gateway 指南，命令参考只保留摘要和入口。
Agent 实验记录放在 `.agents/analysis/`，不作为现行使用文档或当前验收证据。
