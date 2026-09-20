# mosaico.py 命令参考

[返回文档索引](README.md)

在工作区根目录执行 `python mosaico.py --help` 查看顶层入口。使用
`python mosaico.py iris --help`、`python mosaico.py iris transfer --help`
或具体命令的 `--help` 查看参数。

```text
mosaico.py
├── doctor
├── project init
├── iris
│   ├── run / status
│   ├── list / claim / release / reconcile
│   ├── transfer start / status / accept / abort / reconcile
│   ├── logs / memory / crash / rpc
│   ├── app-update
│   ├── system-update
│   └── test enter-recovery / recovery-wifi / bridge-code
├── recover
└── game（本工作区提供的游戏开发工具）
```

## 命令职责

| 命令 | 含义 |
| --- | --- |
| `doctor` | 检查主机环境，不启动 Gateway |
| `project init <name>` | 根据工作区模板创建应用，不启动 Gateway |
| `iris run` | 持续持有一个共享 Gateway 客户端；首次创建时尝试连接唯一设备，Ctrl+C 只释放自身客户端 |
| `iris status [--all]` | 被动查询本项目或同用户跨工作区 Gateway、使用者及设备归属；不启动、不保活 |
| `iris list` | 枚举可见端点和已知设备；发现不等于认领或连接 |
| `iris claim/release/reconcile` | 认领、释放设备，或显式清理已确认失效的普通归属 |
| `iris transfer start/status/accept/abort/reconcile` | 发起、查询、接受、中止或核对设备归属转让 |
| `iris logs` | 显示保留日志并持续跟随；`--snapshot` 只读取保留日志 |
| `iris memory` | 读取内存状态；`--follow` 持续采样 |
| `iris crash` | 查看崩溃信息；`--archive` 归档并解码 Core Dump |
| `iris rpc` | 调用指定的应用 RPC |
| `iris app-update` | 仅更新正常应用代码，要求完整分区表与设备一致 |
| `iris system-update` | 新应用、分区布局或资源变化的推荐入口，按更新包清单写入 |
| `recover` | 初始化或恢复设备基础固件，包括 ESP-Iris 不可达时的恢复 |
| `game` | 游戏创建、Host 仿真和构建，见[游戏开发指南](game-development.zh-CN.md) |

`iris test` 下的命令用于分别测试 Recovery 流程：

| 命令 | 前置条件和成功判据 |
| --- | --- |
| `enter-recovery` | 正常应用通过 ESP-Iris 响应，重启进入已有 Recovery；等待同一 Device ID 以新的 Boot ID 重连。已经在 Recovery 时返回当前状态 |
| `recovery-wifi` | 已进入 Recovery 且 ESP-Iris USB 可用；下发 Wi-Fi 名称和密码并等待联网成功 |
| `bridge-code` | 已进入 Recovery、USB 可用、已配置 Bridge 服务且能够联网；打开设备下载页面，返回配对码、有效期和 Bridge 网站地址 |

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
[Gateway 设备选择](project-gateway.zh-CN.md#设备发现与选择)。

## 选择更新方式

| 场景 | 命令 |
| --- | --- |
| 空白或未经验证的设备，或 normal/Recovery 均不可达 | `python mosaico.py recover` |
| 新应用，或分区布局、外部资源变化 | `python mosaico.py iris system-update --project projects/my_app` |
| 仅修改应用代码，且完整分区表与设备一致 | `python mosaico.py iris app-update --project projects/my_app` |

`recover` 准备经过评审的基础固件并验证 Recovery 就绪；之后还需安装并验收目标
应用。它仅支持本机 Gateway，底层恢复过程也由该命令管理。

从工程构建的 `system-update` 包包含应用、分区表及工程声明的资源镜像，保留
Recovery 固定前缀和 bootloader。仅预留但未使用的 `game_assets` 不需要镜像；
使用外部资源的应用通过 CMake 声明将镜像纳入包。已有包可使用 `--bundle PATH`。
Recovery 自身更新、HTTP(S)/NAND 更新及基础包约束见
[Recovery 说明](../submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery/README.md)。

`app-update` 遇到分区表不同会返回 `partition_layout_mismatch`、设备/构建
SHA-256 及 `system-update` 建议，不会自动扩大写入范围或修改工程分区表。
实际构建配置（包括 `--skip-build`）与更新后的应用都须通过角色、产品、板型、
布局契约和 Recovery ABI 检查。

更新前通过产品工具保存有效 core dump、结构化证据和原始日志。更新成功须确认
同一 Device ID 经 normal → Recovery → normal 返回，产生新的 Boot ID，运行
目标固件并报告 healthy，且产品行为符合预期。空白设备则先完成 Recovery 就绪
验证，再安装应用。上传完成或重连本身不代表验收通过。

## 调试与恢复入口

运行 `python mosaico.py iris run --project projects/my_app` 并打开输出中的 URL，
可持续观察 Gateway Web 工作台。生命周期、设备占用与转让的完整规则见
[Gateway 指南](project-gateway.zh-CN.md)。CLI 和工作台应显示同一设备的 Device ID、
Boot ID 和操作记录。

normal 与 Recovery 都不可达时继续使用 `recover`。仅当命令要求手动进入 ROM
时，由开发者执行：

1. 关闭设备电源。
2. 按住 USB-C 接口左侧的 Boot 键。
3. 保持按住 Boot 键并开机。
4. 进入 ROM 下载模式后松开 Boot 键，告知 Agent 物理操作完成。

之后由 Agent 检测恢复连接，继续 `recover` 并验证设备和目标应用。
不得仅为恢复连接擦除整片 Flash，或未经授权覆盖凭据、身份及 Recovery 数据。

应用烧录和监控避免 USB Serial/JTAG；Gateway 拥有接口时不得并发打开它。
High-Speed USB 默认交给 ESP-Iris；产品功能需要占用它的 normal 应用应记录例外，
并通过其他可用传输保留 ESP-Iris 运维和恢复路径。

## 旧命令兼容

旧入口暂时作为兼容别名保留，新文档和自动化应使用上面的正式入口：

| 旧入口 | 新入口 |
| --- | --- |
| `init` | `project init` |
| `session run/status` | `iris run/status` |
| `list`、`device claim/release/reconcile` | `iris list/claim/release/reconcile` |
| `device transfer`、`device transfer-*` | `iris transfer start`、`iris transfer ...` |
| `monitor` | `iris logs` |
| `memory/crash/rpc` | `iris memory/crash/rpc` |
| `install` | `iris app-update` |
| `system-update` | `iris system-update` |
| `enter-recovery/recovery-wifi/bridge-code` | `iris test enter-recovery/recovery-wifi/bridge-code` |

`recover`、`doctor` 和工作区的 `game` 入口保留。操作记录中的内部操作标识、
JSON 业务结果及取证目录格式继续沿用；`iris status` 新增 `running` 字段。
