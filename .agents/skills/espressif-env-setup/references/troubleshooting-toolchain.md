# FAQ — 工具链 / 环境变量 / export

> **能力边界：** 仅供本 Skill 已启动的全新安装流程进行有限恢复；不得用于响应独立排障请求，也不得用于修复、迁移或切换已有环境。

> 索引见 [troubleshooting.md](troubleshooting.md)。

---
## Q4: 工具链安装失败

**检查一：Python 版本不足**
```bash
python3 --version   # Linux/macOS
python --version    # Windows
```
需 3.10+。若系统 PATH 上有旧版本排在前面，重新安装并确保 PATH 优先级正确。

**检查二：pip SSL 证书错误**

报错包含 `SSLError: certificate verify failed`：
```bash
python3 -m pip config set global.trusted-host mirrors.aliyun.com
```

macOS 专属：运行 Python 目录下的 `Install Certificates.command`。

**检查三：工具链校验失败（checksum mismatch）**

缓存文件损坏，删除后重试：

Linux / macOS：
```bash
rm -rf ~/.espressif/dist
```

Windows CMD：
```bat
rmdir /s /q %USERPROFILE%\.espressif\dist
```

Windows PowerShell：
```powershell
Remove-Item -Recurse -Force "$env:USERPROFILE\.espressif\dist"
```

**检查四：工具链从 github.com 下载，速度极慢**

原因：`IDF_GITHUB_ASSETS` 未在运行安装脚本的同一 session 中设置。

Linux / macOS：
```bash
export IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"
./install.sh
```

Windows CMD：
```bat
cd /d YOUR_INSTALL_PATH
set IDF_GITHUB_ASSETS=dl.espressif.com/github_assets
call install.bat
```

Windows PowerShell：
```powershell
cd YOUR_INSTALL_PATH
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"
.\install.ps1
```

---

---

## Q6: CMake 报错 "IDF_PATH is not set"

**原因（Windows CMD）**：`export.bat` 未用 `call` 调用，shell 执行完 bat 后直接退出。

```bat
call export.bat    ✅
export.bat         ❌
```

**原因（Windows PowerShell）**：执行策略限制导致 `export.ps1` 未能运行。

```powershell
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
.\export.ps1
```

Linux / macOS 不存在此问题，`. YOUR_INSTALL_PATH/export.sh` 即可。

---

---

## Q8: macOS SSL 证书错误

报错：`[SSL: CERTIFICATE_VERIFY_FAILED] certificate verify failed`

```bash
open /Applications/Python\ 3.x/Install\ Certificates.command
```

或手动运行：
```bash
/Applications/Python\ 3.x/python3 -m pip install --upgrade certifi
```

---

---

## Q10: Apple Silicon 工具链报 bad CPU type

需要安装 Rosetta 2：
```bash
/usr/sbin/softwareupdate --install-rosetta --agree-to-license
```

---

---

## Q11: Windows 环境变量常见陷阱

| 问题 | 现象 | 修复 |
|------|------|------|
| `export.bat` 不加 `call` | shell 执行完 bat 后直接退出 | 改为 `call export.bat` |
| PowerShell 设置变量后调用 cmd.exe | 工具链走 GitHub 下载极慢 | 把 `set` 写在 `cmd.exe /c "..."` 内部 |
| 安全软件限制 PowerShell | 脚本无法执行 | 改用 CMD + `install.bat` 方案 |
| Python/Git 安装后找不到命令 | PATH 未刷新 | PowerShell 刷新 `$env:Path`；CMD 重启终端 |
| 日志重定向路径目录不存在 | 日志为空 | 改用 `%TEMP%` 或 `"$env:TEMP\..."` |

---

---

## Q12: export.sh 执行后环境变量未生效（Linux / macOS）

**报错**：`idf.py: command not found` 或 `IDF_PATH is not set`，即使刚刚运行了 export.sh。

**原因**：用 `./export.sh` 执行脚本，脚本在子 shell 中运行，环境变量不会传回当前 session。

```bash
./export.sh          ❌ 子 shell 执行，变量不传回
. YOUR_INSTALL_PATH/export.sh   ✅ source 执行，变量在当前 session 生效
source YOUR_INSTALL_PATH/export.sh  ✅ 等价写法
```

> `. ` 和路径之间必须有空格，漏掉空格会报 `No such file or directory`。

---

## Q12b: 自定义工具链路径但 export 后找不到工具链

**报错**：`xtensa-esp32-elf-gcc: not found`、`WARNING: tool xtensa-esp32-elf has no installed versions`，或 `IDF_TOOLS_PATH` 指向默认 `~/.espressif` 而非自定义目录。

**原因**：使用了自定义 `YOUR_TOOLS_PATH`，但执行 `export.sh` / `export.bat` / `export.ps1` 前**未手动设置** `IDF_TOOLS_PATH`。install 时设过一次不够——每个新终端、每次 export 前都要重新设置。

**修复**：先设 `IDF_TOOLS_PATH`，再 source / call export 脚本：

Linux / macOS：
```bash
export IDF_TOOLS_PATH="YOUR_TOOLS_PATH"
. YOUR_INSTALL_PATH/export.sh
```

Windows CMD：
```bat
set IDF_TOOLS_PATH=YOUR_TOOLS_PATH
cd /d YOUR_INSTALL_PATH
call export.bat
```

Windows PowerShell：
```powershell
$env:IDF_TOOLS_PATH = "YOUR_TOOLS_PATH"
cd YOUR_INSTALL_PATH
.\export.ps1
```

用户未指定自定义路径时，**不要设置** `IDF_TOOLS_PATH`，使用默认 `~/.espressif` / `%USERPROFILE%\.espressif` 即可。

---

---

## Q15: Windows 报 `tool xtensa-esp-elf version ... is installed, but has reported version unknown`

**典型日志：**
```
WARNING: tool xtensa-esp-elf version esp-13.2.0_20240530 is installed, but has reported version unknown
```
（也可能是其它 `xtensa-*` / `riscv32-*` 工具链出现同类 WARNING。）

**可能原因：** 工具链可执行文件被本机**加密 / 管控软件**拦截或改写版本探测结果。企业环境常见如**绿盾**等终端安全软件；杀毒/EDR 实时扫描也可能干扰。

**处理：**
1. **向客户确认**：本机是否安装了绿盾、其它磁盘加密 / 文档透明加密、或强管控杀毒软件。
2. 若有：请客户将 ESP-IDF 安装目录、工具链目录（默认 `%USERPROFILE%\.espressif` 或自定义 `IDF_TOOLS_PATH`）、以及工程 `build` 目录加入排除 / 白名单；或临时关闭相关实时防护后，在同一终端重新 `export` 再编译。
3. 排除后仍异常：删除对应工具目录后，设好 `IDF_GITHUB_ASSETS`，在 IDF 根目录重跑 `install.bat` / `install.ps1`，再 `export` 验证。
4. 与 Q14（编译慢 / 杀毒扫描）可一并排查。

> Agent 遇到此 WARNING 时：**先询问客户是否有绿盾类加密软件**，不要直接当成普通工具链损坏反复重装。
