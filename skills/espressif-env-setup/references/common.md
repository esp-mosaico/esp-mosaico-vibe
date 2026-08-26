# 共性步骤参考（三平台通用）

本文件涵盖 **ESP-IDF / ESP-AT / ESP-ADF** 共用步骤：pip 镜像、网络检查与 Git 镜像、submodule 验证，以及 **Linux / macOS** 的工具链安装、激活、编译与验证。Windows 工具链与编译见 `esp-idf-windows.md`。

占位符 / 路径 / Iron Law → [placeholders.md](placeholders.md)。默认版本 → [defaults.md](defaults.md)。

> **安装前共性依赖：** 任意 SDK 开装前先完成 [prerequisites.md](prerequisites.md)（Python 3.10+ / Git）。Windows 还须先完成 `esp-idf-windows.md` 第一步终端选定。

---

## 第零步 — 前提条件（Python / Git）

→ **完整检测与安装见 [prerequisites.md](prerequisites.md)**（IDF / AT / ADF 相同，勿在各 SDK 文档重复实现）。

---

## 第一步 — pip 镜像配置

配置 pip 使用阿里云镜像（与克隆步骤无 session 依赖，可单独执行）：

Linux / macOS：
```bash
python3 -m pip config set global.index-url https://mirrors.aliyun.com/pypi/simple
```

Windows CMD / PowerShell：
```bat
python -m pip config set global.index-url https://mirrors.aliyun.com/pypi/simple
```

> 备用镜像详见 `mirrors.md`。

---

## 第二步 — 网络检查与 Git 镜像配置

将各 SDK Phase 0 确认的版本与安装路径替换进后续 clone 命令。

**决策流程（权威来源；主判据是 `git ls-remote`，不是 ping）：**

```
1. 用户是否明确要求「使用国内镜像 / 强制国内」？
   ├─ 是 → 国内镜像模式（乐鑫）
   └─ 否 → 继续
2. 清干扰配置后，对 GitHub 与乐鑫各做一次 git ls-remote（见下方速度对比）
   ├─ 仅一侧成功 → 用成功侧（乐鑫成功则国内模式；仅 GitHub 成功则直连）
   ├─ 两侧都成功 → 选更快一侧；相差约 20% 以内优先 GitHub 直连
   └─ 两侧都失败 → 告知用户；可再试国内模式 / 离线包（troubleshooting-git.md Q1）
3. ping www.google.com 仅作参考（公司网常禁 ICMP，失败≠必须国内）
```

> 乐鑫镜像收录 ESP-IDF / ESP-AT / ESP-ADF 等；本 skill **只提供这三套搭建流程**。其它仓库（如 esp-who）即便在镜像站可见，也不在本 skill 范围。

### 速度对比（用户未强制国内时必做）

测前按 [mirrors.md § 清除全部极狐](mirrors.md#clear-jihulab-insteadof)：**先检查** → 有残留才清 → 清后再确认（无残留则跳过清除）：
```bash
# 摘要：极狐已干净后，再：
git config --global --unset url."https://git.espressif.com.cn/".insteadOf
```

**Linux / macOS：**
```bash
time git ls-remote https://github.com/espressif/esp-idf.git HEAD
time git ls-remote https://git.espressif.com.cn/espressif/esp-idf.git HEAD
```

**Windows PowerShell：**
```powershell
Measure-Command { git ls-remote https://github.com/espressif/esp-idf.git HEAD } | Select-Object TotalSeconds
Measure-Command { git ls-remote https://git.espressif.com.cn/espressif/esp-idf.git HEAD } | Select-Object TotalSeconds
```

**Windows CMD（若无 PowerShell 计时）：**
```bat
curl -o NUL -s -w "github %{time_total}\n" --connect-timeout 15 https://github.com/espressif/esp-idf
curl -o NUL -s -w "espressif %{time_total}\n" --connect-timeout 15 https://git.espressif.com.cn/espressif/esp-idf
```

**判定：** 一侧失败 → 用另一侧；都成功 → 更快侧；约 20% 以内 → 优先 GitHub。测 IDF 仓库即可代表同组织 AT/ADF 相对快慢。

> Agent：把两次实测数字告诉用户，再进入「情况一」或「情况二」。

### 情况一：直连模式（GitHub 更快或接近）

```bash
# 先按 [mirrors.md § 清除全部极狐](mirrors.md#clear-jihulab-insteadof)：检查 → 有则清 → 清后再确认；再按 [§ 直连模式](mirrors.md#direct-mode) 解除乐鑫 insteadOf
git config --global --unset url."https://git.espressif.com.cn/".insteadOf
```

随后按各 SDK 文档 clone。**独立 ESP-IDF：clone 不要加 `--recursive`**（大仓如 wifi lib / openthread 放到第三步单独拉，失败走 Q2 而非 Q1）：
```bash
git clone -b YOUR_IDF_VERSION https://github.com/espressif/esp-idf.git YOUR_INSTALL_PATH
# 接着必须进入第三步：git submodule update --init --recursive -f
```

### 情况二：国内镜像模式（用户强制国内 / 乐鑫更快或仅乐鑫可达）

```bash
# 1) 按 [mirrors.md § 清除全部极狐](mirrors.md#clear-jihulab-insteadof)：检查 → 有则清 → 清后再确认（勿只 unset 总开关）
# 2) 再按 [§ 国内镜像模式](mirrors.md#domestic-mirror-mode) 配乐鑫
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
# 3) 若刚执行过清除：再确认 git config --global --get-regexp jihulab 无输出
```

仍用 **GitHub URL** clone（自动走乐鑫）。**独立 ESP-IDF：不要 `--recursive`**：
```bash
git clone -b YOUR_IDF_VERSION https://github.com/espressif/esp-idf.git YOUR_INSTALL_PATH
# 接着必须进入第三步拉子模块
```

AT / ADF 的 clone 见 `esp-at.md` / `esp-adf.md` Phase 1。

> 子模块失败（含 wifi lib / openthread 等大仓超时）→ [troubleshooting-git.md](troubleshooting-git.md) § **Q2**（**不要**当整仓 clone 失败走 Q1）
> 整仓（父仓库）克隆失败 → [troubleshooting-git.md](troubleshooting-git.md) § Q1
> `IDF_GITHUB_ASSETS` 在各平台 install 前设置，不在此处配置。
---

## 第三步 — 验证并修复 submodule（任意 esp-idf 树均适用）

> **强制：** 凡即将用于编译的 **esp-idf 仓库根目录**（记为 `YOUR_IDF_PATH`），在跑 `install` / `export` / 编译前都必须通过本步 Gate。
> - ESP-IDF 主流程：`YOUR_IDF_PATH` = `YOUR_INSTALL_PATH`
> - ESP-AT：`YOUR_IDF_PATH` = `YOUR_AT_INSTALL_PATH/esp-idf`（`build.py install` 拉齐后）
> - ESP-ADF：`YOUR_IDF_PATH` = Phase 0 选定的那份（外部路径、新装路径、或 v2.x 内置 `YOUR_ADF_INSTALL_PATH/esp-idf`）

> <a id="agent-shell-long-timeout"></a>
> **Agent Shell 长任务等待（强制，一开始就要设对）：**
> 子模块（尤其 `esp32-wifi-lib` / openthread 等）体积大，常远超多数 Agent 的**默认** Shell 等待（常见约 30s～5min，例如日志里的 `300000 ms`）。
> 凡本步 `git submodule update`、Q2 里大仓 `git fetch` / `git clone`、AT `build.py install`（拉 `esp-at/esp-idf`）、**ADF `release/v2.x` 根目录 `git submodule update --init --recursive`**，均须按下述原则处理。
>
> **跨 Agent 原则（参数名因平台而异，禁止写死只认一个名字）：**
> 1. **首次在当前会话跑长任务前，先确认一次当前 Shell 工具的等待/超时语义**：参数叫什么、单位是 ms 还是 s、超时后是 **kill 进程** 还是 **转后台继续跑**。常见名字举例：Cursor 的 `block_until_ms`；其它 Agent 可能是 `timeout` / `timeout_ms` / `timeout_seconds` 等——以**本会话工具描述为准**。同一会话已确认过后，后续可直接沿用，**不必反复查**。
> 2. **把该参数调到 ≥ 2 小时**（或该工具支持的等价上限），再跑长拉取；或按该工具方式**后台执行**，直到命令真正结束（有 `exit_code` / 进程退出），禁止默认短等待就开下一轮。
>    - Cursor 示例：`block_until_ms` ≥ `7200000`；或 `block_until_ms: 0` 后台跑，再用 Await / 读终端输出确认结束。
> 3. 日志含 `shell tool terminated ... exceeding timeout 300000 ms`（或同类「因超时结束了 shell」）且仍停在 `Cloning into...` → **是 Agent 侧等待/误杀，不是 Git 已失败**；加长等待后重试，**禁止**据此立刻断定镜像坏了或空转 Q1。
> 4. 若该 Agent **超时会 kill**：禁止在短超时下对同一目录反复全量 clone（易留半拉仓库）；应一次把等待设够，或改走 [troubleshooting-git.md § Q2 方案 2a](troubleshooting-git.md) 浅取。
> 5. 加长等待只能规避「假失败」；真 404 / 极狐残留 / RPC failed 仍走 [troubleshooting-git.md § Q2](troubleshooting-git.md)。

> **独立 IDF 克隆策略（强制）：** 父仓 `git clone` **禁止** `--recursive`；子模块**只**在本步用 `git submodule update --init --recursive -f` 拉取。这样 wifi lib / openthread 等失败会落在本步，按 **Q2** 定点修，而不会被误判成「整仓 clone 失败 → Q1」。

> **失败即停、必须读 FAQ：**
> - `git submodule update` **中途失败/被中断**，或
> - `git status` 出现 `modified: … (new commits, modified content)` / `modified content`，或
> - 报错含超时、404、`Unable to find current revision`、clone into submodule path failed
> → **立刻打开并按 [troubleshooting-git.md § Q2](troubleshooting-git.md)**（步骤 0 → A → B → 方案一…）。
> **禁止**跳过 Gate 继续装工具链 / 克隆 ADF / 编译；**禁止**不读 Q2 只反复整仓 `submodule update`；**禁止**照抄 Q2 示例路径（须用本次 status 中的路径）；**禁止**把子模块超时误开成 Q1 离线包（除非 Q2 方案四明确要求）。父仓 clone 失败才见 § Q1。索引：[troubleshooting.md](troubleshooting.md)。

父仓克隆（或 AT/ADF 拉齐内置 IDF）完成后，**必须**在本步拉齐子模块并检查：

> **预期：** 无 `--recursive` 的 clone 之后，子模块目录常为空或不齐；`submodule update` 中断后 `git status` 也常见大量 `modified: components/...`。这**不等于**环境坏了。**必须先完成本步 Gate**，不要假定 clone 后已可 install。

Linux / macOS：
```bash
cd YOUR_IDF_PATH
git submodule update --init --recursive -f
```

Windows CMD：
```bat
cd /d YOUR_IDF_PATH
git submodule update --init --recursive -f
```

检查仓库状态：

```bash
git status
```

**期望输出**（工作区干净即可；分支名随版本而变）：
```
# 独立 IDF 常见：
On branch release/v6.0
nothing to commit, working tree clean

# tag 安装 / AT 内置 esp-idf 常见（detached / Not currently on any branch 均正常）：
HEAD detached at <commit>
nothing to commit, working tree clean
```

**判定：**
```
git submodule update 是否中断/失败？
├─ 是 → STOP → troubleshooting-git.md § Q2（勿空转重跑）
└─ 否 → git status 是否 working tree clean？
         ├─ 是 → Gate 通过，可继续
         └─ 否（仍有 modified: …）→ 对 status 中每个路径：
              git submodule update --init -- <PATH_FROM_STATUS>
              仍不干净 → § Q2 方案一（deinit 该 PATH）→ 再 status
              禁止进入 install / ADF Phase 2
```

```bash
git submodule update --init -- components/SUBMODULE_PATH
# SUBMODULE_PATH = git status 里 modified 后面的完整相对路径
# 例：components/bootloader/subproject/components/micro-ecc/micro-ecc
```

重复 `git status` 直到干净。**干净之前不得进入工具链 install / 编译 /（ADF）下一步。**

仍失败 → **必须**按 [troubleshooting-git.md](troubleshooting-git.md) § Q2；若 `YOUR_IDF_VERSION` 为正式 tag 且定点修复仍反复失败 → **Q2 方案四**离线 zip，不得自行发明步骤。

---

## 第四步 — Linux / macOS 工具链安装、激活、编译与验证

以下步骤适用于 Linux 和 macOS（bash/zsh）的 **ESP-IDF 主流程**（`YOUR_INSTALL_PATH`）。Fish shell 用户将 `install.sh` / `export.sh` 替换为对应的 `.fish` 脚本。
AT / ADF 的 install/export 路径以各 SDK 文档为准，但仍须先对本步使用的 `YOUR_IDF_PATH` 完成上文第三步。

### 4.1 安装工具链

`export IDF_GITHUB_ASSETS` 必须与 `./install.sh` 在**同一个 shell session** 中执行。

> **⚠️ 下方代码块中的多条命令必须在同一个终端内连续执行，中途请勿关闭或新开终端！**

**情况一：使用默认安装路径（推荐）**
```bash
cd YOUR_INSTALL_PATH
export IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"
./install.sh all
```

**情况二：自定义工具链安装路径**

**必须在运行 install 脚本前手动设置 `IDF_TOOLS_PATH`**；后续每次 export 前也要重新设置（见 4.2、4.3）。

```bash
cd YOUR_INSTALL_PATH
export IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"
export IDF_TOOLS_PATH="YOUR_TOOLS_PATH"
./install.sh all
```

> 默认 `all` 安装所有芯片工具链；可改为 `esp32` 或 `esp32,esp32s2` 节省空间。
> 备用镜像：`export IDF_GITHUB_ASSETS="dl.espressif.cn/github_assets"`
> 失败时 → 参见 [troubleshooting-toolchain.md](troubleshooting-toolchain.md) § Q4

### 4.2 激活环境变量

每次打开新终端都需要重新激活。若使用了自定义工具链路径，**必须先手动设置 `IDF_TOOLS_PATH`，再执行 export 脚本**——顺序不能颠倒，`export.sh` 不会自动沿用 install 时或上次 session 的值。

**情况一：默认路径**
```bash
. YOUR_INSTALL_PATH/export.sh
```

**情况二：自定义路径**
```bash
export IDF_TOOLS_PATH="YOUR_TOOLS_PATH"
. YOUR_INSTALL_PATH/export.sh
```

> `. ` 和路径之间**必须有空格**。写成 `./export.sh` 会在子 shell 执行，环境变量不会传回当前 session。

**可选别名**（写入 `~/.bashrc` 或 `~/.zprofile`）：

默认路径：
```bash
alias get_idf='. YOUR_INSTALL_PATH/export.sh'
```

自定义路径（别名内也要先设 `IDF_TOOLS_PATH`）：
```bash
alias get_idf='export IDF_TOOLS_PATH="YOUR_TOOLS_PATH" && . YOUR_INSTALL_PATH/export.sh'
```

> 不建议将 `export.sh` 加入 shell profile 自动执行——会污染所有终端 session。

### 4.3 编译与验证

> **⚠️ 下方代码块中的多条命令必须在同一个终端内连续执行！**
> 若用户指定了 `YOUR_TOOLS_PATH`，须选用 **情况二**（export 前手动 `export IDF_TOOLS_PATH`）；未指定自定义路径时用 **情况一**，不要设置 `IDF_TOOLS_PATH`。

**① 激活并编译：**

情况一：默认路径
```bash
. YOUR_INSTALL_PATH/export.sh
idf.py -C YOUR_PROJECT_PATH set-target YOUR_TARGET_CHIP
idf.py -C YOUR_PROJECT_PATH build
```

情况二：自定义路径（export 前必须先设 `IDF_TOOLS_PATH`）
```bash
export IDF_TOOLS_PATH="YOUR_TOOLS_PATH"
. YOUR_INSTALL_PATH/export.sh
idf.py -C YOUR_PROJECT_PATH set-target YOUR_TARGET_CHIP
idf.py -C YOUR_PROJECT_PATH build
```

> **新芯片 / preview target：** 部分尚未正式量产的芯片（如 ESP32-S31 等）直接 `idf.py set-target` 可能失败。若报错提示使用 `--preview`，按提示改用：
> `idf.py -C YOUR_PROJECT_PATH --preview set-target YOUR_TARGET_CHIP`
> 详见 [troubleshooting-build.md § Q7](troubleshooting-build.md#q7-idfpy-set-target-失败unknown-target--需-preview)。**禁止**在无报错提示时自行乱加 `--preview`。

**② 编译成功后验证：**

直接枚举当前验证工程 `build/` 下的 `.bin`，确认至少包含 bootloader、partition table 和应用固件，且文件大小均非零。不输出或执行任何烧录命令。

Linux / macOS：
```bash
find YOUR_PROJECT_PATH/build -type f -name '*.bin' -size +0c -print
```

Windows CMD：
```bat
for /r YOUR_PROJECT_PATH\build %F in (*.bin) do @if %~zF GTR 0 echo %~zF %F
```

Windows PowerShell：
```powershell
Get-ChildItem YOUR_PROJECT_PATH\build -Recurse -Filter *.bin |
    Where-Object Length -gt 0 | Select-Object FullName, Length
```

**所需 bin 均存在且大小非零** → 报告成功。
**有文件缺失或为零字节** → 参见 [troubleshooting-build.md](troubleshooting-build.md) § Q5
若本次配置了乐鑫官方镜像 → 报告成功后须加镜像影响与清除提示（[placeholders.md § 安装完成收尾](placeholders.md#post-install-espressif-mirror-notice)）。

**③ 可选 — VS Code ESP-IDF 扩展：**

用户要用扩展或主动询问时，按 `references/vscode-extension.md` 输出三个环境变量及 `idf.customExtraVars` 配置。
