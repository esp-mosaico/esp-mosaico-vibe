# ESP-IDF Environment Setup

本文件仅供主 Skill 已确认的 **ESP-IDF** 全新安装流程使用（Windows / Linux / macOS），直到首次验证编译成功。

官方文档：[ESP-IDF 快速入门](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)

**共用规则（勿在本文件重复；只读对应章节）：**
- Iron Law / 父目录 / 路径 / `IDF_TOOLS_PATH` → [placeholders.md § Iron Law](placeholders.md#the-iron-law) · [§ 父目录](placeholders.md#父目录-vs-仓库根目录) · [§ IDF_TOOLS_PATH](placeholders.md#your_tools_path--idf_tools_path)
- 独立 IDF 现查默认版本 → [defaults.md § 独立 ESP-IDF 默认版本](defaults.md#独立-esp-idf-默认版本必须现查并展示在维护线)（**勿**读 AT/ADF 节）
- 芯片 / 父目录表项 → [defaults.md § 当前默认值](defaults.md#当前默认值其它项) 中 ESP-IDF 行
- pip / 网络镜像 / submodule → [common.md § 第一步](common.md#第一步--pip-镜像配置)～[第三步](common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用)；前提条件 → [prerequisites.md](prerequisites.md)

---

## Phase 0 — 确认四项

执行任何命令前，**主动询问**下列项，并用实际值替换所有 `YOUR_*`（[placeholders.md § 占位符纪律](placeholders.md#占位符纪律)）。

1. **`YOUR_IDF_VERSION`** — 分支或 tag。
   - 用户已指定 → 记下；若芯片特殊（如 **ESP32-S31**）仍对照 [defaults.md § ESP32-S31](defaults.md#独立-esp-idf-默认版本必须现查并展示在维护线) / [COMPATIBILITY_CN.md](https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY_CN.md)。
   - **未指定 → 本回合必须先现查再提问**：按 [defaults.md § 独立 ESP-IDF](defaults.md#独立-esp-idf-默认版本必须现查并展示在维护线) 跑乐鑫 `ls-remote`，把**完整**「在维护线 + 各线最新稳定 tag + `release/v*` + `master`」写进回复（含服务期/维护期标注；**须含仍在维护的 v6.0 等线**），再请客户选。
     - **禁止**只写「我将列出 / 未指定时将按官网列出」却不贴具体列表。
     - 芯片已是 **ESP32-S31** 时：列表仍列全，并逐条标注是否支持 S31；推荐选项标明 `master`（见 defaults S31 专节）。
     - **不必**等 `common.md` 第二步；禁止用缓存数字；禁止未确认就 clone。
2. **`YOUR_TARGET_CHIP`** — 如 `esp32`、`esp32s3`、`esp32s31`。不指定则用 [defaults.md § 当前默认值](defaults.md#当前默认值其它项) 中 `esp32`。
   - 若为 **ESP32-S31**：按 defaults 专节；**禁止**当成 ESP32-S3 笔误去「纠正」；勿默认塞稳定 `v6.0.x`。
3. **安装父目录** — **必须主动询问**（[placeholders.md § 父目录](placeholders.md#父目录-vs-仓库根目录)）。
   - 未提供 → 推荐共用默认父目录（Windows `C:\esp`；Linux/macOS `~/esp`），**说明该默认同样用于 ESP-AT / ESP-ADF**；版本未定时可先问父目录，完整 `YOUR_INSTALL_PATH` 在版本确认后回显。禁止静默采用默认。
   - 已提供 → 拼路径并回显，确认后再克隆。
4. **工具链路径（`YOUR_TOOLS_PATH` / 默认 `.espressif`）** — **必须主动询问**（[placeholders.md § IDF_TOOLS_PATH](placeholders.md#your_tools_path--idf_tools_path)）。
   - 未提供 → 告知默认并等确认；已提供 → 回显；自定义则 install/export 前设 `IDF_TOOLS_PATH`。

**Phase 0 回复顺序（版本未指定时强制）：**
① Shell 现查（乐鑫 tags + 对照支持期限）→ ② **先贴完整版本列表**（defaults 展示模板）→ ③ 再问芯片（若未知）/ 父目录 / 工具链。
若用户首句已给芯片（如「为 ESP32-S31 搭建」）：跳过芯片问句，在版本列表上加 S31 标注后，直接问父目录与工具链。

向客户确认模板（版本未指定；**列表必须已填入本次现查结果，禁止保留「我将列出」空话**）：

```
开始搭建 ESP-IDF 前请确认：

1) ESP-IDF 版本（以下为本次现查，请直接选一项；勿让我「稍后再列」）：

支持期限：https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/versions.html#id6
芯片兼容：https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY_CN.md

【稳定 tag】
1. <现查 tag>（release/vX.Y，服务期/维护期）— （若芯片为 S31：不支持 S31 / 支持 S31）
2. …（须含当前全部在维护线，例如含 v6.0 与 v5.5 等）

【发布分支】
- release/vX.Y
- …（与上表一一对应）

【开发中】
- master（…；若 S31：已支持 S31，当前推荐）

（S31 时加一句：正式稳定支持预期 v6.1.1；此前请用 master / 预发布。）

2) 目标芯片？（已告知则写「已确认为 …」）
3) 安装父目录？（可推荐 C:\esp 或 ~/esp）
4) 工具链路径？（默认 %USERPROFILE%\.espressif 或 ~/.espressif）
```

| 占位符 | 含义 |
|--------|------|
| `YOUR_IDF_VERSION` | 分支或 tag（未指定则展示在维护线 + 各线最新稳定 tag，[defaults.md § 独立 ESP-IDF](defaults.md#独立-esp-idf-默认版本必须现查并展示在维护线)） |
| `YOUR_INSTALL_PATH` | 仓库根目录 = 父目录 + `esp-idf-<version>` |
| `YOUR_TARGET_CHIP` | 目标芯片（默认见 [defaults.md § 当前默认值](defaults.md#当前默认值其它项)） |
| `YOUR_TOOLS_PATH` | 工具链路径；**Phase 0 须问**；确认用默认则不设 `IDF_TOOLS_PATH`，自定义则每次 install/export 前设置（[placeholders.md § IDF_TOOLS_PATH](placeholders.md#your_tools_path--idf_tools_path)） |
| `YOUR_PROJECT_PATH` | 默认 `YOUR_INSTALL_PATH/examples/get-started/hello_world` |

> **Windows：** Phase 0 后立刻按 [esp-idf-windows.md](esp-idf-windows.md) 第一步选定终端，全程固定。

---

## Phase 1 — 依赖 + 镜像 +（按需现查版本）+ 克隆

1. [prerequisites.md](prerequisites.md)。Windows 先完成 `esp-idf-windows.md` 终端选定。
2. 若 Phase 0 **未指定**版本且尚未展示选项 → 按 [defaults.md § 独立 ESP-IDF](defaults.md#独立-esp-idf-默认版本必须现查并展示在维护线) 现查并**列出在维护线 + 稳定 tag** → 客户选定后定稿 `YOUR_IDF_VERSION` 与 `YOUR_INSTALL_PATH`。
3. [common.md § 第一步](common.md#第一步--pip-镜像配置) → [§ 第二步](common.md#第二步--网络检查与-git-镜像配置)（直连或乐鑫 `insteadOf`；与「现查版本」分开：现查已优先打乐鑫）。
4. **克隆独立 IDF（不要 `--recursive`）** → 立刻 [common.md § 第三步](common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用) 拉子模块（`YOUR_IDF_PATH` = `YOUR_INSTALL_PATH`）。
   - 父仓 clone 失败 → [troubleshooting-git.md § Q1](troubleshooting-git.md)
   - `submodule update` 失败/中断（wifi lib、openthread 等）→ [§ Q2](troubleshooting-git.md)（**不要**走 Q1）

**Phase 1 Gate：** `git status` 必须为 `nothing to commit, working tree clean`（detached HEAD 可接受）。有 `modified: components/xxx (new commits, modified content)` 或 update 中断 → [troubleshooting-git.md § Q2](troubleshooting-git.md)（按 status 路径修）；干净前不得进 Phase 2。

---

## Phase 2 — 工具链 + 激活 + 编译 + 验证（10～30 分钟）

按平台文件：install → export → 编译 hello_world → 直接枚举并验证非空 bin。**同一 shell session 连续执行。** 自定义 `YOUR_TOOLS_PATH` 时每次 export 前先设 `IDF_TOOLS_PATH`（[placeholders.md § IDF_TOOLS_PATH](placeholders.md#your_tools_path--idf_tools_path)）。
新芯片 `set-target` 若提示 `--preview` → [troubleshooting-build.md § Q7](troubleshooting-build.md#q7-idfpy-set-target-失败unknown-target--需-preview)。

**Phase 2 Gate：** `build/` 下所需 bin 均存在且大小非零 → 才能报告 Done（[placeholders.md § Iron Law](placeholders.md#the-iron-law)）。若本次配置了乐鑫官方镜像 → Done 后须加 [安装完成收尾提示](placeholders.md#post-install-espressif-mirror-notice)。否则按题号排查，**不要叠加新修复**：
- 工具链 / export → [troubleshooting-toolchain.md](troubleshooting-toolchain.md) § Q4 / Q6
- `set-target` 失败（unknown / 需 `--preview`）→ [troubleshooting-build.md](troubleshooting-build.md) § Q7
- 无 bin / 编译 → [troubleshooting-build.md](troubleshooting-build.md) § Q5

**可选收尾：** VS Code 扩展 → [vscode-extension.md](vscode-extension.md)。

---

## Red Flags — STOP（仅本 SDK；通用见 `SKILL.md`）

- 🚩 独立 IDF `git clone` 带 `--recursive` → **禁止**；先只 clone 父仓，再 `common.md` 第三步 `submodule update`；子模块失败走 **Q2**
- 🚩 跳过克隆/install 直接编译 → 先检 `git status` 与 `idf.py --version`
- 🚩 Phase 0 只说「未指定时我将列出版本」却不贴本次现查的完整列表 → **禁止**；须先 `ls-remote` 再提问
- 🚩 Phase 0 把 `YOUR_IDF_VERSION` 说成单一固定默认、或不列**全部**在维护线/各线最新稳定 tag（如漏掉仍在服务期的 `v6.0`）→ **禁止**；见 [defaults.md](defaults.md#独立-esp-idf-默认版本必须现查并展示在维护线)
- 🚩 稳定选项列表中夹带 `-dev`/`-beta`/`-rc`（S31 预告说明除外）→ **禁止**
- 🚩 把用户说的 **ESP32-S31** 当成 **ESP32-S3** 笔误并改问/改选 → **禁止**；S31 为独立芯片，见 [defaults.md](defaults.md#独立-esp-idf-默认版本必须现查并展示在维护线)
- 🚩 因 ESP32-S31 就从列表删掉 `v6.0` 等在维护线 → **禁止**；应列全并标注「是否支持 S31」；装机仍须 `master`（或待 `v6.1.1`），勿默装 `v6.0.x`
- 🚩 客户要 ESP32-S31 却默认装 `v6.0.x`/`v5.x` 稳定版且不说明须 `master` / 等待 `v6.1.1` → **禁止**
- 🚩 未指定版本却对 GitHub 盲查、或等「速度对比完」才查版本 → **优先**乐鑫 `git ls-remote --tags`
- 🚩 自定义路径 install 设过 `IDF_TOOLS_PATH`，export 不再设 → [placeholders.md § IDF_TOOLS_PATH](placeholders.md#your_tools_path--idf_tools_path)

---

## Related

**按操作系统选择安装文件：**

| 操作系统 | 安装文件 |
|----------|----------|
| Windows | [esp-idf-windows.md](esp-idf-windows.md) |
| Linux | [esp-idf-linux.md](esp-idf-linux.md) |
| macOS | [esp-idf-macos.md](esp-idf-macos.md) |

**按需阅读（按章节）：** [common.md](common.md) · [prerequisites.md](prerequisites.md) · [mirrors.md](mirrors.md) · [troubleshooting.md](troubleshooting.md)（索引） · [vscode-extension.md](vscode-extension.md)
