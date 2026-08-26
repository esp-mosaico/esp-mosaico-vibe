# 共用占位符与门禁（IDF / AT / ADF）

> 各 SDK Phase 0 **只写差异**；下列规则一律按本文件执行，勿在 `esp-idf.md` / `esp-at.md` / `esp-adf.md` 再全文复制。

默认版本数值见 [defaults.md](defaults.md)。

---

## The Iron Law

```
NO "环境装好了" CLAIM UNTIL 验证产物 bin 存在且大小非零
```

| SDK | 验证物 |
|-----|--------|
| ESP-IDF / ESP-ADF | 当前验证工程 `build/` 下的 bootloader、partition table 与应用 bin，均须存在且大小非零 |
| ESP-AT | `build/factory/` 下固件 bin |

在此之前**禁止**说「装好了 / 应该没问题了」。

### 安装完成收尾 — 乐鑫镜像提示（条件）

<a id="post-install-espressif-mirror-notice"></a>

验证产物已通过、准备向客户报告 Done 时：

```
本次搭建是否配置了乐鑫官方 Git 镜像？
（即执行过：git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"）
├─ 否（直连模式）→ 不必提镜像收尾
└─ 是 → 报告成功后**必须加一句**告知客户（见下方模板）
```

**须告知：** 该 `insteadOf` 是**全局**的，之后凡 `https://github.com/` 的 clone / submodule 都会被重定向到 `git.espressif.com.cn`。乐鑫镜像**未收录**的非乐鑫仓库可能失败、变慢或拉错源。

**向客户输出的提示模板（可原样或略改）：**

```
说明：本次安装配置了乐鑫官方 Git 镜像（将 github.com 重定向到 git.espressif.com.cn）。
这会影响之后克隆非乐鑫官方仓库的工程。若不需要该镜像，可清除重定向：

  git config --global --unset url."https://git.espressif.com.cn/".insteadOf

清除命令见 mirrors.md「直连模式」。
```

命令权威出处：[mirrors.md § 直连模式](mirrors.md#direct-mode)。

---

## 父目录 vs 仓库根目录

客户给出的通常是**父目录**（如 `E:\esp`），**不是**仓库根目录。

```
YOUR_*_INSTALL_PATH = <父目录> + 分隔符 + <按版本命名的文件夹>
```

| 客户输入 | SDK + 版本 | 实际仓库路径（示例） |
|----------|------------|----------------------|
| （客户确认采用推荐默认） | IDF `v6.0.2` | Windows `C:\esp\esp-idf-v6.0.2`；Linux/macOS `~/esp/esp-idf-v6.0.2` |
| `E:\esp` | IDF `v6.0.2` | `E:\esp\esp-idf-v6.0.2` |
| `E:\esp` | AT `v4.1.1.0` | `E:\esp\esp-at-v4.1.1.0` |
| `E:\esp` | ADF `release/v2.x` | `E:\esp\esp-adf-release-v2.x` |

### 未提供父目录时（强制）

```
客户是否已给出父目录？
├─ 是 → 按命名规则拼完整路径 → 回显 → 等客户确认 → 再 clone / install
└─ 否 → 必须主动询问
         → 推荐共用默认父目录（Windows `C:\esp`；Linux/macOS `~/esp`）
         → 说明：该默认父目录同时用于 ESP-IDF / ESP-AT / ESP-ADF
           （其下再按命名规则分子目录，如 esp-idf-… / esp-at-… / esp-adf-…）
         → 同时回显将拼出的完整仓库路径
         → 等客户明确确认（接受推荐或给出其它父目录）后，才进入后续步骤
         → 禁止静默采用默认、禁止未确认就 clone
```

推荐默认父目录数值见 [defaults.md § 当前默认值](defaults.md#当前默认值其它项)（IDF / AT / ADF **同一**默认父目录）。

**禁止** `git clone ... E:\esp` 把仓库散落到父目录。Phase 0 **回显完整路径**并取得客户确认后再克隆。

### 命名规则（文件夹名）

| 版本类型 | 分支 / tag 示例 | 目录名示例 |
|----------|-----------------|------------|
| tag | `v6.0.2` / `v4.1.1.0` | `esp-idf-v6.0.2` / `esp-at-v4.1.1.0` |
| master | `master` | `esp-idf-master` / `esp-at-master` / `esp-adf-master` |
| release | `release/v6.0` / `release/v2.x` | `esp-idf-release-v6.0` / `esp-adf-release-v2.x` |

前缀按 SDK：`esp-idf-` / `esp-at-` / `esp-adf-`。

---

## 路径要求

凡客户自定义的安装 / 工具链 / 工程路径：建议**全英文**，**不要**空格、中文、括号、`#` 等。违规时 Phase 0 **停下来建议改路径**，勿硬装。

---

## `YOUR_TOOLS_PATH` / `IDF_TOOLS_PATH`

默认工具链安装位置（乐鑫官方默认，IDF / AT / ADF **共用**）：

| 平台 | 默认路径 |
|------|----------|
| Windows | `%USERPROFILE%\.espressif`（如 `C:\Users\<用户名>\.espressif`） |
| Linux / macOS | `~/.espressif` |

### Phase 0 必须主动询问（IDF / AT / ADF 相同）

```
客户是否已给出工具链路径？
├─ 是 → 记为 YOUR_TOOLS_PATH（须满足路径要求）→ 回显 → 等确认
│       → 之后每次 install / export 前先设 IDF_TOOLS_PATH=YOUR_TOOLS_PATH
└─ 否 → 必须主动询问
         → 告知将使用上表默认路径（并写出本机对应的完整默认路径）
         → 说明：不指定则工具链装到该默认目录；指定则装到自定义目录
         → 等客户明确确认（接受默认或给出其它路径）后，才进入 install
         → 禁止静默跳过不问；禁止未确认就跑 install.sh / install.bat / install.ps1
```

确认后的环境变量规则：

- 客户**确认使用默认** → **不要设置** `IDF_TOOLS_PATH`（让脚本走默认目录）。
- 客户**指定了** `YOUR_TOOLS_PATH` → 每次 `install` / `export` 前都须**先**设 `IDF_TOOLS_PATH`，再跑脚本。`export` 不记住上次 session。

---

## 环境搭建与串口

**环境搭建不需要串口。** Phase 0 及后续步骤都不得询问 `PORT` 或执行 flash / monitor；相关请求应退出本 Skill 并重新 triage。

---

## Windows 终端

先按 [esp-idf-windows.md](esp-idf-windows.md) 第一步选定 CMD 或 PowerShell，**全程固定**，不得中途切换。

---

## 占位符纪律

所有 `YOUR_*` 必须换成实际值后再写进命令；**禁止**把占位符当字面路径执行。
