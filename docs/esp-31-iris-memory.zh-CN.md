# ESP-31：ESP-Iris 内存占用分析

本文保留 ESP-31 阶段的历史基线，不代表当前配置。2026-09-16 的
正常应用 USB/RPC/截图/镜像峰值优化与实机验收见
[25 KB 内部 RAM 验收报告](esp-iris-internal-budget.zh-CN.md)。

本次范围是正常应用中的 ESP-Iris 及其 USB CDC 传输依赖。基线为工作区
`ebaf1d7`、Utils 子模块 `4159735`，在支持 ESP32-S31 的
ESP-IDF `v6.2-dev-2221-g7b9cc1ac79f` 上构建 `projects/hello_world`。
下表取自构建 ELF 的符号大小；地址均在内部 DRAM。这是所列符号的合计，
不是整个应用的 RAM 总量，也不包含任务控制块、堆分配和 PSRAM。

| 常驻符号 | 原始 8,192 B CDC 配置 | 当前 2,048 B CDC 配置 | 说明 |
| --- | ---: | ---: | --- |
| `g_iris` | 9,224 B | 1,040 B | 两个帧缓冲改为 PSRAM 指针，静态内部 RAM 减少 8,184 B |
| `g_iris_log_storage` | 4,124 B | 4,124 B | 4,096 B 日志环和 28 B 元数据；保留 Core Dump 前日志 |
| TinyUSB `_cdcd_epbuf` | 16,384 B | 4,096 B | CDC DMA 端点缓冲 |
| TinyUSB `_cdcd_itf` | 16,628 B | 4,340 B | CDC RX/TX FIFO 和接口状态 |
| stdout/stderr、executor、文件卷等选定符号 | 1,216 B | 1,216 B | 这组符号未改动 |
| **选定常驻符号合计** | **47,576 B** | **14,816 B** | **内部 DRAM 减少 32,760 B** |

按完整链接归档统计，当前 `libesp_iris.a` 的内部 DRAM 为 6,620 B，
TinyUSB 核心 `libespressif__tinyusb.a` 为 9,563 B，ESP-IDF TinyUSB
封装 `libespressif__esp_tinyusb.a` 为 199 B，三者合计 16,382 B。
基线合计为 49,142 B，同样减少 32,760 B。归档统计比上表的选定
符号多计入其余静态变量和对齐开销。

`libesp_iris.a` 的 14,804 B 不能整体改成片外 RAM；静态符号的
可迁移性要按用途判断：

| 内部 DRAM 部分 | 字节 | PSRAM 判断 |
| --- | ---: | --- |
| `g_iris.rx_wire`、`g_iris.tx_wire` | 原占 8,192 | **已迁移**：`esp_iris_start()` 从 PSRAM 分配连续 8,192 B，停止时释放。正常应用的静态 `g_iris` 因两个指针增加 8 B，净省 8,184 B 内部 DRAM。慢速 RPC 的独立 runtime 工作副本也有各自的 PSRAM 帧缓冲。 |
| `g_iris` 其余状态和两个指针 | 1,040 | 包含任务句柄、连接与会话状态、锁、设备身份和崩溃状态；USB 回调也读取任务句柄。继续保留内部。 |
| `g_iris_log_storage` | 4,124 | **技术上可以**：现有 `CONFIG_ESP_IRIS_LOG_RING_STORAGE_PSRAM` 加 `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` 的试验构建已确认它进入片外 Core Dump 段。但 retained-Recovery 契约检查要求正常应用将崩溃前日志留在内部 RAM；PSRAM、cache 或 MSPI 故障还可能使这份证据不可用，因此最终配置仍保留内部。 |
| 其余 ESP-Iris 静态变量 | 1,456 | 包括文件卷表、executor、stdio 缓冲和崩溃相关状态等。部分零初始化对象可以单独评估放入片外 BSS，但不能把整组一概迁移，且省下的空间有限。 |

正常应用已启用 `CONFIG_SPIRAM_XIP_FROM_PSRAM`；按当前 ESP-IDF 文档，
通常的 SPI1 Flash 操作在这种配置下不会关闭 cache，因此正常应用
启用 `CONFIG_ESP_IRIS_WIRE_BUFFERS_PSRAM`。但崩溃、cache/MSPI 故障和其他可能
关闭 cache 的路径仍需要单独验证。普通 `xTaskCreate` 的栈和 TCB
默认使用内部 RAM；允许片外栈也要核对执行 Flash 操作的任务。
TinyUSB DMA 端点缓冲在本项目的 `tusb_config.h` 中明确标记
`DRAM_ATTR`，不在上述 14,804 B 中，也不应和协议帧缓冲一起搬。

目前协议允许 4,000 B payload 和 4,096 B wire frame，所以 `g_iris`
中的两个帧缓冲不能只靠改应用配置缩小。正常应用原来将 TinyUSB
`RX`、`TX` 和 `EP` 都设为 8,192 B；现在三者均设为 2,048 B。
ESP-Iris 从 CDC FIFO 分段读取，在 PSRAM 的 4,096 B 缓冲中组装帧，
TX 数据先复制到 TinyUSB 的内部 FIFO，再由内部端点缓冲送往 DMA；
TX 支持部分写入后继续排队，TinyUSB 的 `RX >= EP` 约束仍满足。
端点缓冲属于 DMA 路径，继续保留在内部 DRAM。2,048 B 可能降低
突发吞吐，仍需实机压力测试。与原始基线相比，DMA 端点缓冲的
12,288 B 差值在初始化数据区，FIFO 的 12,288 B 差值位于 BSS。

除常驻符号外，ESP-Iris 的动态占用还包括：一个 3,328 B 的常驻协议任务栈；
首次慢速 RPC 操作会创建 6,144 B 的 service 栈和一份 1,040 B
的内部 runtime 工作副本，以及这份副本自己的 8,192 B PSRAM 帧缓冲；
空闲 1 秒后释放。与旧版 9,224 B 内部工作副本相比，活跃时再省
8,184 B 内部堆。媒体通道使用最多三个按需分配
的 3,840 B 最新块缓冲；`gsp_hello` 现将它们分配在 PSRAM，媒体
发送前仍复制到 PSRAM wire frame，再复制到内部 CDC FIFO，不直接交给 USB DMA。这样镜像活跃时
每个通道可再省 3,840 B 内部堆，空闲时这些缓冲原本就不分配。
注册文件卷后才创建 6,144 B 的文件任务栈、
3,888 B 工作上下文和两个深度为 1 的队列（队列元素分别为 1,064 B
和 1,068 B）。正常的 `hello_world` 和
`gsp_hello` 不注册文件卷。RPC 服务状态合计 1,776 B，内含 1,024 B 响应体缓冲，
注册了 recovery RPC 后会留存。分配器开销和底层 USB 任务另计。

可直接省掉的部分应先看启用条件：正常应用已关闭 ESP-Iris OTA writer，
文件服务在没有注册卷时不创建任务或队列，媒体缓冲在没有活跃流时
也不分配。若产品只需要 USB，可关闭 ESP-Iris TCP/配对及相关网络
依赖；目前正常应用有意保留 USB 与 TCP 两种可用传输。RPC 服务状态承担三个 retained-Recovery 控制处理器，
不能直接移除。日志环可缩小或经现有 Kconfig 放入 PSRAM，但这样会
减少或降低崩溃前日志的可靠性，当前保留 4,096 B 内部日志环。
协议任务、service/文件任务栈及 USB DMA 端点缓冲继续留在内部 RAM：
ESP-IDF 对 Flash/cache 关闭期间的 PSRAM 访问和 PSRAM 任务栈有限制，
Recovery 的 OTA/系统更新尤其会触及这些路径。详见
[ESP32-S31 片外 RAM 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-guides/external-ram.html)。

现有状态接口提供协议任务栈最低余量、内部堆当前/历史最低空闲量，
`CONFIG_ESP_IRIS_TASK_MEMORY_OBSERVATION` 可按需读取所有任务的栈水位。
但正常应用没有启用后者，当前工作区的 `mosaico.py list` 又因本机
Gateway 与固定 Utils 修订不一致而拒绝操作（2026-09-15 再次查询仍如此），
故没有与该基线对应的实机
栈水位。旧的 ESP-Iris 实机报告记录 OTA 中断路径协议任务最低剩余
540 B、文件空间耗尽路径 816 B；其固件修订和配置不能视为本次构建
的实测值。协议任务现在已是 3,328 B，Kconfig 下限为 3,072 B。
贸然再减 256 B 会使旧报告中的较低余量仅剩 284 B。service 和文件
任务的活跃路径同样缺少水位，因此本次不下调任务栈。

用于后续测量的 `iris_acceptance` 固件夹具已启用 FreeRTOS trace 和
ESP-Iris 任务内存观测。取得兼容 Gateway 与空闲设备后，应在慢速 RPC
运行期间采样，分别记录协议任务和 service 任务的最低剩余栈；文件任务
需要带文件卷的夹具测量。再比较 USB 连续请求、重连、日志拥塞与
Recovery 往返时的内部堆最低余量和实际吞吐。只有覆盖这些路径并设定
最低安全余量后，才能进一步下调对应任务栈。

开启全任务内存观测时，设备每次查询暂时分配一个 1,036 B 的响应
缓冲和 128 个 `TaskStatus_t` 记录（本次 IDF 中每个 36 B，
合计 4,608 B）。这会影响采样瞬间的最低堆余量；正常应用关闭该功能，
它不属于常驻产品开销。

`static_internal_bytes` 状态字段目前统计 `g_iris` 和部分 ESP-Iris 静态
状态，未计入 Core Dump 日志环、TinyUSB CDC 缓冲和底层 USB 驱动；
不能拿它当作上述总量；`g_iris` 缩小后的数值会反映在这个字段中，
但 PSRAM 分配不计入。上述 ELF 符号比较给出了本次固定 RAM 差值。
Recovery 仍保留 8,192 B CDC 缓冲和 6,144 B 协议任务栈，因为它执行
高吞吐的 OTA/系统更新写入；未经实机写入与恢复闭环验证，不把正常
应用的 2,048 B CDC 配置推广至 Recovery。新增的媒体和协议帧 PSRAM
选项都被 Kconfig 限制为无 ESP-Iris Flash writer 的应用，并保持默认关闭。

在当前 `origin/main`（含 ESP-33）上，`hello_world`、`gsp_hello`、
`iris_acceptance`、`iris_crash` 和 Recovery 均完成 ESP32-S31 构建，构建工具
报告的编译告警均为零；ESP-IDF 自身另有组件依赖检查的 CMake 告警。
媒体 PSRAM 编译路径的 C 主机运行测试在 ASan/UBSan 下通过，保留
Recovery 契约和崩溃夹具的六项本地检查通过。USB 突发吞吐、
端点重连、日志拥塞和正常 → Recovery → 正常闭环仍需实机验收。
