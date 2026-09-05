# ESP-Mosaico Vibe

[English](README.md) | [中文](README_CN.md)

本仓库是为 ESP-Mosaico 定制的 **Agent 主导（Agent-Led）人机协同开发统一入口**。
它为 Agent 提供统一的工程能力与设备通道。

用户定义目标并验收实物。Agent 作为默认执行主体，持续推进到真机验证。
涉及授权、物理操作或高风险变更时，Agent 请求用户介入。

## 创建工程

以 [`projects/hello_world`](projects/hello_world) 为参考应用，在 `projects/`
目录下为每个新应用创建独立目录。保留 Recovery 固件是固定版本
`esp-mosaico-tools` 子模块的内部资源，只由 `mosaico.py recover` 使用。

组件仓库和其他项目资料通过 Git 子模组提供。只加载或初始化当前任务所需的
子模组。实现功能前，先查看 [`skills/README.md`](skills/README.md)，并按需读取
相关的 `SKILL.md`，无需一次性加载全部资料。

使用 GSP 绘制界面的工程可先在 PC 上预览 480×480 场景，再烧录真机。入口是
[`tools/gsp-sim`](tools/gsp-sim/README.md)，固定使用
**espressif/esp-gsp 1.1.0**（`submodule/esp-gsp`）。
需要同时支持 PC 仿真和真机运行的 GSP Hello World，可从
[`projects/gsp_hello`](projects/gsp_hello) 开始。

## 统一设备命令

日常安装、日志和恢复统一通过仓库根目录的 `mosaico.py` 完成：

```sh
python mosaico.py doctor
python mosaico.py list
python mosaico.py recover
python mosaico.py install --project projects/<project>
python mosaico.py monitor
```

根目录启动器会转发到固定版本的 `submodule/esp-mosaico-tools`，不会把 CLI
安装到当前 Python 环境。[`.mosaico.json`](.mosaico.json) 由主仓库维护，声明
工程、Recovery、BSP、ESP-Iris 和构建工具路径。首次使用先初始化工具子模块：

```sh
git submodule update --init --recursive submodule/esp-mosaico-tools
```

`list` 会连接 Gateway，列出 Device ID、在线状态、连接方式、固件身份、运行模式和
Boot ID，并保留 Gateway 缓存中的离线设备。使用 `list --details` 查看 endpoint、
ESP-IDF 版本、Session ID 和能力列表，或使用 `list --json` 查看完整 Gateway 记录。

CLI 支持 Linux、macOS 原生终端，以及 Windows PowerShell 和 CMD，不依赖 WSL
或 Git Bash。主机 CLI 最低支持 Python 3.8，并使用满足工作区工程约束的
ESP-IDF，以及由 `submodule/esp-mosaico-tools` 递归锁定的 ESP-Iris。Gateway
会按当前激活 Python 的 major/minor 自动准备隔离环境，ESP-Iris 的
`components/esp_iris/tools/requirements.lock` 中的 PEP 508
条件会自动选择兼容依赖。ESP-IDF 6.1 仍要求 Python 3.10 或更新版本；当 CLI
由 Python 3.8/3.9 启动时，会独立寻找兼容解释器并将 ESP-IDF bootstrap 命令
转交给它；不适合自动发现时可用 `MOSAICO_IDF_PYTHON` 显式指定兼容解释器。
首次操作前运行
`doctor`，可检查 Python、ESP-IDF、ESP32-S31 target、ESP-Iris、主机状态目录和
实时 USB 枚举；该命令不会构建或写入固件。

状态文件遵循各平台约定：Linux 使用 `$XDG_STATE_HOME/esp-mosaico`（未设置时为
`~/.local/state/esp-mosaico`），macOS 使用
`~/Library/Application Support/esp-mosaico`，Windows 使用
`%LOCALAPPDATA%\esp-mosaico`。

在每种原生主机上运行相同的兼容性冒烟测试：

```sh
python -m unittest discover -s submodule/esp-mosaico-tools/tests -v
python -m unittest discover -s tests/mosaico_cli -v
python mosaico.py --version
python mosaico.py --json list
python mosaico.py --json doctor
python mosaico.py monitor --timeout 1 --grep __mosaico_host_smoke__
```

`install` 只通过 **ESP-Iris Developer Gateway** 更新普通应用；设备未完成初始化
时会明确提示先运行 `recover`，不会自动切换成底层烧录。`recover` 默认使用仓库
内经过评审的 Recovery 基础包，并在完成后停留于 Recovery 就绪状态。

Recovery 仅支持 Gateway 本机执行。命令先准备完整基础包，再向本地 Gateway
申请目标设备或物理 USB endpoint 的维护租约；ROM 模式或尚未完成 HELLO、但已被
Gateway 打开的 endpoint 也包含在内。只 detach 该 endpoint，其他设备、日志和操作
保持运行。`recover` 不提供远程 `--gateway-profile`。已运行的本地 Gateway 必须报告与
固定子模块一致的 ESP-Iris revision；不一致时只报错，不会终止该 Gateway。

- 编码 Agent 只通过 `mosaico.py` 操作设备。
- 开发者可以同时打开 Gateway Web 工作台，观察同一设备的日志、画面、Job、
  重启及 recovery 进度。
- CLI 与 Web 工作台共享同一个稳定的 Device ID、Boot ID 和结构化操作记录。
- Gateway 独占 USB 会话，并持久化结构化证据与原始日志；执行 OTA 前应先保存
  有效的 core dump。

ESP-Mosaico 只有一个 High-Speed USB 接口。正常固件和 Recovery 都会把该接口
交给 ESP-Iris，因此 Gateway 在两种模式下都独占该会话。

### 最后恢复

当设备无法被正常固件或 Recovery 识别时，仍然只运行 `python mosaico.py recover`。
命令会先保存可获取的故障证据，并在需要物理操作时提示开发者：

1. 将设备关机。
2. 按住位于 USB-C 接口左侧的 **Boot** 键。
3. 保持按住 **Boot** 键并开机。
4. 设备进入 ROM 下载模式后松开 **Boot** 键，并告知 Agent 物理操作已完成。

开发者只需完成上述按键和上电操作。之后由 Agent 继续运行 `recover` 并验证
设备身份、Recovery 版本和就绪状态；后续应用通过 `install` 安装。

手动进入 ROM 下载模式仅用于最后恢复。不要仅为恢复连接而擦除整片 Flash，
也不应在未经用户明确授权时覆盖凭据、设备身份、recovery 数据或相关分区。

## 仓库结构

- `projects/hello_world`：新开发者工程使用的参考应用。
- `projects/gsp_hello`：支持 PC 仿真和真机安装的 GSP Hello World。
- `components/esp_mosaico_app_recovery`：普通应用进入 Recovery 和健康确认支持。
- `submodule/esp-gsp/`：固定的 ESP-GSP 1.1.0（设备预编译库；主机仿真器与 gspc 另行下载）。
- `tools/gsp-sim/`：打包场景并运行独立的 ESP-GSP `sim`。
- `submodule/esp-mosaico-tools/`：固定版本的仓库本地 `mosaico.py` 实现及其
  内置 Recovery 固件和嵌套锁定的 ESP-Iris 固件/主机运行时，无需全局安装 CLI。
- `skills/`：面向 Agent 和开发者的任务集成指南，详见
  [`skills/README.md`](skills/README.md)。
- `docs/`：面向用户的文档。
- `.mosaico.json`：由工具子模块读取的工作区路径和设备配置。
- `.agents/`：面向 Agent 的私有文档和工具，不承载产品 CLI。
- `AGENTS.md`：供编码 Agent 使用的简明路由与操作规则。

仓库的目标、架构、功能契约和适用边界见
[`docs/repository-specification.zh-CN.md`](docs/repository-specification.zh-CN.md)。
