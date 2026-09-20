# Linux 平台安装步骤

共性步骤参见 `references/common.md`；占位符见 `references/esp-idf.md` Phase 0。

---

## 第一步 — 安装系统依赖

根据发行版选择对应命令：

**Ubuntu / Debian：**
```bash
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

**CentOS 7 / 8：**
```bash
sudo yum -y update && sudo yum install git wget flex bison gperf python3 cmake ninja-build ccache dfu-util libusbx
```

**Arch Linux：**
```bash
sudo pacman -S --needed gcc git make flex bison gperf python cmake ninja ccache dfu-util libusb python-pip
```

> 若发行版不在以上列表，参考其文档找到对应的包管理器命令。
> CMake 最低要求 3.22，若系统版本不足，可通过 `tools/idf_tools.py install cmake` 安装。

验证 Python 版本（需 3.10+）：
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

失败时 → [troubleshooting-toolchain.md](troubleshooting-toolchain.md) § Q4、Q12b；[troubleshooting-build.md](troubleshooting-build.md) § Q5

---

## Linux 特有注意事项

- **Python 版本**：部分发行版默认 `python3` 较旧，确认 `python3 --version` ≥ 3.10
- **虚拟环境冲突**：若系统已有其他 Python 虚拟环境，确保安装时未激活其他 venv
