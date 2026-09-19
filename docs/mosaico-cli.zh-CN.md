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

- 所有设备操作使用同项目的共享 Gateway，没有时自动启动。创建者没有特殊关闭权。
- `iris run` 每次调用都登记独立的持续客户端；即使 Gateway 已存在也保持前台运行。
  Ctrl+C 只释放这次客户端，不影响其他终端、工作台或正在执行的后台任务。
- 普通设备命令执行期间持有客户端，包括构建、更新和等待重连；结束时释放。
- Web 工作台通过事件连接保活。页面关闭或连接失效后释放，不自动唤醒已退出的 Gateway。
- 无有效客户端、无活动请求、更新、Job、维护或流任务后，空闲 **10 秒**自动退出；
  新客户端或新工作加入会取消倒计时。正常关闭会释放普通设备归属，历史证据仍保留。
- CLI 客户端每 5 秒续期，20 秒未续期则失效，之后才开始 10 秒空闲倒计时。
  转让、维护的待核对记录不会被自动清除；中断的写操作不会自动重放。
- `iris status` 不创建 Gateway，不增加客户端、不重置倒计时。无实例时 JSON 返回
  `{"running": false, "session": null}`；遗留归属会额外列出。
- `iris status --all` 直接读取同用户公共注册表，并被动查询各实例。当前项目没有 Gateway
  也能看到其他工作区的项目路径、网关地址、设备归属、客户端和保活原因。
  `--all` 与 `--project` 互斥。此协调范围要求同一主机、同一用户且共用状态目录。
- `iris claim/release/reconcile`、转让操作可以自动启动或加入共享实例；
  `iris transfer status` 保持被动。设备认领本身不永久保活，持续调试使用 `iris run`。
- 不提供显式停止命令。释放全部客户端并等待工作完成即可自动退出。
- 支持 `--gateway-profile` 的命令仍由外部管理 Gateway 生命周期，连接失败不回退到本地实例。

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
共享会话中的 `iris claim` 可以省略设备参数；`iris release` 自动释放唯一拥有的
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

新建项目优先使用 `iris system-update --project ...`。由项目构建的包包含应用、分区表及声明的资源镜像，保留 Recovery 固定前缀和 bootloader。`game_assets` 仅预留但未使用时不需要镜像；有外部资源的游戏通过 CMake 声明将镜像纳入包。

`--device-id` 可独立选定设备：Gateway 优先复用已验证连接，否则先尝试在线 USB，再验证其他候选端点。HELLO 身份必须匹配；失败的新连接释放本次占用。`--endpoint` 是严格限定，其他工作区占用不会被抢走。候选连接重试只发生在写入提交之前。

`app-update` 遇到分区表不同会返回 `partition_layout_mismatch`、设备/构建 SHA-256 及可执行的 `system-update` 建议。它不会自动扩大写入范围或修改工程分区表。实际构建配置（包括 `--skip-build`）与更新完成后的应用都必须通过角色、产品、板型、布局契约、Recovery ABI 检查。
