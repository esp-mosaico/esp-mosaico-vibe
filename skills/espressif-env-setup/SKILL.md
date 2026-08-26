---
name: espressif-env-setup
description: >
  Use only when the user explicitly wants to install a fresh ESP-IDF, ESP-AT,
  or ESP-ADF development environment from scratch on Windows, Linux, or macOS,
  through the first successful verification build.
---

# Espressif Environment Setup（IDF / AT / ADF 入口）

从零搭建 **ESP-IDF / ESP-AT / ESP-ADF** 开发环境，覆盖 Windows / Linux / macOS，并完成首次验证编译；国内镜像统一乐鑫 `git.espressif.com.cn`。

## Skill Role

**Role:** L2 setup workflow

| 目标 | 立即阅读 |
|------|----------|
| ESP-IDF / hello_world | [esp-idf.md](references/esp-idf.md) |
| ESP-AT / `build.py` / AT 固件 | [esp-at.md](references/esp-at.md)（内置 IDF 后须 `common.md` 第三步） |
| ESP-ADF / 音频示例 | [esp-adf.md](references/esp-adf.md)（仅 `master` / `release/v2.x`；先做 ADF↔IDF 兼容 Gate） |

**核心原则：** 占位符先替换 → 同一 shell session 连续执行 → 用 bin 证明成功（[placeholders.md § Iron Law](references/placeholders.md#the-iron-law)）。

**版本默认（只读对应节，勿通读 defaults 全文）：**
- 独立 IDF → **无写死单一默认 tag**；未指定时 Phase 0 **同一回合**须现查并贴出**全部在维护线 + 各线最新稳定 tag**（禁止只说「我将列出」；须含如仍在维护的 `v6.0`；见 [defaults.md](references/defaults.md#独立-esp-idf-默认版本必须现查并展示在维护线)）；附[支持期限](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/versions.html#id6)与 [COMPATIBILITY_CN.md](https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY_CN.md)；**ESP32-S31** 见同节（`master` / 预期 `v6.1-beta1` → `v6.1.1`，列表仍列全并标注兼容性）
- AT → [defaults.md § ESP-AT](references/defaults.md#esp-at支持芯片与默认版本) / [技术选型](https://docs.espressif.com/projects/esp-at/zh_CN/latest/esp32/Get_Started/Technology_selection.html#at)
- ADF → `esp-adf.md` 本文件兼容表（IDF 范围）；分支/父目录 → [defaults.md § 当前默认值](references/defaults.md#当前默认值其它项) ESP-ADF 行

## 能力边界（进入前强制检查）

仅当用户明确要求从零搭建 ESP-IDF、ESP-AT 或 ESP-ADF，并希望完成首次验证编译时，才进入本 Skill。选择具体 SDK 后，只读取对应主流程及其按需引用。

不得用本 Skill 响应以下请求：

- 激活、修复、迁移或切换已有 SDK 环境
- 用户直接提交的 clone、submodule、安装、工具链、export、编译或路径故障
- 已有项目的业务编译、运行时、烧录或性能问题
- ESP-AT 固件运行异常、自定义功能开发，或 ESP-ADF 音频应用调试

上述请求必须退出本 Skill 并重新执行 triage，交由匹配的诊断 Skill 或官方文档检索处理。

只有在本 Skill 已经启动全新安装、且由本 Skill 指示的命令发生可识别故障时，才可按 `troubleshooting*.md` 做有限恢复。恢复仅服务于当前安装流程；若无法恢复，记录环境、命令和原始错误后退出并重新 triage，不得扩展为通用排障。

## 共用前置

1. Windows：`esp-idf-windows.md` 终端选定
2. Python / Git：[prerequisites.md](references/prerequisites.md)
3. pip / 网络镜像 / submodule：[common.md § 第一步～第三步](references/common.md#第一步--pip-镜像配置)
4. 父目录 / 路径 / Iron Law：[placeholders.md](references/placeholders.md)（按小节读）
5. 默认版本：按上方「版本默认」链到 **defaults 对应节**（勿整文件读入）

## 网络与镜像

克隆前按 [common.md](references/common.md) § 第二步：以 **`git ls-remote` 速度对比为主**（ping 仅作参考）；用户说「强制国内」则直接乐鑫镜像。命令见 [mirrors.md](references/mirrors.md)。

## 通用 Red Flags — STOP

（仅在已通过能力边界并进入全新安装流程后适用；各 SDK 文件另有**本 SDK 专用**红旗，此处不重复展开。）

- 🚩 未配镜像就 clone → 先 `common.md` / `mirrors.md`（清**全部**极狐条目）
- 🚩 只 unset 极狐总开关 → 按仓 `insteadOf` 仍会劫持 submodule
- 🚩 submodule 失败先 `--depth=1` / 误开 Q1 离线包 / status 不干净却继续装 → **troubleshooting-git.md Q2**（路径以 `git status` 为准）
- 🚩 子模块拉取仍用默认短 Shell 等待 → 须按 [common.md § Agent Shell 长任务等待](references/common.md#agent-shell-long-timeout) 把**当前工具**的等待/超时调到 ≥ 2h（Cursor 示例：`block_until_ms` ≥ 7200000）；`terminated ... 300000` 且还在 Cloning = 误杀，不是 Git 已失败
- 🚩 独立 IDF clone 带 `--recursive` → 禁止；父仓与子模块分步，见 `common.md` § 第二步～第三步
- 🚩 占位符原样进命令 / 父目录当 clone 目标 → [placeholders.md § 父目录](references/placeholders.md#父目录-vs-仓库根目录)
- 🚩 未问父目录 / 未等确认就用默认路径 → 须主动询问，可推荐默认，**确认后再继续**
- 🚩 未问工具链路径 / 未告知默认 `.espressif` 就 install → Phase 0 须问；不指定则告知默认并确认后再装（[placeholders.md § IDF_TOOLS_PATH](references/placeholders.md#your_tools_path--idf_tools_path)）
- 🚩 独立 IDF 未指定版本却只报一个缓存 tag / 不列在维护线与各线最新稳定 tag → 见 `defaults.md`；S31 勿默认 `v6.0.x`；**禁止**把 S31 改成 S3
- 🚩 AT Phase 0 未贴支持芯片列表 / 默认 master → 见 `esp-at.md`：须编号列出芯片；版本用推荐固件 tag
- 🚩 ADF 未做版本兼容 Gate / master 未按 README 选板 → 见 `esp-adf.md` Red Flags
- 🚩 AT/ADF 跳过 submodule Gate → [common.md § 第三步](references/common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用)
- 🚩 未经验证产物就说「装好了」 → [placeholders.md § Iron Law](references/placeholders.md#the-iron-law)
- 🚩 用了乐鑫镜像却在 Done 后不告知全局 `insteadOf` 影响 / 不给清除命令 → [placeholders.md § 安装完成收尾](references/placeholders.md#post-install-espressif-mirror-notice)
- 🚩 Windows 未探测终端就猜 PowerShell → `esp-idf-windows.md` 第一步

## Related

- [defaults.md](references/defaults.md) · [placeholders.md](references/placeholders.md) · [common.md](references/common.md) · [prerequisites.md](references/prerequisites.md)
- [mirrors.md](references/mirrors.md) · [troubleshooting.md](references/troubleshooting.md)（仅供当前全新安装流程有限恢复）
- 平台：[esp-idf-windows.md](references/esp-idf-windows.md) / [esp-idf-linux.md](references/esp-idf-linux.md) / [esp-idf-macos.md](references/esp-idf-macos.md)
- SDK：[esp-idf.md](references/esp-idf.md) · [esp-at.md](references/esp-at.md) · [esp-adf.md](references/esp-adf.md)
- [vscode-extension.md](references/vscode-extension.md)（用户需要 IDE 时）
- 清理极狐配置：使用 [mirrors.md § 清除全部极狐](references/mirrors.md#clear-jihulab-insteadof) 的内联命令；MCP 不发布辅助脚本。
