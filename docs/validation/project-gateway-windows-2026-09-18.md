# Windows 双设备项目 Gateway 验收

测试日期：2026-09-18。环境：Windows 11 专业版 x64，build 26200；主机 Python 3.8.7 / 3.12.10，Node.js 24.19.0。

**结论：项目 Gateway 的 Windows 核心用例在本轮修复后通过。** 原提交复现了“升级中关闭拥有者后，设备升级成功但附属 CLI 误报失败”的竞态；另外修复了 Windows GSP 工具下载器的 `.exe` 文件名问题。不能将本报告理解为未修改的原提交全部通过。

## 测试基线

- 工作区已切换到 `codex/project-gateway`，远端最新提交为 `34b1e152a63a52b67548d0a6b258dde9960ef53c`。
- Utils 测试基线为 `49f98bd4d10defce4516dab6bd0616acd4ddd729`；本轮修复与 Windows 测试修正已提交为 `da87441`。
- 测试开始前已有 `test_usb_ownership.py`、`test_cli.py` 两处未提交测试修正，涉及 Windows 路径、JSON 转义和异步状态等待。本轮保留并在该基础上完成测试；原始补丁保存为 `preexisting-changes.patch`。
- ESP-IDF：`D:/esp/esp-idf-master`，revision `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`；源码版本头和 CMake 均为 6.2.0，ESP32-S31 支持已核对；构建使用独立 Python 3.12.10 环境。Git describe 的版本标签问题见后文。
- 测试前旧共享 Gateway 已确认无执行中操作，保存数据库、设备状态、操作记录和日志后退出。

| 项目 | 已握手验证的 Device ID | 硬件 MAC | Windows USB 端点 |
| --- | --- | --- | --- |
| hello_world（A） | `4553502d49524953010030eda0f40c28` | `30:ed:a0:f4:0c:28` | `COM22` / `usb:location=1-8:x.0` |
| gsp_hello（B） | `4553502d49524953010030eda0f46056` | `30:ed:a0:f4:60:56` | `COM23` / `usb:location=1-1:x.0` |

上述端点仅为本次实时发现结果；后续操作仍须重新查询，不能作为固定配置。

## 自动化与构建结果

| 检查 | 最终结果 |
| --- | --- |
| ESP-Iris Python 全量测试，Python 3.8 | 355 passed，2 skipped |
| ESP-Iris Python 全量测试，Python 3.12 | 355 passed，2 skipped |
| Recovery 全量 + 工作区 CLI 集成、保留 Recovery 合约、GSP 工具测试，Python 3.8 | 199 passed，4 skipped |
| 同上，Python 3.12 | 199 passed，4 skipped |
| Workbench 单元测试 | 7 passed |
| Workbench TypeScript / Vite 构建 | 通过 |
| Workbench Playwright 浏览器回归 | 7 passed，1 skipped |
| 真实 Web 工作台与 CLI 身份、Boot ID、OTA 操作记录核对 | 两台均通过，未发现前端异常或 HTTP API 错误 |
| GSP 应用 Windows 完整构建 | 通过，500.5 秒，1 条 warning，`gsp_hello.bin` 1,632,496 字节 |
| 已在本机启动的 hello_world / Recovery 构建结果核验 | 均成功；分别 1 / 2 条 warning，原始构建日志已归档 |
| 新增/修改逻辑的 Ruff 检查、两个仓库 `git diff --check` | 通过；Ruff 排除了既有 `UP036` Python 最低版本防护告警 |

跳过项有明确平台原因：ESP-Iris 的 POSIX 串口符号链接和终端控制；Recovery 的 3 个 Linux/OpenSSL 原生夹具及 1 个 Linux UI 夹具。通用 `hardware.spec.ts` 需要专用测试固件，未强行套用到 hello_world；本轮真实设备 Web 验收由独立脚本完成。

## Windows 实机结果

| 用例 | 结果与证据前缀 |
| --- | --- |
| 两个项目同时持有独立 Gateway、端口、状态目录 | 通过；`02-sessions` |
| 被动发现两台 USB 设备，不自动打开未认领设备 | 通过；`01`、`02-passive-*` |
| 按端点显式认领并握手确认 Device ID | 通过；`03`、`04` |
| 非所有者拒绝认领及业务访问 | 通过；`05`、`10`；未知设备可返回 404，已知但不属于本项目的设备返回归属冲突 |
| A → B → A 转让、不重启；同一 transfer_id 幂等 | 通过；`06`、`07`、`11`、`12` |
| B 同时持有两台设备并按 Device ID 路由 | 通过；`08`；省略 Device ID 返回 selection_error，见 `09` |
| 两台分别 normal → Recovery → normal | 通过；`14-enter-recovery-a`、`first-install-a/`、`20-ota-*`、`39-enter-recovery-b` |
| OTA 中转让被拒绝；另一台持续在线且 Boot ID 不变 | 两台均通过；`20-ota-*-busy-transfer`、`20-ota-*-other-during` |
| CLI 与真实 Web 显示相同 Device ID、完整十进制 Boot ID 和 OTA operation_id | 通过；`21-live-*`，包含截图和操作详情 |
| 监控客户端退出不关闭拥有者会话 | 两台均通过；`22` |
| Ctrl-C 关闭空闲拥有者，释放归属且不影响另一项目 | 通过；`23` |
| 新会话不因历史设备记录自动连接 | 通过；`24`，历史设备 connected=false |
| 强杀空闲 Gateway 后归属保持孤立，不能自动抢占 | 通过；`26`、`27`、`28` |
| 显式 reconcile、claim、转回原项目 | 通过；`29`、`30`、`31`；设备未重启 |
| 强杀空闲拥有者 CLI，子 Gateway 经管道 EOF 收尾 | 通过；`32`、`33` |
| 单次 monitor 临时认领、退出释放；后续 list 保持被动 | 通过；`36`、`37` |
| OTA 中 Ctrl-C 关闭拥有者，等待升级完成，附属 CLI 成功退出 | 修复后通过；`40-*`；修复前失败证据保存在 `drain-before-fix/` |
| 正常应用、无新增崩溃、最终归属和测试进程清理 | 通过；`42` 至 `47`、`final-summary.json` |

共执行 5 次正常应用 OTA：A 四次（首次安装、忙碌互斥、收尾竞态复现、修复复测），B 一次。全部通过工作区 `mosaico.py install --skip-build` 完成。两台是已有 Linux 验收记录的设备，本轮重新通过 `mosaico.py enter-recovery` 核对实时 Recovery 0.1 与 OTA 能力，然后安装；未刷写 Recovery、bootloader、分区表、身份、凭据或 UI 数据分区，也未使用直接串口刷机或人工 ROM 入口。

| 场景 | normal 开始 Boot ID | Recovery Boot ID | normal 完成 Boot ID |
| --- | --- | --- | --- |
| A 首次安装 | `15097145832565075895` | `4426842427501225733` | `2917713084975854988` |
| B 安装 | `1805548537330109892` | `8254641568252962296` | `11244592584058481186` |
| A 收尾竞态修复复测 | `18229766503370516918` | `11744445262912476139` | `14508955808883344109` |

Recovery 转换由产品 CLI 在提交 OTA 前执行，因此部分 OTA 记录的 `previous_boot_id` 已是 Recovery Boot ID，`recovery_boot_id` 为 null。上表结合转换命令、实时轮询及最终操作记录，而不是把 null 当作缺少 Recovery 验证。

## 本轮修复

### 拥有者退出后附属 CLI 丢失最终结果

修复前操作 `13f5f1c2-aa06-4880-909b-b893e52c436a` 已完成设备升级，但 Gateway 在最后一次状态轮询前退出，CLI 返回码 4，误报 Gateway 不可达。原始输出及数据库仍保留。

修复位于 Utils 的 `mosaico_cli/session_runtime.py` 和 `gateway.py`：轮询失败后，仅为已核验的本地项目会话尝试读取持久记录；通过项目生命周期锁证明 Gateway 已停止，SQLite 以只读模式打开，只接受本次会话创建后的同一 operation_id、带完成时间的终态记录。运行中的 Gateway、远程配置、旧会话记录、未完成和缺失记录均不能走此恢复路径。失败终态仍交由原错误处理报告，不会变成成功，也不会重新启动 Gateway 或重放写入。

新增 6 个主机回归用例，包含真实 Gateway 子进程退出后读取结果，以及活跃锁、旧记录、未完成记录、失败记录和缺失记录的边界。实机复测操作为 `f88cdea5-f9e9-4325-b388-603b7dce0acb`，CLI 返回 0；raw.log 明确记录通过已持久化结果补全了最后一次读取，结果为 `healthy=true`。

### Windows GSP 发布包可执行文件名

下载器原先从 Windows ZIP 中寻找 `gspc`，实际文件为 `gspc.exe`，导致 GSP 项目配置失败。`tools/gsp-sim/fetch_gspc.py` 现在根据主机平台使用正确文件名，缓存路径保留 `.exe`，共享下载逻辑也适用于 sim。新增 Windows ZIP 提取及缓存复用测试，随后完成了真实 GSP 编译、安装和设备截图验证。

## 已知限制与非通过项

- `mosaico.py doctor` 仍因本机 ESP-IDF Git 标签显示 `v6.1-dev-7333-g7b9cc1ac79`，对 hello_world / Recovery 的 `>=6.2` 约束返回失败。源码版本头和 CMake 已核对为 6.2.0，revision 与 Linux 验收相同，实际构建和两台设备运行已验证。本轮没有修改 ESP-IDF、伪造标签或放宽工程约束；这个环境诊断不计入通过项。
- hello_world 未启用任务内存观测，辅助 `memory` 探测返回 HTTP 501 `task memory observation is unavailable`。单次会话生命周期改用该固件支持的 monitor 验证；未将该探测失败描述为成功。
- Python 3.8 的浏览器测试结束时记录了 Proactor `WinError 10054` 连接关闭告警；7 项浏览器断言通过，真实工作台验收无 API 错误。原始日志保留。
- 全量默认 Ruff 包含既有 `UP036` 最低 Python 版本防护和下载器顶层 `BLE001` 告警；未为消除告警删除兼容保护。新增代码的检查与格式检查通过。
- 本轮硬件覆盖 Windows、同机同用户、两台 USB Highspeed 设备；未重复进行 TCP、跨电脑或不同工具版本组合的实机测试。

## 最终状态与证据

- A：hello_world 1.0.0，Boot ID `14508955808883344109`，ELF SHA-256 `a05aa390a77d6aaef5a88caa75cbc0485e76bef27b4b6af92984d26b4c380e62`，持续输出 `Hello World!`。
- B：gsp_hello 1.0.0，Boot ID `11244592584058481186`，ELF SHA-256 `6bbe35f77b449d4615eb1f8f7050ceaa1183e22ed636e338c6c63d7f91473d72`，已取得并检查 480×480 设备截图。OTA 操作 ID `01d7caf9-6531-4022-88a1-20594251125b`。
- 两台均 `crash_count=0`、无有效 Core Dump；最终 USB 被动发现仍能看到两台设备。
- 所有测试 Gateway、连接记录及设备归属均已清理为 0；旧共享 Gateway 未恢复启动。重新观察时执行 `python mosaico.py session run --project projects/hello_world` 或对应项目，再打开新打印的工作台地址。

本机证据目录为 `.agents/validation/project-gateway-windows-20260918/`，包含测试 XML、CLI 原始输出、归属和操作快照、真实 Web 截图、设备截图、Gateway 数据库/日志、构建日志及验收脚本。`tested-source.json` 记录基线与修改文件哈希，`evidence-manifest.json` 记录证据文件大小和 SHA-256。该目录已通过本地 Git exclude 排除，保持为私有验收材料。

Utils 修复已提交至其 `codex/project-gateway` 分支；主仓库随本报告更新子模块固定版本，并纳入 GSP 下载器修复和测试。本轮仅创建本地提交，未推送。
