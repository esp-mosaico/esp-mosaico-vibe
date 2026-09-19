# mosaico.py 命令结构

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
| `iris run` | 前台持有当前项目的 Gateway，首次启动时尝试自动连接唯一设备；Ctrl+C 后收尾退出 |
| `iris status` | 查询当前项目的 Gateway，不创建会话；没有 Gateway 时报告未运行 |
| `iris list` | 枚举可见端点和已知设备；发现不等于认领或连接 |
| `iris claim/release/reconcile` | 认领、释放设备，或显式清理已确认失效的普通归属 |
| `iris transfer start/status/accept/abort/reconcile` | 发起、查询、接受、中止或核对设备归属转让 |
| `iris logs` | 显示保留日志并持续跟随；`--snapshot` 只读取保留日志 |
| `iris memory` | 读取内存状态；`--follow` 持续采样 |
| `iris crash` | 查看崩溃信息；`--archive` 归档并解码 Core Dump |
| `iris rpc` | 调用指定的应用 RPC |
| `iris app-update` | 构建并安装正常应用，保留 Recovery 流程及启动验证 |
| `iris system-update` | 按经过验证的更新包清单更新固件，实际范围由清单决定 |
| `recover` | 初始化或恢复设备基础固件，包括 ESP-Iris 不可达时的恢复 |

`iris test` 下的命令用于分别测试 Recovery 流程：

| 命令 | 前置条件和成功判据 |
| --- | --- |
| `enter-recovery` | 正常应用通过 ESP-Iris 响应，重启进入已有 Recovery；等待同一 Device ID 以新的 Boot ID 重连。已经在 Recovery 时返回当前状态 |
| `recovery-wifi` | 已进入 Recovery 且 ESP-Iris USB 可用；下发 Wi-Fi 名称和密码并等待联网成功 |
| `bridge-code` | 已进入 Recovery、USB 可用、已配置 Bridge 服务且能够联网；打开设备下载页面，返回配对码、有效期和 Bridge 网站地址 |

## 项目与 Gateway 生命周期

`--workspace` 选择工作区，默认向上查找 `.mosaico.json`。`--project` 选择应用及
它所属的 Gateway 会话；`iris app-update` 和从项目构建的 `iris system-update`
也使用该应用作为构建目标。省略时按当前应用目录、工作区默认应用、唯一应用
候选的顺序选择。不同工作区或应用路径有独立会话。

- `iris run` 创建常驻会话；已有常驻会话时只显示已有会话，不接管其生命周期。
- `iris status` 只查询已有会话，允许观察临时会话；没有会话时成功返回
  `{"running": false, "session": null}`（使用 `--json`）。
- `iris claim/release/reconcile`、`iris transfer ...` 要求常驻会话；没有时直接报错。
- `iris list` 和单次设备操作会复用常驻会话；没有时可以创建临时 Gateway。
  文本输出会显示所选项目、创建或复用方式及 Web 工作台地址。
- 临时 Gateway 随创建它的命令结束而收尾退出，其他操作不能并发接入它。
  监控与更新需要并行时，先启动 `iris run`。
- 支持 `--gateway-profile` 的命令可以连接指定外部 Gateway；连接失败不回退到本地实例。

例如，在一个终端保持：

```sh
python mosaico.py iris run --project projects/hello_world
```

在其他终端明确选择同一项目：

```sh
python mosaico.py iris list --project projects/hello_world
python mosaico.py iris claim --project projects/hello_world --endpoint '<发现的端点>'
python mosaico.py iris logs --project projects/hello_world --device-id '<Device-ID>'
python mosaico.py iris app-update --project projects/hello_world --device-id '<Device-ID>'
```

使用实时握手得到的 Device ID。单次设备操作也可以通过 `--endpoint` 指定首次
连接目标。更多细节见[项目会话与设备归属](project-gateway.zh-CN.md)。

## 单设备免选择

只有一块可用的 ESP-Mosaico 通过 USB 连接时，可以直接执行：

```sh
python mosaico.py iris logs
python mosaico.py iris memory
python mosaico.py iris crash
python mosaico.py iris app-update --project projects/hello_world
python mosaico.py iris system-update --project projects/hello_world
python mosaico.py iris test enter-recovery
python mosaico.py iris test recovery-wifi --ssid '<Wi-Fi 名称>'
python mosaico.py iris test bridge-code
```

`iris rpc` 同样可以省略设备参数，但仍需指定服务、方法和请求内容。
Recovery 测试的模式、USB、网络、凭据等前置条件不变。

选择顺序如下：

1. 显式 `--device-id` / `--endpoint` 优先，失败不改选其他设备。
2. 使用当前项目已连接的唯一设备。
3. 没有已连接设备时，跟随当前项目唯一的已有归属，包括等待离线设备重连。
4. 没有已有归属时，实时枚举本机 USB 并认领唯一可用候选。缓存、未连接的
   TCP/mDNS 端点、ROM 和 USB Serial/JTAG 接口不参与这个自动选择。
5. 多个候选时直接列出候选并要求指定目标。其他会话占用、维护、转让以及孤立
   归属不会被自动抢占或清理。握手确认 Device ID 后，本次操作始终跟随该身份。

`iris run` 只在首次创建会话时尝试一次自动连接。无设备或有歧义时仍保持 Gateway
运行，后续设备操作或 `iris claim` 再发起连接；主动释放后不会被后台重新认领。
常驻会话中的 `iris claim` 可以省略设备参数；`iris release` 自动释放唯一拥有的
设备，即使其暂时离线。同一设备的 USB/TCP 归属合并计数。`iris transfer start`
也可省略 `--device-id`，但仍必须指定 `--to-session`；重试沿用同一 `--transfer-id`。
`reconcile` 和转让记录相关命令仍要求明确目标或记录 ID。

`recover` 先尝试连接唯一的 ESP-Iris 设备，只有未发现可用目标时才继续原有 ROM
接口检测；存在占用、歧义或已有目标连接失败时，实际恢复不会改选另一块板。
显式 `--hardware-mac` 保留原来的硬件身份选择流程。

`iris list` / `iris status` 仍不认领设备；`--gateway-profile` 仍只操作指定外部
Gateway 上已连接的设备，不从当前电脑自动认领 USB。

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
