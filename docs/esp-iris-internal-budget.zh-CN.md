# ESP-Iris：正常应用内部 RAM 25 KB 预算

预算范围为正常应用的 USB、RPC、截图与镜像。上限采用 25,000 B，
而非 25 KiB。实际 TCP 网络连接和 Recovery 固件的烧录运行不属于
此预算；正常应用仍保留 TCP 能力及 retained-Recovery 路径。

请求/结果及服务生命周期的设计见
[服务生命周期重构](esp-iris-service-context-refactor.zh-CN.md)。

## 实现

- 协议任务栈为 3,328 B，service 栈为 4,096 B，TinyUSB 栈为
  2,048 B。所有栈、任务控制块与 USB DMA 端点缓冲仍在内部 RAM。
- CDC RX/TX 各 1,024 B，两个 DMA 端点缓冲各 512 B，总缓冲 3 KiB。
  大 RPC 和 4,096 B wire frame 仍分段传输，没有缩小协议载荷上限。
- service 请求/结果上下文、服务状态显式分配到 PSRAM，分配失败返回
  错误，不回退内部 RAM。协议帧及媒体块继续使用 PSRAM。
- service 任务首次创建后保留到复位，避免任务自删除、旧栈等待空闲任务
  回收、新任务又被创建时的双栈峰值。停止/重启 Iris 不创建第二个实例。
- 保留原有 4 KiB 日志容量，日志环及 lwIP BSS 放到 PSRAM。
  没有产品网络接口时，TCP 不创建 lwIP 任务或监听 socket；接口建立后
  自动恢复监听，不移除 TCP 配对能力。
- 截图后端从 GSP 第一帧开始维护 PSRAM shadow；截图/镜像不再
  pause/resume GSP，因此不创建临时渲染控制对象。初始覆盖位图确保每个
  像素都已绘制，截图缓冲按需分配到 PSRAM，结束时释放。
- 镜像接收完整请求前不编码媒体，避免编码复用 RX scratch 时覆盖
  尚未处理的 COBS 请求。
- Gateway 镜像/日志接收不再同步等待 credit 写入。每通道最多一个
  后台回写任务，合并重复补充，并在关闭时取消、回收，避免 writer、
  设备 TX 和主机接收循环之间形成等待环路。

新增 Kconfig 开关默认关闭或保持原栈值，正常应用显式开启。
Recovery 使用独立配置，其 OTA writer 不采用正常应用的内存压缩配置。

## 计量口径

`tests/firmware/iris_internal_budget` 复用真实 GSP Hello 主组件、UI bundle
和正常应用分区。正常应用不编译测试 hook、记录表、诊断 RPC 或退出
采样定时器。

峰值 = 实际内部堆分配高水位 + 链接后的相关静态 DRAM + 常驻 IRAM。
堆 hook 根据指针地址而非请求 capabilities 判断是否内部 RAM；计入
分配器对齐，并保守加收 16 B/块管理开销。Iris、service、TinyUSB、
tcpip 任务的运行期分配、bootstrap/恢复注册路径、健康任务创建以及
ISR 分配均计入。释放时不要求由原任务释放；trace 溢出/错误使计量无效。

静态账本计入完整 Iris、TinyUSB 及封装、USB HAL/PHY、已链接网络状态、
恢复适配器与截图后端；排除测试自身、应用 UI/BSP、共享 OS/NVS 实现
静态数据。Iris 初始化触发的 NVS 堆分配计入。这是 Iris 相关的内部 RAM
成本，不是整机 RAM 占用。不能用整机 `min_free_internal` 的变化，或
未覆盖 USB 的 Iris 状态接口统计，替代本账本。

DMA/静态对象按 4 B 对齐逐输入段向上取整。诊断固件额外 RPC/诊断能力
也应计入，因此不能直接把其静态成本当作正常固件成本。全任务诊断 RPC
自身的临时分配不属于正常产品 RPC；实际产品的状态、系统清单 RPC 则
必须纳入测量。新增 RPC/文件服务、改变任务配置或启用网络后必须重新
测量，不能将有限工作负载当作任意回调的数学上界。

## 代价与边界

截图后端在空闲期保留 489,600 B PSRAM（478.125 KiB），捕获时增加
460,800 B（450 KiB），合计 928.125 KiB；以较多 PSRAM 换取低内部峰值
及不暂停 UI 的捕获。镜像请求的 FPS 参数不是实际吞吐保证。

PSRAM 日志仍在片外 Core Dump 区，但 cache/MSPI/PSRAM 故障可能使最后
的日志不可用，不能承诺与内部日志同等的崩溃证据可靠性。Recovery
继续保留内部日志；Core Dump、分区和恢复路径没有移除。

设备操作通过 `mosaico.py` 选择设备及管理 Gateway，不直接打开 USB/serial。
每次操作都使用 `mosaico.py list` 当场发现的设备身份，不使用缓存 Boot ID。
