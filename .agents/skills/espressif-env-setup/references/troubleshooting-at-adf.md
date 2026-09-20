# FAQ — ESP-AT / ESP-ADF

> **能力边界：** 仅供本 Skill 已启动的全新安装流程进行有限恢复；不得用于响应独立排障请求，也不得用于修复、迁移或切换已有环境。

> 索引见 [troubleshooting.md](troubleshooting.md)。完整流程见 `esp-at.md` / `esp-adf.md`。

---
## Q-AT1: `build.py build` 成功但 `build/factory/` 无固件 / 为空

**原因**：编译未真正完成、选错模组、或中途失败被忽略。

**处理：**
1. 确认已在 `esp-at` 根目录、且本 session 已对 `esp-at/esp-idf` 执行过 export
2. 重新 `python build.py build`（Linux/macOS：`./build.py build`），检查日志末尾是否有 error
3. 列出并核对产物：
   ```bash
   ls -lh build/factory/   # Windows: dir build\factory\
   ```
4. 若需重选 Platform/Module：删除 `build/module_info.json` 后重走 Phase 1.5 写 JSON，再 `build.py install` / `build`

完整流程见 `esp-at.md`。

---

## Q-AT4: `No module named 'esp_idf_monitor'` / `idf.py` was not spawned within an ESP-IDF shell

**典型日志：** `build.py build` 已打印 `PATH is ...`、`IDF_PYTHON_ENV_PATH is <venv>`，随后仍报缺 `esp_idf_monitor`，最终 `idf.py build failed`。
`<venv>` 在默认工具链下常在 `%USERPROFILE%\.espressif\python_env\...`；**自定义 `IDF_TOOLS_PATH` 时在 `YOUR_TOOLS_PATH\python_env\...`**——禁止写死 `.espressif` 路径。

**原因（按优先级）：**
1. **IDF Python 虚拟环境不完整或损坏**（最常见）——`install-python-env` 未装全 / 中断 / 与当前 IDF 版本不匹配。`build.py` **已在进程内**跑过 `idf_tools.py export`，再手动 export **通常解决不了**缺包。
2. Agent 误判为「没在同一 session export」，于是只反复 `export.bat` / 错误地用 PowerShell `cmd /c export.bat`（环境进不了父 Shell）。

**处理：**
```powershell
cd YOUR_AT_INSTALL_PATH
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"
# 若客户自定义工具链：必须先设，否则会装到默认 .espressif
# $env:IDF_TOOLS_PATH = "YOUR_TOOLS_PATH"
python esp-idf\tools\idf_tools.py install-python-env
```

自检——**只用日志里的 `IDF_PYTHON_ENV_PATH`（记为 `YOUR_IDF_PYTHON_ENV_PATH`）**，不要假设在 `.espressif` 下：

```powershell
# 若本 session 已 export 过，可直接用环境变量：
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" -c "import esp_idf_monitor; print('ok')"

# 若尚未 export：把下一行换成 build 日志中的完整路径
# & "YOUR_IDF_PYTHON_ENV_PATH\Scripts\python.exe" -c "import esp_idf_monitor; print('ok')"
```

能打印 `ok` 后再 `python build.py build`。仍失败可在 `esp-idf` 目录重跑 `install.bat` / `install.ps1`（自定义路径时同样先设 `IDF_TOOLS_PATH`）。

Linux / macOS：补装前若自定义则先 `export IDF_TOOLS_PATH=...`，再
`python3 esp-idf/tools/idf_tools.py install-python-env`，自检：
`"$IDF_PYTHON_ENV_PATH/bin/python" -c "import esp_idf_monitor; print('ok')"`
（未 export 时把 `$IDF_PYTHON_ENV_PATH` 换成日志中的绝对路径）。

---

## Q-AT2: esp-at 克隆很快，但 `build.py install` 克隆 esp-idf 超时

**原因**：未配置乐鑫 `insteadOf`，或本机残留旧极狐**按仓库** `insteadOf`（比乐鑫总开关更具体，会劫持 `esp-wifi` 等 submodule），或仍只用旧文档的极狐直链。

**处理：**
```bash
# 先按 mirrors.md 清除全部 jihulab url.*（勿只 unset 总开关）
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
```
然后**用 GitHub URL** 重新克隆 AT（或在已有仓库内确保后续 git 走重定向），必要时删除不完整的 `esp-at/esp-idf` 后再执行 `build.py install`。详见 `esp-at.md` / `mirrors.md`。

---

## Q-ADF1: 编译 ADF 示例报找不到组件 / ADF_PATH

**原因**：未设置 `ADF_PATH`，或 export IDF 后 `ADF_PATH` 丢失。

**处理：**
1. `export ADF_PATH=YOUR_ADF_INSTALL_PATH`（Windows：`set ADF_PATH=...`）
2. 再对选定的 `YOUR_IDF_PATH` 执行 export
3. 若 export 后 `ADF_PATH` 为空，重新设置后再 `idf.py build`

完整流程见 `esp-adf.md`。

---

## Q-ADF2: ADF master 下没有可用的 `esp-idf` 目录

**原因**：`master` **无**内置 esp-idf；与 `release/v2.x` / tag v2.x 不同。

**处理：** 使用外部已安装的 ESP-IDF，或按主流程新装一份后再设 `ADF_PATH` + IDF `export` 编译。不要假设 `YOUR_ADF_INSTALL_PATH/esp-idf` 存在。

---

## Q-ADF3: v2.x `git submodule update` 拉 IDF 超时

**原因**：未配置乐鑫 `insteadOf`，submodule 仍走 GitHub；或本机残留旧极狐**按仓库**配置，比乐鑫总开关更优先。

**处理：**
```bash
# 先按 mirrors.md 清除全部 jihulab url.*（勿只 unset 总开关）
git config --global url."https://git.espressif.com.cn/".insteadOf "https://github.com/"
```
然后在 ADF 仓库内重试 `git submodule update --init --recursive`。详见 `esp-adf.md` / `mirrors.md`。
