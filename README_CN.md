# ESP-Mosaico Vibe

[English](README.md) | [中文](README_CN.md)

[![CI](https://github.com/esp-mosaico/esp-mosaico-vibe/actions/workflows/ci.yml/badge.svg)](https://github.com/esp-mosaico/esp-mosaico-vibe/actions/workflows/ci.yml)

本仓库是为 ESP-Mosaico 定制的 **Agent 主导（Agent-Led）人机协同开发统一入口**。
它为 Agent 提供统一的工程能力与设备通道。

用户定义目标并验收实物。Agent 作为默认执行主体，持续推进到真机验证。
涉及授权、物理操作或高风险变更时，Agent 请求用户介入。

## 创建工程

以 [`projects/hello_world`](projects/hello_world) 为参考应用，在 `projects/`
目录下为每个新应用创建独立目录。保留 Recovery 固件是固定版本
`esp-mosaico-utils` 子模块的内部资源，只由 `mosaico.py recover` 使用。

组件仓库和其他项目资料通过 Git 子模组提供。只加载或初始化当前任务所需的
子模组。实现功能前，先查看 [`skills/README.md`](skills/README.md)，并按需读取
相关的 `SKILL.md`，无需一次性加载全部资料。

使用 GSP 绘制界面的工程可先在 PC 上预览 480×480 场景，再烧录真机。入口是
[`tools/gsp-sim`](tools/gsp-sim/README.md)，固定使用
**espressif/esp-gsp 1.2.0**（ESP 组件仓库远程依赖）。
需要同时支持 PC 仿真和真机运行的 GSP Hello World，可从
[`projects/gsp_hello`](projects/gsp_hello) 开始。

## 统一设备命令

日常安装、系统更新、日志和恢复统一通过仓库根目录的 `mosaico.py` 完成：

```sh
python mosaico.py doctor
python mosaico.py list
python mosaico.py recover
python mosaico.py install --project projects/<project>
python mosaico.py system-update --project projects/<project>
python mosaico.py monitor
```

根目录启动器会转发到固定版本的 `submodule/esp-mosaico-utils/esp-mosaico-recovery`，不会把 CLI
安装到当前 Python 环境。[`.mosaico.json`](.mosaico.json) 由主仓库维护，声明
工程、Recovery、BSP、ESP-Iris 和构建工具路径。首次使用先初始化工具子模块：

```sh
git submodule update --init submodule/esp-mosaico-utils
```

`list` 会连接 Gateway，列出由出厂 eFuse Base MAC 派生的 Device ID、原始硬件
MAC、在线状态、连接方式、固件身份、运行模式和 Boot ID，并保留 Gateway 缓存中的离线设备。使用 `list --details` 查看 endpoint、
ESP-IDF 版本、Session ID 和能力列表，或使用 `list --json` 查看完整 Gateway 记录。

从旧版 ESP-Iris 升级时，每台设备会从原先保存在 NVS 中的随机 Device ID 一次性
切换为硬件派生 Device ID。请用 `python mosaico.py list` 刷新保存的选择器；旧操作
历史仍保留在离线的旧 ID 下。保留 Recovery 与普通应用应一起升级，确保两种模式
采用相同的身份规则。

CLI 支持 Linux、macOS 原生终端，以及 Windows PowerShell 和 CMD，不依赖 WSL
或 Git Bash。主机 CLI 最低支持 Python 3.8，并使用满足工作区工程约束的
ESP-IDF，以及由 `submodule/esp-mosaico-utils` 与 Recovery 并列锁定的 ESP-Iris。Gateway
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

安装 CI 测试依赖，并在本机运行可重复的主机检查：

```sh
python -m pip install -r requirements-ci.txt
python -m pytest -q submodule/esp-mosaico-utils/esp-mosaico-recovery/tests
python -m pytest -q tests --ignore=tests/firmware
python mosaico.py --version
```

Recovery HTTP 授权主机测试只支持 POSIX；Windows 运行 Tools 测试时需添加
`--ignore=submodule/esp-mosaico-utils/esp-mosaico-recovery/tests/test_http_update_authorization_host.py`。
CI 还会在 Windows 上取消选择 4 项 fixture 硬编码 POSIX 路径显示的 Tools 测试；
这些契约仍会在 Linux 和 macOS 上执行。依赖设备和主机环境的冒烟检查仍由开发者
按需运行：

```sh
python mosaico.py --json list
python mosaico.py --json doctor
python mosaico.py monitor --timeout 1 --grep __mosaico_host_smoke__
```

## 持续集成

[GitHub Actions 工作流](.github/workflows/ci.yml)在 Pull Request、`main` 分支
push 和手动触发时运行。主机矩阵在原生 Linux、macOS、Windows 上测试 Python
3.8 和 3.12；固件矩阵在 GitHub 托管的 `ubuntu-22.04` runner 上构建
`hello_world`、`gsp_hello`、ESP-Iris 验收固件和保留 Recovery。每个固件任务
先递归检出 6.2 开发线上的固定 ESP-IDF revision
`7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`，并用 ESP-IDF 官方 `install.sh`
安装工具链，再执行 low-noise 环境检查和构建。GSP 任务还会编译 PC bridge，并
渲染一张 480×480 的无界面帧。测试报告、构建日志和成功生成的固件产物保留 14 天。

固件 CI 不再需要自托管 runner 或仓库 secret。GitHub 为每个矩阵任务提供全新的
托管虚拟机，工作流在其中安装固定的 ESP-IDF revision、CMake、Ninja、C 工具链和
ESP32-S31 preview 支持。runner 只需能够访问 GitHub、ESP Component Registry
和乐鑫下载站。

CI 不发现、烧录或控制真机，也不发布正式 Release。首次流水线成功后，在 `main`
分支保护规则中把 `CI / required` 配置为必选检查。

`install` 只通过 **ESP-Iris Developer Gateway** 更新普通应用；设备未完成初始化
时会明确提示先运行 `recover`，不会自动切换成底层烧录。`recover` 默认使用仓库
内经过评审的 Recovery 基础包，并在完成后停留于 Recovery 就绪状态。

`system-update` 用于应用之外还需同步更新系统内容的场景。指定 `--project` 时，
命令会构建一个 `.irisfw` 完整更新包，其中包含普通应用、bootloader、分区表，
以及工程声明的可选 `ui_apps` 数据镜像，再通过 Gateway 交给保留的 Recovery
校验、写入并核对更新结果。只修改普通应用时使用 `install`；修改 GSP 场景、字体、
图片、分区布局或 bootloader 时使用 `system-update`。已有完整更新包可通过
`--bundle PATH` 复用；从 HTTP(S) 或 NAND 发起更新的流程及安全限制见
[`Recovery 说明`](submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery/README.md#recovery-从-https-拉取系统更新)。

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
- `tests/firmware/`：仅供集成和验收测试使用的可烧录设备固件。
- `components/esp_mosaico_app_recovery`：普通应用进入 Recovery 和健康确认支持。
- `espressif/esp-gsp==1.2.0`：远程 ESP-GSP 组件（设备预编译库由组件仓库拉取；主机仿真器与 gspc 另行下载）。
- `tools/gsp-sim/`：打包场景并运行独立的 ESP-GSP `sim`。
- `submodule/esp-mosaico-utils/`：固定版本的工具单仓，包含
  `esp-mosaico-recovery` CLI/固件和并列的 `ESP-Iris` 固件/主机运行时，无需全局安装 CLI。
- `skills/`：面向 Agent 和开发者的任务集成指南，详见
  [`skills/README.md`](skills/README.md)。
- `docs/`：面向用户的文档。
- `.mosaico.json`：由工具子模块读取的工作区路径和设备配置。
- `.agents/`：面向 Agent 的私有文档和工具，不承载产品 CLI。
- `AGENTS.md`：供编码 Agent 使用的简明路由与操作规则。

仓库的目标、架构、功能契约和适用边界见
[`docs/repository-specification.zh-CN.md`](docs/repository-specification.zh-CN.md)。
