# macOS 平台安装步骤

共性步骤参见 `references/common.md`；占位符见 `references/esp-idf.md` Phase 0。

---

## 第一步 — 安装系统依赖

### 安装 Xcode Command Line Tools

```bash
xcode-select --install
```

> 若出现 `xcrun: error: invalid active developer path` 错误，必须先执行此步骤。

### 安装 CMake、Ninja、dfu-util

**Homebrew（推荐）：**
```bash
brew install cmake ninja dfu-util ccache
```

**MacPorts：**
```bash
sudo port install cmake ninja dfu-util ccache
```

### 安装 Python 3.10+

macOS 系统自带的 Python 通常为 3.9 或更旧，**不满足** ESP-IDF 最低要求（3.10+），需要单独安装。

**Homebrew：**
```bash
brew install python3
```

**MacPorts：**
```bash
sudo port install python313
```

验证版本：
```bash
python3 --version
```

---

## 第二步 — 镜像配置、克隆、submodule

→ `references/common.md` 第一步～第三步

---

## 第三步 — 工具链安装、激活、编译与验证

→ `references/common.md` 第四步（含 §4.3 编译与验证）

> **自定义工具链路径：** 若用户指定了 `YOUR_TOOLS_PATH`，编译前激活须选用 `common.md` §4.3 **情况二**——先手动 `export IDF_TOOLS_PATH`，再执行 `export.sh`。

> **VS Code 扩展（可选）：** 用户要用扩展或主动询问时，见 `references/vscode-extension.md`。

**SSL 证书错误**（macOS 常见）：若出现 `[SSL: CERTIFICATE_VERIFY_FAILED]`，参见 [troubleshooting-toolchain.md](troubleshooting-toolchain.md) § Q8。

失败时 → [troubleshooting-toolchain.md](troubleshooting-toolchain.md) § Q4、Q8、Q10、Q12b；[troubleshooting-build.md](troubleshooting-build.md) § Q5

---

## macOS 特有注意事项

### Apple Silicon（M1/M2/M3）

部分旧版工具链为 x86_64 架构，需要 Rosetta 2 才能运行：
```bash
/usr/sbin/softwareupdate --install-rosetta --agree-to-license
```

若出现 `bad CPU type in executable` 或 `tool xtensa-esp32-elf has no installed versions`，参见 [troubleshooting-toolchain.md](troubleshooting-toolchain.md) § Q10。

### Python 版本冲突

确认 `python3` 指向 3.10+：
```bash
which python3
python3 --version
```
