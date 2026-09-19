# 项目 Gateway、使用者与设备归属

同一系统用户、同一工作区、同一应用路径共享一个 Gateway。不同项目有独立的
地址、日志和操作记录，通过用户级公共注册表与操作系统锁协调设备归属。
这些信息描述主机项目会话，设备的 Device ID、Boot ID 仍以实时握手和查询为准。

## 开始和结束调试

```sh
python mosaico.py iris run --project projects/hello_world
```

每次 `iris run` 都登记一个独立客户端，已有实例时也保持前台运行。打印的 URL
可打开 Web 工作台。其他终端或 Agent 的设备命令自动复用同项目实例。
普通命令在执行期间持有客户端；应用构建、安装及等待重连也属于执行期。
工作台的事件连接持有一个客户端，页面内部多个面板不会重复登记。

Ctrl+C 结束当前 `iris run` 的客户端，不关闭其他人使用的 Gateway。
普通命令结束同样只释放自身客户端。所有客户端离开且活动请求、更新、设备 Job、
维护或流任务都已结束后，Gateway 空闲 **10 秒**自动退出。新使用者或工作加入会
取消倒计时。没有单独的停止命令，设备被认领本身不构成永久保活。

CLI 每 5 秒续期一次，20 秒未续期后客户端失效，再进入 10 秒空闲等待。
工作台连接通过 WebSocket 心跳判断存活。刷新和短暂断线不会立刻结束 Gateway。
关闭客户端不等于取消已经提交的设备写入；后台仍完成处理并保留操作结果。

Gateway 已退出时，工作台不能自动唤醒它。重新执行 `iris run` 或设备命令后，
使用新输出的工作台 URL。Gateway 使用动态端口，旧地址不保证可复用。

## 查看谁在使用

```sh
python mosaico.py iris status --project projects/hello_world
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

## 跨项目协调

公共注册表位于同用户状态目录下的 `esp-mosaico/ownership/ownership.sqlite3`。
Linux 使用 `XDG_STATE_HOME`，未设置时使用用户的 `.local/state`；macOS 使用
Application Support；Windows 使用 LOCALAPPDATA。不同用户、不同主机或隔离
状态目录不共享此协调视图。独立或远程 Gateway 也不自动参与。

一个设备同一时刻归属于一个项目会话。其他项目不能自动抢占，HTTP 超时也不能
证明拥有者死亡。项目锁防止重复实例，会话锁判断存活，端点锁保护实际连接。

先在接收项目运行 `iris run`，通过 `iris status --all` 获取其 Session ID，然后在
发送项目执行：

```sh
python mosaico.py iris transfer start --device-id '<Device-ID>' --to-session '<接收会话-ID>'
```

活动转让持有接收方客户端直到身份验证结束；归属预约保护断开到重连的间隔。
转让中断时，使用原 `transfer_id` 查询、接受、中止或核对，不重复创建转让。
没有正在执行的转让请求时，待核对记录本身不永久保活，但仍保留资源预约。

正常空闲退出释放普通设备归属。崩溃留下的普通归属需要显式 `iris reconcile`；
维护和转让预约必须使用各自的恢复流程。Gateway 重启不会自动重放设备写操作。

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
