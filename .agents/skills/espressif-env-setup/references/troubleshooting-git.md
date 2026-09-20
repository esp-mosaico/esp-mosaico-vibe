# FAQ — Git / 克隆 / submodule

> **能力边界：** 仅供本 Skill 已启动的全新安装流程进行有限恢复；不得用于响应独立排障请求，也不得用于修复、迁移或切换已有环境。

> 索引见 [troubleshooting.md](troubleshooting.md)。失败时按题号处理，勿跳步。

---

## Q0: `git config --global` 报 Permission denied（写不了 `.gitconfig`）

**报错示例：**
```
error: could not write config file C:/Users/<user>/.gitconfig: Permission denied
```
（配乐鑫 `insteadOf`、清极狐时常见。）

**原因：** 当前用户对全局配置文件无写权限。常见：文件被标为只读、属主不是当前用户、企业管控/杀毒锁定、需管理员才能改。

**Agent：** 出现本错时 **STOP**，先修权限；**禁止**假装镜像已配好继续 clone。修好后再重跑 `mirrors.md` / `common.md` 第二步。

**处理（Windows，在客户本机、以当前登录用户执行）：**

1. 查看属性：

CMD：
```bat
attrib "%USERPROFILE%\.gitconfig"
icacls "%USERPROFILE%\.gitconfig"
```

PowerShell：
```powershell
Get-Item $env:USERPROFILE\.gitconfig -Force | Format-List FullName, Attributes, IsReadOnly
icacls $env:USERPROFILE\.gitconfig
```

2. 去掉只读并恢复当前用户写权限（多数情况够用）：

CMD：
```bat
attrib -R "%USERPROFILE%\.gitconfig"
icacls "%USERPROFILE%\.gitconfig" /grant "%USERNAME%:(M)"
```

PowerShell：
```powershell
attrib -R $env:USERPROFILE\.gitconfig
icacls $env:USERPROFILE\.gitconfig /grant "${env:USERNAME}:(M)"
```

3. 再试：

CMD：
```bat
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
```

PowerShell：
```powershell
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
```

4. 仍失败 → 请客户：关闭可能占用该文件的程序；或用**资源管理器**对 `C:\Users\<用户>\.gitconfig` → 属性 → 取消「只读」→ 安全选项卡确认本人有「修改」；企业电脑可请 IT 解除对 `.gitconfig` 的写保护。
   **不要**建议 Agent 用 Administrator 乱改系统权限，除非客户明确要求。

5. 临时兜底（仅本仓库，不如 `--global` 覆盖 submodule 全面）：在已 clone 的仓库根目录用 `git config --local ...`，仍须保证子模块拉取也走乐鑫；优先把全局 `.gitconfig` 修好。

**Linux / macOS：**
```bash
ls -la ~/.gitconfig
chmod u+w ~/.gitconfig
# 若属主不对：sudo chown "$(whoami)" ~/.gitconfig
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
```

---

## Q1: 父仓库 git clone 超时或 404（不是子模块）

> **分流：** 已 clone 出 `YOUR_INSTALL_PATH`，失败发生在 `git submodule update` / `components/esp_wifi/lib` / openthread 等 → **去 § Q2**（正式 tag 且子模块仍失败时，用 **Q2 方案四** 离线包）。
> 本节仅针对 **esp-idf / esp-at / esp-adf 父仓** `git clone` 本身失败；父仓失败时的离线包命令见下方，与 Q2 方案四相同。

> **先辨「Agent 误杀」：** 若输出含 `shell tool terminated ... timeout 300000`（或其它短超时）且停在 `Cloning...` → 先按 [common.md § Agent Shell 长任务等待](common.md#agent-shell-long-timeout) 把**当前 Shell 工具**的等待/超时调到 ≥ 2h（Cursor 示例：`block_until_ms` ≥ 7200000）再拉；**不要**把误杀当成 Q1/Q2 的网络已死。

**原因**：目标仓库未收录在乐鑫官方镜像，或本机仍残留旧的极狐 `insteadOf`（含**按仓库**的大量条目）导致仍走极狐；网络中断也会超时。

**处理：**
1. 按 `mirrors.md`「清除全部极狐 insteadOf」清干净，再配乐鑫（**不要只 unset 总开关**）：
```bash
# PowerShell 示例：清全部 jihulab url 重定向
git config --global --get-regexp 'url\..*jihulab' | ForEach-Object {
  $key = ($_ -split '\s+', 2)[0]
  git config --global --unset-all $key 2>$null
}
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
git config --global --get-regexp jihulab   # 应无输出
```
2. 仍用 GitHub URL 重试 clone。
3. 若持续失败 → 用下方离线压缩包方案。

**方案：下载离线压缩包（最稳）**

<a id="offline-idf-zip"></a>

Linux / macOS：
```bash
curl -L -o /tmp/esp-idf.zip https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/YOUR_IDF_VERSION/esp-idf-YOUR_IDF_VERSION.zip
cd ~/esp && unzip /tmp/esp-idf-YOUR_IDF_VERSION.zip
```

Windows CMD（需 Windows 10 1803+ 内置 tar）：
```bat
curl -L -o %TEMP%\esp-idf-YOUR_IDF_VERSION.zip https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/YOUR_IDF_VERSION/esp-idf-YOUR_IDF_VERSION.zip
tar -xf %TEMP%\esp-idf-YOUR_IDF_VERSION.zip -C C:\esp\
```

Windows CMD（兼容旧版系统，使用 PowerShell 解压）：
```bat
curl -L -o %TEMP%\esp-idf-YOUR_IDF_VERSION.zip https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/YOUR_IDF_VERSION/esp-idf-YOUR_IDF_VERSION.zip
powershell -Command "Expand-Archive '%TEMP%\esp-idf-YOUR_IDF_VERSION.zip' -DestinationPath 'C:\esp\' -Force"
```

Windows PowerShell：
```powershell
curl -L -o "$env:TEMP\esp-idf.zip" https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/YOUR_IDF_VERSION/esp-idf-YOUR_IDF_VERSION.zip
Expand-Archive "$env:TEMP\esp-idf.zip" -DestinationPath C:\esp\ -Force
```

> 离线包含 `.git` 和全部子模块，解压后直接使用，无需任何 git 操作。

---

## Q2: 子模块 dirty / 拉取失败 / 更新中断

> **分流：** 独立 IDF 父仓应 **无 `--recursive` clone**；子模块只在 `common.md` 第三步用 `git submodule update --init --recursive -f` 拉。
> wifi lib / openthread 等大仓超时、404、中断 → **本节（Q2）**；**不要**一上来当父仓失败去 Q1。
> 若版本是**正式 tag**且方案一/二仍反复失败 → 用本节 **方案四离线 zip**（命令同 Q1 离线包）。

> **Agent Shell 长任务等待（与 [common.md](common.md#agent-shell-long-timeout) 相同，强制）：**
> 进入本节任意 `submodule update` / `git clone` / `git fetch`（wifi-lib 等）前，先在本会话确认一次当前工具的等待/超时语义，再把等待调到 ≥ 2h（或后台跑完再等）。Cursor 示例：`block_until_ms` ≥ `7200000`。
> `shell tool terminated ... 300000 ms` + 仍在 `Cloning...` → **先加长等待重试**，不要当成已走完 Q2 失败。

**会触发本问的现象（任一即 STOP，按本节修，禁止继续 install/export/编译 ADF）：**
- `git status` 出现 `modified: components/... (new commits, modified content)` / `modified content`
- `git submodule update` **中途失败或被中断**（超时、404、RPC failed、agent/会话 interrupted）
- 报错：`Unable to find current revision in submodule path`、`clone into submodule path ... failed`
- **注意区分：** Agent 短超时误杀（见上）vs Git 真报错（`fatal:` / 404 / RPC failed）

**报错示例**：
```
fatal: Unable to find current revision in submodule path 'components/esp_wifi/lib'
fatal: clone of 'https://github.com/espressif/esp32-wifi-lib.git' into submodule path ... failed
error: RPC failed / early EOF / The requested URL returned error: 404
```

**dirty 示例**（clone/`submodule update` 后常见；**不是**「环境坏了到不可救」，但 Gate 未过）：
```
modified:   components/bootloader/subproject/components/micro-ecc/micro-ecc (new commits, modified content)
modified:   components/bt/controller/lib_esp32 (new commits, modified content)
modified:   components/esp_wifi/lib (modified content)
```

> **路径：** `YOUR_IDF_PATH` = 正在修的那份 esp-idf 根目录。
> **强制：** 下列命令里的子模块路径必须来自**本次** `git status` / 报错行，**禁止**照抄示例里的 `components/esp_wifi/lib`（那只是举例）。
> **WiFi 库名：** 预编译库仓库是 **`esp32-wifi-lib`**（不是 `esp-wifi-lib`）。其它子模块以 `.gitmodules` 的 `url` 为准。
> **`HEAD detached` / `Not currently on any branch`：** tag 安装时正常；只要最后有 `nothing to commit, working tree clean` 即 Gate 通过。

**Agent 强制顺序（不得跳步、不得只反复整仓 `submodule update`）：**
步骤 0（镜像）→ 步骤 A（整仓对齐一次）→ 步骤 B（按 status 逐个路径）→ 方案一 deinit → **方案 2a 按 SHA 浅取**（全量超时后优先）→（可选）2b/方案三 → **方案四仅正式 tag 离线 zip**（AT 的 `release/vX.Y` 慎用）。

### 步骤 0 — 先修镜像（最常见根因）

子模块失败时**优先**怀疑极狐残留 / 未走乐鑫，而不是立刻 `--depth=1`。

1. 按 [mirrors.md § 清除全部极狐](mirrors.md#clear-jihulab-insteadof) 清干净，`git config --global --get-regexp jihulab` 应无输出。
2. 国内模式再确认：
```bash
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
```
3. 确认解析：父仓 origin 为 GitHub URL 时，`esp_wifi/lib` 相对路径应落到
   `https://git.espressif.com.cn/espressif/esp32-wifi-lib`（见该[镜像页](https://git.espressif.com.cn/espressif/esp32-wifi-lib)）。

### 步骤 A — 整仓对齐一次（镜像已确认后）

```bash
cd YOUR_IDF_PATH
git submodule update --init --recursive -f
git status
```

- **中途失败/中断** → 不要空转重跑；记下失败路径，做步骤 0（若未做）→ **方案一**（对该路径）→ 再 `git status`。
- **已干净**（`nothing to commit, working tree clean`）→ Gate 通过，结束 Q2。
- **仍有 `modified: …`** → 进入步骤 B（路径以 status 为准）。

### 步骤 B — 按 `git status` 逐个还原（dirty 首选）

对 status 里**每一行** `modified:` 后面的路径执行（可循环；禁止漏路径、禁止用示例路径替代）：

```bash
cd YOUR_IDF_PATH
git submodule update --init -- PATH_FROM_STATUS
# 例：git submodule update --init -- components/bootloader/subproject/components/micro-ecc/micro-ecc
git status
```

全部干净 → Gate 通过。某一路径仍失败或仍 dirty → 对该路径走**方案一**。

### 方案一：deinit 后重拉（步骤 B 仍失败时；路径换成本次 status/报错中的实际路径）

Linux / macOS：
```bash
cd YOUR_IDF_PATH
git submodule deinit -f components/esp_wifi/lib
git submodule update --init components/esp_wifi/lib
git submodule update --init --recursive -f
git status
```

Windows CMD / PowerShell：
```bat
cd /d YOUR_IDF_PATH
git submodule deinit -f components/esp_wifi/lib
git submodule update --init components/esp_wifi/lib
git submodule update --init --recursive -f
git status
```

将其余失败路径中的示例路径换成 **`git status` / 报错中的实际路径** 即可（如 `components/bt/controller/lib_esp32`）。

> **禁止：** 镜像已对仍反复整仓 `submodule update` 却不按 status 逐路径处理；**禁止** status 列的是 micro-ecc/bt 却只对 `esp_wifi/lib` 做 deinit。

### 方案二：按目标 commit 获取（deinit / 全量 clone 超时仍失败时）

先查父仓钉死的 SHA：

```bash
cd YOUR_IDF_PATH
git ls-tree HEAD components/esp_wifi/lib
# 例：160000 commit fccd7befed72f6e05b2fb6105cd28323daf0d39e  components/esp_wifi/lib
```

> **Agent：** `esp32-wifi-lib` 体积大，全量 `git clone` / `submodule update` 常超过默认 Shell 等待（如 300s）被结束或杀掉——这**不等于**镜像一定坏了。
> **禁止**在短等待下反复全量 clone；改用 **2a（按 SHA 浅取）**，并把当前工具的等待/超时调到 ≥ 2 小时或后台执行（见 [common.md](common.md#agent-shell-long-timeout)）。
> **禁止**用 `submodule update --depth=1` / `git clone --depth=1`（只浅拉默认分支 tip，对不上钉死 SHA → `Unable to find current revision`）。

#### 2a — 按 SHA 浅取（全量超时后的首选）

```powershell
cd YOUR_IDF_PATH
Remove-Item -Recurse -Force components\esp_wifi\lib -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path components\esp_wifi\lib | Out-Null
cd components\esp_wifi\lib
git init
git remote add origin https://github.com/espressif/esp32-wifi-lib.git
# 已配乐鑫 insteadOf 会走镜像；也可直写 https://git.espressif.com.cn/espressif/esp32-wifi-lib.git
git fetch --depth 1 origin REQUIRED_COMMIT_HASH
git checkout FETCH_HEAD
cd ..\..\..
git submodule update --init components/esp_wifi/lib
git status
```

Linux / macOS 等价：`rm -rf` → `mkdir -p` → 同上 `git init` / `fetch --depth 1 origin <SHA>`。
`REQUIRED_COMMIT_HASH` 与路径以本次 `git ls-tree` 为准。干净 → Gate 通过。

#### 2b — 全量 clone 再 checkout（仅网络很快或非短超时时）

```bash
cd YOUR_IDF_PATH
rm -rf components/esp_wifi/lib
git clone https://github.com/espressif/esp32-wifi-lib.git components/esp_wifi/lib
cd components/esp_wifi/lib && git checkout REQUIRED_COMMIT_HASH && cd ../..
git submodule update --init --recursive -f
git status
```

### 方案三：限制深度（仅当确认目标 commit 就是默认分支 tip 时）

```bash
cd YOUR_IDF_PATH
git submodule update --init --depth=1 components/esp_wifi/lib
# 若立刻报 Unable to find current revision → 放弃，改回方案 2a
git submodule update --init --recursive -f
```

### 方案四：离线压缩包（仅正式 tag；AT 分支 IDF 慎用）

适用：独立 IDF 且版本为 **正式 tag**（如 `v5.5.2`），步骤 0～2a 仍失败。

**不适用 / 慎用：** `master` / `release/vX.Y`（AT 内置 IDF 常见）。官方 zip 按 tag 发；**不要**因 wifi-lib 失败就用任意 tag zip 替换 `esp-at/esp-idf`（易与模组 `IDF_VERSION` 不一致）。AT 优先 **2a**（并保证 Shell 等待 ≥ 2h，见 [common.md](common.md#agent-shell-long-timeout)）。

命令与 [§ Q1 离线压缩包](#offline-idf-zip) 相同（把 `YOUR_IDF_VERSION` 换成该 tag）：

Linux / macOS：
```bash
# 若已有半残目录，先移走或删掉失败的 YOUR_INSTALL_PATH，再解压
curl -L -o /tmp/esp-idf-YOUR_IDF_VERSION.zip \
  https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/YOUR_IDF_VERSION/esp-idf-YOUR_IDF_VERSION.zip
cd YOUR_PARENT_DIR   # 与 Phase 0 确认的父目录相同，如 ~/esp 或 /mnt/hdd/esp
unzip /tmp/esp-idf-YOUR_IDF_VERSION.zip
# 解压后目录名一般为 esp-idf-YOUR_IDF_VERSION；令 YOUR_INSTALL_PATH / YOUR_IDF_PATH 指向该目录
cd YOUR_INSTALL_PATH
git status   # 离线包含子模块，通常已干净；若有 modified 再跑第三步 update
```

Windows PowerShell：
```powershell
curl -L -o "$env:TEMP\esp-idf-YOUR_IDF_VERSION.zip" `
  "https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/YOUR_IDF_VERSION/esp-idf-YOUR_IDF_VERSION.zip"
Expand-Archive "$env:TEMP\esp-idf-YOUR_IDF_VERSION.zip" -DestinationPath "YOUR_PARENT_DIR" -Force
# YOUR_INSTALL_PATH = YOUR_PARENT_DIR\esp-idf-YOUR_IDF_VERSION
cd YOUR_INSTALL_PATH
git status
```

> 离线 zip **含 `.git` 与全部子模块**，解压后一般无需再 `submodule update`。
> 解压完成后仍须：`YOUR_IDF_PATH` 通过 Gate（`working tree clean`）→ 再 `install` / `export`。
> **AT：** 仅当离线树与模组 `IDF_VERSION` 要求的 commit/tag **一致**时才可替换 `esp-at/esp-idf`；否则不要用本节随意换版本。
> **ADF：** 新装外部 IDF 且版本为 tag 时可用本节；得到的路径作为 `YOUR_IDF_PATH`。

---

## Q3: 子模块目录为空（尚未跑第三步 / update 未完成）

**原因（本 skill 下属正常中间态）：** 独立 IDF **故意** `git clone` 不加 `--recursive`，子模块要等 `common.md` 第三步才拉。若跳过第三步直接 `install`，也会表现为目录为空。

**处理：** 先按 [common.md § 第三步](common.md#第三步--验证并修复-submodule任意-esp-idf-树均适用) 执行：

Linux / macOS：
```bash
cd YOUR_IDF_PATH
git submodule update --init --recursive -f
git status   # 须干净
```

Windows CMD / PowerShell：
```bat
cd /d YOUR_IDF_PATH
git submodule update --init --recursive -f
git status
```

update 失败/中断 → **§ Q2**（不是 Q1）。
