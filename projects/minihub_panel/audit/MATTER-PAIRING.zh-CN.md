# Matter 配对专项对照

对照提交没有变化：

- 移植面板 `feat/minihub-panel`：`aa51ca03bb9f5e7682dc98cae0081619a7cb9033`
- 官方 `espressif/esp-minihub` `master`：`7cf43bf39e754a9f093937fa14c7be696c475f93`
- 参考用的 ESP-Matter `main` `84c242d260151c144281f15d6bb9c684bf9039fb` 与 Vibe `d6aa4a8d3277b0c7ad3d38e54c403f2388814d1e` 也未变

没有改仓库源码，没有提交、推送或 PR，没有操作硬件。主机检查只在 `/tmp/audit-refs/matter-status-harness.c`，日志在 `/opt/cursor/artifacts/minihub-audit/logs/matter-status-harness.txt`（`HARNESS_OK`，退出码 0）。该程序按源码抄出状态转移并断言，不链接 CHIP，也不能证明某台灯为什么失败。

## 结论

**部分复用，不是完全一致。** 配网引擎、REST 入口和异步完成回调直接用官方 `minihub_matter` / `minihub_web`。面板自己决定发现方式、Wi-Fi 凭证从哪来、多久算超时，以及屏幕上写什么字。官方 Web 也有“失败原因被截短、阶段和超时不完全对齐”的源码行为。不能由此推出官方实机同样失败，也不能从这些差异断定用户那几次失败的根因。没有设备日志。

## 对上一条审查结论的更正

1. **“离线时配对命令可能静默丢失”对屏幕操作不成立。** 屏幕走 `app_hub_add_device`。Wi-Fi 未置 `HUB_CONNECTED_BIT` 时，函数在入队前写入 `Network is not ready` 并返回 `APP_HUB_ERR_INTERNAL`。
   https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L3496
   Worker 里 `HUB_CMD_ADD_DEVICE` 若在出队时仍无连接，也会写同一句。
   https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L2946
   静默丢掉只发生在 `HUB_CMD_MATTER_COMMISSION`：无连接则既不调用也不写失败。
   https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L2936
   `app_panel.c` 不调用 `app_hub_matter_commission`。用户在屏上点添加，不会走这条静默分支。

2. **“配网等待可达 300 秒占住 hub_client”不成立。** 300000 ms 是官方看门狗和面板截止时间的常量，不是 `hub_client` 里的一次阻塞等待。
   - 官方 `minihub_matter_commission_wait()` 若被调用，会在调用者任务上 `xSemaphoreTake` 直到回调或超时。头文件写明不要从 httpd 调用。
     https://github.com/espressif/esp-minihub/blob/7cf43bf39e754a9f093937fa14c7be696c475f93/components/minihub_matter/include/minihub_matter.h#L61
     https://github.com/espressif/esp-minihub/blob/7cf43bf39e754a9f093937fa14c7be696c475f93/components/minihub_matter/matter_ctrl.cpp#L1135
   - 官方 `POST /api/matter/devices` 明确不调用它，注释是 “Never block httpd on commission_wait”。它调用 `commission_code_ex` 后立刻返回 `commission: started`。
     https://github.com/espressif/esp-minihub/blob/7cf43bf39e754a9f093937fa14c7be696c475f93/components/minihub_web/minihub_web.c#L4939
   - `commission_code_ex` 用 `esp_timer` 单次定时器做看门狗，回调里写 `commission timed out`。默认定时是 `CONFIG_MINIHUB_MATTER_COMMISSION_WAIT_MS`，缺省 300000。头文件仍有一处写 “180s”，与实现不一致。
     https://github.com/espressif/esp-minihub/blob/7cf43bf39e754a9f093937fa14c7be696c475f93/components/minihub_matter/matter_ctrl.cpp#L75
     https://github.com/espressif/esp-minihub/blob/7cf43bf39e754a9f093937fa14c7be696c475f93/components/minihub_matter/matter_ctrl.cpp#L365
   - 面板 `process_matter_commission` 只发一次 HTTP POST，然后返回。截止时间记在 `add_deadline`，由 `update_add_progress` 用 tick 比较。`commission_active` 时 worker 每次 `ulTaskNotifyTake` 约 1200 ms，中间穿插状态 GET。
     https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L1978
     https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L3026
   - `commission_code_ex` 里的 `ensure_dnssd_ready` 最多 `vTaskDelay` 约 3000 ms，跑在处理该 POST 的 httpd 任务上，不是 300 秒，也不是 `hub_client` 的整个配网窗口。面板 HTTP 客户端超时是 5000 ms（`HUB_HTTP_TIMEOUT_MS`）。

3. **栈和套接字仍只是资源风险，不能当作这几次失败的根因。** 本次没有 coredump、没有套接字计数、没有配网日志。

## 链路

屏上提交：`submit_add_field` → `app_hub_add_device` → 队列 `HUB_CMD_ADD_DEVICE` → `process_add_device` → `process_matter_commission`。

POST 体由面板写死：`pairing_code`、`endpoints=1`、`merge/reload=true`、`discovery=ble_wifi`、`transport=wifi`，并附上本机 STA 的 SSID/密码。
https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L2112

官方 Web `submitMatterAdd` 打同一个 URL。默认发现方式不是 Wi-Fi：`openMatterAdd` 把 Wi-Fi 开关设为关，`matterDiscoveryValue` 在开关关闭时返回 `ble_thread`（`ble_thread` 状态位为 false 时改为 `network`）。只有用户打开 Wi-Fi 开关才发 `ble_wifi`，并且要求手填 SSID。
https://github.com/espressif/esp-minihub/blob/7cf43bf39e754a9f093937fa14c7be696c475f93/components/minihub_web/web/js/overview.js#L1837
https://github.com/espressif/esp-minihub/blob/7cf43bf39e754a9f093937fa14c7be696c475f93/components/minihub_web/web/js/overview.js#L2009

之后两边进入同一处理函数 `handle_api_matter_devices`：preflight、写入 waiting 卡片、`commission_code_ex`、`set_pending_device`、立刻 JSON 返回。真正的 PASE / 成功 / 失败在 CHIP 回调里，再由 `matter_finalize` 任务写 `commission_phase`。成功或失败都不在 HTTP 处理函数里等到配网结束。

Zigbee 版面板没有可用的 Thread BR（`minihub_thread` 是桩，`CONFIG_MINIHUB_THREAD_ADAPTER` 未开）。这条固件上的 Matter 只可能是 **Matter over Wi-Fi 的首次 BLE 配网**。Thread 灯在这版上会在 preflight 被拒绝，如果有人改成 `ble_thread`。面板没有提供这个选项。官方完整 Host 可以做 BLE+Thread、BLE+Ethernet 或已在网上的 `network`。

## 对照

| 环节 | 官方源码 | 移植 | 归类 |
| --- | --- | --- | --- |
| `matter_ctrl.cpp` 配网、回调、看门狗、status JSON | 实现在官方组件 | 工程直接编入该组件，面板不重写 | 直接复用 |
| `POST /api/matter/devices` 异步启动 | httpd 不等 `commission_wait` | 面板只是这个接口的客户端 | 直接复用 |
| 发现方式 | Web 默认 `ble_thread` 或 `network`；Wi-Fi 要手开并填 SSID | 永远 `ble_wifi` + 本机 STA 凭证 | 移植新增策略 |
| 看门狗时长 | Kconfig 默认 300 s，`esp_timer` 回调 | 同一 Kconfig；面板另外用 300 s tick 截止 | 底层相同；面板多一层截止 |
| Web 轮询 | `pollMatterCommissionCard` 只等到 180 s，然后 toast 超时；后端看门狗仍到 300 s | 约 1.2 s 轮询 `GET /api/matter/status`，截止 300 s | 官方 Web 自己更早放弃提示；面板等到看门狗 |
| 阶段文案 | `discovering` / `binding` / `activating` 反映到卡片和 toast | 字符串表有 Binding/Activating，渲染时标题始终是 “正在发现...” | 移植把阶段藏起来 |
| 启动失败文案 | toast 带 `message`，最多 80 字符 | 具体 `message` 被改成 “Could not start device add” | 移植把原因抹掉 |
| 进行中的 CHIP 错误 | `last_error` 最长 127 字符，卡片注释写明适配器字段约 96 字节 | 拷进 `commission.error` / `add.status`，长度 64（可见 63） | 两边都截断；面板更短 |
| 失败标题 | Web 用 “commission failed” 加阶段和错误 | 标题固定 “入网失败”，副文案才是 status | 移植更笼统 |
| 取消 | `cancel` 或 `all:true` | 超时或取消时 POST `cancel` 并按 `wifi.matter_n{id}_ep1` 删除 | 复用取消接口；点 ID 写死 ep1 |
| WebSocket | 官方配网进度靠 HTTP 轮询，不靠 WS | 本机 Hub 不启面板 WebSocket | 这条链路上不是 WS 丢事件 |
| 凭证 | BLE+Wi-Fi 用请求里的 SSID，缺了再读 provision NVS | 面板在 GOT_IP 时写入 provision，POST 时再带 STA | 复用同一注入口；来源是本机已连网络 |
| 栈 / httpd 20 套接字 | 官方 web 写死 20 套接字、16 KB 栈 | 面板 sdkconfig 里的 4 套接字和外部栈不是合法符号 | 不能当成已证实的失败原因 |

## 状态为什么看起来不清楚（源码可确认，不是实机根因）

进行中：`render_add_page` 只要还在配网，标题就写 `COPY_ADD_DISCOVERING`。`COPY_ADD_BINDING` 和 `COPY_ADD_ACTIVATING` 只有定义，没有引用。副文案是 `add.status`，启动成功后被设成 `Waiting for device`，阶段变化不会改这句。
https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_panel/app_panel.c#L314
https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_panel/app_panel.c#L3150

官方在 `matterCommissionLabel` 里用 `/api/matter/status` 的 `commission_stage` 显示发现、绑定、激活。
https://github.com/espressif/esp-minihub/blob/7cf43bf39e754a9f093937fa14c7be696c475f93/components/minihub_web/web/js/overview.js#L1910

启动被拒：`process_matter_commission` 会把服务器 `message` 放进 `commission.error`，随后 `process_add_device` 看到 `FAILED` 就把添加状态改成 `Could not start device add`。屏幕副文案用的是添加状态，所以 “ble_wifi needs wifi_ssid” 或 “matter commissioner not ready” 到不了标题下面。主机模型已断言这次覆盖。
https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L1936
https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L1972

配网进行中的失败：`update_add_progress` 会把 `commission.error` 抄进添加状态，这时副文案有内容，但最长 63 字符，标题仍是 “入网失败”。官方失败回调先把原因格式化进 96 字节缓冲再写入 128 字节 `s_last_error`，Web toast 再切到 80 字符。官方源码同样不把完整 CHIP 字符串原样展示。

队列满：`app_hub_add_device` 返回 `APP_HUB_ERR_BUSY`。`submit_add_field` 对 BUSY 仍把 `add_commissioning` 设为 true。命令没有入队，界面却进入配网页。这是移植自己的反馈错误。

面板 HTTP 5 秒超时是尚未在设备上测量的风险：`ensure_dnssd_ready` 最多等约 3 秒，再加上持 CHIP 锁调用 `pairing_code_wifi`。若整次 POST 超过 5 秒，客户端会当成 “Commission request failed”，服务器可能已经启动配网。官方浏览器没有这 5 秒限制。这只是时序上的可能，没有抓包或日志证明用户失败就是它。

## 官方是否也有这些问题

- **源码里有、不能说实机也失败：** 失败字符串被截断；DNS-SD 超时经常不进失败回调，要靠看门狗（官方注释写了）；Web 轮询 180 秒就提示超时，看门狗却是 300 秒；`commission_wait` 的头文件还写着 180 秒。
- **官方 Web 没有、面板有：** 标题永远停在 “正在发现”；启动失败被换成 “Could not start device add”；发现方式锁死 `ble_wifi`；用本机 STA 而不是让用户确认要下发给灯泡的 SSID；5 秒 HTTP 客户端超时；队列满仍进入配网页。
- **实机同样失败：** 没有日志，不能下这个判断。

## 修改建议（只标位置，不改代码）

1. 高：`app_panel.c` 的 `render_add_page` 读取 `app_hub_matter_commission_snapshot`，按 `discovering/binding/activating` 用已有的三句文案。失败副文案用 `last_error`，不要在 `process_add_device` 里覆盖成 “Could not start device add”。把 `APP_HUB_STATUS_LEN` 提到至少 128，或副文案允许两行。
2. 高：`process_matter_commission` 的 `discovery` 不要写死。Zigbee 镜像只提供 Matter over Wi-Fi，并在界面写明 “灯泡将加入当前 Wi-Fi”。Thread 设备应明确不可用，而不是配到一半失败。下发前展示 SSID（不要展示密码）。
3. 中：`HUB_HTTP_TIMEOUT_MS` 对这次 POST 单独加长，或 POST 返回前不要把客户端超时当成配网失败；超时后先 `GET /api/matter/status`，若已是 `running` 就继续轮询。
4. 中：`submit_add_field` 在 `APP_HUB_ERR_BUSY` 时显示 “队列已满”，不要进入配网页。
5. 低：官方 Web 若要一致，把 `pollMatterCommissionCard` 的 180000 改成与 `MINIHUB_MATTER_COMMISSION_WAIT_MS` 相同，并修正头文件里的 180 秒注释。这是上游问题，不该在面板里私自改 `matter_ctrl.cpp`。

## 可验收的测试

主机（已有模型，可再接到真实 `app_hub` 桩）：

- Wi-Fi 未连接时添加 Matter：状态是 `Network is not ready`，不是沉默。
- 服务器返回 `message=ble_wifi needs wifi_ssid...`：副文案保留这句，而不是 “Could not start device add”。
- 状态依次为 discovering、binding、activating：标题三句都出现。
- 129 字符错误：界面保留前 127 字符或明确省略号，不能只剩 63 字符还无省略标记。
- 队列满：不进入 “正在发现”。

实机（本次未跑）：

- 2.4 GHz、手动 11 位码、灯处于配网广播：日志里有 `commission BLE+WiFi`，随后 stage 从 discovering 到 binding 或失败回调。
- 错误码：数秒内 `result=error`，屏幕出现 `last_error`，不留下永久 waiting 卡。
- 当前 Wi-Fi 与灯泡不能加入的网络：preflight 或 commissioning failure 的原文可见。
- Thread 灯：界面说明本镜像不支持，而不是转圈 300 秒。

## 请用户采集的最小日志

不要提供密码、配对码、Bearer token 或完整 SSID。每条保留时间戳和这些字段即可：

- `app_hub`：`Matter commissioning`、`Commission request`、HTTP 状态码、`commission=`、`stage=`、`node_id`、`err=`
- `minihub_matter`：`commission BLE+WiFi` 或 `commission refused`、`pre-commission Dnssd`、`commission stage ->`、`PASE session failed`、`commission failed at`、`commission watchdog`
- `minihub_web`：`/api/matter/devices` 的结果是 `started` 还是 `error`，以及 `message` 去掉凭证后的文本
- 重启原因：是否 panic，以及 panic 所在任务名（不要整段 coredump 里的密钥）

有这些字段才能区分 “POST 没发出去”、“preflight 拒绝”、“CHIP 回调失败” 和 “看门狗超时”。现在这四类在屏幕上都可以变成含糊的 “入网失败” 或一直 “正在发现”。
