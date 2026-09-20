# 前提条件：Python / Git（所有 SDK 共用）

ESP-IDF、ESP-AT、ESP-ADF（及后续上层 SDK）**安装前都必须就绪**：

| 依赖 | 最低版本 |
|------|----------|
| Python | **3.10+** |
| Git | **2.x** |

> **适用：** 走 `esp-idf.md` / `esp-at.md` / `esp-adf.md` 任一流程时，Phase 1 第一步均执行本文。
> **Windows：** 须先完成 `references/esp-idf-windows.md` **第一步（终端选定）**，再执行下文 Windows 检测/安装；后续命令固定在选定终端。
> **Linux / macOS：** 系统包装依赖见 `references/esp-idf-linux.md` / `references/esp-idf-macos.md`；装完后仍须用下文「验证」确认 `python3`/`git` 可用。

---

## Windows — 检测与安装

> **检测原则**：Python 和 Git 分开独立检测。每个检测按 PowerShell → 刷新 PATH → CMD 回退 → 重开 CMD 四步逐步尝试，任一步成功即认定可用。检测失败时先刷新 PATH 再重试，重试仍失败才执行安装。
> 检测到 Python 但版本低于 3.10，视同缺失，重新安装。安装 Python 时须带 `PrependPath=1`（加入 PATH）。
> **CMD 回退的意义：** opencode 等工具启动的 PowerShell 子进程 PATH 可能不完整，但 CMD 可能通过注册表继承完整 PATH。若 PowerShell 找不到但 CMD 能找到，后续命令统一使用 CMD 语法，并考虑改选 CMD 终端（见 `esp-idf-windows.md` 第一步）。

### 检测 Python

按以下顺序逐步检测，**任一步成功即认定 Python 已就绪**，跳过后续步骤。

**① PowerShell 直接检测：**
```powershell
python --version
```

**② 若失败，刷新 PATH 后重试：**
```powershell
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
python --version
```

**③ 若仍失败，通过 CMD 回退检测：**
```powershell
cmd /c "python --version"
```

**④ CMD 无法原生刷新 PATH。若 ③ 也失败，关闭当前窗口重新打开 CMD 后重试：**
```bat
:: 重新打开 CMD 后执行
python --version
```

- 输出版本号且 ≥ 3.10 → Python 已就绪，**记录可用的调用方式**（PowerShell 直接 / CMD 回退），继续检测 Git
- 所有方式均失败 → 执行下方「安装 Python」

### 检测 Git

按以下顺序逐步检测，**任一步成功即认定 Git 已就绪**，跳过后续步骤。

**① PowerShell 直接检测：**
```powershell
git --version
```

**② 若失败，刷新 PATH 后重试：**
```powershell
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
git --version
```

**③ 若仍失败，通过 CMD 回退检测：**
```powershell
cmd /c "git --version"
```

**④ 若 ③ 也失败，关闭当前窗口重新打开 CMD 后重试：**
```bat
:: 重新打开 CMD 后执行
git --version
```

- 输出版本号 → Git 已就绪
- 所有方式均失败 → 执行下方「安装 Git」

**Python 和 Git 都就绪** → 返回当前 SDK 流程，继续 `references/common.md`（pip → 网络/镜像 → 克隆 → submodule）。

### 安装 Python

> **为当前用户安装**（`InstallAllUsers=0`），不写入所有用户 / 系统级目录。

**PowerShell：**
```powershell
Invoke-WebRequest -Uri "https://registry.npmmirror.com/-/binary/python/3.12.4/python-3.12.4-amd64.exe" -OutFile "$env:TEMP\python-installer.exe"
Start-Process "$env:TEMP\python-installer.exe" -ArgumentList "/quiet InstallAllUsers=0 PrependPath=1 Include_test=0" -Wait
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
```

**CMD：**
```bat
curl -L -o %TEMP%\python-installer.exe https://registry.npmmirror.com/-/binary/python/3.12.4/python-3.12.4-amd64.exe
start /wait "" %TEMP%\python-installer.exe /quiet InstallAllUsers=0 PrependPath=1 Include_test=0
:: 安装后需重启终端以刷新 PATH
```

验证：
```bat
python --version
```

### 安装 Git

> **为当前用户安装**（`/CURRENTUSER`），不为所有用户安装。

**PowerShell：**
```powershell
Invoke-WebRequest -Uri "https://registry.npmmirror.com/-/binary/git-for-windows/v2.45.2.windows.1/Git-2.45.2-64-bit.exe" -OutFile "$env:TEMP\git-installer.exe"
Start-Process "$env:TEMP\git-installer.exe" -ArgumentList "/VERYSILENT /NORESTART /CURRENTUSER" -Wait
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
```

**CMD：**
```bat
curl -L -o %TEMP%\git-installer.exe https://registry.npmmirror.com/-/binary/git-for-windows/v2.45.2.windows.1/Git-2.45.2-64-bit.exe
start /wait "" "%TEMP%\git-installer.exe" /VERYSILENT /NORESTART /CURRENTUSER
:: 安装后需重启终端以刷新 PATH
```

验证：
```bat
git --version
```

---

## Linux / macOS — 验证

先按平台文件安装系统依赖，再验证：

```bash
python3 --version   # 须 ≥ 3.10
git --version
```

- Linux → `references/esp-idf-linux.md` 第一步
- macOS → `references/esp-idf-macos.md` 第一步

通过后继续 `references/common.md`。
