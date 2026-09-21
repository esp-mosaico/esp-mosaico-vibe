# ESP-61：Recovery 二维码与 Download Ideas 页面实现计划

状态：页面方案待实施；已完成需求与源码核对、
[固件大小分析及数组拆分优化](recovery-size-analysis.zh-CN.md)，尚未实施页面或操作设备。

需求：[ESP-61 — Recovery 固件添加二维码并修改链接](https://linear.app/loop233/issue/ESP-61/recovery-固件添加二维码并修改链接)。
工作分支：`feat/ESP-61`，已同步到 `upstream/main` 的 `e038e59`；
Utilities 已更新到包含数组拆分优化的 `552f956`，容量分析原始基线为 `ec2213b`。

## 目标与现状

将 Recovery 首页及 Download Ideas 页面的旧域名统一改为
`mosaico-ideas.espressif.com`。下载页增加二维码，扫码内容为完整的
`https://mosaico-ideas.espressif.com/`；二维码下方显示小号网址与醒目的实时配对码。
本期按“扫码打开网站，再输入配对码”的流程设计。

源码核对结果：

- 界面为 480×480、RGB565、ESP-GSP 1.4.0，已有可复用的原生 PC backend。
- 旧域名出现在 Recovery `ui/main.json` 的首页与下载页，以及 Recovery README。
- 配对码由 `main/vibe_ui.c` 的 `VIBE_BRIDGE` 分支读取快照并更新；现有字号为 40px。
- Bridge 已支持后台预取、返回保留会话、Cancel 结束会话和下载页内过期续期。
- 当前预置包的 `factory.bin` 为 1,831,984 字节，固定槽为 1,835,008 字节，
  余量仅 **3,024 字节**。这是预置包基线，新源码构建的余量须重新测量。
  优化前源码实测余量为 **2,416 字节**；更新状态零值数组拆分现已落实，
  正式构建缩减 22,528 字节，余量为 **24,944 字节**，详见大小分析报告。

## 推荐界面与技术方案

延续黑底、白字、橙色强调的现有风格。推荐从上到下排列：

| 区域 | 建议呈现 |
| --- | --- |
| 顶栏 | 保留返回按钮与 `Download Ideas` 标题 |
| 二维码 | 居中，约 180–200px，黑码白底，完整保留四模块白色边界；最终尺寸取模块整数倍 |
| 网址 | 二维码下方，优先复用 16px 灰字，显示完整域名 |
| 配对区 | `PAIRING CODE` 小标题、独立卡片、高对比度 40px 配对码 |
| 状态与操作 | 倒计时或等待/失败提示、简短操作说明、底部 Cancel |

先制作 480×480 静态布局稿，确认整体布局后再修改生产场景。布局须容纳状态提示，
保留底部按钮的触摸空间；常规码优先复用现有 40px 字体子集以控制体积。
协议缓冲区允许最多 15 个字符，不能只按模拟器的 10 字符示例验收：
若长码超过大字区域，使用现有较小字号的备用完整显示，验证字形覆盖与资源成本，
不裁剪、不省略、不修改实际码值。

二维码采用**构建前离线生成并纳入版本控制的静态图片资源**，通过 GSP 的 `image`
对象编入现有 bundle，再走现有 Zopfli 压缩与 ROM 解压链路。提供可复现的生成脚本，
固定 URL、纠错级别、边界和整数缩放参数；普通固件构建直接消费已生成资源。
编码库仅用于主机资源生成。最终是否满足预算，以压缩后的 GSPB 与完整固件大小为准。

二维码保持固定网站入口，配对码通过既有 `bridge_code` 绑定动态显示。
网页域名与 HTTPS Bridge 后端是不同用途的地址，此任务只替换用户访问网站的链接。
配对预取、注册、续期、下载轮询及 Recovery 分区/ABI 沿用现有契约。

## 实施顺序

1. **设计与资源预算**
   - 制作下载页布局稿，覆盖有配对码与等待配对码两种状态。
   - 生成真实二维码，用解码器确认内容精确等于目标 HTTPS URL。
   - 打包候选场景，比较压缩 GSPB 与基线的增量，优先复用既有字号与字符集。
     如体积不足，调整图片表示、字号子集和重复资源后再继续，不扩大固定 Recovery 槽。

2. **修改 Recovery 页面**
   - 在 Utilities 子模块内建立实现分支，修改 `ui/main.json` 的两个域名与下载页布局。
   - 增加二维码资源和生成脚本；保持 URL 文本与二维码内容一致。
   - 保留现有绑定、返回和 Cancel 回调；仅在长码完整显示需要时调整 `vibe_ui.c`。
   - 注意场景使用数字 `parent` 索引：插入对象后核对后续页面的父子关系，
     重新生成 bundle/header，避免 NAND、更新和结果页错挂层级。

3. **原生模拟器验证**
   - 按工作区 `gsp-sim` 流程运行 Recovery 场景与现有 `pc/` backend，使用 ESP-GSP 1.4.0。
   - 使用 `python3 tools/gsp-sim/run.py
     submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery/ui/main.json
     --headless` 启动同源模拟器；交互观察时使用 `--interactive`。
   - 从实际渲染截图解码二维码，验证资源打包和屏幕呈现后的内容，不能只解码源图片。
   - 复用 `test_recovery_ui_host.py` 的真实输入流程；必要时扩展
     `pc/platform_pc.c` 的长码、等待、失败与续期快照。
   - 验证进入下载页、返回再进入复用码、Cancel 后重新获取、过期换码、失败退出重试；
     补查 NAND 与更新/结果页以发现数字父索引变化造成的回归。

4. **固件构建与设备验收**
   - 先核验 Recovery 声明的 ESP-IDF `>=6.2`、Python 环境及 ESP32-S31 支持，
     再按低噪声构建流程生成候选 Recovery，保留完整日志与大小报告。
   - 必须通过现有 `recovery-slot-check`，报告最终 `factory.bin` 大小和剩余空间；
     同时观察加载二维码后的内存与页面切换表现。
   - 使用工作区 `mosaico.py` 查询实时设备与 Gateway 归属；候选自更新包的
     `target_layout_sha256` 必须与设备当前布局相符。
   - 对支持自更新的设备，通过 `python mosaico.py iris system-update --bundle ...`
     安装只含 Recovery 的候选包；空白或未验证设备按现有 `recover` 流程处理。
     完整操作说明以 [Recovery 文档](../submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery/README.md)
     为准。自更新是单副本原地写入，设备验证时须遵循其 ROM 兜底流程。
   - 真机扫码打开目标网站，验证配对码清晰、完整且可用；操作返回、Cancel 与续期，
     保存截图和运行日志。
   - 以同一 Device ID、新 Boot ID 验证 normal → Recovery → normal，核对目标 ELF
     与健康状态；CLI 与 Gateway Web 工作台的设备身份和操作记录须一致。

5. **更新交付物**
   - 候选通过设备验收后，使用现有制包工具原子更新完整预置包及 manifest，
     校验布局、大小、哈希与来源记录，核对 bootloader、分区表和初始 OTA 数据。
   - 更新 Recovery README 中的网站入口、扫码说明与预置包验证记录。
   - 提交 Utilities 的固件/资源/测试/预置包变更，再更新工作区的子模块指针；
     工作区文档链接到组件说明，避免复制组件协议。

## 预计变更位置

以下路径均相对 `submodule/esp-mosaico-utils/esp-mosaico-recovery/`：

| 文件或目录 | 变更用途 |
| --- | --- |
| `firmware/recovery/ui/main.json` | 两处网址、二维码与配对区布局 |
| `firmware/recovery/ui/assets/`（新增） | 固定网站二维码资源 |
| `firmware/recovery/tools/` | 可复现的二维码生成工具及生成依赖说明 |
| `firmware/recovery/main/vibe_ui.c`（按需） | 长码显示选择，保持实际码值与会话行为 |
| `firmware/recovery/pc/platform_pc.c`（按需） | 可重复的边界状态模拟 |
| `tests/test_recovery_ui_host.py` | 原生渲染二维码解码、边界状态与现有导航回归 |
| `tests/test_recovery_ui_font_contract.py`（按需） | 动态配对码备用字号的字形覆盖 |
| `firmware/recovery/README.md` | 正确网站与扫码操作说明 |
| `firmware/recovery/prebuilt/recovery/` | 验收通过后的完整包、manifest 和验证记录 |

优先复用现有 `test_bridge_host.py`、`test_bridge_code.py`、
`test_pack_vibe_bundle.py`、`test_product_contract.py` 等检查。
新增检查应覆盖二维码实际可解码、码值完整呈现与导航行为，不为坐标或颜色本身新增测试。

## 完成标准与当前限制

- 首页与下载页都显示正确域名，手机从真机页面扫码后进入正确 HTTPS 网站。
- 网址完整但视觉次要，配对码醒目且无截断；等待、过期、失败状态都有可读反馈。
- 返回复用码、Cancel 结束会话以及既有更新流程通过相关回归检查。
- 原生截图、固件容量、设备健康与转换证据齐全，预置包和工作区子模块指针对应已验收版本。

本计划以 Issue 指定 URL 为目标；本次网页读取工具未能访问该站点，
尚未验证网站加载及移动端配对体验。新二维码体积与真机扫码效果同样待实施阶段实测。
若硬件不可用，可交付源码和模拟器证据，但设备验收及预置包替换保持待完成。
