# ESP-AT 开发环境搭建

> **适用前提：** 主 Skill 已确认用户要从零安装 ESP-AT。本文件只服务于全新安装和首次验证出固件；不得据此响应独立排障、已有环境修复、固件运行异常或自定义功能开发请求。

ESP-AT 是基于 ESP-IDF 的应用工程。每个模组在 `module_config/.../IDF_VERSION` 中固定依赖的 IDF 分支 / commit / 仓库；`build.py install` 会按所选 Platform / Module 把对应 IDF 克隆到 `esp-at/esp-idf`。**不同芯片 / 模组依赖的 IDF 版本可能不同**，不要假设与本机已装的独立 ESP-IDF 相同。

官方文档：
- [技术选型（支持芯片 / 推荐 AT 固件）](https://docs.espressif.com/projects/esp-at/zh_CN/latest/esp32/Get_Started/Technology_selection.html#at)
- [本地编译 ESP-AT 工程](https://docs.espressif.com/projects/esp-at/zh_CN/latest/esp32/Compile_and_Develop/How_to_clone_project_and_compile_it.html)
- [如何更新 ESP-IDF 版本](https://docs.espressif.com/projects/esp-at/zh_CN/latest/esp32/Compile_and_Develop/How_to_update_IDF.html#esp-idf)

**共用规则（勿在本文件重复；只读对应章节）：**
- Iron Law / 父目录 / 路径 / `IDF_TOOLS_PATH` → [placeholders.md § Iron Law](placeholders.md#the-iron-law) · [§ 父目录](placeholders.md#父目录-vs-仓库根目录) · [§ IDF_TOOLS_PATH](placeholders.md#your_tools_path--idf_tools_path)
- AT 支持芯片 + 推荐固件 → [defaults.md § ESP-AT](defaults.md#esp-at支持芯片与默认版本)（**不要**默认 `master`；**勿**读 IDF/ADF 节）
- pip / 网络镜像 / submodule → [common.md § 第一步](common.md#第一步--pip-镜像配置)～[第三步](common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用)；[prerequisites.md](prerequisites.md)

---

## Phase 0 — 确认（克隆前）

执行任何命令前，**主动询问**下列项；不得静默套用默认值而不告知。路径拼装与 Iron Law 见 [placeholders.md](placeholders.md)；芯片/版本规则见 [defaults.md § ESP-AT：支持芯片与默认版本](defaults.md#esp-at支持芯片与默认版本)。

**询问顺序（强制）：先芯片 → 再版本 → 再父目录 → 再工具链路径**（版本推荐依赖芯片；**禁止**先问版本并默认 `master`）。

1. **芯片型号 → `YOUR_PLATFORM`**
   - **未提供芯片时（强制）：** **只**打开 [defaults.md § ESP-AT](defaults.md#esp-at支持芯片与默认版本)，把该节 **「向客户展示模板」**（权威页：[技术选型](https://docs.espressif.com/projects/esp-at/zh_CN/latest/esp32/Get_Started/Technology_selection.html#at)）**原样贴进对话**，等其回复序号或芯片名。
     - **禁止**在本文件或对话里另写一套芯片列表；**禁止**只写「如 ESP32、ESP32C2 等」；**禁止**假设、禁止默认第一项、禁止推荐表外芯片。
     - 缓存表与官网不一致时 → **以官网为准**，并可顺手更新 `defaults.md`。
   - **已提供**：须落在该节缓存表内；映射为 Platform（如 ESP32-C2 → `PLATFORM_ESP32C2`）并回显。表外芯片 → STOP。

2. **`YOUR_AT_VERSION`** — 分支或 tag（**须在芯片已确认之后**再问/再定）。
   - **已提供**：使用客户指定（可顺带告知该芯片官方推荐 tag）。
   - **未提供**：按该芯片行 **「推荐的 AT 固件」** 推荐（如 ESP32-C2 → `v4.1.1.0`），告知客户后确认再用。
   - **禁止**未指定时默认 / 暗示「默认拉取 `master`」。仅客户明确要新功能时才用 `master`。
   - 未定芯片前**禁止**给出任何默认版本路径（如 `esp-at-master`）。

3. **安装父目录** — **必须主动询问**（[placeholders.md § 父目录](placeholders.md#父目录-vs-仓库根目录)）。
   - 未提供 → 推荐共用默认父目录（Windows `C:\esp`；Linux/macOS `~/esp`），**说明该默认同样用于 ESP-IDF / ESP-ADF**，并按**已确认的** `YOUR_AT_VERSION` 回显完整 `YOUR_AT_INSTALL_PATH`；**等客户确认后**再克隆。禁止静默采用默认。
   - 已提供 → 拼完整路径并回显，确认后再克隆。

4. **工具链路径** — **必须主动询问**（[placeholders.md § IDF_TOOLS_PATH](placeholders.md#your_tools_path--idf_tools_path)）。
   - 未提供 → 告知默认（Windows `%USERPROFILE%\.espressif`；Linux/macOS `~/.espressif`），**等客户确认**后再装工具链。
   - 已提供 → 回显并确认；之后 install/export 前设 `IDF_TOOLS_PATH`。

> **Module name** 与 **silence mode** 不要在 Phase 0 询问——须 **Phase 1 克隆完成后**读 CSV（Phase 1.5）。

向客户确认时使用下列骨架（**第 1 项的芯片列表不得在本文件维护**；未给芯片时把 `defaults.md`「向客户展示模板」整段替换进去）：

```
开始搭建 ESP-AT 前请确认（请按顺序回复）：
1) 芯片型号？（未指定时：粘贴 defaults.md § ESP-AT「向客户展示模板」全文，含序号与推荐固件）
2) esp-at 版本？不指定则按你所选芯片的「推荐 AT 固件」安装；**不会**默认 master（仅你明确要求 master 时才用）
3) 安装父目录？未提供时可推荐 C:\esp 或 ~/esp（IDF / AT / ADF 共用）；完整路径将在版本确认后回显（如 C:\esp\esp-at-v4.1.1.0）
4) 工具链安装路径？不指定则默认 %USERPROFILE%\.espressif（Windows）或 ~/.espressif（Linux/macOS）；请确认后再安装工具链
```

| 占位符 | 含义 | 确认规则 |
|--------|------|----------|
| 芯片 / `YOUR_PLATFORM` | 芯片 → Platform | **无默认**；仅 [defaults.md § ESP-AT](defaults.md#esp-at支持芯片与默认版本) 表内 |
| `YOUR_AT_VERSION` | 分支或 tag | 未指定 → 该芯片「推荐的 AT 固件」；非默认 `master` |
| `YOUR_AT_INSTALL_PATH` | esp-at 仓库根目录 | 父目录 + 命名规则（[placeholders.md § 父目录](placeholders.md#父目录-vs-仓库根目录)） |
| `YOUR_MODULE` / `YOUR_SILENCE` | Module / silence | **Phase 1.5** 对话确认后写入 `build/module_info.json`；禁止猜测 |
| `YOUR_TOOLS_PATH` | 工具链路径 | **Phase 0 须问**；确认默认则不设 `IDF_TOOLS_PATH`；自定义则 install/export 前设置 |

> **重新选择模组：** 删除 `build/module_info.json`（及可选 `sdkconfig`）后重走 Phase 1.5，再 `build.py install`。
> **Windows：** 先按 [esp-idf-windows.md](esp-idf-windows.md) 第一步选定终端，全程固定。
> **初学者：** 建议先完成一次独立 ESP-IDF hello_world，再搭 AT（非必须）。

---

## Phase 1 — 依赖 + 镜像 + 克隆 esp-at

**共用前半（勿重复发明）：**

1. **前提条件：** [prerequisites.md](prerequisites.md)。Windows 先 `esp-idf-windows.md` 第一步终端选定。
2. **pip + 网络/镜像判定：** [common.md § 第一步](common.md#第一步--pip-镜像配置)～[第二步](common.md#第二步--网络检查与-git-镜像配置)（国内镜像统一乐鑫，见 `mirrors.md`；不再使用极狐）。
3. 按下方命令克隆 esp-at。

### 1.1 直连模式（速度对比判定 GitHub 更快 / 接近，或用户未强制国内且已按 common.md 选直连）
```bash
# 先按 mirrors.md 清除全部极狐 url.*insteadOf，再：
git config --global --unset url."https://git.espressif.com.cn/".insteadOf
git clone -b YOUR_AT_VERSION --recursive https://github.com/espressif/esp-at.git YOUR_AT_INSTALL_PATH
```

### 1.2 国内镜像模式（用户强制国内 / 乐鑫更快或仅乐鑫可达；见 common.md 第二步）
```bash
# 先按 mirrors.md「清除全部极狐 insteadOf」清干净（勿只 unset 总开关）
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
# 确认：git config --global --get-regexp jihulab  应无输出
git clone -b YOUR_AT_VERSION --recursive https://github.com/espressif/esp-at.git YOUR_AT_INSTALL_PATH
```

始终带 `-b YOUR_AT_VERSION`（未指定时为该芯片「推荐的 AT 固件」tag，见 [defaults.md § ESP-AT](defaults.md#esp-at支持芯片与默认版本)），不要省略分支参数以免落到非预期分支。

**Phase 1 Gate：** `YOUR_AT_INSTALL_PATH` 存在且含 `build.py`，以及
`components/customized_partitions/raw_data/factory_param/factory_param_data.csv`。

---

## Phase 1.5 — 从 CSV 确认 Module / silence（install 前必做）

### 为何不在 Agent Shell 里交互 `build.py`？

客户打字在 **Cursor 对话**，不在 Agent Shell 的 stdin。`build.py` 的 `input()` / `choose:` **接不到**聊天回复；Shell 里干等 → 超时，或无输入 → `EOF when reading a line`。
**正确模型：** 在对话里问完 → 客户回复 → Agent 落盘配置 → **非交互**跑 `build.py install`。

### 推荐做法（写 `module_info.json`，跳过三问）

`build.py` 若发现 `build/module_info.json` 合法，**直接跳过** Platform / Module / Silence 交互（见 `choose_project_config()`）。因此 **不需要** `PLATFORM_INDEX`、stdin pipe、here-string。

CSV 路径：

`YOUR_AT_INSTALL_PATH/components/customized_partitions/raw_data/factory_param/factory_param_data.csv`

列：`platform`, `module_name`, `description`, ...

**Agent 强制流程：**

1. **读本机 CSV**，筛 `platform == YOUR_PLATFORM`（大小写不敏感；写入 JSON 时用 CSV/脚本得到的 **大写** `PLATFORM_…` 与 **大写** `module_name`，与 `build.py` 一致）。
2. 把该平台下的 `module_name` + `description` **原样**展示给客户（可带序号方便选，但以**模块名为准**）。
3. 说明 Silence：`0` = No（保留日志），`1` = Yes。
4. **等客户在对话里回复** Module（名或序号）+ Silence。禁止猜测、禁止默认第一项。
5. 在 `YOUR_AT_INSTALL_PATH` 创建 `build/`（若不存在），写入 `build/module_info.json`（见下），再进 Phase 2.1。

**解析客户回复：** 常写成 `Module: 2. ESP32C5-4MB` / `Silence: 3. 0`——此处 `2.`/`3.` 是**题号**，不是菜单索引。有模块名 → 按名匹配；Silence 取 `0`/`1`。

提问模板：

```
Platform 已定为 YOUR_PLATFORM（来自 Phase 0 芯片，无需再选序号）。

请从下列模组选择（回复模块名或序号）：
N. <module_name>	(Firmware description: <description>)
...

Silence mode：0 = No，1 = Yes。请一并回复。
例：ESP32C5-4MB / 0
```

写入 JSON（字段名固定；`platform` / `module` 须能在 CSV 中命中，`silence` 为整数 0 或 1）：

```json
{
  "platform": "PLATFORM_ESP32C5",
  "module": "ESP32C5-4MB",
  "description": "4MB, Wi-Fi + BLE, OTA, TX:23 RX:24",
  "silence": 0
}
```

**强制用 Python 写文件**（`build.py` 用 `json.load` 读；Windows PowerShell 的 `ConvertTo-Json | Set-Content -Encoding utf8` 常带 **UTF-8 BOM**，会报 `Expecting value: line 1 column 1 (char 0)`）。

```powershell
cd YOUR_AT_INSTALL_PATH
New-Item -ItemType Directory -Force -Path build | Out-Null
python -c "import json; json.dump({'platform':'YOUR_PLATFORM','module':'YOUR_MODULE','description':'YOUR_DESCRIPTION','silence':0}, open('build/module_info.json','w',encoding='utf-8'))"
# 自检（须打印 dict，不能报错）：
python -c "import json; print(json.load(open('build/module_info.json',encoding='utf-8')))"
```

Linux / macOS 同样用 `python3 -c "import json; json.dump(...)"`。把 `YOUR_*` 换成客户确认值；`description` 从 CSV 同行抄，可空字符串 `""`；`silence` 为 `0` 或 `1`。

> **禁止** `ConvertTo-Json | Set-Content -Encoding utf8`（BOM）。若已踩坑：删文件后按上面 Python 一行重写，再 `build.py install`。
> **重选模组：** 删 `build/module_info.json`（必要时也删 `sdkconfig`），再走 Phase 1.5。
> **禁止**把 [defaults.md](defaults.md) 芯片表序号当成任何 install 菜单序号。

### 备选（仅人工本机终端）：stdin 注入三问

仅当客户**自己**在交互终端跑 `build.py`、或无法写 JSON 时：按 CSV 首次出现顺序得到与 `build.py` 相同的 Platform/Module 序号，再 `printf` / here-string 注入。Agent **默认不要**走这条（易错、且 Shell 接不到聊天输入）。

---

## Phase 2 — install（拉 IDF）→ submodule 校验 → 工具链 → 编译 → 验证

**所有子步须在同一 shell session 内连续执行。**

`build.py install` 会：
1. 安装 AT / IDF 相关 Python 依赖（prerequisites）
2. 读 `build/module_info.json`（Phase 1.5 已写好则**跳过**三问）；若无该文件才会交互选 Platform / Module / silence
3. 按该模组 `IDF_VERSION` 克隆对应 esp-idf 到 `YOUR_AT_INSTALL_PATH/esp-idf`，并 `git submodule update --init --recursive`
4. **自动安装工具链**：调用 `esp-idf/tools/idf_tools.py install-python-env` 与 `idf_tools.py install --targets <chip>`（与手动跑 `install.sh` / `install.bat` 同类）

> **自定义工具链路径仍然有用，但必须在本步之前生效：**
> `idf_tools.py` 读环境变量 `IDF_TOOLS_PATH`。若客户确认了 `YOUR_TOOLS_PATH`，**在启动 `build.py install` 之前**就必须设好 `IDF_TOOLS_PATH`（及 `IDF_GITHUB_ASSETS`），否则工具链会装到默认 `%USERPROFILE%\.espressif` / `~/.espressif`，等于白问。
> 客户确认使用默认工具链目录 → **不要**设 `IDF_TOOLS_PATH`。

**强制：** `build.py install` 拉齐 `esp-idf` 后，**不要指望此刻 `git status` 已干净**（常见大量 submodule `modified`，或中途被超时打断）。令 `YOUR_IDF_PATH=YOUR_AT_INSTALL_PATH/esp-idf`，**先执行 [common.md § 第三步](common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用)** 对齐子模块，`git status` 干净后，再进入该目录设 `IDF_GITHUB_ASSETS`（及可选 `IDF_TOOLS_PATH`）跑 **export**（若 `build.py install` 已成功装过工具链，一般不必再跑一遍完整 `install.sh`；工具链缺失或装到了错误目录时再补跑 install，见 2.2）。

> **Agent：** `build.py install`（拉 IDF + 子模块）与随后的 `common.md` 第三步，须按 [common.md § Agent Shell 长任务等待](common.md#agent-shell-long-timeout) 把当前工具等待调到 ≥ 2h（Cursor 示例：`block_until_ms` ≥ 7200000）。短等待误杀 ≠ Git 失败。

> **`build.py install` / submodule 中途失败**（超时、404、`Unable to find current revision`、某组件如 esp_wifi 拉不下来）→ 先区分是否 Agent 短超时误杀；真失败则 **立刻停，打开 [troubleshooting-git.md](troubleshooting-git.md) § Q2**（必要时先 `mirrors.md` 清极狐），修好后再重试；禁止跳过继续装工具链。

### 2.1 pip + install（非交互；依赖 Phase 1.5 的 module_info.json）

**前置：** `build/module_info.json` 已按客户选择写好。若尚未确认 Module/Silence → **先停在 1.5**。
JSON 错误或要改选 → 删除该文件后重做 1.5。

**Agent 强制流程：**

1. 确认 `build/module_info.json` 存在；**必须**先 `python -c "import json; print(json.load(open('build/module_info.json',encoding='utf-8')))"` 能打印 dict。若 `Expecting value: line 1 column 1` → 多半是 BOM/空文件，按 1.5 用 Python 重写后再装。
2. **直接**跑 `python build.py install`（**不要**再 pipe stdin；**不要**在 Shell 里干等 `choose:`）。
3. 若仍出现 Platform 菜单 → JSON 未生效（路径不对 / 缺字段 / 工作目录不是仓库根）→ 停并修 JSON，禁止瞎注入序号。
4. install 拉完 IDF 后，**立刻**对 `YOUR_AT_INSTALL_PATH/esp-idf` 跑 [common.md § 第三步](common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用)；通过后再做 2.2。

Linux / macOS：
```bash
cd YOUR_AT_INSTALL_PATH
python3 -m pip config set global.index-url https://mirrors.aliyun.com/pypi/simple
export IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"
# export IDF_TOOLS_PATH="YOUR_TOOLS_PATH"   # 仅自定义工具链时
./build.py install
```

Windows PowerShell：
```powershell
cd YOUR_AT_INSTALL_PATH
python -m pip config set global.index-url https://mirrors.aliyun.com/pypi/simple
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"
# $env:IDF_TOOLS_PATH = "YOUR_TOOLS_PATH"   # 仅自定义工具链时
python build.py install
```

Windows CMD：
```bat
cd /d YOUR_AT_INSTALL_PATH
python -m pip config set global.index-url https://mirrors.aliyun.com/pypi/simple
set IDF_GITHUB_ASSETS=dl.espressif.com/github_assets
:: set IDF_TOOLS_PATH=YOUR_TOOLS_PATH
python build.py install
```

成功时通常**不再**打印 `choose(range...)`，而是直接进入拉 `esp-idf` / 装工具链。Shell 等待 ≥ 2h（见 [common.md](common.md#agent-shell-long-timeout)；Cursor：`block_until_ms` ≥ 7200000）。

> **备选 stdin 注入**仅给本机人工交互用；Agent 默认禁止依赖 pipe 三问（聊天输入进不了 Shell）。

### 2.2 在 esp-at/esp-idf 内激活工具链（必要时补装）

> 前置：[common.md § 第三步](common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用) 已通过（`YOUR_IDF_PATH=YOUR_AT_INSTALL_PATH/esp-idf`）。
> **`build.py install` 通常已调用 `idf_tools.py install` 装过工具链。** 本步以 **export 激活**为主；仅当工具链缺失、或当时未设 `IDF_TOOLS_PATH` 导致装到了默认目录而客户要自定义目录时，再补跑 `install.sh` / `install.bat` / `install.ps1`（自定义路径时 install/export 前都要先设 `IDF_TOOLS_PATH`）。

Linux / macOS：
```bash
cd YOUR_AT_INSTALL_PATH/esp-idf
export IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"
# 若自定义工具链：export IDF_TOOLS_PATH="YOUR_TOOLS_PATH"
# 仅当需要补装时：./install.sh
. ./export.sh
cd ..
```

Windows CMD：
```bat
cd /d YOUR_AT_INSTALL_PATH\esp-idf
set IDF_GITHUB_ASSETS=dl.espressif.com/github_assets
:: 若自定义工具链：set IDF_TOOLS_PATH=YOUR_TOOLS_PATH
:: 仅当需要补装时：install.bat
export.bat
cd ..
```

Windows PowerShell：
```powershell
cd YOUR_AT_INSTALL_PATH\esp-idf
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"
# 若自定义工具链：$env:IDF_TOOLS_PATH = "YOUR_TOOLS_PATH"
# 仅当需要补装时：.\install.ps1  （若无则在本目录用 CMD：cmd /c "install.bat"）
.\export.ps1
cd ..
```

> **PowerShell 禁止** `cmd /c export.bat` 后再在 **同一条 PowerShell** 里跑 `build.py`：`cmd /c` 结束后环境变量**不会**回到 PowerShell。应优先 `.\export.ps1`；若只有 `export.bat`，把「export + build」整段放进**同一个** `cmd /c "..."`（见下 2.3）。
> 备用 CDN：`dl.espressif.cn/github_assets`。
> 新开终端后须重新 export，再回到 `esp-at` 根目录执行 `build.py`。

### 2.3 menuconfig → build → 验证

> **`build.py build` 会自己调用** `esp-idf/tools/idf_tools.py export --format=key-value` 注入本进程环境（日志里会出现 `PATH is ...`、`IDF_PYTHON_ENV_PATH is ...`）。因此：**即使本 session 忘了手动 export，也不等于没激活。**
> 若已出现上述两行仍报 `No module named 'esp_idf_monitor'` → **不是**「没 export」，而是 IDF Python 虚拟环境缺包 / 损坏 → 见 [troubleshooting-at-adf.md](troubleshooting-at-adf.md) § Q-AT4（补跑 `install-python-env`），禁止只会反复 export。

Linux / macOS：
```bash
cd YOUR_AT_INSTALL_PATH
./build.py menuconfig   # 可选；默认配置可跳过
./build.py build
```

Windows（已用 `export.ps1` 激活的同一 PowerShell，或依赖 build.py 自带 export）：
```powershell
cd YOUR_AT_INSTALL_PATH
python build.py build
```

Windows（仅有 `export.bat`、须保证 bat 环境与 build 同进程时）：
```bat
cmd /c "cd /d YOUR_AT_INSTALL_PATH\esp-idf && call export.bat && cd /d YOUR_AT_INSTALL_PATH && python build.py build"
```

**验证（Gate）：**

```bash
# Linux / macOS
ls -lh YOUR_AT_INSTALL_PATH/build/factory/

# Windows CMD
dir YOUR_AT_INSTALL_PATH\build\factory\

# PowerShell
Get-ChildItem YOUR_AT_INSTALL_PATH\build\factory\
```

目录下关键固件 bin 存在且大小非零 → 可报告编译成功。
缺失 → 见 [troubleshooting-at-adf.md](troubleshooting-at-adf.md) § Q-AT1。
若本次配置了乐鑫官方镜像 → 报告成功后须加镜像影响与清除提示（[placeholders.md § 安装完成收尾](placeholders.md#post-install-espressif-mirror-notice)）。

## Red Flags — STOP（仅本 SDK；通用见 `SKILL.md`）

- 🚩 「机器上已有 ESP-IDF，跳过 install 里的克隆」→ AT 模组钉死特定 commit，必须用 `YOUR_AT_INSTALL_PATH/esp-idf`
- 🚩 「随意升级 `esp-at/esp-idf`」→ 与预编译 AT 库版本不一致会运行异常；官方不建议改
- 🚩 「install 没三问就当选过了」→ 检查是否已有 `build/module_info.json`；内容须与客户选择一致
- 🚩 「`No module named 'esp_idf_monitor'` 就只反复 export」→ 先看日志是否已有 `IDF_PYTHON_ENV_PATH`；有则补 `idf_tools.py install-python-env`（[troubleshooting-at-adf.md](troubleshooting-at-adf.md) § Q-AT4）
- 🚩 「PowerShell 里 `cmd /c export.bat` 再 `python build.py build`」→ bat 环境不回传；用 `export.ps1` 或整段 `cmd /c "call export.bat && python build.py build"`
- 🚩 「ConvertTo-Json | Set-Content -Encoding utf8 写 module_info.json」→ UTF-8 BOM → `Expecting value: line 1 column 1`；必须用 `python -c "json.dump(...)"` 无 BOM 写入
- 🚩 「在 Agent Shell 里交互 choose: / 等客户往 Shell 打字」→ 做不到；对话确认后写 `module_info.json`，再非交互 install
- 🚩 「算 PLATFORM_INDEX + stdin 注入」→ Agent 默认禁止；用 JSON 跳过三问
- 🚩 「不读 CSV / 套用其它版本 Module 列表 / 替用户选 Module·silence」→ 读本机 CSV，展示后等客户；再写 JSON
- 🚩 「把『Module: 2. ESP32C5-4MB』当成序号 2」→ `2.` 常是题号；有模块名则按名写入 JSON
- 🚩 「克隆前就问 Module」→ Module / silence 仅 Phase 1.5（clone 之后）
- 🚩 「未给芯片就猜 ESP32 / 只写例如 ESP32C2 / 不贴 1～7 列表」→ **必须**贴出 defaults「向客户展示模板」或等价编号列表，等客户选
- 🚩 「未给版本就默认 master」/「路径先写成 esp-at-master」→ 未指定须用该芯片「推荐的 AT 固件」；**禁止**默认 master；先定芯片与版本再回显路径
- 🚩 「先问版本/父目录、后问芯片」→ 顺序必须：芯片 → 版本 → 父目录
