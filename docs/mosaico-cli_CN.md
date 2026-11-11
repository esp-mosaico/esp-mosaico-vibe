# mosaico.py 命令参考

[English](mosaico-cli.md) | [返回文档索引](README_CN.md)

在工作区根目录执行 `python mosaico.py --help` 查看顶层入口。使用
`python mosaico.py iris --help`、`python mosaico.py iris takeover --help`
或具体命令的 `--help` 查看参数。

```text
mosaico.py
├── doctor
├── project init / sim / package / validate / upload
├── account login / status / logout
├── iris
│   ├── run / status
│   ├── list / claim / release / reconcile
│   ├── takeover start / status / resume / abort / reconcile
│   ├── logs / memory / crash / rpc
│   ├── device-status / screenshot / operation-status
│   ├── operation-reconcile
│   ├── flash / app-flash / run-app / restart
│   ├── app-update
│   ├── system-update
│   └── test enter-recovery / recovery-wifi / bridge-code
├── recover
└── game create/new/sim/run/build（空白模板、引擎示例与 Host）
```

模式定义及 AI/Boot 按键区别见[设备模式](device-modes_CN.md)。
`recover` 写入基础固件；`iris test enter-recovery` 只进入已有 Vibe Mode。

## 命令职责

| 命令 | 含义 |
| --- | --- |
| `doctor` | 检查主机环境，不启动 Gateway |
| `project init <name>` | 根据工作区模板创建应用，不启动 Gateway |
| `project sim` | 运行应用原生 C UI 后端；显式选择场景或导出图片时仅渲染场景 |
| `project package/validate/upload` | 离线生成 Ideas ZIP、验证 ZIP，或将已有 ZIP 上传为草稿 |
| `account login/status/logout` | 管理 Mosaico Ideas 浏览器登录和凭据 |
| `iris run` | 持续持有一个共享 Gateway 客户端；首次创建时尝试连接唯一设备，Ctrl+C 只释放自身客户端 |
| `iris status [--all]` | 被动查询本项目或同用户跨工作区 Gateway、使用者及设备归属；不启动、不保活 |
| `iris list` | 通过所选项目 Gateway 枚举端点和设备，必要时启动 Gateway；不认领或连接设备 |
| `iris claim/release/reconcile` | 认领、释放设备，或显式清理已确认失效的普通归属 |
| `iris takeover start/status/resume/abort/reconcile` | 请求接管、查询记录、继续验证、撤销或核对归属 |
| `iris logs` | 显示保留日志并持续跟随；`--snapshot` 只读取保留日志 |
| `iris memory` | 读取内存状态；`--follow` 持续采样 |
| `iris crash` | 查看崩溃信息；`--archive` 归档并解码 Core Dump |
| `iris rpc` | 调用指定的应用 RPC |
| `iris device-status` | 读取实时设备身份、Boot ID 和固件状态；拒绝缓存或身份不匹配的响应 |
| `iris screenshot <output>` | 直接保存设备图像并返回图像及操作信息；核对截图前后 Device ID 和 Boot ID 一致 |
| `iris operation-status <operation-id>` | 查询已有操作并保留其实际状态；不认领设备、不重放操作 |
| `iris operation-reconcile <operation-id>` | 继续验证；ROM 恢复按原记录中的身份重连，不重复写入或复位 |
| `iris flash` / `iris app-flash` | 经 Gateway 执行对应的原生 IDF 烧录目标，可随后恢复控制台；这是开发流程，不等同产品更新验收 |
| `iris run-app` | 通过 USB 或 UART 请求 Vibe Mode 重试已安装的应用 |
| `iris restart` | 重启所选设备，并确认其以新的 Boot ID 返回 |
| `iris app-update` | 仅更新正常应用代码，要求完整分区表与设备一致 |
| `iris system-update` | 新应用、分区布局或资源变化的推荐入口，按更新包清单写入 |
| `recover` | 初始化或恢复设备基础固件，包括 ESP-Iris 不可达时的恢复 |
| `game` | 从空白模板或引擎示例创建游戏，调用 Host 仿真及 native/Iris 构建，见[游戏开发入口](game-development_CN.md) |

`iris test` 下的命令用于分别测试 Vibe Mode 流程：

| 命令 | 前置条件和成功判据 |
| --- | --- |
| `enter-recovery` | 正常应用通过 ESP-Iris 响应，重启进入已有 Vibe Mode；等待同一 Device ID 以新的 Boot ID 重连。已经在 Vibe Mode 时返回当前状态 |
| `recovery-wifi` | 已进入 Vibe Mode 且 ESP-Iris USB 可用；下发 Wi-Fi 名称和密码并等待联网成功 |
| `bridge-code` | 已进入 Vibe Mode、USB 可用、已配置 Bridge 服务且能够联网；测试旧 Bridge 维护配对接口，返回配对码、有效期和网站地址；与当前六位 Ideas 编号查询分开 |

## Ideas 文件发布

按 `mosaico-ideas/project-release/1` 编写 `mosaico-ideas.json`，并填写有效的
站点分类 `project.category_id`；先构建应用的 `.irisfw`。打包将网站内容、固件和
中英文设备展示资源冻结为不可变 ZIP：

```sh
python mosaico.py project package --project projects/my_app --output dist/release.zip
python mosaico.py project validate dist/release.zip --json
python mosaico.py account login
python mosaico.py project upload dist/release.zip --create --yes --json
# 使用云端 public_id 替换尚未提交审核的草稿：
python mosaico.py project upload dist/release.zip --update PUBLIC_ID --yes --json
```

package/validate 离线执行；upload 接收已有 ZIP，不重新构建，并同步创建或替换草稿，
正式发布是另一步操作。上传没有幂等回执或任务轮询；响应不确定时，先检查网站再决定
是否重试。普通发布不能包含 Vibe Mode 或 bootloader。元数据、资源、认证和服务端校验见
[文件发布指南（英文）](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/docs/ideas-file-release/README.md)。

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

GSP 应用使用固定版本的独立 GSPC 编译器引导，无需为此安装 Python `gsp`
模块。首次无缓存时需要下载；离线可用 `GSPC_EXECUTABLE` 指定兼容编译器的
绝对路径。应用使用 GSPC 0.6.1 和 ESP-GSP 1.5.1；Vibe Mode 使用自己的轻量 UI，
不依赖 GSP/GSPC。共享下载逻辑位于 utils 的 `shared/gsp`，应用版本由
`mosaico-tools/gsp-toolchain.json` 固定。

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
已审查基础包和 ROM 恢复流程见
[Utils 指南](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/README.md#rom-recovery-operations)。

`app-update` 遇到分区表不同会返回 `partition_layout_mismatch`、设备/构建
SHA-256 及 `system-update` 建议，不会自动扩大写入范围或修改工程分区表。
应重新选择更新方式，不得为绕过检查而修改预期分区布局。
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

ROM 写入完成但 Vibe Mode 验收超时时，执行
`python mosaico.py iris operation-reconcile <operation-id> --project projects/my_app --json`。
该命令使用原目标身份和已保存的写入结果，经 Gateway 归属检查后重新连接，验证
Vibe Mode 身份、启动实例、固件和 OTA 就绪状态，不重复写入或复位。原操作状态保留，
新增的 `reconciliation.outcome` 仅在证据充分时返回 `observed_success`；证据缺失或
写入进程仍在运行时不会报告验收成功。
如果 Gateway 崩溃留下孤立归属，先等待写入进程结束，再通过
`iris reconcile --device-id <Device-ID> --project projects/my_app`
清理已确认失效的归属，然后重试 `operation-reconcile`。

浏览器自动化和 CLI/Web 对照用于工作台自身测试或用户明确要求的场景。
生命周期、设备占用与接管的完整规则见 [Gateway 指南](project-gateway_CN.md)。

排查连接后 normal 与 Vibe Mode 仍均不可达时，使用 `recover`。仅当命令要求
手动进入 ROM 时，由开发者按[物理入口步骤](device-modes_CN.md#物理入口)执行；
随后由 Agent 核实 ROM 连接、继续 `recover` 并验证设备和目标应用。
不得仅为恢复连接擦除整片 Flash，或未经授权覆盖凭据、身份、Vibe Mode 数据或分区。

Gateway 拥有 UART 或 USB Serial/JTAG 端点时，不得直接并发打开。通过受支持的 UART
或 USB Serial/JTAG 连接执行原生开发烧录时，使用 `python mosaico.py iris flash`
或 `app-flash`；Gateway 会协调端点并可恢复控制台采集。这些命令不替代通过
`app-update`、`system-update` 或 `recover` 完成的产品安装与验收。Vibe Mode 始终将
High-Speed USB 交给 ESP-Iris，normal 应用默认也如此。
产品功能需要占用它的 normal 应用应记录例外，并通过其他可用传输保留 ESP-Iris 运维和恢复路径。

## 旧命令兼容

旧入口暂时作为兼容别名保留，新文档和自动化应使用上面的正式入口：

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

`recover`、`doctor` 和工作区的 `game` 入口保留。操作记录中的内部操作标识、
JSON 业务结果及取证目录格式继续沿用；`iris status` 新增 `running` 字段。

GSP 预览使用 `python mosaico.py project sim --project projects/my_app`；
工作区不预置应用，先执行 `project init`。公共实现位于 utils。
