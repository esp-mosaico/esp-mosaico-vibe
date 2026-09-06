# ESP-Iris 修复与验收报告

更新日期：2026-09-06。原始问题与基线见 [ESP-Iris 评估](esp-iris-review.zh-CN.md)；产品编排层边界见 [esp-mosaico-tools 评估](esp-mosaico-tools-review.zh-CN.md)。

## 交付状态与范围

本轮按用户要求实现 IRIS-A02、P02、P03、P04、P05、P06、I01、I02、I03、T01、T02，共 11 项。采用多 Agent、独立 worktree 实现并统一集成；最终 Iris 修复序列为 11 个问题提交，HEAD 为 `1fc38b5`（另含 USB 重枚举验收及 Boot ID 精确显示 2 个独立支持提交）。逐项提交见本文末尾清单。

**11 项实现及主机回归通过，正确目标设备的 Recovery、应用安装、慢 RPC 和 TCP 未握手释放已通过。** 工作台与 CLI 的完整 Device ID、Boot ID 及操作记录核对通过。本报告只覆盖 ESP-Mosaico / ESP32-S31，不推广为全 ESP32 产品族支持。

IRIS-A01（跨芯片与调试能力产品化）、IRIS-P01（网络安全通道）、IRIS-I04（普通 OTA 自动保存旧 Core Dump）、IRIS-I05（日志持久化执行方式）由用户明确保留在设计规划内，本轮不实施、不计入已修复项。保留 Recovery、正常应用经 `mosaico.py install` 安装的既有产品架构继续适用。

## 已完成的回归与证据

以下结果来自本轮集成验证；测试通过与硬件验证分开记录。

| 验证项 | 已确认结果 | 证据边界 |
| --- | --- | --- |
| Windows / Python 3.8 | 262 passed、2 skipped | 两项为 POSIX 专用测试，不是任意排除失败用例 |
| Windows / Python 3.12 | 262 passed、2 skipped | 同上 |
| Linux / Python 3.12 | 264 passed | 主机测试，不代表板上测试 |
| C runtime ASan / UBSan | 8 个配置通过 | 单/多传输 × 无服务、OTA、只读 Inventory、OTA + System Update + Inventory；硬件接口采用确定性桩 |
| Python 静态检查 | Ruff 通过；mypy 检查的 33 个源码文件通过 | 33 是源码文件数，不是测试数 |
| 前端单元测试 | 7 passed | 工作台单元回归 |
| 前端构建 | 通过 | 生成静态工作台产物 |
| Playwright | 5 passed、1 skipped | 跳过项为按配置启用的真实硬件 fixture；其余不替代实机验收 |
| ESP-IDF 修复过程构建 | HelloWorld、Recovery、acceptance fixture、services_usb、coredump、system_inventory、system_update、disabled 均构建通过 | 最终目标设备产物、提交与原始构建记录见实机区 |

本机主机测试证据索引：[Windows 3.8 XML](../.agents/iris-audit-20260905/final-win38.xml)、[Windows 3.12 XML](../.agents/iris-audit-20260905/final-win312.xml)、[Linux 3.12 XML](../.agents/iris-audit-20260905/final-linux312.xml)、[sanitizer XML](../.agents/iris-audit-20260905/final-sanitizers.xml)。这些位于私有运行目录，发布时应随正式证据包归档，不能假设其他检出自动包含这些文件。

## IRIS-A02

**实现。** 将 RPC、OTA 与 System Update 的慢工作移入有界服务执行器，协议任务负责提交和取回完成结果。控制请求可在慢服务工作期间处理；对执行中状态、取消、断开连接及停止的资源生命周期作了明确处理。构建依赖改为在 Kconfig 解析前声明的 profile，并保留禁用组件的构建路径。

**回归要点。** 完整 runtime/services 桩测试覆盖 RPC、OTA、System Update 工作在途时的控制响应、执行器忙状态、取消、清理和生命周期；在单/多传输与 OTA 开关组合运行。disabled 与相应 ESP-IDF 工程完成过构建。

**边界。** 有界执行器不承诺抢占任意用户回调，也不能撤销已发生的 Flash 或业务副作用。板上控制延迟与内存实测数据见实机区，不用源码行数证明性能。

## IRIS-P02

**实现。** 单传输配置也以临时认领开始；未完成握手的连接在认领期限后释放。合法握手和重新开启会话按协议更新认领状态，不再因为只有一个 TCP 传输就永久占有设备。

**回归要点。** C runtime 测试验证单/多传输下未握手超时释放、合法握手提交及认领更新。该修复解决未握手占用，不将其描述为已经实现通用网络认证或加密。

## IRIS-P03

**实现。** HELLO 增加明确固件角色、芯片与产品兼容性元数据，包括产品契约、board/layout、Recovery ABI、健康超时和必需特性。未知角色不再通过项目名或版本字符串猜测为 Recovery。写前检查明确角色、芯片及产品要求；进入 Recovery 后再次核对契约，失败时不进入更新写入。

**回归要点。** 覆盖普通项目包含 recovery 字符串、字段缺失/不匹配、未知必需特性、旧对端边界、产品/通用模式及 CLI 兼容性参数。正常应用、Recovery 和 tools 的 ESP-Mosaico 契约对接属于本轮配套修改。

**边界。** 元数据明确当前支持范围，不等于已实现 A01 的全芯片 profile，也不替代 P01 的网络安全设计。

## IRIS-P04

**实现。** 设备校验输入通道 sequence 与 RPC request ID 的会话内前进关系，拒绝重复或过旧请求，包括 A、B、A 的延迟重放。主机重新打开连接通过协商获得新会话边界，避免旧请求混入新会话；现有 v1 framing 与原 golden vectors 保留。

**回归要点。** 完整 C runtime 覆盖重放、旧帧、重复握手和重新开启；Python 会话测试覆盖协商开启及串行写入关系。

**边界。** 这是会话内拒绝重放契约，不承诺跨设备重启的 exactly-once，也不因没有收到响应就授权主机重放写入。

## IRIS-P05

**实现。** operation ID 持久绑定设备、动作、actor/scopes 与规范化参数指纹；相同 ID、相同请求复用原记录，内容冲突返回 HTTP 409。SQLite 唯一键处理并发登记；旧数据库中无法验证的历史指纹不被伪造补齐。

所有 execute/submit 调用点同步核对实际适配器输入：raw/structured RPC、完整 console 命令、input moves、mirror description、audio 和 file 内容均参与身份；OTA 与 System Update 绑定镜像/归档摘要及兼容性约束。文件上传先在 32 MiB 上限内临时落盘并计算真实摘要，登记后再流式送设备。截图复用返回原 operation 与复用标记，不重新采图或把持久化摘要误当图像数据。

**回归要点。** 覆盖同长度不同内容的 HTTP 409、相同内容仅调用一次、raw RPC text/hex/base64 等价输入、actor 边界、跨数据库连接并发、迁移、重启与原记录不变。详见 [操作身份契约](../submodule/esp-mosaico-tools/submodule/esp-iris/docs/operation-identity.md)。

## IRIS-P06

**实现。** 更新写入后缺少重连/HEALTHY、失联或运行中写任务被中断时保留 outcome_unknown，不因观察失败就认定写入失败。健康期限支持产品 HELLO 值与明确的主机 override。新增只读 reconciliation API/CLI，独立记录当前观察结果并链接原 operation，保留原终态历史、不重放写入。

OTA 对账需目标 project/ELF 摘要、正常角色、新 Boot 与相同 Boot 的 HEALTHY。System Update 还检查同一 operation receipt、目标布局、目标 bootloader 摘要和与请求 BIN 摘要绑定的应用身份；缺失旧证据保持 unknown。读取 inventory 后再次核对 Boot，正常 OTA 最后 STATUS 也必须对应 HEALTHY 所属 Boot。

**回归要点。** 覆盖健康缺失、取消、断线、摘要/角色/Boot 不符、成功/失败/不匹配 receipt、组件绑定、观察期间再次重启、记录持久化与导出。详见 [只读对账契约](../submodule/esp-mosaico-tools/submodule/esp-iris/docs/operation-reconciliation.md)。

## IRIS-I01

**实现。** RPC 回调返回长度必须在响应缓冲区范围内；过长值及错误返回路径不再继续复制未验证长度。

**回归要点。** C runtime 回归覆盖容量边界、超出容量以及极端长度，结合 ASan/UBSan 检查无越界读取/写入。测试编译实际服务实现，不把原评估中截取函数的探针当作最终完整测试。

## IRIS-I02

**实现。** 接收处理在回复待发送时保留尚未消费的输入；部分写入、背压以及同一次读取中的多个短帧不会因单个 TX 缓冲区占用而丢失后续请求或响应。

**回归要点。** 覆盖合并短请求、分片输入、部分 TX、畸形数据及后续有效帧恢复；通过完整 C runtime 与 sanitizer 配置验证。

## IRIS-I03

**实现。** 主机发送 sequence 按 uint32 回绕，并在串行写入顺序内分配，避免到达最大值后编码失败或并发时顺序错乱。

**回归要点。** 覆盖最大值附近的回绕、后续合法编码及会话/写入顺序；不将 sequence 回绕等同于重置设备身份。

## IRIS-T01

**实现。** 修正 Windows 上 POSIX 模块导入、路径假设与 C 测试动态库导出差异；保留平台中立测试。补强异步关闭和重连测试，避免取消过程挂住；保持 Python 3.8 与现代版本兼容。

**回归要点。** Windows 两版本各 262 通过、2 个 POSIX 专用跳过；Linux 264 通过。CI 声明的 macOS 任务是后续可执行矩阵，本报告不声称本轮已经在 macOS 实跑通过。

## IRIS-T02

**实现。** 修正资源预算路径与执行入口，增加完整 runtime 固件桩、故障与畸形帧 corpus、sanitizer 配置、主机跨平台 CI、System Inventory/System Update/disabled 构建以及发布产物证据清单。必需编译器或产物缺失时不静默跳过；源码、固件与前端预算分组检查。

**回归要点。** 本轮主机、静态检查、前端及 sanitizer 结果见上表。发布清单应关联源码 revision、工作区差异/文件摘要、实际 IDF、sdkconfig、BIN/ELF/map 和启动链产物；正式证据与最终实机固件一致性见下节。

**预算解释。** 旧评估基线已有文件超过旧行数阈值。本轮重新基线化须注明原超限事实及新增执行器/协议实现的容量，不把提高行数阈值或 BIN 上限描述为性能优化。板上时延、堆与任务栈仍由实际测量验收。

## 实机验收（2026-09-06）

正确目标实时 Device ID 为 `da2493eae521409c9b03a684d532a83f`；High-Speed USB 为 COM19，独立 Serial/JTAG 为 COM8（MAC `30:ED:A0:F4:0C:28`）。接口号仅作为本次观察，不作后续自动选择依据。用户确认先前 COM14 接错设备；该次写入和失败校验保留日志，不计入目标设备验收，也未再次操作那块设备。

| 验收项 | 实际结果 |
| --- | --- |
| 独立接口 Recovery | `mosaico.py recover --recovery-port COM8 --source current` 成功；同 Device ID，Boot `1678605295699863992` → `8752542896310210733`，Recovery 角色/OTA 服务就绪，镜像 hash 校验通过 |
| 双接口租约 | 主 lease `13341dbd-46f3-48ba-84b0-53fd58ac22bb` 完成 released；独立 lease `49b8ba91-2db1-41d3-a0c9-4dcb0217e59a` 由产品命令释放；crash index 已保存，本次未发现可保存的有效 Core Dump |
| 产品契约与分区 | `esp32s31 / esp-mosaico/v1 / esp-mosaico / mosaico-retained-recovery-v1 / ABI 1`；System Inventory 分区摘要前缀 `068246e2e1f0` 与应用一致，写前校验通过 |
| 验收应用 | operation `d5d7a955-4207-4143-bd01-51ae7a76f6be` succeeded；Boot `14701253264432484984`、目标 ELF 身份匹配、HEALTHY |
| 慢 RPC 与控制 | RPC 2.042 秒成功；在途 6 次 CONTROL STATUS 最大 13.68 ms；无 Boot 改变 |
| 错误响应长度 | 超容量报告返回失败，设备未重启；测试回调不执行真实越界写 |
| TCP 未握手释放 | 保持 pairing 开启，通过 Gateway lease 释放 USB 后，两次 TCP 收到同 Device ID 的 HELLO 后分别 2.391/2.047 秒 EOF，未发送任何字节；USB 恢复且 Boot 不变 |
| 最终正常 → Recovery → 正常 | acceptance Boot `14701253264432484984` → Recovery `6267820438310272020` → HelloWorld `12238782771570883527`；enter operation `126f0e15-26eb-4619-8b29-38662b2707c5` 与 install operation `b1f0830d-61de-4cc4-9666-c05b5cf87b61` 均 succeeded |
| 产品行为 | 最终 `hello_world 1.0.0`、normal、HEALTHY，日志持续 `Hello World!`；ELF SHA `8f837ef6cfa2d157ae0a208c19c86b2aa13c00147ff65944da5b04212de38a3e` |
| CLI / Web | 完整 Device ID、Boot ID `12238782771570883527` 与成功安装 operation 均与 CLI 一致，浏览器实际 DOM 核对通过 |

实机重做还暴露并修正了两项问题：A02 执行器对共用 channel 10 的 Inventory 漏分发，已并入 A02 并扩展为 8 个生产 C 配置回归；维护完成阶段对 USB 重枚举瞬态断开提前失败，另行提交有界只读重试支持，保持原 Device ID、新 Boot 和版本验收，不重放写入。coredump fixture 缺少直接组件依赖已并入 T02。

RPC 验收后 STATUS：可用内部堆 255423 B、历史最低 221424 B、协议任务最小剩余栈 2168 B、worker 最长活动段 549 µs；Iris 报告内部占用合计 44332 B。该数据是 Wi-Fi 验收 fixture 的单次观察，不是全负载 P95/P99 或任意用户回调的性能保证。

TCP 初次脚本把建立 TCP 连接前的网络延迟也算入认领时间，得到 6.078 秒并失败；保留 [初次记录](../.agents/iris-audit-20260905/hardware-tcp-claim-first.json)。补充连接耗时及首字节时刻后重新验收，按收到 HELLO 后到 EOF 的时间检查；没有调整固件超时或放宽测试阈值。此项证明本板 USB+TCP 配置的未握手释放，不替代单 TCP 配置主机 C 回归，也不声称已验收 P01 网络安全方案。

独立接口仍通过 `mosaico.py` 的完整 Recovery bundle 流程执行，无全片擦除，不引入绕过产品命令的分区写入口。此前失败记录保留；最终成功日志为 [Recovery](../.codex-runs/mosaico/20260906T061132Z-recover/raw.log)、[acceptance install](../.codex-runs/mosaico/20260906T061448Z-install/raw.log)、[HelloWorld install](../.codex-runs/mosaico/20260906T061724Z-install/raw.log)。

### 构建与产物

实际 IDF 为 `D:/esp/esp-idf-master`，revision `7b9cc1ac79f8`，构建工具报告 `v6.1-dev-7333-g7b9cc1ac79`，IDF Python 3.12.10。五个 test app 最终均构建通过，四项 BIN 预算通过：[构建索引](../.agents/iris-audit-20260905/release-testapps.json)、[预算结果](../.agents/iris-audit-20260905/artifact-budgets-final.json)。runner 对嵌套 `idf.version` 约束格式存在 doctor 解析限制，已直接核对实际 manifest 与 IDF；不据此声称该工具问题已修复。

三份上板固件的 sdkconfig、BIN/ELF/map、bootloader、分区与源码 SHA 见 [设备固件索引](../.agents/iris-audit-20260905/release-device-firmware.json)。固件中 ESP-Iris 组件及主机测试的 Iris 仓库源树为 `e2a5953260eabc02092a4dec9fd7a4aba7772225`；后续仅工作台精度支持有独立记录。索引不替代实机操作证据；验收 Wi-Fi 凭据仅存私有头文件与本地构建产物，不进入提交。

| 应用 BIN | 字节 | SHA-256 |
| --- | ---: | --- |
| hello_world.bin | 1171616 | `b6ce470d20e138c65d9697cee399b3cc31109f095414689324cf83fa6eda3296` |
| iris_acceptance.bin | 985008 | `8adc65d1d41d299314ae2738fe0f30fb6e2f6a1825309cc319c65e5b367f90a8` |
| factory.bin | 1648000 | `1d4dbb0b5ef8fd18b661fdc577da1abae3edb34765829d2b4587b8c8898092ea` |

结构化证据：[主机矩阵](../.agents/iris-audit-20260905/final-host-matrix.json)、[Recovery lease](../.agents/iris-audit-20260905/recovery-com8-verified.json)、[RPC](../.agents/iris-audit-20260905/hardware-rpc-acceptance.json)、[TCP](../.agents/iris-audit-20260905/hardware-tcp-claim.json)、[操作记录](../.agents/iris-audit-20260905/hardware-operations.json)、[最终设备](../.agents/iris-audit-20260905/hello-final-live.json)。这些是本地私有运行文件，不随 Git 自动分发；公开交付应另行归档不含凭据的证据包。

### 逐项提交

每个 IRIS 编号各一个 commit；实机阶段对同问题的补修已合并回对应提交。USB 重枚举和 Boot ID 显示支持作为额外独立提交，工具层契约/独立接口路由和应用 fixture 位于各自仓库。

| Commit | 变更 |
| --- | --- |
| `e856181a77f70b75ef7fda9e993fe74c86659e05` | fix(IRIS-I01): bound RPC callback response lengths before copying |
| `abab94274e2bf371104f93db4a55938aa177e3c7` | fix(IRIS-I02): preserve coalesced receive tails under TX backpressure |
| `902e47bd8262d0ffccc052b84100ba74876b4710` | fix(IRIS-P05): bind operation IDs to durable request and payload fingerprints |
| `521ec3ce014dd2bb9ac3eaecc901f3617eb304de` | fix(IRIS-P06): preserve uncertain writes and append read-only reconciliation |
| `18af7208d730380f65c34c2fb38ff031a7874f23` | fix(IRIS-P03): declare firmware roles and enforce update compatibility |
| `cb6c5920a5cbe3f5999cb734d89e6aa78da39b82` | fix(IRIS-P02): expire unhandshaken single-transport owners |
| `78acded834bcbed76c128298b61a43a2247e9af2` | fix(IRIS-P04): reject session replays and negotiate bounded host reopen |
| `e76f30d520d984a0aa15d471df82a20b526080ba` | fix(IRIS-I03): wrap host sequence counters in serialized wire order |
| `eb5e77cc1d0535a53e597f4412b681349d4ea5b1` | fix(IRIS-T01): make host validation and shutdown portable across Python runtimes |
| `c63615f40501e5b580c19ece130ef3264579db90` | fix(IRIS-A02): bound service execution and define build profiles with concurrent control |
| `5a2b85f2e956d7203fb6517e8d793b3188f08764` | fix(IRIS-T02): enforce portable release and firmware regression gates |
| `1363ee6bcf96ebda4a39429706c48b67ac900403` | fix(gateway): retry maintenance observation through USB reenumeration |
| `1fc38b5cbfd18bcb931af1eefb7ee50bb263b743` | fix(workbench): preserve exact uint64 boot identity display |

Boot ID 支持补修额外验证：Python 3.8/3.12 各 29 项定向测试通过，关联操作身份/对账回归 54 项通过；前端单元 7 项、Playwright 5 项通过/1 项按配置跳过，mypy 33 个源码文件通过。固件源码不受此补修影响。工作台截图及原始响应见 [实看证据](../.agents/iris-audit-20260905/workbench-evidence.json)。

根仓库应用与外层 tools Recovery 的追溯信息单独见 [工作区源码索引](../.agents/iris-audit-20260905/release-workspace-sources.json)，含两个仓库 revision、全部 tracked 文件 SHA 和子模块版本；未包含私有 Wi-Fi 头文件。

## 本轮不据此关闭的事项

A01、P01、I04、I05 保持用户指定的设计规划内。esp-mosaico-tools 的 MOS 编号问题保持原评估状态，本轮只完成 P03 契约对接与独立 USB Serial/JTAG Recovery 路由。macOS CI 已配置但未在本机执行；未执行断电故障全矩阵、跨芯片推广、CPU 停机调试和生产安全验收。Recovery current 属于当前源码候选，不等于已经发布新的 reviewed Recovery 包。
