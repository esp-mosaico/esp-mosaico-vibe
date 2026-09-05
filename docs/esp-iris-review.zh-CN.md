# ESP-Iris 作为 ESP32 默认烧录与调试流程的评估

评估日期：2026-09-05。配套文档：[esp-mosaico-tools 评估](esp-mosaico-tools-review.zh-CN.md)。

## 1. 结论与适用边界

ESP-Iris 已有成为统一设备开发入口的基础：二进制分帧、稳定 Device ID、Boot ID、日志与事件、崩溃证据、RPC、OTA、文件服务，以及同一 Gateway 下的 CLI 和 Web 工作台。建议继续以它作为统一控制与观测层建设，但当前版本尚不适合直接宣布为整个 ESP32 产品族的默认生产级烧录与调试基线。

首先需要解决的具体问题是：RPC 响应长度错误处理仍会进入越界复制、合并接收的短请求可能丢响应、操作幂等键未绑定请求内容、更新结果未知的处理不完整，以及普通 OTA 缺少自动保存旧崩溃证据的步骤。网络链路安全、跨芯片适配和可重复的测试矩阵也是推广门槛。

“默认流程”应定义成一套统一编排，而不是要求每个阶段都使用 Iris 设备协议：

| 场景 | 应有的默认处理 | 当前边界 |
| --- | --- | --- |
| 正常应用开发 | Gateway 独占设备链路，统一日志、RPC、更新和证据 | 已有主要能力 |
| 应用不能健康启动 | 保留 Recovery 提供维修入口 | 依赖产品分区、启动和健康验收策略 |
| 空白设备或启动链损坏 | 同一产品入口管理 ROM 引导和初次配置 | Iris 固件尚未运行，必须由主机适配层处理 |
| 断点、单步、寄存器、CPU 停机 | 编排 JTAG/OpenOCD/GDB，关联固件与设备身份 | 当前 Iris 服务集合未实现这些调试功能 |
| panic 后分析 | 保存 Core Dump，匹配崩溃固件 ELF，离线解析 | 已有读取与归档基础，完整诊断链需加强 |

ESP-IDF 的 JTAG 调试由 OpenOCD/GDB 等工具承担；Iris 运行时日志和 RPC 不能等价替代 CPU 停机调试。[官方 JTAG 指南](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/jtag-debugging/index.html)

## 2. 评估基线与证据等级

| 项目 | 本次基线 |
| --- | --- |
| 宿主仓库 | `1b31669e888ca563e33f05c4f652c838ddce719c` |
| esp-mosaico-tools | `3ab3f7ff44b902e9c3b0d70409f364351dc6cf7b` |
| ESP-Iris | `b484e47df74b105713cdec466c7622cd664a2c77` |
| 组件声明 | `version: 0.1.0`；ESP-IDF `>=5.5` |
| 本机测试环境 | Windows、Python 3.8.7、pytest 8.3.5、MinGW `cc`、Node 24.19.0 |
| 改动范围 | 评估文档与 `.agents/iris-audit-20260905/` 私有验证材料；未修改产品实现 |

两个子模块原本已初始化且源码工作区干净。本报告评估上述检出版本，不将本地源码等同于上游最新版本。外部资料仅用于核实 ESP-IDF 的更新、USB 和调试边界。

下文区分三类证据：**复现**表示本机原函数调用或明确标注的 C 函数级桩验证；**源码确认**表示可以从所列控制流确定；**待验证/优化**表示设计风险或能力缺口，尚无本次性能或硬件实验结论。P0 表示进入相应推广场景前必须满足的门槛，P1 表示优先修复，P2 表示后续完善；P0 不等于宣称已经发生严重设备事故。

未执行 ESP-IDF 构建、设备烧录、物理断电实验或真实浏览器 HIL。本次没有查询或使用缓存设备身份作为实时设备证据。

## 3. 当前架构及建议保留的设计

```mermaid
flowchart LR
  UI[CLI / Web / Agent] --> API[Gateway REST / WebSocket]
  API --> OP[OperationManager / SQLite / 证据]
  OP --> HUB[Hub / 设备与端点监督]
  HUB --> SESSION[Session / Codec]
  SESSION --> LINK[USB CDC / USB Serial-JTAG / TCP]
  LINK --> CORE[Iris worker / 服务]
  CORE --> PRODUCT[产品 Recovery 与 Flash 策略]
  PRODUCT --> IDF[ESP-IDF OTA / Flash]
```

应保留：Gateway 单一物理连接所有者；Device ID、Boot ID、Session ID 分离；COBS、CRC 与有界载荷；日志和媒体有界缓冲；文件仅暴露注册逻辑卷；正常应用与 Recovery 写入权限分离；未知结果不自动重放写入；实际新固件身份和 HEALTHY 共同参与更新验收。以下改进均建立在这些设计之上。

## 4. 架构问题与优化项

### IRIS-A01 · P1 · 芯片与调试能力边界尚未产品化

**状态（2026-09-05 用户确认）：设计规划内，本轮不实施，不计入本轮修复验收。**

**源码确认。** [Kconfig](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/Kconfig) 将 TinyUSB CDC 限定为 ESP32-S31；USB Serial/JTAG 使用 SoC 能力条件；TCP 是无 USB 选项时的默认传输。目前没有 UART 传输实现。组件的 `idf >=5.5` 声明并不能证明每个芯片、IDF 版本及传输组合均已可用。[组件 manifest](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/idf_component.yml)

**影响。** 无原生 USB、尚未配置网络、板级 USB 复用或睡眠导致掉线的设备不能直接沿用 Mosaico 流程。USB Serial/JTAG 的连接状态、复位控制和睡眠行为也需要按目标区分。[官方 USB Serial/JTAG 说明](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/usb-serial-jtag-console.html)

**方向。** 建立 SoC 能力表和独立 Board Profile，分别声明运行时链路、ROM 入口、JTAG 路径、复位能力、Flash 容量及恢复策略；按需要增加 UART 适配。提供调试适配接口管理 OpenOCD/GDB 的启动、结束和链路交接。

**验收。** 每个标为 supported 的组合都具有构建、正常连接、复位、睡眠唤醒、Recovery 与故障诊断证据；未验证组合明确返回 unsupported，不能仅凭编译条件宣传支持。

### IRIS-A02 · P1 · 服务回调仍与控制链路共享执行时间

**源码确认，性能影响待测。** RPC handler 在 Iris worker 内同步调用，deadline 在 handler 返回后才判断；OTA DATA 的 Flash 写入也在 worker 中执行。[esp_iris_services.c](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/src/esp_iris_services.c)，`handle_rpc` L1099–1110、`handle_ota` L1679–1680。它不能中断阻塞 handler，也不能保证 deadline 之前业务没有产生副作用。

**方向。** 将短时控制 handler 与耗时任务分开；慢操作经有界 job 队列执行，Flash 写入使用专门 worker，控制线程只处理协议和进度。为取消规定“可取消”和“已进入提交”的边界。按 minimal、service、recovery 配置裁剪媒体、文件及网络依赖；当前 [CMakeLists.txt](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/CMakeLists.txt) 即使部分功能关闭仍列出多项公共依赖，需实测 disabled/minimal 的最终体积和 RAM。

**验收。** 在日志洪泛、文件 I/O、Flash 擦写和慢 handler 并发下测量控制响应 P95/P99、取消延迟、worker 栈与内部堆；超时结果必须说明是否可能已执行，不能表达成事务回滚。

## 5. 协议与操作契约问题

### IRIS-P01 · P0（网络推广）· TCP 配对不是完整安全通道

**状态（2026-09-05 用户确认）：设计规划内，本轮不实施，不计入本轮修复验收。**

**源码确认。** TCP 默认监听所有设备接口，配对默认关闭；启用后只在 HELLO_ACK 校验 challenge-HMAC。之后的普通帧仍是 COBS + CRC32，传输代码使用普通 socket，没有逐帧 MAC 或加密。[协议 TCP pairing](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/protocol/spec.md)、[TCP 实现](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/src/esp_iris_transport_tcp.c)、[Kconfig](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/Kconfig)

**影响。** HMAC 可以证明主机持有 token，但不保护后续流量的机密性和端到端真实性；CRC32 只能检出意外损坏。主机 Web API 的 TLS 不会自动保护 Gateway 到设备这一段。

**方向。** 明确开发隔离网和可部署网络两种安全 profile。网络 profile 默认要求认证，并采用经评审的 TLS 或 AEAD 会话；绑定双方身份、会话与序列号，加入凭据轮换和写入权限策略。USB 的物理信任假设也应单独写明。

**验收。** 无凭据不能执行写入；被动抓包不能读取受保护数据；篡改、重放、旧 token、错误设备身份和降级连接均被拒绝。已有 HMAC 测试继续保留，不将其作为整段链路加密的证明。

### IRIS-P02 · P1 · 单 TCP 配置缺少未握手连接的主动释放

**源码确认。** [esp_iris_transport.c](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/src/esp_iris_transport.c) L132 只对未 committed 候选执行 claim timeout，而 L166 在仅启用一个传输时立即将物理连接标记 committed。TCP listener 对已有 client 时的新连接直接关闭。

**影响。** 在 TCP-only 配置下，一个保持连接却不发 HELLO_ACK 的客户端可占住唯一会话；多传输仲裁的超时设计没有覆盖这个配置。SO_KEEPALIVE 不等于握手期限。

**方向与验收。** 分离物理连接、握手完成、已认证 owner 三种状态；所有配置均设置握手期限，认证后设置可配置的会话存活策略。空连接、慢速握手及连续失败认证不得长期阻止合法 Gateway 接入。该项尚未做板端实测。

### IRIS-P03 · P1 · Recovery 身份依赖命名推断，兼容性信息不足

**复现。** [hub.py](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/tools/iris_gateway/hub.py) L27–34 通过 project/version 中的 `recovery`、`normal` 等子串推断模式。输入项目名 `recovery-analysis-app`、版本 `1.0.0` 会得到 `recovery`。这是命名推断的反例，不是本次发现了误烧设备。

**方向。** 在可扩展 TLV 中定义显式 firmware role、产品协议版本、chip/board、布局摘要、Recovery ABI、健康策略版本及安全能力。缺少新字段的旧固件使用明确兼容路径，不能把推断值提升为已验证身份。保留 Device ID、Boot ID、Session ID 的现有含义。

**验收。** 任意合法项目名称不改变固件角色；旧 Gateway/新固件、新 Gateway/旧 Recovery 的支持组合有版本矩阵，未知必需能力在写入前被拒绝。

### IRIS-P04 · P1 · 设备请求去重只覆盖有限情况

**源码确认。** `handle_rpc` 只比较 `last_rpc_request_id`，设备接收分发未统一检查输入 sequence；因此会话内 A、B、再次 A 的顺序不能依靠当前机制保证 A 只执行一次。[esp_iris_services.c](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/src/esp_iris_services.c) L1064–1071；[esp_iris.c](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/src/esp_iris.c) L488–518。

**边界。** 当前协议实际只承诺拒绝“最后一个 RPC ID”的重复，不能把它宣传为普遍 exactly-once。TCP 有序可靠也不能解决业务超时后重新提交的问题。普通 OTA 使用顺序 offset 和 STATUS 查询，已有同会话 ACK 丢失恢复，不能说完全没有恢复机制。

**方向。** 明确每类请求的重试策略：只读可重试；有副作用请求绑定请求摘要与有限重放窗口；长操作绑定 operation ID 和可查询 receipt。若强化 OTA stream/job 绑定，应协商新能力，兼容当前 host DATA 不填 stream ID 的行为。

**验收。** 覆盖 A/B/A、同 ID 不同 payload、旧 session、ACK 丢失、ID 回绕及断线后的旧 job；副作用次数与协议声明一致。

### IRIS-P05 · P1 · HTTP operation ID 没有绑定请求身份

**复现。** [operations.py](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/tools/iris_gateway/operations.py) 的 `submit`/`execute` 遇到已有 operation ID 直接复用记录，没有比较 device、action 和 params。内存 GatewayStore 验证中，先提交 device-a 的 OTA，再用相同 ID 提交 device-b 的 restart，得到 device-a 的旧 OTA 成功结果，第二个回调没有执行。

**影响。** 幂等重试变成错误结果复用；多设备脚本或客户端 ID 管理错误会得到具有误导性的成功记录。此结论不依赖外部攻击。

**方向与验收。** 持久化规范化请求摘要，至少绑定 device_id、action、artifact/hash、参数和适用的调用者边界。同 ID 同摘要返回原记录；同 ID 不同摘要返回 409 Conflict。并发首次提交也应通过数据库约束实现同一规则。

### IRIS-P06 · P1 · 更新失败、结果未知和事后核对没有完全闭合

**复现与源码确认。** `closed_loop_ota` 在 45 秒内未看到新 boot 的 HEALTHY 时抛出 `RuntimeError`；OperationManager 将其写为 `failed`，尽管新镜像可能已启动。[gateway.py](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/tools/iris_gateway/gateway.py) L833–847；[operations.py](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/tools/iris_gateway/operations.py) L353–368。用同一异常调用原 OperationManager 已复现该分类。

重启 Gateway 时已有 `_recover_interrupted` 将相关写操作标为 `outcome_unknown` 且不重放，这是正确基础；但当前终态不可变，缺少面向同一 operation 的后续核对结果模型。[cli.py](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/tools/iris_gateway/cli.py) L94–113。

**方向。** 设备明确拒绝才归类 failed；写入后丢观测或验收超时归类 outcome_unknown。健康期限来自产品策略。增加只读 reconciliation 记录，关联原 operation、设备实际固件、Boot ID 和设备 receipt；保留原始终态历史，不通过新建写操作来探测结果。

**验收。** END/COMMIT 响应丢失、Gateway 被终止、健康事件丢失和慢启动后仍能核对原操作；最终可以区分成功、已确认失败和无法确认。

## 6. 实现问题

### IRIS-I01 · P1 · RPC 超长响应错误分支仍继续复制

**C 函数级桩复现。** [esp_iris_services.c](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/src/esp_iris_services.c) L1105–1119：`response_size > CONFIG_ESP_IRIS_RPC_BODY_BYTES` 时只设置错误码，没有清零或提前退出，随后仍用该长度执行 memcpy。

本次提取原 `handle_rpc` 函数体，以 1024 字节容量、回调报告 1025 字节验证；插桩捕获到 `copy_requested=1025`。为避免真实非法读取，插桩拦截了这次复制。它证明错误控制流存在，不是板端 exploit 复现。更大错误长度还可能超过 RX 缓冲区。

**修复与验收。** 检查失败立即构造零长度错误响应并返回；所有外部回调长度在复制、转换成协议长度字段之前验证。覆盖容量、容量+1、极大 size_t、回调返回错误但填写长度等边界，配合 ASan/UBSan 的完整服务测试。

### IRIS-I02 · P1 · 同一 read 中多个短请求可能只返回一个响应

**C 函数级桩复现。** `queue_frame` 只允许一个待发送帧，TX 非空即拒绝；`feed_rx` 会连续处理同一输入缓冲区里的所有帧，而多个 handler 忽略入队错误。[esp_iris.c](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/src/esp_iris.c) L107–133、L520–544、L660–663。

本次使用真实 codec、原 `feed_rx` 和 `queue_frame`，用 PING 响应分发桩输入两个合法短帧：76 字节输入解出 2 个请求，仅入队 1 个响应，另 1 个被拒绝。该验证未包含完整固件调度和 USB 驱动。标准主机多数请求串行执行会降低触发概率，但 TCP/USB 的 read 分组不能成为协议正确性的前提。

**修复与验收。** 使用有界响应队列，或在 TX 产生后暂停解析并保留未消费输入；不能静默丢弃必须响应的请求。将相同帧序列按每字节、任意分块、多个帧合并输入，比较响应集合与顺序；加入写背压和 CREDIT 混合请求。

### IRIS-I03 · P2 · 主机发送 sequence 没有 u32 回绕

**复现。** [session.py](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/tools/iris_gateway/session.py) L206 直接累加 sequence，而 encode_frame 要求 u32。将计数设为 `0xffffffff` 后发送 PING，原函数抛出 `ProtocolError: sequence does not fit in u32`。接收侧已使用模 2³² 比较，request ID 也已有回绕，发送侧需要一致。

**修复与验收。** 明确 sequence 的回绕规则并统一 C/Python 实现，覆盖 `fffffffe → ffffffff → 0 → 1`、半空间比较及重连。若设计选择提前重建会话，应声明并测试，不应让编码异常决定连接生命周期。

### IRIS-I04 · P1 · 普通 OTA 没有自动保存旧 Core Dump

**状态（2026-09-05 用户确认）：设计规划内，本轮不实施，不计入本轮修复验收。**

**源码确认。** Gateway 已实现 `preserve_coredump`，maintenance 和 system-update 路径会调用；普通 `closed_loop_ota` 与 factory-recovery 路径未调用。[gateway.py](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/tools/iris_gateway/gateway.py)，`preserve_coredump` L324、`closed_loop_ota` L679、`factory_recovery` L1470、system-update L874。

**影响。** OTA 本身不以 coredump 分区为写入目标，但新应用或 Recovery 再次崩溃可能覆盖旧证据；保留分区并不等价于已经把证据安全归档。Mosaico CLI 又在创建 OTA operation 前先进入 Recovery，跨层问题见配套文档 MOS-A03。

**方向。** 将更新前证据保存放进统一 Gateway 写操作前置阶段，记录来源 boot、崩溃 ELF SHA、文件摘要和归档状态；保存失败时按明确策略停止。离线解析按崩溃固件 ELF 匹配，而不是使用更新后的 ELF。[官方 Core Dump 指南](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/core_dump.html)

**验收。** 旧固件 panic → 保存证据 → 更新 → 新固件再次 panic 后，两份证据仍可分别追溯到对应固件，且能够在无设备时解析。

### IRIS-I05 · P2 · 高速日志持久化可能阻塞 Gateway 事件循环

**状态（2026-09-05 用户确认）：设计规划内，本轮不实施，不计入本轮修复验收。**

**源码确认，性能待测。** [store.py](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/tools/iris_gateway/store.py) L150–166 对事件同步写 SQLite 并 commit；L606–631 对日志逐条打开 gzip、写入、更新索引并再次 commit。单条日志还保存在事件数据库与压缩原始日志中。

**方向。** 使用有界持久化队列、批量事务和日志段写入；定义磁盘满、慢磁盘、队列溢出及进程异常退出的策略。限制 operation 待执行队列、事件留存和归档容量，并提供明确丢失指标。不能为降低开销牺牲关键操作和崩溃证据的持久性。

**验收。** 在多设备和高日志速率下测量 API P99、持久化吞吐、内存上界与磁盘增长，证明写入背压不会阻塞控制。这里没有声称已测到具体吞吐瓶颈。

## 7. 测试、文档和发布问题

### IRIS-T01 · P1 · Windows 与声明的 Python 3.8 支持缺少完整回归

本次结果如下；失败和排除项没有计入通过率：

| 执行内容 | 结果 | 解释 |
| --- | --- | --- |
| `pytest -q --ignore=tests/e2e` | 收集失败 | `tests/test_link.py:6` 无条件 import termios，Windows 不具备该模块 |
| 进一步排除 `test_link.py` | 283.7 秒后主动终止 | 停留在 supervisor reconnect 用例附近，未得到完整结论 |
| 单独限时验证 supervisor reconnect | 30 秒期限触发并退出 | 栈位于 asyncio.run 的 `_cancel_all_tasks`；确认当前环境下取消/清理不能按时完成，根因仍需进一步定位 |
| 再排除该 supervisor 用例 | 134 passed、2 failed、1 deselected，23.40 秒 | Linux 路径断言失败；MinGW 动态库 weak 符号测试导出失败 |
| 前端 `npm run test:unit` | 2 个文件、3 项通过 | 小规模单元测试通过，不代表完整 UI 流程覆盖 |
| 前端 `npm run build` | 通过 | TypeScript/Vite 构建成功 |
| 私有 Python 审计探针 | 已复现所列契约问题 | 仅内存对象、临时数据库和 mock，无设备访问 |
| C 函数级审计探针 | 已复现超长复制与合并响应问题 | 原函数体加明确 host 桩，不是 HIL |

测试依赖安装到私有 Python 3.8 venv，使用 `requirements-test.txt` 解析；没有宣称这是 `requirements.lock` 的逐项锁定复现。准确依赖清单、命令、JUnit、输出与探针结果保存在 [审计材料目录](../.agents/iris-audit-20260905/)。前端构建子进程退出码为 0；首次打印日志的私有包装脚本遇到 GBK 无法显示勾号，已修正包装脚本编码，该显示错误不属于产品构建失败。

**方向。** Windows、Linux、macOS 至少覆盖当前支持的 Python 基线及主力版本；将 POSIX 专用测试明确标记，将平台无关部分保留在所有平台；C host fixture 支持 PE/DLL 和 ELF 的导出差异。异步测试必须有总体期限和可靠 teardown。

### IRIS-T02 · P1 · 当前 HIL、CI 与资源预算不足以证明全产品族成熟度

已有 [2026-08-31 实机报告](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/TEST_REPORT_2026-08-31.md) 记录 ESP32-S31 的 20 个场景取得通过证据，但它对应 `3fe1619f737a`，且包含全量运行与修复后定向复测的合并结果，不能当作本次 HEAD 的一次全量通过。

[.gitlab-ci.yml](../submodule/esp-mosaico-tools/submodule/esp-iris/.gitlab-ci.yml) 有 Linux Python 3.8/3.12、前端和 ESP32-S31 示例构建；未见多 OS 主机矩阵、其他 SoC 的构建矩阵及自动硬件断电任务。普通 pytest 会跳过需显式开启的破坏性 HIL。system_update/system_inventory 测试应用也未列入当前构建 job。

[resource_budgets.json](../submodule/esp-mosaico-tools/submodule/esp-iris/components/esp_iris/resource_budgets.json) 仍指向不存在的 `common_components/esp_iris` 和 `template/build`；架构文档引用的 `tools/check_esp_iris_budgets.py`、`tools/ci.py` 在本子模块中不存在，当前 CI 未调用这些预算检查。实际 `gateway.py` 为 1960 行，`session.py` 为 1108 行，已超过旧文件中对应的 1250/1000 行阈值。行数本身不是质量判据，但说明所描述的约束没有生效。

架构文档还将产品更新后端描述为固定 release key、已认证 manifest 和 internal RAM staging，而当前 Mosaico 后端接受 unsigned plan、保护镜像使用 PSRAM。应明确通用接口的可选能力、参考安全设计与当前产品实际策略，不能将三者混写。

**方向与验收。** 修正文档、路径和可执行预算；对所有 supported profile 生成 BIN/ELF/map/sdkconfig/IDF 与子模块 revision 清单；增加完整 C 服务测试、跨语言协议 corpus、边界 fuzz、掉线与断电矩阵。形成按发布版本索引的证据，而不只保存人工报告文字。

## 8. 建议实施顺序与发布门槛

以下为原评估的总体路线。本轮按用户确认仅实施 A02、P02–P06、I01–I03、T01–T02；A01、P01、I04、I05 保留为设计规划。总体发布门槛不能等同于本轮交付范围。

| 阶段 | 工作 | 完成标准 |
| --- | --- | --- |
| 1：先修正确性 | I01、I02、P05、P06、I04；同步修 tools 的操作状态契约 | 所列反例成为稳定回归；普通 OTA 有完整旧证据和结果核对记录 |
| 2：明确默认契约 | P03、P04、A01；定义更新、Recovery、ROM、JTAG 的职责 | 新旧兼容矩阵、Board Profile schema、真实角色和安全能力可查询 |
| 3：允许网络与产品推广 | P01、P02；与 tools 的签名/启动安全策略对齐 | 未认证写入、篡改、重放、握手占用测试通过；安全 profile 不静默降级 |
| 4：规模与发布 | A02、I03、I05、T01、T02 | 多平台、多 SoC、多设备测试及资源/延迟预算全部可重复 |

建议为每个支持组合执行以下统一验收：

1. 同一 Device ID 完成 normal → Recovery → normal，每次真正重启使用不同 Boot ID；新固件 identity、布局和健康策略均匹配。
2. 在 BEGIN、DATA、END、切换启动分区、重启前后逐点断线；在涉及 Flash 持久化的阶段执行受控断电实验，记录实际可启动状态。
3. Gateway/CLI 在每个阶段异常退出，重新启动后只能查询和核对原 operation，不能自动重放不确定写入。
4. 多设备同时接入、端口重枚举、旧身份缓存、错误镜像和命名冲突均不能造成选错设备或错误成功。
5. CPU 停机、应用卡死、异常 NVS、无网络与 USB 故障分别验证诊断和恢复路径；不要用正常连接成功替代故障路径证明。

现有 v1 帧头应保持稳定；新增能力优先采用可跳过 TLV、能力位和明确的版本协商。需要改变安全或更新架构的 profile 应另行设计、评审和迁移，本次评估不改变仓库保留 Recovery 的现行约束。
