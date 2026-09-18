# 项目会话、设备归属与转让

Gateway 由项目开发会话持有。同一项目内的 CLI、Agent 和 Web 工作台复用
一个持续会话；不同项目拥有独立端口、运行状态、数据库和日志。
设备同一时刻由一个会话使用。共享归属记录和系统锁负责互斥，不需要全局
常驻服务。首版支持同机、同用户下使用本版工具的项目会话。

## 开始开发

在一个终端保持前台会话运行：

```sh
python mosaico.py session run --project projects/hello_world
```

命令打印本项目的 Gateway 地址和 `session_id`。浏览器打开该地址，
在“设置 → 项目会话与设备归属”查看候选端点、归属及转让状态。
Ctrl-C 结束拥有者会话，等待当前操作收尾后退出；关闭其他监控终端不会关闭网关。
崩溃退出通过父子进程管道触发收尾，而非依赖 PID 或最后一次 HTTP 请求。
升级客户端若错过网关退出前的最终响应，会在确认本项目网关已经退出后，
读取本次会话已持久化的完成记录；不会重启网关或重新提交升级。
普通客户端没有关闭网关的 HTTP 接口。收尾最多等待 15 分钟；未解决的操作
或维护租约保留归属记录，后续必须显式核对，不会因超时向其他项目自动开放。

在其他终端选择同一项目：

```sh
python mosaico.py list --project projects/hello_world
python mosaico.py session status --project projects/hello_world --json
```

发现只枚举 USB 描述符、监听 mDNS，不打开未分配设备。首次连接 USB 设备时
可能尚不知道 Device ID；从发现结果选择一个 `endpoint`：

```sh
python mosaico.py device claim --project projects/hello_world --endpoint 'usb:location=<发现结果中的位置>'
```

握手得到 Device ID 后，安装、监控等命令继续通过产品入口运行：

```sh
python mosaico.py install --project projects/hello_world --device-id <Device-ID>
python mosaico.py monitor --project projects/hello_world --device-id <Device-ID>
```

也可以不启动持续会话，直接执行带目标的单次命令：命令创建临时 Gateway，
完成操作和验证后关闭。`monitor` 的临时会话持续至监控结束。
其他命令不会复用一个临时会话；并行开发应先使用 `session run`。
未选择设备的临时会话不会自动占有唯一可见设备。
首次操作可用 `--endpoint` 指定候选端点，之后使用已验证的 Device ID。

TCP 可以使用发现结果或显式 `--endpoint tcp:<地址>:<端口>`。
需要配对时通过 `--pairing-token-file <私有文件>` 提供 64 位十六进制 token。
持续会话可在启动时配置默认 token，`device claim` 也可按端点提供 token。
凭据不写入转让记录，不会由源项目自动交给目标项目。

## 重启与归属

归属独立于连接：OTA、正常固件/Recovery 切换和掉线期间保留归属。
仅当前所有者自动重连，重连前检查共享归属，握手再验证 Device ID。
端口名或 IP 变化不代表新设备；描述符和广播只是候选身份，业务操作以
握手结果为准。重连超时不会自动将设备开放给其他项目。

每次新开发会话生成新的项目会话 ID；它与设备的协议 Session ID 不同。
项目历史中连接过某台设备，并不表示新的会话可以自动连接它。

## 转让设备

先在第二个终端启动目标项目会话，并从其输出获取会话 ID。源项目执行：

```sh
python mosaico.py device transfer --project projects/hello_world \
  --device-id <Device-ID> --to-session <目标项目会话-ID>
```

设备忙于升级、维护或已知异步任务时立即返回忙碌，不后台排队。
转让按“暂停新操作 → 持久保留给目标 → 关闭源连接 → 目标握手验证 → 提交归属”执行。
第三个会话不能在中间窗口抢占；源项目不会自动抢回。
转让不刷机，不迁移正在执行的任务，历史日志仍保存在原项目。

CLI 会打印 `transfer_id`。网络或进程异常时，先查询持久记录：

```sh
python mosaico.py device transfer-status --project <项目路径> --transfer-id <转让-ID>
python mosaico.py device transfer-accept --project <目标项目路径> --transfer-id <转让-ID>
```

同一个转让 ID 可重试，不会重复创建转让。失败或超时不自动回滚。
源会话仍在时，可显式 `device transfer-abort`；已经开始接管的目标会话
必须先结束，以证明其不再持有连接。成功的转让不能回滚，需发起反向转让。

双方都异常退出后，任一参与项目的新持续会话可执行
`device transfer-reconcile --transfer-id ...`，核对双方已退出和端点锁可用后
把未完成转让恢复为当前会话的归属，再显式连接设备。

普通设备占用者崩溃后不会自动抢占。可在持续会话内执行
`device reconcile --device-id ...`（或 `--endpoint ...`）清理已确认失效的
普通归属，再 `device claim`。维护租约不能通过此入口清理；它仍要求原维护
token，避免网关退出后另一个执行器尚在使用设备时误开放端点。

使用结束也可主动释放空闲设备：

```sh
python mosaico.py device release --project <项目路径> --device-id <Device-ID>
```

首次握手失败、尚无 Device ID 时，也可用 `device release --endpoint ...`
释放已认领的端点。`session status` 在没有持续会话时使用一个临时会话完成查询。

## 接口与状态

CLI、Web 和 Agent 使用同一个项目 Gateway 地址。项目控制 API 只接受本机请求；
设备业务继续使用 `/v1/devices/{device_id}/...`，Gateway 在请求进入时核对当前归属。
项目会话 ID 用于持有和转让，Device ID 用于业务路由，Boot ID 用于区分重启。

| 接口 | 输入与作用 |
| --- | --- |
| `GET /v1/project` | 返回当前会话、其他会话存活情况、发现端点、归属和转让记录 |
| `POST /v1/project/acquire` | `device_id` 或 `endpoint`，可同时提供用于握手核对；可选 `pairing_token` |
| `POST /v1/project/release` | `device_id` 或 `endpoint`；先关闭连接再释放空闲归属 |
| `POST /v1/project/transfer` | `device_id`、`target_session_id`、可选 `transfer_id`；协调双方完成交接 |
| `GET /v1/project/transfers/{transfer_id}` | 查询持久转让状态，响应丢失后优先使用 |
| `POST /v1/project/accept`、`abort` | `transfer_id`；分别由原目标接收、原源会话回收 |
| `POST /v1/project/reconcile` | `device_id` 或 `endpoint`；显式核对并清理普通孤立归属 |
| `POST /v1/project/reconcile-transfer` | `transfer_id`；双方已退出后由参与项目的新会话核对恢复 |

底层 `prepare` 接口接收与 `transfer` 相同的字段，只完成保留目标和源端断连；
通常使用完整的 `transfer` 接口。转让依次经过 `preparing → offered → accepting → completed`，
显式恢复可能进入 `aborted`。归属的 `maintenance` 状态由 Recovery 租约管理，
不等同于设备离线。错误返回中，`400` 表示参数错误，`409` 表示归属冲突或忙碌，
`504` 表示等待超时；超时不代表已释放设备或转让已撤销。

## 运行数据与兼容性

- 项目键包含规范化工作区路径和应用路径，区分不同项目及 worktree。
- 用户状态目录的 `esp-mosaico/project-sessions/<项目键>/` 保存项目数据库、
  Gateway 原始日志和当前连接信息；`esp-mosaico/ownership/` 保存共享协调记录。
- 工作区原有运行日志继续由配置的 `run_dir` 保存。
- 工程固定自己的 ESP-Iris 版本。复用会话时核对版本和会话身份，
  不会因为不兼容而停止其他实例。切换工具版本后应结束旧会话再启动。
- 旧的共享 Gateway 不会被自动关闭，原日志也不会迁移或删除。迁移时应先
  结束旧网关，再使用项目会话。旧版自动连接网关不支持转让保留协议，不能
  混用并声称具备同样的归属保证。
- 项目会话 API 只接受本机请求。跨电脑协商设备归属不在本版范围内。
- 本地项目端口自动分配，原 `MOSAICO_LOCAL_GATEWAY_URL` 不再选择本地实例。
  显式远程 `--gateway-profile` 仍走已有流程，不参与本机项目转让协调。

实机验收应通过 `mosaico.py` 执行，核对 CLI 与 Web 中的 Device ID、Boot ID、
操作 ID 和转让记录。模拟连接及主机测试不能替代真实 OTA/Recovery 验收。

已完成的实机记录见[2026-09-18 双设备项目 Gateway 端到端验收](validation/project-gateway-e2e-2026-09-18.md)。
Windows 补测及本轮修复见[2026-09-18 Windows 双设备验收](validation/project-gateway-windows-2026-09-18.md)。
