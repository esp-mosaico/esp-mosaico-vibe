# 项目 Gateway、使用者与设备归属

[English](project-gateway.md) | [返回文档索引](README_CN.md)

同一系统用户、同一工作区、同一应用路径共享一个 Gateway。不同项目有独立的
地址、日志和操作记录，通过用户级公共注册表与操作系统锁协调设备归属。
这些信息描述主机项目会话，设备的 Device ID、Boot ID 仍以实时握手和查询为准。

## 开始和结束调试

```sh
python mosaico.py iris run --project projects/my_app
```

每次 `iris run` 都登记一个独立客户端，已有实例时也保持前台运行。打印的 URL
可打开 Web 工作台。其他终端或 Agent 的设备命令自动复用同项目实例。
普通命令在执行期间持有客户端；应用构建、安装及等待重连也属于执行期。
工作台的事件连接持有一个客户端，页面内部多个面板不会重复登记。

Ctrl+C 结束当前 `iris run` 的客户端，不关闭其他人使用的 Gateway。
普通命令结束同样只释放自身客户端。所有客户端离开且活动请求、更新、设备 Job、
主机操作或流任务都已结束后，Gateway 空闲 **10 秒**自动退出。新使用者或工作加入会
取消倒计时。没有单独的停止命令，设备被认领本身不构成永久保活。

CLI 每 5 秒续期一次，20 秒未续期后客户端失效，再进入 10 秒空闲等待。
工作台连接通过 WebSocket 心跳判断存活。刷新和短暂断线不会立刻结束 Gateway。
关闭客户端不等于取消已经提交的设备写入；后台仍完成处理并保留操作结果。

Gateway 已退出时，工作台不能自动唤醒它。重新执行 `iris run` 或设备命令后，
使用新输出的工作台 URL。Gateway 使用动态端口，旧地址不保证可复用。

## 查看谁在使用

```sh
python mosaico.py iris status --project projects/my_app
python mosaico.py iris status --all
python mosaico.py iris status --all --json
```

这些查询不会启动 Gateway、安装主机环境、登记客户端或延长空闲倒计时。
`--all` 与 `--project` 互斥，当前项目没有 Gateway 时仍可查询其他工作区。

每个实例显示项目及工作区路径、URL、源版本、会话 ID、占用设备、客户端列表、
保活原因和空闲倒计时。客户端信息包含类型、命令、PID（适用时）、连接及最近
保活时间；不展示凭据和包含密码的命令参数，不将进程标识推断成真实用户姓名。

JSON 的 `running` 描述会话进程锁是否仍被持有，`reachable` 描述 HTTP 查询是否
成功。`state` 可为 `running`、`idle`、`draining`、`unreachable`、`legacy` 或
`orphaned`。`lifecycle.clients` 是使用者，`lifecycle.keepalive` 是仍需运行的工作。
无实例时单项目查询返回 `{"running": false, "session": null}`；遗留归属另行列出。

`device_ids` 按身份去重，未完成身份验证的端点单列。占用可以跨设备重启、断线保留，
因此占用状态不等于在线状态。注册表中的旧会话不能作为当前设备身份的证明。

## 设备发现与选择

`iris list` 被动枚举候选端点和已知设备，不认领设备。发现记录可能包含缓存中的
离线设备；实际 Device ID、Boot ID 和在线状态须在操作时通过握手确认。

```sh
python mosaico.py iris list --project projects/my_app
python mosaico.py iris claim --project projects/my_app --endpoint '<发现的端点>'
python mosaico.py iris logs --project projects/my_app --device-id '<Device-ID>'
```

单设备操作可省略设备选择器。选择顺序如下：

1. 显式 `--device-id` / `--endpoint` 优先，失败不改选其他设备。
2. 使用当前项目已连接的唯一设备。
3. 没有已连接设备时，跟随当前项目唯一的已有归属，包括等待离线设备重连。
4. 没有已有归属时，实时枚举本机 USB 并认领唯一可用候选。缓存、未连接的
   TCP/mDNS 端点、ROM 和 USB Serial/JTAG 接口不参与这个自动选择。
5. 多个候选时直接列出候选并要求指定目标。其他会话占用、接管预约以及孤立
   归属不会被自动抢占或清理。握手确认 Device ID 后，本次操作始终跟随该身份。

`iris run` 只在首次创建会话时尝试一次自动连接。无设备或有歧义时仍保持 Gateway
运行，后续设备操作或 `iris claim` 再发起连接；主动释放后不会被后台重新认领。
共享会话中的 `iris claim` 可以省略设备参数；`iris release` 自动释放唯一拥有的
设备，即使其暂时离线。同一设备的 USB/TCP 归属合并计数。`iris takeover start`
要求显式指定 `--device-id` 或 `--endpoint`；重试沿用同一 `--takeover-id`。
`reconcile` 和接管记录相关命令仍要求明确目标或记录 ID。

`recover` 先尝试连接唯一的 ESP-Iris 设备，只有未发现可用目标时才继续原有 ROM
接口检测；存在占用、歧义或已有目标连接失败时，实际恢复不会改选另一块板。
显式 `--hardware-mac` 保留原来的硬件身份选择流程。

`iris list` / `iris status` 仍不认领设备；`--gateway-profile` 仍只操作指定外部
Gateway 上已连接的设备，不从当前电脑自动认领 USB。

`--device-id` 可独立选定设备：Gateway 优先复用已验证连接，否则先尝试在线 USB，再验证其他候选端点。HELLO 身份必须匹配；失败的新连接释放本次占用。`--endpoint` 是严格限定，其他工作区占用不会被抢走。候选连接重试只发生在写入提交之前。

从旧版随机 Device ID 升级为 eFuse Base MAC 派生身份时，用 `iris list` 刷新
保存的选择器。旧 ID 的操作历史仍保留，normal 与 Recovery 应使用兼容的身份规则。

## 跨项目协调

公共注册表位于同用户状态目录下的 `esp-mosaico/ownership/ownership.sqlite3`。
Linux 使用 `XDG_STATE_HOME`，未设置时使用用户的 `.local/state`；macOS 使用
Application Support；Windows 使用 LOCALAPPDATA。不同用户、不同主机或隔离
状态目录不共享此协调视图。独立或远程 Gateway 也不自动参与。

一个设备同一时刻归属于一个项目会话。其他项目不能自动抢占，HTTP 超时也不能
证明拥有者死亡。项目锁防止重复实例，会话锁判断存活，端点锁保护实际连接。

接收项目可以主动请求接管，命令会启动或复用接收方 Gateway：

```sh
python mosaico.py iris takeover start --project projects/my_app --endpoint /dev/ttyACM0
python mosaico.py iris takeover start --project projects/my_app --device-id '<Device-ID>' --force --timeout 120
```

默认只交接空闲设备；忙碌时返回具体操作、镜像或 Job。`--force` 拒绝旧 Gateway
对该设备的新操作，取消尚未开始的操作，等待当前写入安全结束，再停止屏幕、图像、
音频镜像并请求后台 Job 取消。只有收到终止确认后才交接。其他设备和旧 Gateway
的客户端继续运行；仅打开 Web 日志或保留 `iris run` 不阻止交接。

超时保留原归属并恢复接收新请求；已经停止的镜像、取消的任务不会自动恢复，仍在
执行的写入不会被中断。命令输出 takeover ID，通信中断后先查询：

```sh
python mosaico.py iris takeover status --project projects/my_app --takeover-id '<ID>'
python mosaico.py iris takeover resume --project projects/my_app --takeover-id '<ID>'
```

`status` 被动查询，不启动 Gateway。`resume` 在原接收会话继续身份验证；若尚未
生成预约记录，沿用原 ID 重新执行 `start`。活动请求保活接收方直到验证结束，
预约保护断开到重连的间隔；没有活动请求时，记录本身不会永久保活 Gateway。

需要撤回未完成的交接时，在**原持有项目**执行
`iris takeover abort --takeover-id ...`；已完成的接管不能回滚，正在接收的会话
仍存活时也不能强制撤销。双方原会话均已退出时，任一参与项目可以执行
`iris takeover reconcile --takeover-id ...`，核验端口锁后收回预约归属。
普通崩溃遗留的归属仍使用 `iris reconcile`。

接管只协调同用户、本机、使用新版工具的项目会话。CLI 的 `iris transfer` 命令组、
旧别名、旧 HTTP 转让入口均已移除；工作台只提供接管入口。
Gateway 重启不会自动重放设备写操作。

## 常见问题

| 现象 | 处理方式 |
| --- | --- |
| 工作台旧地址打不开 | 用 `iris run --project ...` 重新持有会话，打开新输出的 URL |
| 设备被其他项目占用 | 用 `iris status --all` 找到当前归属，在接收项目执行 `iris takeover start`，不终止其他客户端 |
| 明确指定的设备离线 | 等待该身份重连，核对连接；失败后不改选另一块设备 |
| 崩溃后留下普通归属 | 核实原会话已失效后执行 `iris reconcile`；接管记录使用 `iris takeover` 查询与恢复 |
| 更新后的应用无响应 | 保存日志与有效 core dump，按 [CLI 恢复入口](mosaico-cli_CN.md#调试与恢复入口)处理 |

## 旧实例与外部 Gateway

旧的临时／常驻实例仍可查询，无法提供客户端明细时会明确显示。新的设备命令要求
兼容的 API 和共享生命周期能力；不兼容时通过旧实例原来的持有终端结束它，
工具不会自动结束其他人的会话。

公共注册表保留旧表布局，以附加表记录新元数据，允许其他工作区旧工具继续访问。
`--gateway-profile` 的生命周期仍由外部负责；连接失败不会回退到本地实例。

## 组件边界与版本策略

主机产品工具位于 `submodule/esp-mosaico-utils/mosaico-tools`；Recovery 固件、
已审查镜像与共享持久化 ABI 位于 `esp-mosaico-recovery`。旧工具入口保留转发兼容。
CLI 通过 ESP-Iris 公开主机接口查询本机状态，不读取其 SQLite 表或私有锁结构。

默认按 Gateway API 和命令所需能力复用实例，不要求整个工具仓库的 Git 提交一致。
需要固定实际运行源码时，可在 `.mosaico.json` 添加
`"gateway": {"source_policy": "exact"}`；该模式对 Python、依赖锁和工作台构建产物
计算内容指纹，包含未提交变更。发现不一致会报错，不会终止其他用户的 Gateway。

安装所需的 Recovery 版本和分区哈希由产品 CLI 提交给 Gateway。Gateway 在同一个
操作中完成切换、重新连接、校验、写入和健康验证；校验不通过不会开始写入。
详细职责与接口见[组件边界说明](../submodule/esp-mosaico-utils/docs/component-boundaries.md)。

## 设备状态与 ROM 恢复

设备对外显示五种状态：离线、连接中、空闲、忙碌、需恢复。项目归属与固件模式
（Normal / Recovery / ROM / 未知）单独显示。日志页面、客户端保活不构成设备忙碌；
镜像、后台 Job 和正在执行的操作会给出具体忙碌原因。

### 实时证据与下一步

设备操作前先查看主机归属和被动发现结果：

```sh
python mosaico.py iris status --all --json
python mosaico.py iris list --details --json
```

协调归属后，通过 `iris run --project <project>` 或 `iris claim` 建立实时握手。
打开命令输出中的 Gateway Web 工作台，检查
`GET /v1/devices/<device-id>` 返回 `stale=false`，并与 CLI 证据核对
Device ID、Boot ID。离线或握手失败时，不能把旧查询结果当成当前状态。

下表按现场情况说明决策；固件模式、连接状态和活动操作分别核对，
这些情形并不是同一个状态枚举：

| 现场情况 | 必需证据与下一步 |
| --- | --- |
| 正常应用 | 实时 `firmware_mode=normal`、Device ID、Boot ID，以及预期工程和版本；安装验收还须确认 healthy 和目标产品行为。 |
| Recovery | 实时 `firmware_mode=recovery` 且为同一 Device ID；更新前确认预期 Recovery 版本及 `capability_names` 中的 `ota`，USB 重连本身不够。 |
| 更新、重启或交接中 | 检查活动 operation 或 takeover 记录，跟随原记录与已选 Device ID 等待完成，不重复写入或改选其他板。 |
| ROM 下载 | 由 `mosaico.py recover` 流程确认实时 ROM 端点；此时没有 ESP-Iris 握手或 Boot ID，初始化前须核对硬件身份。 |
| 离线或未知 | 尚无成功实时握手，或仅有缓存发现与归属；先检查拥有者、活动切换和连接，不能据此认定 ROM 模式、空白 Flash 或设备损坏。 |

Gateway 的 `running` / `reachable` 描述主机进程，不证明设备健康；归属可以在断线后保留。
实际重启后必须确认同一 Device ID 和新的 Boot ID，不能仅凭端口名称或屏幕判断固件模式。
可能破坏证据的恢复操作前，先通过产品工具保存原始日志、结构化证据和有效 core dump。
完整更新验收条件见 [CLI 更新方式](mosaico-cli_CN.md#选择更新方式)。

### ROM 恢复操作

`python mosaico.py recover` 在准备好固件后，通过 Gateway 提交一次 ROM 恢复操作。
操作覆盖端口排他、证据保存、ROM 烧录及重连验证，沿用普通操作记录和查询接口。
执行进程持有物理端口锁；关闭 CLI 或 Gateway 异常退出不会提前释放仍在烧录的端口。
等待超时应按输出的 operation ID 检查进度，不自动重试写入。

不再存在独立 maintenance 状态、维护租约或续期接口。操作结束后保留结果与日志，
释放临时占用；烧录过程中的预期断连仍显示忙碌。只有实时设备证据才能判定需恢复，
普通通信超时不能直接推断固件损坏。旧版租约流程不再兼容，需要统一使用新版主机工具。
