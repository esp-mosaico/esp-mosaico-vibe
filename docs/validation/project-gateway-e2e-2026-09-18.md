# 双设备项目 Gateway 端到端验收

测试日期：2026-09-18，Linux PC，Python 3.12.3。测试对象为本机两台通过 USB Highspeed 连接的 ESP32-S31。

**结论：项目 Gateway 的实机核心用例通过。** 已验证设备发现、显式归属、跨项目转让、双设备路由、OTA 后重连、忙碌互斥、会话退出和崩溃恢复。实机 Web 检查发现一处历史设备轮询错误，已修复并回归。

本轮正常应用安装全部通过工作区 `mosaico.py install` 完成。共执行 3 次 Recovery-first OTA：两个项目各 1 次，另用 hello_world 补测升级中退出拥有者。使用现有构建产物及 ELF 哈希验证，没有重建或刷写 Recovery，也没有擦除身份、凭据或数据分区。

## 环境和设备

- ESP-IDF：`v6.2-dev-2221-g7b9cc1ac79f-dirty`，revision `7b9cc1ac79f8`；已通过 `mosaico.py doctor` 验证 Python 环境、工程约束和 ESP32-S31 支持。
- ESP-Mosaico Utils 基线：`94eb779279e5df5512f5bc289da1c232951e3058`，包含本轮尚未提交的实现；各修改文件的 SHA-256 见证据包 `tested-source.json`。
- 两个项目初始使用独立的本地端口 `40453`、`38517`，独立数据库和原始日志。
- 测试前保存并退出了空闲的旧共享 Gateway，原历史数据保留。其缓存中还有一台离线设备，未作为本轮硬件目标。

| 项目 | 已握手验证的 Device ID | 硬件 MAC | USB 端点 |
| --- | --- | --- | --- |
| hello_world（A） | `4553502d49524953010030eda0f40c28` | `30:ed:a0:f4:0c:28` | `usb:location=1-7:1.0` |
| gsp_hello（B） | `4553502d49524953010030eda0f46056` | `30:ed:a0:f4:60:56` | `usb:location=1-2:1.0` |

## 实机结果

| 用例 | 结果与证据文件前缀 |
| --- | --- |
| 两个项目同时启动，独立端口和状态目录 | 通过；`session-a-original`、`session-b-original` |
| 未认领设备只发现，不自动打开连接 | 通过；`01-discover-*`、`02-passive-*` |
| 按端点显式认领，握手确认 Device ID | 通过；`03-claim-*`、`05-web-status-*` |
| 非所有者不能直接认领或操作设备 | 通过；`06-reject-cross-claim-*`、`15-old-owner-rejected` |
| A → B → A 转让，不重启设备 | 通过；`10-transfer-a-to-b`、`14-transferred-web-*`、`16-transfer-b-back-to-a` |
| 同一 transfer_id 重试幂等 | 通过；`11-transfer-idempotent` |
| B 同时持有两台设备，按 Device ID 分别路由 | 通过；`13-route-device-*`；省略 ID 的操作被拒绝，见 `12-ambiguous-device-rejected` |
| 两台设备各完成 normal → Recovery → normal | 通过；`20-ota-*-transitions`，身份不变、Boot ID 更新、最终 ELF 一致、healthy=true |
| OTA 正在执行时拒绝转让 | 两台均通过；`20-ota-*-busy-transfer` |
| 一台升级不影响另一台 | 两次均通过；另一台在线且 Boot ID 不变，见 `20-ota-*-other-during` |
| CLI 与真实 Web 的 Device ID、Boot ID、OTA 操作 ID 一致 | 通过；`21-web-evidence-*` 和 `21-live-*` 截图 |
| 监控客户端退出不关闭持续会话 | 两台均通过；`22-follower-monitor-*` |
| 关闭 A 拥有者后，A 网关退出、释放归属，B 不受影响 | 通过；`25-after-owner-a-exit` |
| 重建项目会话，不因历史记录自动连接 | 通过；`26-restarted-project-passive` |
| 强制终止空闲 Gateway，归属成为孤立记录，不能自动抢占 | 通过；`28-fault-injection`、`29-orphan-visible`、`30-no-automatic-orphan-takeover` |
| 显式核对孤立归属后认领，再转回项目 | 通过；`31` 至 `34`；两个设备 Boot ID 均未因故障注入改变 |
| 强制终止拥有者 CLI，子 Gateway 经管道 EOF 收尾退出 | 通过；`35-owner-crash-injection`、`36-owner-crash-cleanup` |
| 单次命令临时认领，结束后释放；下一次 list 仍只发现 | 通过；`37-one-shot-command-lifecycle` 至 `39-one-shot-list-stays-passive` |
| OTA 执行中关闭拥有者，Gateway 等待操作结束，附属 CLI 收到成功 | 通过；`44-draining-project`、`44-drain-install`，CLI 返回码 0 |
| 最终应用正常、设备无新崩溃，所有测试 Gateway 及归属清理 | 通过；`45-final-live-summary`、`46-final-doctor`、`47-final-cleanup` |

首次两台 OTA 的 Boot ID 证据如下（保持十进制字符串，避免浏览器大整数精度损失）：

| 设备 | 初始 normal | Recovery | OTA 后 normal |
| --- | --- | --- | --- |
| A | `872106807831897216` | `4885967060106695086` | `14477026393259826589` |
| B | `6863568274095816640` | `2910554840081661009` | `1805548537330109892` |

OTA 操作 ID：

- A：`06cb2707-4278-4b7a-9cc1-6b679001afa2`。
- B：`3fc776ec-325c-485b-8cfe-45b4a3c5b121`。
- A 升级中关闭拥有者：`ddcb3282-7e62-4f13-9d04-0edf3fef6bf8`；Recovery Boot ID `9660567949220692640`，完成后 normal Boot ID `15097145832565075895`。

转让记录 ID：`3152bf28-8283-431b-9840-edc4875914b5`、`fc488bcf-c12f-49e9-9c53-ca51800b0f76`、`6c4cfe4d-26c1-4bac-afb6-f7f6ca29d694`，均为 `completed`。

## 本轮修复和回归

转让后的旧设备仍留在项目历史中。工作台原先优先选中排序靠前的离线历史设备，并继续请求该项目已无权访问的实时状态，产生重复 HTTP 409。

修复后，初始选择优先使用在线设备；查看离线历史时展示缓存并标记 stale，不再请求它的实时状态。新增浏览器回归覆盖“在线设备默认选择 + 手动查看转出设备历史 + 不发出越权状态轮询”。实际 B 工作台已复现修复前错误，并验证修复后无此 API 错误。

回归结果：前端单元测试 7 项、浏览器回归 7 项通过，TypeScript/Vite 构建通过。通用 `hardware.spec.ts` 需要另一套测试固件，本轮浏览器回归中跳过；上述双设备实机 Web 核验由本机 HIL 脚本单独完成。

## 验收范围和辅助探测

- 本轮实机覆盖 Linux、同机同用户、USB Highspeed；TCP、跨平台和不同工具版本组合未在本轮硬件上重复验证。
- 第三会话在转让保留窗口抢占、双方同时崩溃等更细分竞态仍以此前主机测试为依据，不能把它们描述成本轮实机已覆盖。
- hello_world 未注册探测时尝试的通用 RPC `1/2`，返回设备错误 `0x105`；截图请求也返回空帧描述。这两项辅助探测未通过，不计入网关归属/转让的通过项。hello_world 的产品行为凭据使用正常启动、周期性 `Hello World!` 日志、健康检查和 ELF 身份；gsp_hello 的实机截图已获取并确认画面正常。没有为测试额外改变应用功能。

## 最终状态与证据

- A 运行 hello_world 1.0.0，最终 Boot ID `15097145832565075895`，ELF SHA-256 `5bfbf6bffebedda59798b011922ad79e33ed2848869f9725632bb3866b3febad`。本次安装的是工作区已有构建，哈希不同于测试开始时的 `763d76bbe97962d9f793b21b19f82a926c0106c660fa0565590b47c6801824ea`。
- B 运行 gsp_hello 1.0.0，最终 Boot ID `1805548537330109892`，ELF SHA-256 `3aea12133f0737ecb8017198d39dc85ba466521e66c3ed554eaab1be02848944`，与测试前固件相同。
- 两台设备最终均无有效 Core Dump，crash_count=0；USB 枚举仍能看到两台设备。
- 所有测试 Gateway 已退出，归属表和当前连接记录为空。旧共享 Gateway 未重新启动，原日志和状态保留；先前测试地址已关闭。
- 再次开发时运行 `python mosaico.py session run --project projects/hello_world` 或对应项目，使用新输出的地址打开工作台。

本机完整证据保存在 `.agents/validation/project-gateway-e2e-20260918/`：CLI 原始输出、操作记录、归属快照、Boot ID 转换、浏览器截图、Gateway 原始日志、CLI raw.log 副本和测试脚本。`evidence-manifest.json` 提供文件大小和 SHA-256；该目录是本机验收材料，不属于发布包。
