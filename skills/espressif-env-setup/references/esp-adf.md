# ESP-ADF 开发环境搭建

> **适用前提：** 主 Skill 已确认用户要从零安装 ESP-ADF。本文件只服务于全新安装和首次验证编译（仅 `master` / `release/v2.x`）；不得据此响应独立排障、已有环境修复或音频应用调试请求。

ESP-ADF（Espressif Audio Development Framework）在 ESP-IDF 之上提供音频组件。编译 ADF 工程前须：**设置 `ADF_PATH` → 配置好 IDF 环境（`export`）→ 再 `idf.py` 编译示例**。

官方文档：
- [Get Started — Installation Step by Step](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#installation-step-by-step)
- [release/v2.x 支持的 IDF 版本](https://github.com/espressif/esp-adf/tree/release/v2.x#idf-version)

**共用规则（勿在本文件重复）：** Iron Law / 父目录 / 路径 / `IDF_TOOLS_PATH` → [placeholders.md](placeholders.md)。ADF 分支默认与父目录 → [defaults.md § 当前默认值](defaults.md#当前默认值其它项) 中 ESP-ADF 行。搭配 IDF 须过**下方**兼容表（勿读 defaults 里的 IDF/AT 节）。

---

## 本 skill 支持的 ADF 版本范围

本流程**仅支持**以下 ADF 分支的搭建：

| 支持 | 不在本 skill 范围 |
|------|-------------------|
| `master`（ADF v3.0 开发线） | 其它未列出的分支 |
| `release/v2.x` | 单独的历史 tag（如 `v2.8`）——若用户坚持，可按 `release/v2.x` 的 IDF 依赖规则处理，并告知以官方该 tag 的 README 为准 |

---

## ADF ↔ ESP-IDF 版本依赖（强制校验）

**在确认 IDF 版本 / 本地 IDF 路径之后、执行任何 clone / install 之前，必须按下来表校验。不兼容则停下来与客户改选，禁止硬装。**

权威依据：
- ADF `master`：[Platform Requirements — ESP-IDF v5.5.2 or later](https://github.com/espressif/esp-adf/blob/master/README.md#platform-requirements)
- ADF `release/v2.x`：[IDF Version 表](https://github.com/espressif/esp-adf/tree/release/v2.x#idf-version)

| ADF 版本 | 支持的 ESP-IDF | 不支持 |
|----------|----------------|--------|
| **`master`** | **v5.5.2 及以上**稳定正式 tag / 对应 release 线（如 `v5.5.2`、`v5.5.x`、`v6.0.x`） | 低于 **v5.5.2**；非正式后缀（`-dev`/`-beta`/`-rc`）除非客户明确要求 |
| **`release/v2.x`** | `release/v5.1`～`release/v5.5`（含这些线上的稳定 tag） | 高于 `release/v5.5`（如 `v6.0.x`、IDF `master`） |

### 校验算法（Phase 0 Gate）

```
已选 YOUR_ADF_VERSION + 拟用的 IDF（客户路径上的版本，或 YOUR_IDF_VERSION）
        ↓
按上表判断是否兼容？
├─ 是 → 回显「ADF=… 搭配 IDF=… 兼容」后继续
└─ 否 → STOP：说明原因，给出本 ADF 下的推荐选项，请客户重选
```

**推荐默认（客户未指定且需新装 IDF 时告知）：**

| ADF | 向客户推荐的 IDF |
|-----|------------------|
| `master` | 从乐鑫镜像现查最新稳定正式 tag（须 ≥ v5.5.2），或客户指定的兼容版本 |
| `release/v2.x` | `v5.4` 或 `v5.5.x`（勿用 v6） |

> 客户已有本地 IDF：用 `idf.py --version` / 目录名 / `git describe` 确认后同样跑上表；不兼容则换路径、改 IDF/ADF，或（仅 v2.x）改用内置 IDF。

---

## master vs release/v2.x（强制区分）

| 类型 | 是否内置 esp-idf | 克隆方式 |
|------|------------------|----------|
| **master** | **无** | `git clone -b master ...`；**必须**外部已装或另行安装的 ESP-IDF（版本见上表） |
| **release/v2.x** | **有**（submodule） | **不要**加 `--recursive`；再用 `git submodule update --init --recursive`；也可用外部 IDF（版本须在上表 `release/v5.1`～`v5.5`） |

---

## Phase 0 — 占位符确认

执行任何命令前，**主动询问**下列项；不得静默套用默认值而不告知。
路径拼装 / Iron Law → [placeholders.md § 父目录](placeholders.md#父目录-vs-仓库根目录) / [Iron Law](placeholders.md#the-iron-law)。
ADF 分支默认与父目录 → [defaults.md § 当前默认值](defaults.md#当前默认值其它项) 中 ESP-ADF 行（**不要**通读 defaults 全文）。
**IDF 须按上方兼容表校验；未通过不得进入 Phase 1。**

1. **`YOUR_ADF_VERSION`** — **必须先把两个选项都展示给客户**，等其选择后再继续。
   - 仅支持：`master` / `release/v2.x`。
   - **禁止**只展示其中一个；**禁止**未展示选项就替客户定 `release/v2.x`（或 `master`）。
   - 客户明确说「不指定 / 你定」→ 告知将用默认 **`master`**，再请其确认。
   - 选定后**立即口头列出该分支支持的 IDF 范围**（见本文件兼容表）。
   - 简述差异：`master` = ADF v3 线、无内置 IDF、需 IDF ≥ v5.5.2；`release/v2.x` = 有内置 IDF（也可用外部）、IDF 仅 v5.1～v5.5。
2. **安装父目录** — **必须主动询问**（[placeholders.md § 父目录](placeholders.md#父目录-vs-仓库根目录)）。
   - 未提供 → 推荐共用默认父目录（Windows `C:\esp`；Linux/macOS `~/esp`），**说明该默认同样用于 ESP-IDF / ESP-AT**，并回显完整 `YOUR_ADF_INSTALL_PATH`；**等客户确认后**再继续。禁止静默采用默认。
   - 已提供 → 拼完整路径并回显，确认后再克隆。
3. **目标芯片 / 开发板**
   - **`release/v2.x`（默认 `play_mp3_control`）：** 询问 `YOUR_TARGET_CHIP`（用于 `idf.py set-target`）。未提供则主动问，不得假设。
   - **`master`（默认 `play_music_control`）：** Phase 0 **不必**先猜芯片；Phase 2 按示例 README：`idf.py bmgr -l` → **把本机列表给客户确认开发板** → 再 `bmgr -b`（板选型会带上目标芯片）。客户若提前指定板名也可记下，但仍以实际 `bmgr -l` 列表为准。
4. **ESP-IDF 来源 + 版本依赖校验（编译 ADF 必需）** — **主动询问：本地是否已有可用的 ESP-IDF？**

```
是否已有 ESP-IDF？
├─ 有 → 请客户提供安装路径，并确认该 IDF 实际版本
│       → 用「ADF ↔ IDF 版本依赖」表校验
│       → 不通过：STOP，请客户换 IDF / 改 ADF /（仅 v2.x）改用内置
│       → 通过：YOUR_IDF_PATH = 该路径（禁止再指向 ADF 内置 esp-idf）
│       → v2.x 时 submodule 跳过内置 esp-idf
└─ 没有
       ├─ 询问要安装的 YOUR_IDF_VERSION（须落在兼容表内）
       │   → 校验通过后：走 esp-idf.md 安装到 YOUR_INSTALL_PATH
       │   → YOUR_IDF_PATH = YOUR_INSTALL_PATH
       └─ 仅 release/v2.x：客户可改选「使用 ADF 内置 esp-idf」
           → YOUR_IDF_PATH = YOUR_ADF_INSTALL_PATH/esp-idf
           → 仍建议告知内置 IDF 大致落在 v5.1～v5.5 支持带内
```

**硬性规则：**
- 凡客户提供了本地路径、或由本 skill 新装了 IDF，`YOUR_IDF_PATH` **必须**指向该路径；Phase 2 **禁止**改用内置。
- **不兼容示例：** ADF=`release/v2.x` + IDF=`v6.0.x` → 拒绝开装。

向客户确认时可按此格式（**第 1 项须原样给出两档，不得省略 master**）：

```
开始搭建 ESP-ADF 前请确认：
1) esp-adf 分支？请选其一（本 skill 仅支持这两档）：
   A) master —— ADF v3 开发线；无内置 IDF；需外部 IDF ≥ v5.5.2（含 v6.x）
   B) release/v2.x —— 有内置 IDF（也可用外部）；IDF 仅 release/v5.1～v5.5（勿用 v6）
   （若不指定，将默认 A) master，请确认）
2) 安装父目录？未提供时可推荐 C:\esp 或 ~/esp（IDF / AT / ADF 共用默认父目录；完整路径如 C:\esp\esp-adf-master）；请确认或给出其它父目录后再继续
3) 芯片 / 开发板？
   - 若选 release/v2.x：芯片型号（如 esp32 / esp32s3）
   - 若选 master：可先不指定；编译前会 `idf.py bmgr -l` 列板，再请你选
4) 本地是否已有 ESP-IDF？
   - 有：请提供路径（将按所选 ADF 兼容表校验）
   - 无：请指定要安装的 IDF 版本（须落在兼容表内）
     或（仅选了 release/v2.x 时）改选使用内置 esp-idf
5) 工具链安装路径？不指定则默认 %USERPROFILE%\.espressif（Windows）或 ~/.espressif（Linux/macOS）；请确认后再安装工具链
```

| 占位符 | 含义 | 确认规则 |
|--------|------|----------|
| `YOUR_ADF_VERSION` | 分支 | **须展示 A/B 两档**；仅 `master` / `release/v2.x`；客户不指定则默认 `master`（须告知） |
| `YOUR_ADF_INSTALL_PATH` | 仓库根目录 | 父目录 + 命名规则（[placeholders.md § 父目录](placeholders.md#父目录-vs-仓库根目录)） |
| `YOUR_TARGET_CHIP` | 目标芯片 | **v2.x 主动询问**；**master 默认示例以 `bmgr` 选板为准**（不必 Phase 0 猜芯片） |
| `YOUR_IDF_PATH` | 实际用于编译的 IDF 根目录 | 本地 / 新装 /（仅明确选内置时）ADF 内置 |
| `YOUR_IDF_VERSION` | 需新装时的 IDF 版本 | **须落在兼容表内** |
| `YOUR_PROJECT_PATH` | 默认示例工程 | 见下表 |
| `YOUR_TOOLS_PATH` | 工具链路径 | **Phase 0 须问**；确认默认则不设 `IDF_TOOLS_PATH`；自定义则 install/export 前设置（[placeholders.md § IDF_TOOLS_PATH](placeholders.md#your_tools_path--idf_tools_path)） |

### 默认编译示例

| ADF 版本 | 默认 `YOUR_PROJECT_PATH`（首次验证） | 编译依据 |
|----------|--------------------------------------|----------|
| `master` | `YOUR_ADF_INSTALL_PATH/adf_examples/player/play_music_control` | **以该示例 README 为准**（见下「master 官方示例」） |
| `release/v2.x` | `YOUR_ADF_INSTALL_PATH/examples/get-started/play_mp3_control` | 以该示例 README 为准（[play_mp3_control](https://github.com/espressif/esp-adf/tree/release/v2.x/examples/get-started/play_mp3_control)） |

客户指定其它官方示例时：将 `YOUR_PROJECT_PATH` 改为该示例目录，**仍按该目录 README 编译**（不要套用另一示例的步骤）。

#### master · 官方示例（强制按 README + `bmgr`）

适用于默认 `play_music_control`，以及客户指定的其它 `adf_examples/...` 官方示例。

编译前 **必须**：

1. **打开刚克隆的 ESP-ADF 仓库中该示例目录的英文或中文 README**（这是目标项目文件，不是 TS Skills MCP reference；勿凭记忆或照搬其它示例）。只执行其中的依赖配置与 build 步骤；到首次编译成功即停止，不执行 flash / monitor。
2. 在已 `export` 的 IDF 环境中安装 board 助手（每环境一次）：`pip install esp-bmgr-assist`（需升级时再 `pip install --upgrade esp-bmgr-assist`）。
3. `cd YOUR_PROJECT_PATH` → 若 README 要求 Board Manager（绝大多数 master 音频示例如此）：执行 `idf.py bmgr -l`，把**本机实际输出**的开发板列表原样展示给客户。
4. **向客户确认选哪块开发板**（序号或板名）；未确认前 **禁止** `idf.py bmgr -b` / `idf.py build`。禁止猜板、禁止默认第一项。列表随 `esp_boards` 版本变化，以本次 `bmgr -l` 为准。
5. 客户确认后：`idf.py bmgr -b <board_index|board_name>` 配置板子，再按 README 继续（通常 `idf.py build`；Wi-Fi / `menuconfig` 等仅当 README 要求时再做）。

> master 示例多用 ESP Board Manager 管板级外设；**不要**只对 master 示例执行 `idf.py set-target` 就当选完板。
> 默认验证路径在线参考：[play_music_control](https://github.com/espressif/esp-adf/tree/master/adf_examples/player/play_music_control)。

> **Windows：** 先按 [esp-idf-windows.md](esp-idf-windows.md) 选定终端，全程固定。

---

## Phase 1 — 依赖 + 镜像 + 克隆 esp-adf
（+ 按需装 IDF）

> **前置：** Phase 0 的 ADF↔IDF 兼容 Gate 已通过。

**共用前半：**

1. **前提条件：** [prerequisites.md](prerequisites.md)。Windows 先 `esp-idf-windows.md` 第一步。
2. **pip + 网络/镜像：** [common.md § 第一步](common.md#第一步--pip-镜像配置)～[第二步](common.md#第二步--网络检查与-git-镜像配置)（国内镜像统一乐鑫；不再使用极狐）。
3. 若需**新装外部 IDF** → 用**已校验兼容**的 `YOUR_IDF_VERSION` 走 `esp-idf.md`，装到 `YOUR_INSTALL_PATH`，令 `YOUR_IDF_PATH=YOUR_INSTALL_PATH`，再继续克隆 ADF。
4. 按下方命令克隆 esp-adf；`release/v2.x` 再按是否用内置 IDF 处理 submodule。
   > **Agent Shell 长任务等待（强制）：** `release/v2.x` 下的 `git submodule update --init --recursive`（及带 `--force`）会拉大量子模块（含可能的内置 esp-idf），耗时长。须按 [common.md § Agent Shell 长任务等待](common.md#agent-shell-long-timeout) 把当前工具等待调到 ≥ 2h 或后台跑完再等（Cursor 示例：`block_until_ms` ≥ `7200000`）。短等待误杀 ≠ Git 失败。
5. 对最终选定的 **`YOUR_IDF_PATH`** 执行 [common.md § 第三步](common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用)（submodule Gate）；干净后才进入 Phase 2。
   若 `git status` 仍有 `modified: … (new commits, modified content)`，或 `submodule update` 中断/失败 → **立刻**按 [troubleshooting-git.md § Q2](troubleshooting-git.md)（步骤 0→A→B，路径以 status 为准）；禁止跳过 Gate、禁止不读 Q2 只反复整仓 update。

### 1.1 直连模式（速度对比判定 GitHub 更快 / 接近，且用户未强制国内镜像）

#### A) master（无内置 esp-idf）
```bash
# 先按 mirrors.md 清除全部极狐 url.*insteadOf，再：
git config --global --unset url."https://git.espressif.com.cn/".insteadOf
git clone -b master https://github.com/espressif/esp-adf.git YOUR_ADF_INSTALL_PATH
```

#### B) release/v2.x（先不 --recursive）
```bash
# 先按 mirrors.md 清除全部极狐 url.*insteadOf，再：
git config --global --unset url."https://git.espressif.com.cn/".insteadOf
git clone -b release/v2.x https://github.com/espressif/esp-adf.git YOUR_ADF_INSTALL_PATH
cd YOUR_ADF_INSTALL_PATH

# 使用外部 IDF（客户路径或新装路径）时 — 跳过内置 esp-idf：
git config submodule.esp-idf.update none
git submodule update --init --recursive --force

# 仅当客户明确选内置 IDF 时改用：
# git submodule update --init --recursive --force
# 然后 YOUR_IDF_PATH=YOUR_ADF_INSTALL_PATH/esp-idf，并跑 common.md 第三步
```

### 1.2 国内镜像模式（无法 ping 通 / 用户强制国内 / 速度对比乐鑫更快）

#### A) master
```bash
# 先按 mirrors.md「清除全部极狐 insteadOf」清干净（勿只 unset 总开关）
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
git clone -b master https://github.com/espressif/esp-adf.git YOUR_ADF_INSTALL_PATH
# 若还需新装外部 IDF：已用乐鑫镜像，可直接走 esp-idf.md（见 mirrors.md / common.md）
```

#### B) release/v2.x
```bash
# 先按 mirrors.md「清除全部极狐 insteadOf」清干净（勿只 unset 总开关）
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
git clone -b release/v2.x https://github.com/espressif/esp-adf.git YOUR_ADF_INSTALL_PATH
cd YOUR_ADF_INSTALL_PATH

# 外部 IDF：跳过内置 esp-idf
git config submodule.esp-idf.update none
git submodule update --init --recursive --force

# 内置 IDF：不要设 update none，直接 submodule update，再对 esp-idf 跑 common.md 第三步
```

> **不要**在 clone 时加 `--recursive`。
> 使用外部 IDF 时：**必须** `git config submodule.esp-idf.update none`，且 `YOUR_IDF_PATH` 指向外部/新装路径。

**Phase 1 Gate：** `YOUR_ADF_INSTALL_PATH` 存在；`YOUR_IDF_PATH` 已确定且已通过 [common.md § 第三步](common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用)。

---

## Phase 2 — 设置 ADF_PATH + 在 YOUR_IDF_PATH 上 export → 编译 → 验证

**所有子步须在同一 shell session 内连续执行。**

编译顺序（强制）：

1. 设置 `ADF_PATH` = `YOUR_ADF_INSTALL_PATH`
2. 设置 `IDF_GITHUB_ASSETS`（及可选 `IDF_TOOLS_PATH`）
3. **进入 Phase 0 确定的 `YOUR_IDF_PATH`（外部 / 新装 / 或明确选的内置）** → `install`（若尚未装工具链）→ `export`
4. 确认 `ADF_PATH` 仍有效
5. `cd YOUR_PROJECT_PATH` → **按该示例 README 编译**（见上「默认编译示例」）
   - **`master`（任一官方示例）：** 读该示例 README → `pip install esp-bmgr-assist` → `idf.py bmgr -l` → **等客户确认开发板** → `idf.py bmgr -b …` → 再按 README（通常 `idf.py build`）
   - **`release/v2.x` / `play_mp3_control`：** 按该示例 README；通常 `idf.py set-target YOUR_TARGET_CHIP` → `idf.py build`
6. 按 Iron Law 验证 bin

> **禁止：** 客户已提供或新装了外部 IDF，却仍 `cd YOUR_ADF_INSTALL_PATH/esp-idf`。
> **禁止：** master 官方示例未读 README、未 `bmgr -l`、未等客户确认开发板就 `bmgr -b` / `build`。

### 2.1 Linux / macOS

```bash
export ADF_PATH="YOUR_ADF_INSTALL_PATH"
export IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"
# 若自定义工具链：export IDF_TOOLS_PATH="YOUR_TOOLS_PATH"

cd "YOUR_IDF_PATH"
# 若尚未安装工具链：
./install.sh
. ./export.sh

# 若 export 后 ADF_PATH 丢失，再设一次：
export ADF_PATH="YOUR_ADF_INSTALL_PATH"

cd "YOUR_PROJECT_PATH"

# --- master / play_music_control（按示例 README）---
# pip install esp-bmgr-assist
# idf.py bmgr -l                    # 把列表给客户，等确认
# idf.py bmgr -b <board_index|name> # 客户确认后再执行
# idf.py build

# --- release/v2.x / play_mp3_control ---
# idf.py set-target YOUR_TARGET_CHIP
# idf.py build
```

### 2.2 Windows CMD

```bat
set ADF_PATH=YOUR_ADF_INSTALL_PATH
set IDF_GITHUB_ASSETS=dl.espressif.com/github_assets
:: 若自定义工具链：set IDF_TOOLS_PATH=YOUR_TOOLS_PATH

cd /d YOUR_IDF_PATH
install.bat
export.bat

set ADF_PATH=YOUR_ADF_INSTALL_PATH

cd /d YOUR_PROJECT_PATH

:: master / play_music_control：pip install esp-bmgr-assist
:: idf.py bmgr -l
:: （把列表给客户确认后）idf.py bmgr -b <board_index|name>
:: idf.py build

:: v2.x / play_mp3_control：
:: idf.py set-target YOUR_TARGET_CHIP
:: idf.py build
```

### 2.3 Windows PowerShell

```powershell
$env:ADF_PATH = "YOUR_ADF_INSTALL_PATH"
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"
# 若自定义工具链：$env:IDF_TOOLS_PATH = "YOUR_TOOLS_PATH"

cd YOUR_IDF_PATH
.\install.ps1   # 或 .\install.bat
.\export.ps1    # 或 cmd /c export.bat

$env:ADF_PATH = "YOUR_ADF_INSTALL_PATH"

cd YOUR_PROJECT_PATH
# master：同 2.1 / 2.2 — bmgr -l → 客户确认板 → bmgr -b → build
# v2.x：idf.py set-target YOUR_TARGET_CHIP; idf.py build
```

> **不要**只 export ADF 自带脚本却未保证进入的是 Phase 0 的 `YOUR_IDF_PATH`。
> **禁止**在已选定外部/新装 IDF 时仍进入 `YOUR_ADF_INSTALL_PATH/esp-idf`。

### 2.4 验证（Gate）

直接枚举当前验证工程 `build/` 下的 `.bin`，确认 bootloader、partition table 与应用固件均存在且大小非零。通过后才能报告 Done，不输出或执行烧录命令。
若本次配置了乐鑫官方镜像 → Done 后须加 [安装完成收尾提示](placeholders.md#post-install-espressif-mirror-notice)。

## Red Flags — STOP（仅本 SDK；通用见 `SKILL.md`）

- 🚩 「只展示 / 只推荐 release/v2.x，不提 master」→ **禁止**；Phase 0 须同时给出 `master` 与 `release/v2.x`，等客户选（不指定才默认 master）
- 🚩 「ADF 官方写了 gitee / `--recursive`，照抄就行」→ 国内先 `insteadOf` 再 clone GitHub URL；**v2.x 先 clone 再 submodule**（勿对 ADF 根目录 `--recursive`）
- 🚩 「master 也能用 ADF 里的 esp-idf」→ **master 无内置 esp-idf**，必须外部 IDF
- 🚩 「没设 ADF_PATH 直接 idf.py build」→ 必须先设 `ADF_PATH`
- 🚩 「只装了 ADF、没确认 IDF / 编译时仍进 ADF 内置 esp-idf」→ Phase 0 问清 IDF；`YOUR_IDF_PATH` 须指向客户路径或新装路径（仅明确选内置时例外）
- 🚩 「v2.x 外部 IDF 仍把内置 esp-idf submodule 拉下来」→ **`git config submodule.esp-idf.update none`**
- 🚩 「不问兼容就开装 / v2.x + IDF v6」→ 对照本文件兼容表；不通过则 STOP（v2.x 仅 IDF v5.1～v5.5）
- 🚩 「master 官方示例直接 set-target / 不读 README / `bmgr -l` 后替客户选第一块板」→ 按该示例 README：`bmgr -l` → **客户确认** → `bmgr -b` → `build`
