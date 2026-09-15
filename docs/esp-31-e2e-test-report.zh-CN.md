# ESP-31 迁移后端到端测试报告

状态：**等待匹配的 ESP-Iris Gateway 接管设备；尚未完成实机验收**。
本报告只把设备实际返回的结果列为通过；编译、模拟器和旧 Gateway 的
缓存记录均不代替 ESP-31 固件的设备证据。

## 被测版本和准备结果

| 项目 | 结果 | 证据 |
| --- | --- | --- |
| 工作树 | `feat/ESP-31`，主仓库 `f4b1fac`，Utils 子模块 `a6b4554` | 两个仓库均无未提交改动 |
| PC/ESP-IDF | 通过；Python 3.12.3，ESP-IDF `v6.2-dev-2221-g7b9cc1ac79f-dirty`，支持 ESP32-S31 | `python3 mosaico.py --json doctor`，9 项检查通过，检测到 1 个兼容 USB 设备 |
| 正常应用、夹具、Recovery 构建 | 通过；`hello_world`、`gsp_hello`、`iris_acceptance`、`iris_crash`、Recovery 五项 ESP32-S31 构建，编译告警计数 0 | 各项目的 `.codex-runs/idf-low-noise-build/` 原始日志及 `result.json` |
| 本地契约检查 | 通过；6 项 | `python3 -m unittest tests.test_retained_recovery_contract tests.test_iris_crash_fixture` |
| GSP 截图后端生命周期宿主检查 | 通过；1 项 | `python3 -m unittest tests.test_gsp_mirror_lifecycle` |
| 截图视觉基准 | 已生成 480×480 GSP 场景图；暗色背景、Hello World 卡片、System load 和 Level 控件 | `.codex-runs/ESP-31-e2e-20260915/gsp_expected.png`，SHA-256 `b74d881d63d9d8b306f18f37d12bfc2d2a733aa30e8580ad21f8a45b9140a862` |

被测构建的 SHA-256：`iris_acceptance.bin`
`92344f21d0d794ec498bac81e5ed234eaa825f1338e2d4cd33fbc3053687fada`；
`gsp_hello.bin`
`57c58e4b156c83c9898dba5f44b3bb3dbd95ae751856eb886fa9aad76ff7abb4`；
`ui_apps.bin`
`5a3884671e5278af51265222ea6d908f868e5ac9c4d6120c4f361d5a423a05f4`。
这些是本地文件哈希；设备固件身份还需由安装结果和设备状态验证。

## 实机测试矩阵

| 检查 | 判定条件 | 现状 |
| --- | --- | --- |
| Gateway 与实时设备 | Gateway 修订与当前 Utils 固定修订一致；`mosaico.py list --details` 得到在线 Device ID、Boot ID、模式和分区契约 | **阻塞**：本机 8443 端口是另一工作区的 Gateway，修订不一致；`mosaico.py` 拒绝操作，也未停止它 |
| 安装与健康 | 通过 `mosaico.py install` 安装被测 normal 固件；同一 Device ID 返回新的 Boot ID，ELF SHA/固件身份匹配且 healthy | 未执行 |
| 慢速 RPC 与工作副本 | `iris_acceptance` 方法 1 返回 4 B 成功计数并记录 `SLOW_BEGIN`/`SLOW_END`；方法 2 返回 `ESP_ERR_INVALID_SIZE` 且不重启；方法 3 返回 12 B 计数；期间协议仍可响应状态 | 未执行 |
| 日志 | `mosaico.py monitor --snapshot` 和 20 秒跟随能取到 `IRIS_READY`、健康消息、夹具 RPC 日志及 GSP ready 日志；同一 Boot ID，记录丢弃计数 | 未执行 |
| 屏幕截图 | `gsp_hello` 注册屏幕后端后，Gateway 读取并保存单张 480×480 PNG；画面非空且与基准场景内容一致，截图后仍能读取状态/日志 | 未执行 |
| 内存与稳定性 | `mosaico.py memory` 给出内部 RAM、PSRAM 和任务栈水位；截图与 RPC 后无非法帧增长、无重启、无持续堆下降 | 未执行 |
| 正常 → Recovery → 正常 | `mosaico.py enter-recovery` 和 `install` 完成；同一 Device ID，三个不同 Boot ID，Recovery ready，最终 normal 固件健康 | 未执行 |

截图服务内部可能临时拉取完整的一帧，但本次**不测试持续实时视频流**、
镜像帧率或 IMAGE/AUDIO 媒体通道。

## 待执行设备步骤

1. 释放另一工作区的 Gateway 后，运行 `python3 mosaico.py --json list --details`，
   以在线信息选择设备。检查并保存设备现有 Core Dump，确认 Recovery
   与分区契约后再安装。设备未验证时先运行 `python3 mosaico.py recover`。
2. 安装 `tests/firmware/iris_acceptance`，在 normal 状态读取列表、日志、
   内存和三个专用 RPC。方法 1 的 deadline 设为至少 5 秒；记录调用前后
   Device ID、Boot ID、内部堆最低值、PSRAM 空闲值、栈水位及帧计数。
3. 经 `mosaico.py enter-recovery` 回到 Recovery，安装 `gsp_hello` 并核实
   `ui_apps` 是被测场景资源；若设备上的 UI 资源不同，应按该项目文档
   使用 `mosaico.py system-update` 更新 UI 资源，不能仅凭 app 安装成功
   宣称截图有效。
4. 使用当前 Utils 的 ESP-Iris Gateway **只读** `ctl screenshot` 命令保存
   单张 PNG，检查文件签名、尺寸、画面内容和截图操作记录。用
   `mosaico.py monitor` 与 `memory` 复查服务健康，不启动持续 mirror。
5. 再次进入 Recovery 并通过 `mosaico.py install` 返回 normal，核对同一
   Device ID 的三个 Boot ID、Recovery ready、最终固件身份和健康状态。

所有安装、Recovery 切换、日志和内存查询均由 `mosaico.py` 管理；截图
只通过当前版本 Gateway 的只读接口取得。原始设备日志、操作记录、截图
及每一步 JSON 输出应存放于 `.codex-runs/ESP-31-e2e-20260915/`，最终
报告须把上表“未执行”替换成实测结果和证据路径。
