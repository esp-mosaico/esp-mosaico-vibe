# Recovery UI 真机验证（2026-09-16）

离线页面交互、截图和保留 Recovery 往返验证通过。联网后的自动续接、Spark
配对码获取和实际固件下载尚未验证：设备没有保存 Wi-Fi，本次未获得可用凭据。
设备最终停留在从 Download From Spark 进入的 Wi-Fi 配网页。

## 设备与固件

- Device ID：`4553502d49524953010030eda0f46056`，ESP32-S31 / ESP-Mosaico v1.2。
- 最终 Recovery：`factory 0.1`，Boot ID `2119017339086551212`。
- 最终 ELF SHA-256：`5f275c9bd1af5bd95ad8ddeeb4b4fffaef5558c9fd3a02a6b81b09dcf47181cc`。
- Gateway Web 工作台：<http://127.0.0.1:18443/>。CLI 和 Web 的 Device ID、
  Boot ID、固件哈希及成功的交互操作记录已交叉核对。
- ESP-IDF `v6.2-dev-2221-g7b9cc1ac79f-dirty`，Python 3.12.3；固件构建零警告。
- 原生 LVGL 与 Recovery 回归：171 tests、45 subtests 通过。

## 真机结果

| 场景 | 结果 | 证据 |
| --- | --- | --- |
| 首页布局 | 主按钮突出，底部三颗次按钮均可完整显示，网址与引导无重叠 | [首页截图](../artifacts/recovery-ui/device/15-final-home.png) |
| 未配网点击主按钮 | 进入 Wi-Fi 页，显示继续 Spark 的提示；Bridge 为 IDLE、未启动 | [Wi-Fi 截图](../artifacts/recovery-ui/device/04-spark-needs-wifi.png) |
| 普通 Wi-Fi 入口 | 显示普通离线提示，没有 Spark 续接意图 | [普通配网](../artifacts/recovery-ui/device/05-wifi-direct.png) |
| 密码格式错误 | 使用单字符测试输入，本地校验拒绝并停留在密码页，没有尝试连接 | [校验提示](../artifacts/recovery-ui/device/07-password-validation.png) |
| TCP pairing 入口 | 可正常打开和返回，布局完整 | 本地证据 `08-tcp-pairing.png` 含配对 token，不在报告中展示 |
| NAND update 入口 | 扫描完成，显示没有固件包，未执行 NAND 更新 | [NAND 列表](../artifacts/recovery-ui/device/11-nand-scanned.png) |
| Spark 下载页 | 使用现有 USB 打开页 RPC 检查离线显示，网址、标题、提示均完整 | [下载页截图](../artifacts/recovery-ui/device/10-spark-offline.png) |
| 取消下载 | 返回首页，Bridge 为 CANCELLED、running=false | [状态记录](../artifacts/recovery-ui/device/cancelled-bridge-status.json) |
| Web 真实点击 | 浏览器开启交互输入，点击主按钮后设备进入配网页；操作记录成功 | [工作台截图](../artifacts/recovery-ui/device/workbench-final-wifi.png)、[设备截图](../artifacts/recovery-ui/device/16-web-click-wifi.png) |
| 启动健康 | HEALTHY，crash_count=0，无有效保留 Core Dump | [状态](../artifacts/recovery-ui/device/final-status.json)、[崩溃检查](../artifacts/recovery-ui/device/final-crash.json) |

以上截图来自设备的实际 RGB565 画面，不是模拟器图片。

## 更新与往返

所有固件写入均经工作区 `mosaico.py system-update` / `install`。
默认 Recovery 自更新包首次被分区表前置校验拒绝，未写入。随后使用与设备实时
分区表 SHA-256 一致的本地分区表重新制包，只包含 Recovery 组件。
最终 bootloader 和分区表哈希均与更新前一致，没有清除身份、凭据或 NAND 数据。

| 状态 | Boot ID | 结果 |
| --- | --- | --- |
| 更新后的 Recovery（含 USB 输入适配） | `3041487950019254728` | HEALTHY |
| gsp_hello 普通应用 | `16216601123426034104` | 安装及运行画面验证通过 |
| 返回新版 Recovery | `12405745766326214343` | Recovery 就绪，固件哈希一致 |
| 再次安装 gsp_hello | `4462500620196780929` | HEALTHY，完成 normal → Recovery → normal |
| 最后返回 Recovery | `2119017339086551212` | 等待 Wi-Fi 配置 |

普通应用使用 `projects/gsp_hello` 已有构建，经过设备分区表匹配与 ELF SHA-256
验证。正常应用最终保留在应用分区；当前运行的是新版 Recovery。

## 验证中补充的代码

原 Recovery 提供截图，但没有 Gateway 触摸 RPC，自动点击最初返回
`ESP_ERR_NOT_FOUND`。新增 `factory_ui_input.c`，实现 Gateway 现有 `0x1001/1`
协议，将按下/移动/抬起同步交给真实 LVGL pointer，限定在 USB 会话使用。
原生测试也改为通过该输入适配点击按钮，覆盖无效坐标、消息长度和 TCP 拒绝。

设备截图和触摸证据采集工具位于
`.agents/tools/recovery_ui_device.py`。其输入通过 Gateway API，不直接访问 USB。
详细操作及截图元数据位于 `artifacts/recovery-ui/device/`；原始固件操作日志由
`mosaico.py` 保存在 `.codex-runs/mosaico/`。

## 尚未覆盖

- Wi-Fi 连接成功后自动继续到 Spark、取得配对码及实际下载，需要可用网络。
- NAND 实际更新未执行，本机 NAND catalog 为空；本次只验证入口与空列表显示。
- 当前固件未启用任务级内存观测，`mosaico.py memory` 返回 HTTP 501。
  常规状态快照仍可读取，未发现崩溃。
- Web 的通用 `system.info` RPC 不在该 Recovery 中提供；设备身份通过状态 API
  和 CLI 核对，不以这个不支持的调用判断固件健康。
