# ESP-31 迁移后端到端测试报告

状态：**实机端到端验收通过**。2026-09-16 在一台 ESP32-S31 上验证了 ESP-Iris 正常应用、Recovery 往返、RPC、日志、单张屏幕截图和内存水位。本次不包含持续实时视频流传输。GSP 应用未启用任务内存观察，相关水位改用 ESP-Iris STATUS 的整体堆指标；这是观察限制，不影响截图和服务健康判定。

## 被测环境

| 项目 | 结果 |
| --- | --- |
| 工作树 | `feat/ESP-31`；实机验收发生在同步前的主仓库 `ffacfbc`、Utils `a6b4554`。同步 `origin/main` 后，对应 ESP-31 提交为主仓库 `7e92092`、Utils `1ee0324`；当前宿主回归测试全部通过，未重复烧写设备 |
| PC/ESP-IDF | Python 3.12.3；ESP-IDF `v6.2-dev-2221-g7b9cc1ac79f-dirty`，ESP32-S31 可用；`mosaico.py doctor` 9 项通过 |
| 构建 | `hello_world`、`gsp_hello`、`iris_acceptance`、`iris_crash`、Recovery 均完成 ESP32-S31 构建；原始日志见各项目 `.codex-runs/idf-low-noise-build/` |
| 宿主检查 | retained Recovery 与 crash fixture 6 项、GSP 截图生命周期 1 项、System Update 制包 4 项均通过 |
| Gateway | 实机验收使用修订 `a6b4554feaf27700ce2dc7c3f89acbf99874a6bb`；该提交已无冲突变基为当前 Utils 的 `1ee0324`。验收时健康接口 ready |
| 设备 | Device ID `4553502d49524953010030eda0f45156`，MAC `30:ed:a0:f4:51:56`，USB Highspeed；契约 `esp-mosaico/v1`，布局 `mosaico-retained-recovery-2m-v1`，Recovery ABI 1 |

本地二进制 SHA-256：`iris_acceptance.bin` 为 `92344f21d0d794ec498bac81e5ed234eaa825f1338e2d4cd33fbc3053687fada`；`gsp_hello.bin` 为 `57c58e4b156c83c9898dba5f44b3bb3dbd95ae751856eb886fa9aad76ff7abb4`；`ui_apps.bin` 为 `5a3884671e5278af51265222ea6d908f868e5ac9c4d6120c4f361d5a423a05f4`。设备返回的 acceptance ELF SHA 为 `697e87d57507fff63c835469e1dba8d2268986772fe8c82fec828c8e145f12f6`，GSP ELF SHA 为 `aa389b6d2ba72ae3bf474a9c9b3131d4c4bc40e2024ef036c53ef7e9d1ae6c80`；两次安装和 System Update 均校验匹配并返回 healthy。

## 实机测试矩阵

| 检查 | 实测结果 | 主要证据 |
| --- | --- | --- |
| Gateway 与实时身份 | 通过；匹配修订的 Gateway 实时发现同一在线 Device ID。Web workbench 使用的 `/v1/devices` 和 `/v1/operations/{id}` 与 CLI 返回相同最终 Boot ID、安装及截图成功记录。 | `.codex-runs/ESP-31-e2e-20260916/gateway-web-api-verification.json` |
| Core Dump 保存 | 写入前保存了原有 17,312 B Core Dump，SHA-256 `9266e52902f58f5ec0adc5930a9995df0c34822bbe22165a6c1e47c1e361a4ef`。缺少其对应旧 ELF，未归因于 ESP-31。 | `.codex-runs/mosaico/20260916T022137Z-crash/raw.log`；Gateway artifacts |
| acceptance 安装与健康 | 通过；Recovery Boot ID `1655024390838925042` → normal `12858974127846144778`，512,624 B 写入，ELF SHA 匹配且 healthy。 | `.codex-runs/mosaico/20260916T022257Z-install/raw.log` |
| RPC | 通过；方法 1 约 2 秒后返回 4 B，计数 1；方法 2 被设备以 `0x104`（`ESP_ERR_INVALID_SIZE`）拒绝；方法 3 随后返回 12 B，三个小端 u32 为 `(1, 1, 1)`。Boot ID 未变，设备继续在线。 | `.codex-runs/mosaico/20260916T022327Z-rpc/`、`20260916T022341Z-rpc/`、`20260916T022355Z-rpc/` |
| 日志 | 通过；保留日志和 20 秒跟随取得 `ACCEPTANCE_ALIVE`、`SLOW_BEGIN`、`SLOW_END`；GSP 首次启动与重装后均取得 `GSP Hello World ready at 480x480 RGB565`。记录各自 Boot ID，日志丢失 0 B。 | `.codex-runs/mosaico/20260916T022355Z-monitor/`、`20260916T023212Z-monitor/` |
| GSP System Update | 通过；目标分区表 SHA `4f8786d12001684a3c02385f31ef167247827bd73b744801494a8c5851bc7231`、应用、`ui_apps` 三组件验证；Recovery `14399326049662327677` → GSP normal `4753894582060732080`，healthy。 | `.codex-runs/mosaico/20260916T022807Z-system-update/raw.log` |
| 截图 | 通过；Gateway 只读接口取得 480×480 RGB PNG，214 色，画面可辨认 Hello World 卡片、System load 和 Level 控件。与模拟器基准同尺寸，首图平均 RGB 通道绝对差均小于 0.7/255。普通重装后再次成功截图，证明 `ui_apps` 保留。 | `.codex-runs/ESP-31-e2e-20260916/gsp_device_screenshot.png`、`gsp_final_screenshot.png`、`gsp_final_screenshot_operation.json` |
| 截图帧释放与稳定性 | 通过；两次离散截图前后内部空闲均 303,883 B、PSRAM 空闲均 14,760,148 B；最低 PSRAM 空闲 13,818,056 B，显示临时帧占用后回收。Boot ID 不变，非法帧 0、日志丢失 0 B、crash_count 0。 | `.codex-runs/ESP-31-e2e-20260916/gsp_status_before_second_screenshot.json`、`gsp_status_after_second_screenshot.json` |
| normal → Recovery → normal | 通过；GSP normal `4753894582060732080` → Recovery `3453352666897986329` → 普通安装后 GSP normal `2217508675654415449`。同一 Device ID，三个不同 Boot ID，最终 ELF SHA 匹配、healthy、UI ready 且可截图。 | `.codex-runs/mosaico/20260916T023104Z-enter-recovery/raw.log`、`20260916T023116Z-install/raw.log`、最终截图操作 JSON |

acceptance 的 `mosaico.py memory` 在启动约 13 秒时读得内部空闲 402,515 B、PSRAM 空闲 16,340,712 B、内部历史最低 368,052 B；约 56 秒运行并完成 RPC 后，当前空闲仍为 402,515 B 和 16,340,712 B，历史最低分别为 363,100 B 和 16,332,516 B。9 个任务栈历史最小剩余值中最低为 708 B，其次 712 B；两次采样未继续下降。原始查询见 `.codex-runs/mosaico/20260916T022321Z-memory/` 与 `20260916T022403Z-memory/`。这些是当前固件配置的整体水位，不能推算各 ESP-Iris PSRAM 对象精确字节数，也不能跨不同应用做同口径节省比较。

GSP normal 的 STATUS 报告 ESP-Iris 核心内部动态 14,748 B、静态 2,332 B、合计 17,080 B。GSP 启用了不同的屏幕和 UI 功能，不能与先前其他固件配置的 14,804 B 直接比较。GSP 和 Recovery 的任务级内存 RPC 均返回 HTTP 501 `task memory observation is unavailable`；本轮用 acceptance 夹具任务查询和 GSP 整体 STATUS 补齐观察面。

## 发现、修正与范围

GSP 普通安装最初因设备分区表 SHA `6f0d33a90336cf8a5361aadf787d819ab53e741d8f3c6614f0b8d40d28000fff` 与目标表不一致，在写入前被 `mosaico.py` 拒绝。首次 System Update 又在 `validating_plan` 返回 `0x10a`（`ESP_ERR_INVALID_VERSION`）：制包器把最低 Recovery 版本硬编码为 `2.5.0-recovery`，本仓库已评审包和设备均为 `0.1`。核对设备对应源码修订 `ad3b24c` 的 Recovery 实现，确认已包含目标表、应用和数据分区的校验与写入路径。本分支将制包门槛对齐到 `0.1`，对应 4 项宿主测试通过；重新制包后设备自主完成全部校验和更新。失败与成功原始记录在 `.codex-runs/mosaico/20260916T022426Z-install/`、`20260916T022455Z-system-update/`、`20260916T022807Z-system-update/`。

方法 2 的预期设备错误在 `mosaico.py` 顶层被笼统表示为 `device_unavailable`；原始 Gateway 响应明确为设备错误 `0x00000104`，随后设备列表与方法 3 均成功。该 CLI 错误分类可能误导自动化读取顶层错误类型，后续可单独改进。

截图服务内部暂时分配完整帧，但本次没有启动持续 mirror、测试帧率，也没有传输 IMAGE/AUDIO 实时媒体通道。所有固件写入、Recovery 切换、日志和内存查询由 `mosaico.py` 管理；截图仅使用当前 Gateway 的只读接口。
