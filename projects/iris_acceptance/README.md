# ESP-Iris 实机验收夹具

用于 IRIS-A02（慢 RPC 不阻塞协议控制）和 IRIS-I01（错误返回长度有界处理）的本轮验收。
项目沿用 `hello_world` 的 retained Recovery 分区、PSRAM 配置和
`esp_mosaico_app_recovery`；没有屏幕/BSP/LVGL 依赖。正常固件关闭 OTA writer，
使用 `iris_ota_support_start()` 保留 enter-Recovery RPC、System Inventory，
启动约 3 秒后由原有组件自动报告 healthy。

当前采用固定系统前缀 2 MiB 的 `mosaico-retained-recovery-2m-v1` 契约；
Recovery 分区为 1.75 MiB。此前验收报告记录的是旧布局，不能作为当前布局的
实机验收证据。旧布局设备须先完成保留数据的产品迁移，不能直接安装本构建；
普通安装仍校验实际分区表摘要。

从仓库根目录通过 `python mosaico.py install --project projects/iris_acceptance`
安装；空白或未验证设备须先执行 `python mosaico.py recover`。
使用 `python mosaico.py monitor --timeout 20` 观察
`IRIS_READY ... mode=normal writer=0`、`IRIS_OTA_HEALTHY` 和
`ACCEPTANCE_ALIVE service=0x6a02`。安装后须记录同一 Device ID 的
normal → Recovery → normal、新 Boot ID、目标 ELF SHA 和 healthy 证据。
这些是可重复执行的验收步骤；2026-09-06 的目标板实跑结果见
[修复与验收报告](../../docs/esp-iris-fix-acceptance.zh-CN.md)。

## 专用 RPC

Service ID 为 **0x6A02（27138）**，所有方法请求 body 均为空（0 字节）。
返回值使用小端 u32。所有计数在设备重启后归零。

| Method ID | 行为 | 验收预期 |
| --- | --- | --- |
| 1 | 输出 `SLOW_BEGIN`，等待约 2000 ms，增加完成计数并输出 `SLOW_END` | deadline ≥ 5000 ms 时成功返回 4 字节完成计数；慢调用期间 CONTROL PING/STATUS 仍应及时响应 |
| 2 | 不访问 response 缓冲区，只报告 `response_capacity + 1` | 返回 `ESP_ERR_INVALID_SIZE`，响应 body 长度为 0；无崩溃、无越界、无 Boot ID 改变 |
| 3 | 返回三个 u32：慢调用完成次数、超长报告次数、本方法调用次数（含本次） | 返回 12 字节；方法 2 后仍能调用成功，确认进程继续工作 |

方法 1 的短 deadline（例如 100 ms）用于观察超时边界：回调一旦开始，等待和
计数副作用不会被强制撤销；它完成后协议可返回超时。不要因超时重放该 RPC。
RPC 方法 3 本身也进入同一 RPC 执行队列，不能用它代替 CONTROL PING/STATUS
判断协议线程是否阻塞。若 Gateway 在主机端串行等待所有请求，应区分主机排队
延迟和设备控制响应延迟；实机记录应包含发送时刻、回复时刻及该调用的 request ID。

方法 2 只故意违反回调的返回长度约定，并不执行实际越界写；用于验证框架边界，
不会伪造真实内存损坏。设备操作与观察保持由 `mosaico.py` 和其 Gateway 管理，
不要另开 USB/串口会话抢占 Gateway。

## 可选 Wi-Fi

默认不编译或启动 Wi-Fi。需要 TCP 验收时，构建进程环境变量
`IRIS_ACCEPTANCE_WIFI_HEADER` 指向仓库外或 `.agents/` 下现有私有头文件的绝对路径。
该私有头文件定义两个 C 字符串宏：`IRIS_ACCEPTANCE_WIFI_SSID` 和
`IRIS_ACCEPTANCE_WIFI_PASSWORD`。不要把实际凭据写入本目录、sdkconfig 或提交记录。

私有头文件直接参加编译，不复制到项目；SSID/密码只配置到 `WIFI_STORAGE_RAM`，
不写入 NVS。连接成功仅输出 `ACCEPTANCE_WIFI_READY ip=...`，不打印凭据。
编译产物仍会包含凭据，按私有验收产物保存。TCP pairing 保持参考项目默认开启。
改变或取消该环境变量后，应重新配置构建目录；Gateway 仍按单个物理所有者规则
管理 USB/TCP 连接，不应同时抢占两个入口。
