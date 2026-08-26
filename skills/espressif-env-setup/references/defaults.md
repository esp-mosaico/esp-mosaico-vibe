# 默认版本（单一维护点）

> **范围：** 下列 IDF 默认**仅适用于独立 ESP-IDF 主流程**（`esp-idf.md`）。AT / ADF 各自按技术选型或兼容表选版本（见下文 / `esp-adf.md`）。

---

## 解析规则（所有 SDK）

```
用户是否指定了版本？
├─ 是 → 使用用户指定（仍须做该 SDK 的合法性 / 兼容校验）
└─ 否 → 主动询问，并告知本文件中的默认；客户接受后再用
```

---

## 独立 ESP-IDF 默认版本（必须现查并展示在维护线）

客户**未指定** `YOUR_IDF_VERSION` 时：Agent **必须**现查并展示「当前仍在维护的发布线 + 各线最新稳定正式 tag」，请客户选择后再装。**禁止**只报一个死记版本、禁止把 `-dev` / `-beta` / `-rc` 当作推荐稳定选项（S31 等预告信息除外，见文末专节）。

### 权威参考（向客户展示时一并给出）

| 主题 | 链接 |
|------|------|
| 支持期限（服务期 / 维护期 / EOL） | [ESP-IDF 版本简介 — 支持期限](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/versions.html#id6) |
| 支持周期示意（SVG） | https://dl.espressif.com/dl/esp-idf/support-periods.svg |
| IDF ↔ 芯片版本兼容性 | [COMPATIBILITY_CN.md（master）](https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY_CN.md) |
| Tag / 分支现查 | 乐鑫镜像 https://git.espressif.com.cn/espressif/esp-idf |

> **以现查 / 官网当前信息为准**，本文件不维护版本号缓存表（避免 Agent 照抄过期数字）。支持期限以官网说明与 [support-periods.svg](https://dl.espressif.com/dl/esp-idf/support-periods.svg) 为准（每条主要/次要线自**首个稳定版**起约 30 个月：服务期 12 个月 + 维护期 18 个月；新工程优先**服务期**版本）。

### 时机

```
用户是否已指定 YOUR_IDF_VERSION？
├─ 是 → 跳过本展示；仍建议按需对照 COMPATIBILITY_CN（尤其新芯片 / 芯片 revision）
└─ 否 → Phase 0 同一回合内：先 ls-remote 现查 → 立刻贴完整列表 → 再问其它项
         → 禁止只回复「我将列出请选择」
         → 客户选定后再走 common.md 第二步镜像与 clone
```

可与询问父目录/工具链同条消息发出，但**版本列表必须已出现在该条消息中**；**不必**等速度对比完成。

### 现查步骤（强制）

1. **识别仍在维护的发布线（须列全，禁止漏线）**
   - 打开 / 对照 [支持期限](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/versions.html#id6) 与 [support-periods.svg](https://dl.espressif.com/dl/esp-idf/support-periods.svg)，列出**全部**尚未 EOL 的 `vX.Y` 线（对应分支 `release/vX.Y`）。
   - 典型仍会同时出现多条服务期/维护期线（例如同时有 `v6.0` 与 `v5.5` 等）——**每一条都要进列表**。
   - 用 `git ls-remote --heads https://git.espressif.com.cn/espressif/esp-idf.git "refs/heads/release/*"` 核对远端仍存在的 `release/v*`；**禁止**因客户芯片特殊（如 S31）就删掉 `release/v6.0` 等仍在维护的线。
   - **不要**把已 EOL 的线当作推荐选项（可注明已结束支持）。

2. **查各线最新稳定正式 tag**（优先乐鑫）：
```bash
git ls-remote --tags https://git.espressif.com.cn/espressif/esp-idf.git
```
   乐鑫失败再：
```bash
git ls-remote --tags https://github.com/espressif/esp-idf.git
```

3. **Tag 过滤规则**（稳定选项列表）：
   - 只保留 `vX.Y` / `vX.Y.Z`（去掉 `^{}` 剥皮行）。
   - **排除**名称含（大小写不敏感）：`-dev`、`-beta`、`-rc` 及类似预发布后缀。
   - 对**每一个**仍在维护的 `vX.Y` 线，取该线下最大的稳定 bugfix tag（版本号以本次 `ls-remote` 为准，禁止手填记忆数字）。
   - 另可列出对应分支名供选择：`release/vX.Y`（跟分支拿持续 bugfix）以及可选 `master`（最新特性，有风险；量产不推荐）。

4. **推荐默认（客户仍说「你定」时）**
   - **普通芯片：** 优先推荐**处于服务期**的线中、**本次现查**到的最新稳定正式 tag（常含 `v6.0.x` 与 `v5.5.x` 中较新的服务期线）。
   - **ESP32-S31：** 见下节——推荐 `master`（或已含 S31 的预发布），**不要**因「你定」就装 `v6.0.x`/`v5.x` 稳定版；但展示列表时仍须含这些线并标注「不支持 S31」。
   - 须向客户说明：新工程建议服务期版本；维护期版本仅严重/安全修复。

### 向客户展示模板（未指定版本时按此结构输出；**全部版本号必须来自本次现查**，禁止填写下文占位以外的死记数字）

```
未指定 ESP-IDF 版本。当前仍在维护的发布线与各线最新稳定正式 tag 如下
（已排除 -dev / -beta / -rc；以乐鑫镜像现查 + 官网支持期限为准；须列全在维护线，勿漏 v6.0 等）：

支持期限说明：https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/versions.html#id6
芯片兼容性：https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY_CN.md

【稳定 tag — 建议量产/日常开发选用】
1. <本次现查的 tag>     （release/vX.Y 线，服务期/维护期：…）
2. …
（按在维护线从新到旧逐行列出，例如同时有 v6.0 与 v5.5 时两者都要出现）

【发布分支 — 需持续跟该线 bugfix 时可选】
- release/vX.Y
- …
（与上表在维护线一一对应，禁止少列）

【开发中】
- master（含最新特性，未全部完成人工测试；量产不推荐）

建议：新工程优先选【服务期】线上的最新稳定 tag。
若你指定芯片，请说明型号，我可对照 COMPATIBILITY_CN 帮你收窄可选版本。

请回复选用的 tag 或分支（例如：vX.Y.Z / release/vX.Y / master）。
```

展示时可用简短表格标明各线「服务期 / 维护期」（日期来自官网 SVG/发布说明，禁止臆造）。

### ESP32-S31 支持（客户提到 S31 时必读）

- **ESP32-S31 是独立芯片型号，不是 ESP32-S3 的笔误。**
  - 用户写「ESP32-S31」/「esp32s31」→ **原样接受**为 `YOUR_TARGET_CHIP=esp32s31`。
  - **禁止**改写成 ESP32-S3、禁止反问「是不是 S3」、禁止把选项默认成「是，ESP32-S3」。
  - 若用户其实要 S3，须由其**主动**改口；Agent 不得诱导改选。
- **ESP-IDF `master` 分支已支持 ESP32-S31。**
- 预期节奏（以乐鑫公布为准，可能调整）：
  - 预发布版本：`v6.1-beta1`
  - 预计正式发布版本：`v6.1.1`
- 在 **`v6.1.1`（或官方宣布正式支持 S31 的稳定 tag）发布前**：S31 开发须用 **`master`**（或已含 S31 支持的预发布 tag）；预发布/master **不在**常规支持期限政策覆盖内，量产风险更高。
- **`set-target`：** S31 等尚未正式量产的芯片，直接 `idf.py set-target esp32s31` 可能失败；若报错提示 `--preview`，按提示使用 `idf.py --preview set-target esp32s31`（见 [troubleshooting-build.md § Q7](troubleshooting-build.md#q7-idfpy-set-target-失败unknown-target--需-preview)）。
- **展示版本列表时仍须列全当前在维护线**（含 **`release/v6.0` / 其最新稳定 tag `v6.0.x`**，以及 v5.5 / v5.4 …），但须**逐条标注是否支持 S31**：
  - `master` → 支持 S31（当前可用选项）
  - `v6.0.x` / `release/v6.0` 及更旧的稳定线 → **通常不支持 S31**（勿删除该行，只标注不兼容）
  - 待 `v6.1.1`（或官方正式 tag）发布后，再将其作为 S31 的稳定推荐。
- **禁止**：为「突出 S31」而从列表中抹掉仍在维护的 `v6.0` 线；**禁止**在未说明的情况下把 S31 工程装到 `v6.0.x`/`v5.x` 稳定版。
- 其它芯片的 IDF 最低/推荐版本 → 查 [COMPATIBILITY_CN.md](https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY_CN.md)（以 master 文件为准）。

### 与 clone 的关系

现查只为定版本；真正 `git clone` 仍按 [common.md § 第二步](common.md#第二步--网络检查与-git-镜像配置) 选直连或乐鑫 `insteadOf`。

---

## ESP-AT：支持芯片与默认版本

权威来源（开装前宜核对页面是否有更新）：
[技术选型 — AT 软件方案选型](https://docs.espressif.com/projects/esp-at/zh_CN/latest/esp32/Get_Started/Technology_selection.html#at)

### 芯片（未提供时只能从此表选）

客户**未提供芯片**时：把下表芯片列表**按「向客户展示模板」编号原样**展示给客户选择；**禁止**改成「例如 ESP32 / ESP32C2 等」；**禁止**猜测、禁止推荐表外芯片、禁止默认第一项。

客户**已提供芯片**时：须落在下表内；若不在表内 → STOP，说明本 skill / 官方 AT 方案未列该芯片，请客户改选或联系乐鑫商务/技术支持。

### 版本（未提供时按「推荐的 AT 固件」）

客户**未提供** `YOUR_AT_VERSION` 时：在客户已选定芯片后，用该行 **「推荐的 AT 固件」** 列的 tag 作为推荐默认，明确告知客户；客户接受后再用。
**禁止**默认 `master`（除非客户明确要求新功能 / master）；版本取该芯片「推荐的 AT 固件」。

客户**已提供**版本 → 用客户指定（仍建议告知该芯片官方推荐 tag，便于对照）。

### 缓存表（以官网为准；有出入时以官网为准并更新本表）

核对依据页同上；缓存日：2026-07-22。

| 芯片 | Platform name（映射） | 推荐的 AT 固件（→ `YOUR_AT_VERSION`） | 说明 |
|------|----------------------|--------------------------------------|------|
| ESP32-C6 | `PLATFORM_ESP32C6` | `v4.1.1.0` | |
| ESP32-C5 | `PLATFORM_ESP32C5` | `v5.0.1.0` | |
| ESP32-C61 | `PLATFORM_ESP32C61` | `v5.0.1.0` | |
| ESP32-C3 | `PLATFORM_ESP32C3` | `v4.1.1.0` | |
| ESP32-C2 | `PLATFORM_ESP32C2` | `v4.1.1.0` | |
| ESP32 | `PLATFORM_ESP32` | `v4.1.1.0` | |
| ESP32-S2 | `PLATFORM_ESP32S2` | `v4.1.1.0` | 官网说明：推荐性价比更高的 ESP32-C 系列 |

> 官网另述：稳定性优先可用该芯片最新已发布版本对应分支；要更多新功能可用 `master`——**仅当客户明确选择时**再用 `master`，不要当作未指定时的默认。

### 向客户展示模板（未给芯片时）

```
本流程仅支持官方 AT 技术选型表中的芯片（https://docs.espressif.com/projects/esp-at/zh_CN/latest/esp32/Get_Started/Technology_selection.html#at）：
1. ESP32-C6（推荐 AT 固件 v4.1.1.0）
2. ESP32-C5（推荐 AT 固件 v5.0.1.0）
3. ESP32-C61（推荐 AT 固件 v5.0.1.0）
4. ESP32-C3（推荐 AT 固件 v4.1.1.0）
5. ESP32-C2（推荐 AT 固件 v4.1.1.0）
6. ESP32（推荐 AT 固件 v4.1.1.0）
7. ESP32-S2（推荐 AT 固件 v4.1.1.0）
请选择序号或芯片名。若不另行指定版本，将按该芯片「推荐的 AT 固件」安装。
```

---

## 当前默认值（其它项）

| SDK | 占位符 | 不指定时的默认 | 说明 |
|-----|--------|----------------|------|
| **ESP-IDF（独立）** | `YOUR_IDF_VERSION` | **在维护线各最新稳定 tag（现查）**；「你定」→ 服务期最新稳定 tag | 仅 `esp-idf.md`；见上文专节 |
| **ESP-IDF（独立）** | `YOUR_TARGET_CHIP` | `esp32` | 仅独立 IDF |
| **共用** | 默认父目录 | Windows `C:\esp`；Linux/macOS `~/esp` | **ESP-IDF / ESP-AT / ESP-ADF 相同**；主动询问时须说明这一点 |
| **共用** | 默认工具链目录 | Windows `%USERPROFILE%\.espressif`；Linux/macOS `~/.espressif` | Phase 0 **须询问**；不指定则告知该默认并等确认后再 install；见 [placeholders.md § IDF_TOOLS_PATH](placeholders.md#your_tools_path--idf_tools_path) |
| **ESP-AT** | 芯片 / `YOUR_PLATFORM` | **无默认**；仅上表七款 | 见「ESP-AT：支持芯片与默认版本」 |
| **ESP-AT** | `YOUR_AT_VERSION` | 该芯片行的「推荐的 AT 固件」 | **不要**默认 `master` |
| **ESP-ADF** | `YOUR_ADF_VERSION` | `master` | 仅 `master` / `release/v2.x`；**Phase 0 须两档都展示**，禁止只推 v2.x |
| **ESP-ADF** | 搭配的 IDF | 见 `esp-adf.md` 兼容表 | |

独立 IDF 默认完整路径 = 父目录 + `esp-idf-` + 现查到的 tag（例：`C:\esp\esp-idf-v6.0.2`）。
AT 默认完整路径 = 父目录 + `esp-at-` + `YOUR_AT_VERSION`（例：芯片 ESP32-C2 未指定版本 → `C:\esp\esp-at-v4.1.1.0`）。
ADF 默认完整路径 = 父目录 + `esp-adf-` + 分支名规则（例：`C:\esp\esp-adf-master`）。

---

## 与其它文档的关系

- 父目录 / 路径 / Iron Law → [placeholders.md](placeholders.md)
- ADF↔IDF 兼容 → [esp-adf.md](esp-adf.md)
- AT 支持芯片与推荐固件 → [defaults.md](defaults.md)「ESP-AT」；权威页 [技术选型](https://docs.espressif.com/projects/esp-at/zh_CN/latest/esp32/Get_Started/Technology_selection.html#at)
- AT 模组钉死的 IDF → 仓库内 `module_config/.../IDF_VERSION`（不以本文件 IDF 默认为准）
