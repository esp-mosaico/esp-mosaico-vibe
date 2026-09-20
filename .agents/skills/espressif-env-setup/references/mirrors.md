# 国内镜像地址速查

> **决策流程**：见 [common.md](common.md) § 第二步（以 `git ls-remote` 为主；ping 仅参考）。
> **本 skill 搭建范围**：仅 ESP-IDF / ESP-AT / ESP-ADF。镜像站可能还有其它仓库（如 esp-who、esp-iot-solution），**无对应搭建流程**，勿因镜像收录而套用本 skill。

## Git 镜像配置命令

**统一使用乐鑫官方 Git 镜像**（`https://git.espressif.com.cn/`）。**不再使用极狐（jihulab）**。

本地仓库安装可使用随 Skill 提供的辅助脚本；MCP 不发布脚本文件，必须使用下文「清除全部极狐」的内联命令。

**Agent（Windows）强制：** 一旦 `get-regexp` 发现有 jihulab 残留，使用下文独立的 PowerShell 代码块；若当前在 CMD，先进入 PowerShell。禁止手写嵌套 `-Command`（`$key`/`$_` 会被外层 Shell 吃掉导致 `MissingFileSpecification`）；禁止在 PowerShell 里对 git 使用 CMD 的 `2>nul`（会触发 `Out-File` / `com1` 类错误）。验证用：

```powershell
git config --global --get-regexp "url\..*jihulab"
# 无输出 = 已干净
```

### 清除全部极狐 `insteadOf`（必做，再配乐鑫或直连）

<a id="clear-jihulab-insteadof"></a>

旧版镜像脚本常写入**大量「按仓库」**的重定向，例如：

```
url.https://jihulab.com/esp-mirror/espressif/esp-wifi.insteadof=https://github.com/espressif/esp-wifi
url.https://jihulab.com/esp-mirror/ARMmbed/mbedtls.insteadof=https://github.com/ARMmbed/mbedtls
...
```

下列命令**只清总开关，清不掉上面这些条目**：

```bash
git config --global --unset url."https://jihulab.com/esp-mirror/".insteadOf
```

更具体的 `insteadOf` 会**优先于**乐鑫的 `url.https://git.espressif.com.cn/.insteadOf https://github.com/`，导致 submodule（如 `esp-wifi`）仍走极狐 → 404 / 超时 / 拉错源。

**Agent 强制顺序（省 token；勿无脑连跑清除+二次确认）：**

```
0) 若 git config --global 报 Permission denied → 先 troubleshooting-git.md Q0，修好再继续
1) 检查：git config --global --get-regexp "url\..*jihulab"
   ├─ 无输出 → 已干净，跳过清除与二次确认，继续后续步骤
   └─ 有输出 → 执行下方「清除」命令
2) 仅当步骤 1 有残留并已清除后：再 get-regexp 确认无输出
```

**1) 检查是否残留：**
```bash
git config --global --get-regexp "url\..*jihulab"
```

**2) 仅当有输出时 — 清除全部极狐 url 重定向：**

**Linux / macOS / Git Bash：**
```bash
git config --global --get-regexp '^url\..*jihulab.*\.insteadof$' | while read -r key _; do
  git config --global --unset-all "$key" 2>/dev/null || true
done
# 兼容旧总开关写法
git config --global --unset-all url."https://jihulab.com/esp-mirror/".insteadOf 2>/dev/null || true
```

**Windows PowerShell：**
```powershell
git config --global --get-regexp 'url\..*jihulab' | ForEach-Object {
  $key = ($_ -split '\s+', 2)[0]
  git config --global --unset-all $key 2>$null
}
```

**Windows CMD：** 先进入 PowerShell，再执行上方 PowerShell 代码块；完成后用 `exit` 返回 CMD。
```bat
powershell -NoProfile
```

**3) 仅当执行过清除后 — 再确认：** `git config --global --get-regexp jihulab` 应无匹配。

### 直连模式（清除已有重定向）

<a id="direct-mode"></a>
```bash
# 1) 先按上一节：检查极狐 → 有则清 → 清后再确认（无则跳过）
# 2) 再解除乐鑫重定向
git config --global --unset url."https://git.espressif.com.cn/".insteadOf
```

### 国内镜像模式（IDF / AT / ADF / 其它已收录上层 SDK 相同）

<a id="domestic-mirror-mode"></a>
```bash
# 1) 先按「清除全部极狐 insteadOf」：检查 → 有则清 → 清后再确认（勿只 unset 总开关）
# 2) 将 GitHub 重定向至乐鑫官方镜像（含子模块）
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
```

随后仍用 **GitHub URL** 执行 `git clone`（由 `insteadOf` 自动走乐鑫镜像），例如：
```bash
# 独立 ESP-IDF：不要 --recursive；子模块见 common.md 第三步
git clone -b YOUR_VERSION https://github.com/espressif/esp-idf.git YOUR_INSTALL_PATH
git clone -b YOUR_AT_VERSION --recursive https://github.com/espressif/esp-at.git YOUR_AT_INSTALL_PATH
git clone -b YOUR_ADF_VERSION https://github.com/espressif/esp-adf.git YOUR_ADF_INSTALL_PATH
```

### 速度对比速查（两边都通时）

测前先按「直连模式」清掉全部干扰的 `insteadOf`（含全部极狐条目 + 乐鑫总开关），再分别对下面两个 URL 执行 `git ls-remote ... HEAD`（或 `common.md` 中的 `time` / `Measure-Command` / `curl`），**时间更短的更快**：

| 源 | 测速 URL（勿经 insteadOf） |
|----|---------------------------|
| GitHub 直连 | `https://github.com/espressif/esp-idf.git` |
| 乐鑫镜像 | `https://git.espressif.com.cn/espressif/esp-idf.git` |

- GitHub 更快或接近 → 直连模式
- 乐鑫更快 → 国内镜像模式（配 `insteadOf` 后仍 clone GitHub URL）
完整命令与判定阈值见 `common.md` § 第二步。

---

## 工具链二进制文件（乐鑫 CDN）

在运行安装脚本之前，在**同一 shell session** 中设置：

Linux / macOS（bash/zsh）：
```bash
export IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"
```

Windows CMD：
```bat
set IDF_GITHUB_ASSETS=dl.espressif.com/github_assets
```

Windows PowerShell：
```powershell
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"
```

备用节点（中国大陆优化）：
```
dl.espressif.cn/github_assets
```

---

## pip（Python 包）

阿里云 PyPI 镜像（三平台相同，Windows 用 `python` 替代 `python3`）：
```bash
python3 -m pip config set global.index-url https://mirrors.aliyun.com/pypi/simple
```

备用镜像：
- 清华：`https://pypi.tuna.tsinghua.edu.cn/simple`
- 北京外国语大学：`https://mirrors.bfsu.edu.cn/pypi/web/simple`
- 中科大：`https://pypi.mirrors.ustc.edu.cn/simple`

---

## Python 安装包（Windows）

```
https://registry.npmmirror.com/-/binary/python/3.12.4/python-3.12.4-amd64.exe
```

其他版本浏览：`https://registry.npmmirror.com/-/binary/python/`

## Git 安装包（Windows）

```
https://registry.npmmirror.com/-/binary/git-for-windows/v2.45.2.windows.1/Git-2.45.2-64-bit.exe
```

其他版本浏览：`https://registry.npmmirror.com/-/binary/git-for-windows/`

---

## ESP-IDF 离线压缩包（克隆失败时备用，三平台通用）

```
https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/YOUR_IDF_VERSION/esp-idf-YOUR_IDF_VERSION.zip
```

此压缩包含 `.git` 目录和全部子模块，解压后可直接使用，无需再执行 git 操作。

Linux / macOS 解压：
```bash
cd ~/esp
unzip /tmp/esp-idf-YOUR_IDF_VERSION.zip
```

Windows PowerShell：
```powershell
Expand-Archive "$env:TEMP\esp-idf-YOUR_IDF_VERSION.zip" -DestinationPath C:\esp\ -Force
```
