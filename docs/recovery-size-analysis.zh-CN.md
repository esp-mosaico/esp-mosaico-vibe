# Recovery 固件大小分析与缩减实验

分析日期：2026-09-21。分析基线为上游 `main` 的 `e038e59`，Utilities 为
`ec2213b`；工作分支为 `feat/ESP-61`。后续数组拆分实现已提交至 Utilities
`d01374a`（rebase 前为 `552f956`）。原 ESP-61 页面实现计划保留。

**已落实系统更新状态中的大零值数组拆分，实测节省 22,528 字节；需要更大余量时，
可再考虑只开启静默断言，单独实测节省 74,656 字节。** 前者不裁剪产品能力或诊断文字，
后者保留断言检查和普通错误检查日志，但断言失败的位置需要借助对应 ELF 解码。

本文保留优化前基线与候选构建记录。正式源码只采用数组拆分，生产配置与基线一致，
预置包仍是此前已评审版本；没有设备写入或真机验收。
下列实验的节省量各自相对同一个基线，不可直接相加。

正式实现验证：`factory.bin` 为 **1,810,064 字节**，固定槽余量 **24,944 字节**；
低噪声构建通过，仍有 1 条分区接近满载警告。5 项主机测试通过，包括启用
ASan/UBSan 的 Bridge worker/backend/control，以及产品记录和 Recovery 版本契约检查。
ELF 确认 22,272 字节计划数组位于 `.dram0.bss`；配置逐项与基线一致。
本地构建记录与验证摘要位于
[implementation](../.agents/analysis/esp-61-recovery-size/implementation/)。

## 基线与测量口径

| 项目 | 结果 |
| --- | ---: |
| 固定 `factory` 槽 | 1,835,008 字节（1.75 MiB） |
| 当前已评审预置 `factory.bin` | 1,831,984 字节 |
| 优化前源码重建 `factory.bin` | **1,832,592 字节** |
| 优化前源码剩余容量 | **2,416 字节** |
| GSP 静态库实际链接内容，排除 BSS | 377,210 字节，约占固件 20.6% |
| 编入固件的压缩 GSPB | 42,523 字节 |
| GSPB 解压后 | 145,424 字节，其中字体资源 111,296 字节 |
| 完整根证书资源 | 56,076 字节 |
| 初始化内部 RAM 数据段 `.dram0.data` | 79,884 字节，也占用镜像空间 |

环境已用低噪声 runner 的 `doctor` 核验：ESP-IDF
`v6.2-dev-2221-g7b9cc1ac79f-dirty`，revision `7b9cc1ac79f8`，Python 3.12.3，
目标 ESP32-S31，符合 Recovery 的 `idf >=6.2` 约束。该 IDF checkout 存在已有本地修改，
实验未改动它；所有对比均使用相同环境，跨工具链的绝对大小需要重测。

最终容量以实际 `factory.bin` 为准，ELF/map 用于解释变化。源码已经启用 `-Os`、
编译及链接 LTO、Picolibc、Zopfli 压缩；SoftAP、企业 Wi-Fi、TLS 服务端、
LVGL 显示适配和 GSP JPEG 解码均已关闭，这些不是本次可重复获得的新收益。

## 完整固件构建对比

| 单独应用的实验 | `factory.bin` 字节 | 节省字节 | 槽内余量字节 | 主要代价 |
| --- | ---: | ---: | ---: | --- |
| 基线 | 1,832,592 | — | 2,416 | 当前行为 |
| 拆分系统更新状态数组 | 1,810,064 | **22,528** | **24,944** | 小范围内部结构调整；DRAM 总占用增加 8 字节 |
| 只开启静默断言 | 1,757,936 | **74,656** | **77,072** | 断言表达式/位置文字减少，依赖 ELF 解码 |
| 静默断言 + 静默错误检查 | 1,671,744 | **160,848** | **163,264** | `ESP_RETURN_ON_*` 等失败原因文字也被移除 |
| 完整根证书包改为 common 集合 | 1,790,144 | **42,448** | **44,864** | 受信任根证书范围缩小 |
| 关闭 ESP/mbedTLS 错误说明字符串 | 1,824,736 | **7,856** | **10,272** | 部分现有日志会退化为 `UNKNOWN ERROR` |

所有候选均通过低噪声固件构建和固定槽容量检查。静默错误检查组合产生 9 条
NAND 驱动未使用变量警告：这些变量只供错误日志格式参数使用，关闭文字后不再引用。
基线及其余候选的构建各有 1 条接近分区满载的警告。

## 1. 优先：拆分零值数组，保留功能和诊断

位置：[factory_system_update.c](../submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery/main/factory_system_update.c)。

优化前 `s_update` 内嵌 96 项更新计划数组，整个结构为 22,364 字节。
其静态初始化只有 `.active_index = -1` 的 4 个字节非零，但这使整个结构进入
`.dram0.data`，连同其余零值一起存入镜像。

正式实现采用已验证的方案：将计划数组拆成独立静态数组，结构内保留指针和原来的 `-1` 初值；
清理函数改用独立数组的 `sizeof`。这样不引入启动阶段的延迟初始化，
更新组件上限仍是 96，原有索引与会话语义保持一致。

ELF 证据：

- `s_update_plan`：22,272 字节，进入 `.dram0.bss`，启动清零而无需保存零值镜像。
- `s_update`：缩至 96 字节，仍在 `.dram0.data`。
- 完整镜像净减 22,528 字节，已包含链接、代码布局与对齐变化。
- 内部 RAM 总占用从 241,240 增至 241,248 字节；这是 Flash 优化，不是 RAM 节省。
- 现有 Bridge worker、更新 backend、control 三项 host 测试在 ASan/UBSan 下全部通过。

候选补丁与测试日志保存在本地
[split_update_plan](../.agents/analysis/esp-61-recovery-size/split_update_plan/)。
更新预置包之前仍需验证 USB/Bridge/NAND 更新、取消与异常重试、Recovery 自更新及正常启动转换。

还有类似现象值得后续检查：`g_iris` 为 9,224 字节，仅 20 个初始字节非零；
TinyUSB `_cdcd_epbuf` 为 16,384 字节且全零，却因 DMA/DRAM section 属性进入数据段；
Core Dump 日志缓冲也有专门 section 要求。前者涉及启动锁和生命周期，后两者涉及
DMA 与崩溃证据，不能仅因内容为零就改成普通 BSS。本次未修改或计入它们的收益。

## 2. 大幅缩减：保留检查，减少诊断文字

只启用 `CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT=y`，并让
`CONFIG_COMPILER_OPTIMIZATION_CHECKS_SILENT` 保持关闭，实测即可节省 74,656 字节。
失败的断言仍会触发中止；普通错误检查与 INFO 日志仍保留。生产采用时，应保存
与设备镜像对应的 ELF，并确认 Gateway 的 Core Dump 获取和定位流程可用。

同时启用 `CONFIG_COMPILER_OPTIMIZATION_CHECKS_SILENT=y` 后，总计节省 160,848 字节。
相对于仅静默断言，此组合额外减少 86,192 字节；这不是第二个开关单独开启的测量值。
本地 `esp_check.h` 确认条件判断、错误返回与跳转仍在，但失败原因文字消失，
Recovery 的故障排查会更依赖结构化错误状态。当前没有必要为 ESP-61 一次用尽这项收益。

这两类配置的语义与限制见
[ESP-IDF 官方体积优化说明](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/size.html)。

## 3. 证书包：收益明确，需管理兼容范围

设置 `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN=y`，保留证书验证。
本地 IDF 的 common 集合包含 33 个根证书，生成资源从 56,076 降至 13,622 字节，
完整镜像净减 42,448 字节。

源码中的 Bridge 请求使用同一个可配置 HTTPS Origin，并禁止自动重定向。
本次使用从同一 IDF common 集合生成的 CA 文件，经 PC OpenSSL 对默认
`iris-bridge.esp-claw.com` 做主机名及证书链验证，TLS 1.2 与 TLS 1.3 均成功。
这仅证明当前端点在 PC 上的证书链兼容；固件当前使用 TLS 1.2，仍需设备握手、
注册和下载验证。自定义 Bridge 域名与未来证书链轮换也需要纳入覆盖范围。
证书包配置与维护方式见
[ESP-IDF 官方证书包文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_crt_bundle.html)。

## 4. UI 资源与 GSP：区分近期空间和长期裁剪

当前 GSPB 已使用 Zopfli 200 次迭代及其他 Deflate 候选择优，继续提高无损压缩强度
不应作为主要空间来源。历史字体实验的基线 GSPB 与当前构建 SHA-256 完全一致，
本次用当前压缩器重算候选资源得到：

| 资源变化 | 压缩 GSPB 字节 | 相对当前资源节省 |
| --- | ---: | ---: |
| 当前页面 | 42,523 | — |
| 32px 标题统一为 24px | 36,745 | 5,778 字节 |
| 所有 24px 文字改为 20px | 34,026 | 8,497 字节 |
| 配对码 40px 改为 32px | 38,066 | 4,457 字节 |

这些是资源包实验，没有对改字号的页面做视觉验收或完整固件重建。
ESP-61 要求配对码醒目，建议保持大配对码，优先考虑减少其他标题字号种类。
动态 SSID、文件名等不能未经分析就收缩字符集。

GSP 库本身约 377 KB，比压缩 UI 资源大得多。当前使用官方预编译库，
`ENABLE_IMAGE_CACHE`、`ENABLE_TRANSITION_SNAPSHOTS` 等是运行时选项，
库仍含对应实现；缩小对象池或关闭缓存主要影响 RAM，不能承诺移除相同规模的 Flash 代码。
长期可与 GSP 组件维护方评估面向 Recovery 的构建能力裁剪，例如限定 RGB565 与所需控件、
编解码器，但应由组件提供匹配的 bundle 能力检查及模拟器验证；本次没有测量可移除量。

## 避免误判与实施次序

当前 map 把约 179 KB 的合并字符串池归到
`bootloader_support_esp_err_codes.c.obj`。该对象原始字符串 section 只有 `0x46` 字节，
链接合并后显示为 `0x2bb08`；因此不能据 `size-components` 表格推断“删掉 bootloader
错误码就能省 179 KB”。最终应以单项重建和 ELF section 对比判断收益。

同理，去掉 ELF 调试信息或清理构建目录不减少 `factory.bin`；降低 task stack
主要改变运行时 RAM。Recovery 自更新会将镜像填充到整个固定槽，降低固件有效大小
是为代码/资源腾出容量，不代表该更新协议的写入范围随之缩小。

建议按以下顺序实施：

1. 状态数组拆分已作为独立源码变更落实，取得约 22 KiB 空间；设备更新回归仍待完成。
2. 实施 ESP-61 的静态二维码与布局，保持配对码可读性，重新测量完整镜像。
3. 如需要数十 KiB 的长期余量，再单独评估静默断言及错误解码工作流。
4. common 证书包作为另一条可选路径，建立默认及自定义服务器的证书兼容检查。
5. 更大规模需求再推进 GSP 组件裁剪；各方案组合后必须重新构建，不能相加代替测量。

## 本地证据

[实验目录](../.agents/analysis/esp-61-recovery-size/) 包含各版本的 `factory.bin`、
ELF、map、sdkconfig、逐组件/逐文件报告、主机测试日志和临时源码补丁。
每个目录的 `build-result.json` 记录原始构建日志的绝对路径；
`experiment.py` 记录各项配置实验。该目录被 Git 忽略，报告中的测量结果才是版本化记录。
正式采用任何方案时，按 [Recovery 文档](../submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery/README.md)
完成设备验收后再更新已评审预置包。
