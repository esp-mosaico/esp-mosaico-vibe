# mosaico.py 命令参考

[English](mosaico-cli.md) | [返回文档索引](README_CN.md)

在工作区根目录执行 `python mosaico.py --help` 查看顶层入口。使用
`python mosaico.py iris --help`、`python mosaico.py iris takeover --help`
或具体命令的 `--help` 查看参数。

```text
mosaico.py
├── doctor
├── project init
├── iris
│   ├── run / status
│   ├── list / claim / release / reconcile
│   ├── takeover start / status / resume / abort / reconcile
│   ├── logs / memory / crash / rpc
│   ├── device-status / screenshot / operation-status
│   ├── app-update
│   ├── system-update
│   └── test enter-recovery / recovery-wifi / bridge-code
├── recover
└── game create/new/sim/run/build（空白模板、BSP 示例与引擎 Host）
```

模式定义及 AI/Boot 按键区别见[设备模式](device-modes_CN.md)。
`recover` 写入基础固件；`iris test enter-recovery` 只进入已有 Vibe Mode。

## 命令职责

| 命令 | 含义 |
| --- | --- |
| `doctor` | 检查主机环境，不启动 Gateway |
| `project init <name>` | 根据工作区模板创建应用，不启动 Gateway |
| `iris run` | 持续持有一个共享 Gateway 客户端；首次创建时尝试连接唯一设备，Ctrl+C 只释放自身客户端 |
| `iris status [--all]` | 被动查询本项目或同用户跨工作区 Gateway、使用者及设备归属；不启动、不保活 |
| `iris list` | 枚举可见端点和已知设备；发现不等于认领或连接 |
| `iris claim/release/reconcile` | 认领、释放设备，或显式清理已确认失效的普通归属 |
| `iris takeover start/status/resume/abort/reconcile` | 请求接管、查询记录、继续验证、撤销或核对归属 |
| `iris logs` | 显示保留日志并持续跟随；`--snapshot` 只读取保留日志 |
| `iris memory` | 读取内存状态；`--follow` 持续采样 |
| `iris crash` | 查看崩溃信息；`--archive` 归档并解码 Core Dump |
| `iris rpc` | 调用指定的应用 RPC |
| `iris restart` | 软件重启所选设备并核对新 Boot ID；应用行为需另行验证 |
| `iris device-status` | 读取实时设备身份、Boot ID 和固件状态；拒绝缓存或身份不匹配的响应 |
| `iris screenshot <output>` | 直接保存设备图像并返回图像及操作信息；核对截图前后 Device ID 和 Boot ID 一致 |
| `iris operation-status <operation-id>` | 查询已有操作并保留其实际状态；不认领设备、不重放操作 |
| `iris app-update` | 仅更新正常应用代码，要求完整分区表与设备一致 |
| `iris system-update` | 新应用、分区布局或资源变化的推荐入口，按更新包清单写入 |
| `recover` | 初始化或恢复设备基础固件，包括 ESP-Iris 不可达时的恢复 |
| `game` | 从 BSP 示例创建游戏，调用引擎 Host 仿真或构建，见[游戏开发入口](game-development_CN.md) |

`iris test` 下的命令用于分别测试 Vibe Mode 流程：

| 命令 | 前置条件和成功判据 |
| --- | --- |
| `enter-recovery` | 正常应用通过 ESP-Iris 响应，重启进入已有 Vibe Mode；等待同一 Device ID 以新的 Boot ID 重连。已经在 Vibe Mode 时返回当前状态 |
| `recovery-wifi` | 已进入 Vibe Mode 且 ESP-Iris USB 可用；下发 Wi-Fi 名称和密码并等待联网成功 |
| `bridge-code` | 已进入 Vibe Mode、USB 可用、已配置 Bridge 服务且能够联网；打开设备下载页面，返回配对码、有效期和 Bridge 网站地址 |

## 构建工具兼容性与诊断

`doctor` 优先读取 ESP-IDF 的 `tools/cmake/version.cmake` 判定版本约束。
JSON 中 `idf.version` 是用于判定的版本，`idf.version_source` 标明来源，
`idf.reported_version` 保留 `idf.py --version` 的 Git 描述。版本文件缺失或
无法解析时才回退到后者，无需人为创建 `v6.2-dev` 标签。低噪声构建 doctor
采用同一规则，Python 和目标支持检查仍然执行。

在 `.mosaico.json` 的 `build` 对象中可配置 configdep：

```json
{"runner": "builtin", "configdep": "auto"}
```

`configdep` 支持 `auto`（默认）、`on`、`off`。锁定的 configdep 0.2.3 会把
源文件中的 `CONFIG_*` 宏名转成路径，可能产生 Windows 保留名 `AUX` 等。
因此 `auto` 在 Windows 上默认关闭此优化，其他平台沿用 ESP-IDF 默认值。
`auto` 尊重已有的 `IDF_CONFIGDEP_ENABLE` 环境变量；显式 `on` / `off`
优先于环境变量。该设置覆盖应用构建、Vibe Mode 构建及 system-update 打包。
独立低噪声脚本采用相同的平台默认值，可通过该环境变量显式覆盖。
关闭 configdep 仍保留 Ninja 增量构建，但修改配置可能重新编译更多源文件。

应用模板和 Vibe Mode 均使用固定版本的独立 GSPC 编译器引导，无需为此安装
Python `gsp` 模块。首次无缓存时需要下载；离线环境可用 `GSPC_EXECUTABLE`
指定兼容编译器的绝对路径。应用使用 GSPC 0.6.1 和 ESP-GSP 1.5.1；
Vibe Mode 独立保持 GSPC 0.5.0 和 ESP-GSP 1.4.0。显式指定的编译器必须与
当前构建工程的组件版本匹配。

构建错误摘要会包含 configdep 的 `touch_file` 失败原因及后续路径信息；
无法识别具体原因时回退到 `FAILED:` 上下文。文本错误写入 stderr，JSON 错误
位于 `details.diagnostic`。完整输出保存在报错所列日志中。

## 选择工程与设备

`--workspace` 选择工作区，默认向上查找 `.mosaico.json`。`--project` 选择应用及
它所属的 Gateway 会话；更新命令也使用该应用作为构建目标。省略时按当前应用
目录、工作区默认应用、唯一应用候选的顺序选择。

新建工程不会改变默认工程，因此下面的操作显式指定 `--project`：

```sh
python mosaico.py iris list --project projects/my_app
python mosaico.py iris logs --project projects/my_app --timeout 20
python mosaico.py iris memory --project projects/my_app
python mosaico.py iris crash --project projects/my_app --archive
```

只有一块可用 USB 设备时可省略设备选择器；多设备时使用 `--device-id` 或
`--endpoint`。使用实时确认的 Device ID。自动选择顺序、等待重连和归属限制见
[Gateway 设备选择](project-gateway_CN.md#设备发现与选择)。

## 选择更新方式

| 场景 | 命令 |
| --- | --- |
| 空白或未经验证的设备，或排查连接后 normal/Vibe Mode 仍均不可达 | `python mosaico.py recover` |
| 新应用，或分区布局、外部资源变化 | `python mosaico.py iris system-update --project projects/my_app` |
| 仅修改应用代码，且完整分区表与设备一致 | `python mosaico.py iris app-update --project projects/my_app` |

`recover` 准备经过评审的基础固件并验证 Vibe Mode 就绪；之后还需安装并验收目标
应用。它仅支持本机 Gateway，底层恢复过程也由该命令管理。

从工程构建的 `system-update` 包包含应用、分区表及工程声明的资源镜像，保留
Vibe Mode 固定前缀和 bootloader。仅预留但未使用的 `game_assets` 不需要镜像；
使用外部资源的应用通过 CMake 声明将镜像纳入包。已有包可使用 `--bundle PATH`。
Vibe Mode 自身更新、HTTP(S)/NAND 更新及基础包约束见
[Vibe Mode 说明](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/esp-mosaico-recovery/firmware/recovery/README.md)。

`app-update` 遇到分区表不同会返回 `partition_layout_mismatch`、设备/构建
SHA-256 及 `system-update` 建议，不会自动扩大写入范围或修改工程分区表。
实际构建配置（包括 `--skip-build`）与更新后的应用都须通过角色、产品、板型、
布局契约和 Recovery ABI 检查。

更新前通过产品工具保存有效 core dump、结构化证据和原始日志。更新成功须确认
同一 Device ID 经 normal → Vibe Mode → normal 返回，产生新的 Boot ID，运行
目标固件并报告 healthy，且产品行为符合预期。空白设备则先完成 Vibe Mode 就绪
验证，再安装应用。上传完成或重连本身不代表验收通过。

## 调试与恢复入口

运行 `python mosaico.py iris run --project projects/my_app` 保持调试会话。
输出中的工作台 URL 可供开发者观察。Agent 通过 CLI 获取设备证据：

```sh
python mosaico.py iris device-status --project projects/my_app --json
python mosaico.py iris screenshot device.png --project projects/my_app --json
python mosaico.py iris operation-status <operation-id> --project projects/my_app --json
```

直接查看保存的设备图像。截图结果包含文件路径、Device ID、Boot ID、operation ID
和 Gateway artifact 信息。截图过程中发生重启会导致验证失败；已保存的图像不能证明当前启动状态。
操作查询可启动或复用项目 Gateway 来读取保留记录，不连接设备。
`ok: true` 仅表示查询成功；实际结果读取 `operation.status`，包括 `failed` 或 `outcome_unknown`。
浏览器自动化和 CLI/Web 对照用于工作台自身测试或用户明确要求的场景。
生命周期、设备占用与接管的完整规则见 [Gateway 指南](project-gateway_CN.md)。

排查连接后 normal 与 Vibe Mode 仍均不可达时，使用 `recover`。仅当命令要求
手动进入 ROM 时，由开发者按[物理入口步骤](device-modes_CN.md#物理入口)执行；
随后由 Agent 核实 ROM 连接、继续 `recover` 并验证设备和目标应用。
不得仅为恢复连接擦除整片 Flash，或未经授权覆盖凭据、身份及 Vibe Mode 数据。

0.2 实现恢复原版 `idf.py flash monitor` 和 OpenOCD/JTAG。原生监控或烧录前，
先释放 Gateway 对控制台接口的占用。Mosaico 应用需要匹配的 0.2 基础固件和保留
test 分区契约；第三方 ESP-IDF 工程可以直接使用其原有分区表，完整烧录后通过
`recover` 重新安装 Mosaico 基础固件。默认仍使用 Mosaico 更新流程。
High-Speed USB 提供独立控制台和数据接口；UART 与 USB Serial/JTAG 仅提供控制链路。

## 迁移到 0.2 命令路径

旧命令别名已移除，现有脚本需要改为正式入口：

| 旧入口 | 新入口 |
| --- | --- |
| `init` | `project init` |
| `session run/status` | `iris run/status` |
| `list`、`device claim/release/reconcile` | `iris list/claim/release/reconcile` |
| `monitor` | `iris logs` |
| `memory/crash/rpc` | `iris memory/crash/rpc` |
| `install` | `iris app-update` |
| `system-update` | `iris system-update` |
| `enter-recovery/recovery-wifi/bridge-code` | `iris test enter-recovery/recovery-wifi/bridge-code` |

`recover`、`doctor` 和工作区的 `game` 入口保留。`.mosaico.json` 需要 schema 2，
0.2 状态与凭据和旧版本隔离，详见
[迁移指南](../submodule/esp-mosaico-utils/mosaico-tools/docs/migration-0.2.md)。
评审基础包已更新为通过实机验收的 0.2.0，已测范围和剩余缺口见
[验证记录](../submodule/esp-mosaico-utils/esp-mosaico-recovery/docs/validation-0.2.md)：
S31 High-Speed USB 的组合 `flash monitor` 仍有首次枚举时序问题，
OpenOCD/JTAG 实机验证仍未完成。0.2 工具不接受 0.1 基础包。

GSP 预览使用 `python mosaico.py project sim --project projects/my_app`；
工作区不预置应用，先执行 `project init`。公共实现位于 utils。
