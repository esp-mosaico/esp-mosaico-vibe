# ESP-61 实现与验证记录

验证日期：2026-09-22，Utilities 实现提交 `08f5f1b`。
实现包括 Download Ideas 二维码、完整网址、长配对码适配，
以及 OTA 当前状态、传输速率、整包和分组件进度。

## 已实现

- 首页和下载页统一使用 `mosaico-ideas.espressif.com`。185×185 静态二维码包含
  `https://mosaico-ideas.espressif.com/`，带四模块白边，不增加设备端 QR 编码库。
- 常规配对码使用 40px；最长 15 字节的 ASCII 码完整显示，必要时复用 24px 字体。
  保留后台预取、返回复用、Cancel 异步停止及可见页面内过期续期的现有逻辑。
- OTA 显示阶段、已传输/总容量、百分比与约一秒窗口的有效负载速率。
  Bridge/USB/TCP/NAND 对应 Download/Receive/Receive/Read 标签。
- 系统更新按整包有效字节数计算主进度，另列组件 ID、组件百分比和校验数量。
  任务或组件切换、计数回退会重置速率；停滞显示零，非传输阶段显示 `--`。
  未知总大小显示 `--`，失败页保留传输百分比。接收 100% 不等于更新成功。
- 统计字段仅扩展 Recovery 本地快照，不改变 ESP-Iris 线协议、Recovery ABI 或固定槽。

下图分别为同源原生模拟器下载页与真机 USB 系统更新画面：

![Download Ideas 原生模拟器](images/esp-61/download-ideas.png)

![USB 系统更新真机截图](images/esp-61/ota-usb.png)

## 主机与构建验证

使用 ESP-GSP 1.4.0 / gspc 0.5.0，Recovery 和 PC backend 共用控制器与场景。

| 检查 | 结果 |
| --- | --- |
| 原生 UI 输入与渲染 | 12 个场景通过；新增测试从实际截图解码二维码，覆盖普通/长码/等待、组件切换、停滞、重试、新任务、未知大小及提交失败 |
| 独立速率采样 C 测试 | 通过；覆盖不规则采样、重复时间、时钟回退、来源/任务/组件切换、计数回退及 64 位进度 |
| Bridge worker/backend/control | 3 项通过，启用 ASan/UBSan；检查整包统计及拒绝重复写入时不重复累计字节 |
| 字体、资源打包、产品契约、版本、Bridge 默认配置 | 5 项通过 |
| Recovery 构建 | ESP-IDF 6.2-dev `7b9cc1ac79f8`、Python 3.12.3、ESP32-S31，构建及固定槽检查通过 |
| Hello World 构建 | 同一 IDF 环境，构建通过，无警告 |

Recovery `factory.bin` 为 **1,812,688 字节**，固定槽 1,835,008 字节，
剩余 **22,320 字节**。相较仅完成 BSS 数组优化的 1,810,064 字节增加 2,624 字节；
仍比优化前源码基线小 19,904 字节。Recovery 构建保留一项空间余量警告。

## 已完成的真机回路

固定 Device ID：`4553502d49524953010030eda0f4518e`，硬件 v1.2，USB Highspeed。
通过工作区 `mosaico.py` 和项目 Gateway 执行写入及验证，未使用另一台 ROM 模式设备。
初始设备留有旧失败状态和 core dump；首次自更新前产品工具自动保存了 core dump，
未将其当成本次变更产生的故障。

| 操作 | Boot ID 与结果 |
| --- | --- |
| Recovery 自更新 | `4206326526575172203` → `6123387197345837199`；新 Recovery ELF SHA-256 与构建一致，HEALTHY |
| USB 系统更新 | Recovery → Hello World，Boot ID `5719236912961588250`；三个组件全部写入、校验、提交，目标应用健康 |
| USB 普通 OTA | normal `5719236912961588250` → Recovery `52331897880165680` → normal `10042825046923618859`；固件哈希及健康状态通过 |
| 下载页 | 真机截图独立解码得到完整目标 URL；真实 Bridge 注册取得配对码，返回再进入复用原码，Cancel 异步停止，重开取得有效码 |
| Gateway 工作台 | 页面所示 Device ID、Boot ID 与同一 Gateway 实时状态一致；操作记录留存于该项目 Gateway |

目标 Recovery ELF SHA-256：
`2bbea706db4907b3fd3939340eaa646f87f56d7b98da86503fd5a7521bc520e5`。
USB 测试目标 Hello World ELF SHA-256：
`86628a6925d30097e4958fb2b4eddafcf3f409733ccdd770d4a4f45c31602a8f`。

设备已有应用布局，与 Recovery 工程默认表不同。专用自更新包使用实时 inventory
返回的 `4f8786d12001684a3c02385f31ef167247827bd73b744801494a8c5851bc7231`
作为布局前置条件，包内仅有 Recovery；自更新前后分区表与 bootloader 哈希一致。
后续 Hello World 系统包的目标表也与设备现有表一致。

连续真机画面记录了 USB 系统更新约 135 KiB/s、普通 OTA 约 64 KiB/s 的接收速率。
系统更新中整包进度持续推进，切换第三个组件时整包没有退回零，短组件首次采样显示
`--`。这些是对应采样窗口的实测值，不能作为设备吞吐量上限。

## 尚待完成

- 网站可正常打开并进入 Hello World 在线烧录页，配对前出现 Cloudflare 人机验证。
  已请求操作者完成验证并触发下载；正在保留设备配对页和连续采集。Bridge 的实际
  HTTPS 下载、提交及应用重启尚不能标记通过。
- TCP 与 NAND 的真机传输、受控网络中断重试尚未验证；主机已覆盖对应数据和状态
  边界，不替代硬件证据。本次 Gateway 的文件卷/目录接口返回 HTTP 501，未通过
  此路径传入 NAND 测试包。未执行真机断电测试。
- 预置 Recovery 包保持原评审版本，待剩余设备验收完成后整体更新。

## 证据位置

完整原始日志、结构化命令结果与连续截图存于本工作区的
`.agents/analysis/esp-61-e2e/`；产品命令原始日志位于 `.codex-runs/mosaico/`。
Recovery 构建日志为 Recovery 工程下
`.codex-runs/idf-low-noise-build/20260922-104925-build-2754999/raw.log`。

关键 Gateway 操作 ID：

- Recovery 自更新：`2dbd7bc1-351f-40de-9507-a8244a6ae5df`
- USB 系统更新：`0b9afce7-7924-40fb-b1ab-f89c4d9fb76b`
- USB 普通 OTA：`0675470e-e691-4830-a45e-38c5ff97c416`

本记录中保留的截图已随文档纳入版本控制；原始设备与浏览器会话证据留在本地。
