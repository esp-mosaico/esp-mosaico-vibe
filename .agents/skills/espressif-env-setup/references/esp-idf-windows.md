# Windows 平台安装步骤

共性步骤参见 `references/common.md`；占位符见 `references/esp-idf.md` Phase 0。

---

## 第一步 — 检测终端可用性并选定终端

**必须先完成此步骤，后续所有命令固定在选定的终端中执行，不得中途切换。**

### 检测 PowerShell 是否可用

在开始菜单搜索 PowerShell，打开后执行：
```powershell
Write-Host "PowerShell OK"
```

- 输出 `PowerShell OK` → PowerShell 可用
- 报错、无法打开、或被安全软件拦截 → PowerShell 不可用，**使用 CMD**

### 选定终端

| 情况 | 选定终端 |
|------|----------|
| PowerShell 可用，且用户未指定 | **PowerShell**（默认） |
| PowerShell 可用，用户指定 CMD | **CMD** |
| PowerShell 不可用 | **CMD** |
| PowerShell PATH 缺失 Python/Git，但 CMD 能找到 | **CMD**（PowerShell 的 PATH 不完整，改用 CMD 更可靠） |

> 选定后，本文档后续所有步骤只看对应终端的代码块，另一个终端的代码块可忽略。
>
> **注意：** opencode 等工具启动的 PowerShell 子进程可能继承不完整的 PATH，导致 `python`/`git` 找不到。若 PowerShell 检测失败但 CMD 回退成功，应切换到 CMD 终端，后续所有命令使用 CMD 语法。

## 第二步 — 前提条件检查（Python / Git）

→ **共用文档 [prerequisites.md](prerequisites.md)**（IDF / AT / ADF 相同）。
本文件不再重复安装命令；检测失败时按该文档安装（当前用户：`InstallAllUsers=0` / `/CURRENTUSER`）。

**Python 和 Git 都就绪** → 跳至 `references/common.md` 第一步（pip 镜像配置）

---

## 第三步 — 镜像配置、克隆、submodule

→ `references/common.md` 第一步～第三步

---

## 第四步 — 安装工具链

请根据是否需要自定义工具链安装路径（`IDF_TOOLS_PATH`），选择对应的执行方式：

> **⚠️ 关键提示：下方代码块中的多条命令必须在同一个终端内连续执行，中途请勿关闭或新开终端！**

**情况一：使用默认安装路径（推荐）**
未设置自定义路径时，工具链默认安装到 `%USERPROFILE%\.espressif`。

**当前是 CMD：**
```bat
cd /d YOUR_INSTALL_PATH
set IDF_GITHUB_ASSETS=dl.espressif.com/github_assets
call install.bat all
```

**当前是 PowerShell：**
```powershell
cd YOUR_INSTALL_PATH
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"
.\install.ps1 all
```

**情况二：自定义工具链安装路径**

如果需要自定义安装位置，**必须在运行 install 脚本前手动设置 `IDF_TOOLS_PATH`**；后续每次 export 前也要重新设置（见第五步、第六步）。

**当前是 CMD：**
```bat
cd /d YOUR_INSTALL_PATH
set IDF_GITHUB_ASSETS=dl.espressif.com/github_assets
set IDF_TOOLS_PATH=YOUR_TOOLS_PATH
call install.bat all
```

**当前是 PowerShell：**
```powershell
cd YOUR_INSTALL_PATH
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"
$env:IDF_TOOLS_PATH = "YOUR_TOOLS_PATH"
.\install.ps1 all
```

> **参数说明：**
> - 默认指令使用 `all` 安装所有芯片的工具链。
> - 如需节省空间仅安装特定芯片，可将 `all` 替换为具体芯片名（如 `esp32`），多个芯片用逗号分隔（如 `esp32,esp32s2`）。
>
> 首次运行耗时 5～15 分钟，请耐心等待。
> 备用镜像：`dl.espressif.cn/github_assets`（中国大陆优化节点）
> 失败时 → 参见 [troubleshooting-toolchain.md](troubleshooting-toolchain.md) § Q4

> **⚠️ 安装完成后不要关闭当前终端**，直接继续执行第五步激活环境变量。

---

## 第五步 — 激活环境变量

每次打开新终端，都必须执行环境变量激活脚本。若使用了自定义工具链路径，**必须先手动设置 `IDF_TOOLS_PATH`，再执行 export 脚本**——顺序不能颠倒，`export.bat` / `export.ps1` 不会自动沿用 install 时或上次 session 的值。

> **⚠️ 关键提示：下方代码块中的多条命令必须在同一个终端内连续执行，中途请勿关闭或新开终端！**

**情况一：使用默认安装路径**

**CMD：**
```bat
cd /d YOUR_INSTALL_PATH
call export.bat
```

> 必须用 `call export.bat`，直接执行 `export.bat` 会导致 shell 退出，环境变量不保留。

**PowerShell：**
```powershell
cd YOUR_INSTALL_PATH
.\export.ps1
```

> 如果 PowerShell 提示执行策略限制，先运行：`Set-ExecutionPolicy RemoteSigned -Scope CurrentUser`

**情况二：使用了自定义工具链安装路径**

**CMD：**
```bat
set IDF_TOOLS_PATH=YOUR_TOOLS_PATH
cd /d YOUR_INSTALL_PATH
call export.bat
```

**PowerShell：**
```powershell
$env:IDF_TOOLS_PATH = "YOUR_TOOLS_PATH"
cd YOUR_INSTALL_PATH
.\export.ps1
```

环境变量**仅在当前 session 有效**，每次打开新终端都需要重新执行此步骤。

---

## 第六步 — 编译与验证

> **⚠️ 关键提示：下方代码块中的多条命令必须在同一个终端内连续执行，中途请勿关闭或新开终端！**
> 若用户指定了 `YOUR_TOOLS_PATH`，须选用下方 **情况二**（export 前手动 `set IDF_TOOLS_PATH`）；未指定自定义路径时用 **情况一**，不要设置 `IDF_TOOLS_PATH`。

**① 激活并编译：**

情况一：默认路径

CMD：
```bat
cd /d YOUR_INSTALL_PATH
call export.bat
idf.py -C YOUR_PROJECT_PATH set-target YOUR_TARGET_CHIP
idf.py -C YOUR_PROJECT_PATH build
```

PowerShell：
```powershell
cd YOUR_INSTALL_PATH
.\export.ps1
idf.py -C YOUR_PROJECT_PATH set-target YOUR_TARGET_CHIP
idf.py -C YOUR_PROJECT_PATH build
```

情况二：自定义路径（export 前必须先设 `IDF_TOOLS_PATH`）

CMD：
```bat
set IDF_TOOLS_PATH=YOUR_TOOLS_PATH
cd /d YOUR_INSTALL_PATH
call export.bat
idf.py -C YOUR_PROJECT_PATH set-target YOUR_TARGET_CHIP
idf.py -C YOUR_PROJECT_PATH build
```

PowerShell：
```powershell
$env:IDF_TOOLS_PATH = "YOUR_TOOLS_PATH"
cd YOUR_INSTALL_PATH
.\export.ps1
idf.py -C YOUR_PROJECT_PATH set-target YOUR_TARGET_CHIP
idf.py -C YOUR_PROJECT_PATH build
```

> **新芯片 / preview target：** 部分尚未正式量产的芯片（如 ESP32-S31 等）直接 `idf.py set-target` 可能失败。若报错提示使用 `--preview`，按提示改用：
> `idf.py -C YOUR_PROJECT_PATH --preview set-target YOUR_TARGET_CHIP`
> 详见 [troubleshooting-build.md § Q7](troubleshooting-build.md#q7-idfpy-set-target-失败unknown-target--需-preview)。**禁止**在无报错提示时自行乱加 `--preview`。

如需捕获编译日志（CMD，默认路径示例）：
```bat
cd /d YOUR_INSTALL_PATH
call export.bat
idf.py -C YOUR_PROJECT_PATH build > %TEMP%\idf_build.log 2>&1
type %TEMP%\idf_build.log
```

**② 编译成功后验证：**

直接枚举当前验证工程 `build/` 下的 `.bin`，确认至少包含 bootloader、partition table 和应用固件，且文件大小均非零。不输出或执行任何烧录命令。

CMD：
```bat
for /r YOUR_PROJECT_PATH\build %F in (*.bin) do @if %~zF GTR 0 echo %~zF %F
```

PowerShell：
```powershell
Get-ChildItem YOUR_PROJECT_PATH\build -Recurse -Filter *.bin |
    Where-Object Length -gt 0 | Select-Object FullName, Length
```

**所需 bin 均存在且大小非零** → 报告成功。
**有文件缺失或为零字节** → 查看编译日志，参见 [troubleshooting-build.md](troubleshooting-build.md) § Q5
若本次配置了乐鑫官方镜像 → 报告成功后须加镜像影响与清除提示（[placeholders.md § 安装完成收尾](placeholders.md#post-install-espressif-mirror-notice)）。

**③ 可选 — VS Code ESP-IDF 扩展：**

用户要用扩展或主动询问时，按 `references/vscode-extension.md` 输出三个环境变量及 `idf.customExtraVars` 配置。

失败时 → [troubleshooting-toolchain.md](troubleshooting-toolchain.md) § Q4、Q6、Q11、Q15；[troubleshooting-build.md](troubleshooting-build.md) § Q5、Q7、Q13、Q14

> 编译明显偏慢：先将 IDF / 工具链路径加入安全软件白名单；仍慢再查 Microsoft PC Manager Service，见 **Q14**。
> 若出现 `tool xtensa-esp-elf ... reported version unknown`：可能与绿盾等加密/管控软件有关，先请客户确认，见 **Q15**。
