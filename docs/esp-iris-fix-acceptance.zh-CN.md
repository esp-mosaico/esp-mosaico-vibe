# ESP-Iris 修复与验收报告

更新日期：2026-09-06。原始问题与基线见 [ESP-Iris 评估](esp-iris-review.zh-CN.md)；产品编排层边界见 [esp-mosaico-tools 评估](esp-mosaico-tools-review.zh-CN.md)。

## 交付状态与范围

本轮按用户要求实现 IRIS-A02、P02、P03、P04、P05、P06、I01、I02、I03、T01、T02，共 11 项。采用多 Agent、独立 worktree 实现并统一集成；最终 Iris 修复序列为 11 个问题提交，HEAD 为 `b88c0e2`。逐项完整提交 SHA 由最终集成清单补齐，本文不根据中间 worktree 提交推断。

**实现与主机回归已通过，目标设备的最终实机验收尚未通过，正在重做。** 本文不能作为整套默认烧录流程已完成实机验收的证明，也不将本轮修复推广为全 ESP32 产品族支持。

IRIS-A01（跨芯片与调试能力产品化）、IRIS-P01（网络安全通道）、IRIS-I04（普通 OTA 自动保存旧 Core Dump）、IRIS-I05（日志持久化执行方式）由用户明确保留在设计规划内，本轮不实施、不计入已修复项。保留 Recovery、正常应用经 `mosaico.py install` 安装的既有产品架构继续适用。

## 已完成的回归与证据

以下结果来自本轮集成验证；测试通过与硬件验证分开记录。

| 验证项 | 已确认结果 | 证据边界 |
| --- | --- | --- |
| Windows / Python 3.8 | 248 passed、2 skipped | 两项为 POSIX 专用测试，不是任意排除失败用例 |
| Windows / Python 3.12 | 248 passed、2 skipped | 同上 |
| Linux / Python 3.12 | 250 passed | 主机测试，不代表板上测试 |
| C runtime ASan / UBSan | 4 个配置通过 | 单/多传输 × OTA 禁用/启用；硬件接口采用确定性桩 |
| Python 静态检查 | Ruff 通过；mypy 检查的 32 个源码文件通过 | 32 是源码文件数，不是测试数 |
| 前端单元测试 | 3 passed | 工作台单元回归 |
| 前端构建 | 通过 | 生成静态工作台产物 |
| Playwright | 5 passed、1 skipped | 跳过项为按配置启用的真实硬件 fixture；其余不替代实机验收 |
| ESP-IDF 修复过程构建 | HelloWorld、Recovery、acceptance fixture、system_inventory、system_update、disabled 均构建过 | 初轮构建证据；最终目标设备使用的产物与提交关联仍需在实机区补齐 |

本机主机测试证据索引：[Windows 3.8 XML](../.agents/iris-audit-20260905/final-win38.xml)、[Windows 3.12 XML](../.agents/iris-audit-20260905/final-win312.xml)、[Linux 3.12 XML](../.agents/iris-audit-20260905/final-linux312.xml)、[sanitizer XML](../.agents/iris-audit-20260905/final-sanitizers.xml)。这些位于私有运行目录，发布时应随正式证据包归档，不能假设其他检出自动包含这些文件。

## IRIS-A02

**实现。** 将 RPC、OTA 与 System Update 的慢工作移入有界服务执行器，协议任务负责提交和取回完成结果。控制请求可在慢服务工作期间处理；对执行中状态、取消、断开连接及停止的资源生命周期作了明确处理。构建依赖改为在 Kconfig 解析前声明的 profile，并保留禁用组件的构建路径。

**回归要点。** 完整 runtime/services 桩测试覆盖 RPC、OTA、System Update 工作在途时的控制响应、执行器忙状态、取消、清理和生命周期；在单/多传输与 OTA 开关组合运行。disabled 与相应 ESP-IDF 工程完成过构建。

**边界。** 有界执行器不承诺抢占任意用户回调，也不能撤销已发生的 Flash 或业务副作用。最终板上控制延迟与内存数据须由实机结果补充，不用源码行数证明性能。

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

**回归要点。** Windows 两版本各 248 通过、2 个 POSIX 专用跳过；Linux 250 通过。CI 声明的 macOS 任务是后续可执行矩阵，本报告不声称本轮已经在 macOS 实跑通过。

## IRIS-T02

**实现。** 修正资源预算路径与执行入口，增加完整 runtime 固件桩、故障与畸形帧 corpus、sanitizer 配置、主机跨平台 CI、System Inventory/System Update/disabled 构建以及发布产物证据清单。必需编译器或产物缺失时不静默跳过；源码、固件与前端预算分组检查。

**回归要点。** 本轮主机、静态检查、前端及 sanitizer 结果见上表。发布清单应关联源码 revision、工作区差异/文件摘要、实际 IDF、sdkconfig、BIN/ELF/map 和启动链产物；正式证据与最终实机固件一致性仍需在下节补齐。

**预算解释。** 旧评估基线已有文件超过旧行数阈值。本轮重新基线化须注明原超限事实及新增执行器/协议实现的容量，不把提高行数阈值或 BIN 上限描述为性能优化。板上时延、堆与任务栈仍由实际测量验收。

## 实机结果待根追加

**状态：未通过最终验收，正在对正确目标设备重做。** 用户已确认先前独立 USB Serial/JTAG 接到了错误设备；此前使用 COM14 的操作与失败结果不得作为目标设备验收证据。当前用户指认的正确独立接口为 COM8，接口 MAC 为 `30:ED:A0:F4:0C:28`。COM 路径可能变化，该 MAC 也不能替代 Iris 的 live Device ID；执行时仍须重新枚举并关联主连接身份。

以下由负责设备操作的根任务填入本次真实结果，不能用已存在的主机测试替代：

| 验收项 | 实际结果/证据 |
| --- | --- |
| 操作前实时 Device ID、Boot ID、主/独立接口关联 | 待根追加 |
| Recovery current 构建 revision、IDF 与产物摘要 | 待根追加 |
| 双维护 lease、旧 crash/core dump 保存、route 与原始日志 | 待根追加 |
| 同 Device ID 进入新 Boot 的 Recovery，服务就绪且固件符合目标 | 待根追加 |
| acceptance fixture 与控制/慢服务在板行为 | 待根追加 |
| 应用安装后的新 Boot、固件身份及 HEALTHY | 待根追加 |
| 最终恢复 HelloWorld，产品行为正常 | 待根追加 |
| Gateway CLI/Web 身份、operation 与证据一致 | 待根追加 |
| 最终逐项提交 SHA、构建及实机证据索引 | 待根追加 |

独立接口 Recovery 是用户新增授权的产品入口能力，经 `mosaico.py recover --recovery-port ...` 执行，仍保留 Gateway 维护和最终身份验收；不绕过 `mosaico.py`，不全擦 Flash。本轮未引入只写 Recovery 分区的隐含捷径。该能力的主机定向测试通过，也不能证明某个物理 COM 口属于预期板卡。

## 本轮不据此关闭的事项

A01、P01、I04、I05 保持计划内；esp-mosaico-tools 的 MOS 编号问题保持原评估状态，本轮只完成 Iris-P03 契约对接和用户新增的独立 USB Serial/JTAG Recovery 路由。未执行的断电矩阵、跨芯片推广、CPU 停机调试和生产安全方案均不纳入已通过结论。待最终正确目标设备闭环完成，再更新本报告的实机区与发布结论。
