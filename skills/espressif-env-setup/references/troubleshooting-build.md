# FAQ — 编译 / 目标芯片 / 平台杂项

> **能力边界：** 仅供本 Skill 已启动的全新安装流程进行有限恢复；不得用于响应独立排障请求，也不得用于修复、迁移或切换已有环境。

> 索引见 [troubleshooting.md](troubleshooting.md)。

---
## Q5: idf.py build 完成但 build/ 下没有 .bin 文件

**原因**：`idf.py` 和 `export` 脚本不在同一 shell session 中运行，`IDF_PATH` 未设置。

Linux / macOS：
```bash
. YOUR_INSTALL_PATH/export.sh
idf.py -C YOUR_PROJECT_PATH build
```

Windows CMD：
```bat
cd /d YOUR_INSTALL_PATH
call export.bat
idf.py -C YOUR_PROJECT_PATH build
```

Windows PowerShell：
```powershell
cd YOUR_INSTALL_PATH
.\export.ps1
idf.py -C YOUR_PROJECT_PATH build
```

---

<a id="q7-idfpy-set-target-失败unknown-target--需-preview"></a>
## Q7: idf.py set-target 失败（unknown target / 需 --preview）

按报错分流，**不要**一上来换芯片名或乱加参数。

### A) `unknown target …`（当前 IDF 版本根本不认识该芯片）

**示例：** `unknown target esp32c5`
**原因：** 该芯片需更高 IDF 版本才支持（如 **ESP32-C5 从 ESP-IDF v5.5.2 起正式量产支持**；更早的 preview/实验支持不等于正式量产线）。
**处理：** 换到支持该芯片的 IDF 版本（对照 [COMPATIBILITY_CN.md](https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY_CN.md) / [defaults.md](defaults.md)），勿只改 `set-target` 参数。C5 正式量产请用 **≥ v5.5.2**。

### B) 报错提示使用 `--preview`（芯片已支持，但属 preview / 未正式量产）

部分较新、尚未正式量产的芯片（如 **ESP32-S31** 等）在当前分支已可用，但默认 `set-target` 会拒绝，并提示改用 preview。

**处理：** **按指令报错原文**改用：

```bash
idf.py -C YOUR_PROJECT_PATH --preview set-target YOUR_TARGET_CHIP
```

Windows 同理（先 `export.bat` / `export.ps1`）：

```bat
idf.py -C YOUR_PROJECT_PATH --preview set-target YOUR_TARGET_CHIP
```

然后照常 `idf.py -C YOUR_PROJECT_PATH build`。

> **禁止**在报错未提及 `--preview` 时自行添加；**禁止**把用户芯片名改成别的型号来「绕过」。

---

## Q13: Windows 编译报错路径过长（path too long）

**报错示例**：
```
FileNotFoundError: [Errno 2] No such file or directory: '...\components\...'
CMake Error: The source directory path is too long
```

**原因**：Windows 默认路径长度限制为 260 个字符，工程路径较长时触发。正常安装流程**不需要**提前开启长路径支持；仅在编译出现上述报错时再按本节处理。

**方案一：开启系统长路径支持（推荐，需管理员权限）**

CMD（管理员）：
```bat
reg add "HKLM\SYSTEM\CurrentControlSet\Control\FileSystem" /v LongPathsEnabled /t REG_DWORD /d 1 /f
```

PowerShell（管理员）：
```powershell
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name LongPathsEnabled -Value 1
```

同时开启 Git 长路径支持：
```bat
git config --global core.longpaths true
```

**修改后需重启系统生效。**

**方案二：缩短路径（无需管理员权限）**

将 ESP-IDF 和工程目录移到路径更短的位置，例如 `C:\esp\` 下，避免多层嵌套目录。

---

## Q14: Windows 编译速度慢

编译慢通常由以下几个原因导致，逐一排查：

### ① 安全软件实时扫描（优先）

编译过程会产生大量临时文件，杀毒 / 安全软件实时扫描会严重拖慢速度。

**方案**：将以下目录加入安全软件**白名单 / 排除项**：
- ESP-IDF 安装目录（`YOUR_INSTALL_PATH`）
- 工具链目录（`YOUR_TOOLS_PATH`，默认 `%USERPROFILE%\.espressif`）
- 工程 build 目录（`YOUR_PROJECT_PATH\build`）

Windows Defender 排除路径设置：`设置 → 隐私和安全性 → Windows 安全中心 → 病毒和威胁防护 → 管理设置 → 排除项`

### ② Microsoft PC Manager（电脑管家）后台服务

若已加白名单后编译仍很慢，检查是否安装了微软官方 **Microsoft PC Manager（电脑管家）**。该应用会在系统注册后台服务 **Microsoft PC Manager Service**，可能持续占用资源拖慢编译。

**如何确认本机是否安装：** 在 PowerShell 中执行：
```powershell
Get-Service | Where-Object { $_.DisplayName -like "*PC Manager*" }
```
- 有输出 → 系统上安装过该应用
- 无输出 → 未安装，可跳过本项

**停止该服务：**
1. `Win + R` → 输入 `services.msc` → 回车
2. 在服务列表中找到 **Microsoft PC Manager Service**
3. 右键 → **停止**（若希望避免开机自启再拖慢编译：右键 → **属性** → 启动类型改为「禁用」或「手动」）

也可以在任务管理器（`Ctrl + Shift + Esc`）查看是否有相关进程占用大量 CPU。

**不想直接停服务时：** 只把 ESP-IDF 相关目录加入 PC Manager 的白名单 / 排除项（类似 Defender 排除文件夹），通常在 PC Manager「防护中心」设置里的「排除项」中配置，可保留防护同时减轻对编译的影响。

### ③ 未启用 ccache

ccache 可以缓存编译结果，二次编译速度大幅提升。ESP-IDF 默认未启用，手动开启：

CMD：
```bat
set IDF_CCACHE_ENABLE=1
idf.py -C YOUR_PROJECT_PATH build
```

PowerShell：
```powershell
$env:IDF_CCACHE_ENABLE = "1"
idf.py -C YOUR_PROJECT_PATH build
```

或在工程的 `sdkconfig` 中永久启用：
```
CONFIG_IDF_BUILD_USE_CCACHE=y
```

### ④ 工程路径在机械硬盘或网络驱动器上

将工程和 ESP-IDF 安装目录移至 SSD，编译速度差异可达数倍。
