# Mosaico 游戏开发入口

[English](game-development.md) | [返回索引](README_CN.md)

游戏优先使用 **Raylib Lite Engine**（Registry 组件 `espressif2022/raylib-lite-engine`，固定 `0.1.1`）。完整参考游戏、
运行时、渲染、资源、行为测试和 Host 仿真由引擎维护；BSP 负责产品板级支持与外设示例。
原生引擎示例使用自己的 Board Manager 适配器，原生构建本身不添加 Iris 或保留 Vibe Mode 布局。

## 阅读入口与版本

本页负责 **Mosaico 中的创建、仿真、构建与安装**。游戏架构和引擎 API 由下表链接的引擎文档维护；skill 只保留执行约束，不再复述完整流程。

组件固定 `0.1.1`，配套工具和下列技术文档固定在 utils 的 `raylib-toolchain.json` 所记录提交。组件不更新也可以使用这些资料；不要改工具缓存来维护文档。

| 你要做什么 | 技术引导 |
| --- | --- |
| 组织模型、视图、反馈与模块入口，声明 `game.sim.json` | [游戏开发指南](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/docs/game-development.CN.md) |
| 选择绘制路径，设计输入、反馈和资源清单 | [可复用设计方法](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/docs/reference-designs.CN.md) |
| 确认线程、生命周期、帧所有权、输入、音频和存档规则 | [公开 API 契约（英文）](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/API.md) |
| 选择公开接口、底层接口或 Raylib 兼容层 | [接口使用边界](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/docs/custom-api.CN.md) |
| 实现 Scene、按钮命中、多指针和焦点 | [Scene/UI 契约](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/docs/scene-ui.CN.md) |
| 配置 Host 输入回放、状态输出和仿真选项 | [Host 参考（英文）](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/host/README.md) |
| 挑选完整参考游戏 | [示例与开发板矩阵（英文）](https://github.com/espressif2022/raylib-lite-engine/blob/040b84e75640c325bca45515a4135f4b1640fd82/examples/README.md) |

上表是架构与接口参考，尚无覆盖全部功能的从零逐步教程。先从本页创建工程，再按所需功能查阅；引擎文档里的独立 CLI 命令在本工作区使用下文 `mosaico.py` 入口。安装和恢复始终遵循[产品 CLI](mosaico-cli_CN.md)。
工具缓存路径可用 `python mosaico.py game prepare --json` 查询；离线时在返回目录读取同名文档和 `include/` 头文件。

## 从空白游戏开始

使用 Python 3.10+、Git 2.28+，初始化 utils 和 BSP；引擎运行时通过 ESP-IDF Component Registry 获取：

```sh
git submodule update --init submodule/esp-mosaico-utils submodule/esp-mosaico-bsp
python mosaico.py game prepare
python mosaico.py game create my_game
python mosaico.py game sim --project projects/my_game
python mosaico.py game sim --project projects/my_game --headless --frames 120
# 首次构建可能需要下文的 Board Manager 配置步骤。
python mosaico.py game build --project projects/my_game
python mosaico.py iris system-update --project projects/my_game
```

默认 `blank` 模板呈现黑色 480×480 RGB565 画布，包含 Host/设备入口、Iris 服务和
保留分区布局，没有示例玩法、GSP 占位画布、atlas、音频或游戏资源分区。固件使用
工作区固定 ESP-IDF；空白或未验证设备先按[更新方式](mosaico-cli_CN.md#选择更新方式)初始化。

| 路径 | 职责 |
| --- | --- |
| `components/app_game/` | 纯 C 模型和游戏状态 |
| `components/app_game_runtime/` | 引擎生命周期/输入适配、绘制与 JSON |
| `main/game_entry.c` | 选择游戏模块 |
| `game.sim.json` | Host 与设备共用的源文件 |

此表是空白模板的落点；参考游戏的模型/视图/反馈分层见上方技术引导。需要继承用户亮度、Wi-Fi、语言等配置时，按[设置复用](vibe-settings_CN.md)在设备适配层读取。

## Registry 运行时与固定版本开发工具

新游戏声明 `espressif2022/raylib-lite-engine: "==0.1.1"`，不带 `override_path`。
IDF 将固件引擎下载到 `managed_components/`。组件包不含 PC Host 仿真器；
`game prepare` 从固定提交的源码归档获取配套 Host 与空白游戏的启动器、Board 配置，
校验 SHA-256 后缓存到工作区运行目录。版本、提交和摘要记录在 utils 的
`mosaico-tools/raylib-toolchain.json`。Host 使用的引擎源码与 Registry 组件源码一致。
首次使用需要联网，后续复用缓存可离线运行。`game create`、`game sim`、`game build`
会自动准备缓存；空白模板的 `game create --dry-run` 不下载。
`game prepare --json` 返回准确目录，可供自动化和 Board Manager 使用。

只有开发引擎时，才在 `.mosaico.json` 中设置 `dependencies.raylib` 指向源码 checkout；
显式路径无效会报错，不会偷偷换用缓存。该字段选择 Host/空白模板工具，Registry 示例和固件依赖不随它切换。
需要编译修改后的引擎时，在应用的 Engine 依赖中显式添加 `override_path`。
普通应用保持精确版本依赖。

旧游戏不会被自动重写。迁移时移除 Engine 的 `override_path`、固定 `==0.1.1`，将
`RAYLIB_LITE_ENGINE_ROOT` 指向准备好的工具目录（优先使用 CLI 传入的环境变量），
并按该路径重新生成 Board Manager 文件。不要为迁移删除源码或清空已有构建；
旧路径阻止重新配置时，改用独立构建目录。

## 从引擎示例开始

`game create/new` 支持 `shooter`、`sky-hop`、`tower-defense`、`last-zone`、
`living-worlds`、`neon-rift-rally` 及 `blank`。完整示例通过 Registry `0.1.1`
下载，携带自己的 `shared/` 启动器和 Board，再复制到用户工程。
首次下载需要固定的 ESP-IDF 环境（`IDF_PATH` 或 `--idf-path`）；已有缓存时，
创建不需要 SDK 或联网。空白和示例模板的 `--dry-run` 都不下载依赖。

单独下载并定位示例，可在期望存放示例的父目录执行上游命令：

```sh
idf.py create-project-from-example "espressif2022/raylib-lite-engine=0.1.1:last_zone_extraction"
```

工作区也提供下载到版本缓存并返回准确目录的入口：

```sh
python mosaico.py game prepare --example last_zone_extraction --json
```

`game build last_zone_extraction --target iris` 及按名称仿真均定位 Registry 示例缓存，
不会转用源码树中的同名示例。修改自己的副本时使用显式工程路径。

```sh
python mosaico.py game create my_platformer --template sky-hop
python mosaico.py game sim --project projects/my_platformer
python mosaico.py game build --project projects/my_platformer --target iris
# 使用构建实际输出的完整更新包路径：
python mosaico.py iris system-update --project projects/my_platformer --bundle /path/to/my_platformer-system-update.irisfw
```

`game build` 默认为 `--target native`，按源工程原样构建。`--target iris` 在工作区
运行目录下生成独立工程，添加产品服务和兼容分区，再输出 System Update 包；也接受
引擎游戏名，如 `game build sky_hop --target iris`。构建不安装固件，原生与 Iris
产物分别存放。适配工程引用 `main/` 和可选的 `components/`，不采用源工程顶层
分区表或自定义顶层 CMake 逻辑；资源应在组件 CMake 中声明。详见[集成契约](https://github.com/esp-mosaico/esp-mosaico-utils/blob/587005cee2db759deb357ec24cf92ece667a25a2/mosaico-tools/docs/application-integration.md#user-owned-raylib-lite-games)。

所有创建入口支持 `--dry-run`、拒绝覆盖已有目标，并初始化独立 Git 仓库和质量
配置，见[工程创建](project-init_CN.md)。

## 生成 Board Manager 配置

当前引擎要求 `components/gen_bmgr_codes`，工作区游戏构建器尚不自动生成。
新工程首次构建会解析托管依赖，随后可能报 `gen_bmgr_codes: unknown name`。
激活固定版本的 ESP-IDF 环境，再显式加载 Board Manager 扩展：

```sh
# 在工作区根目录执行，先由首次构建完成依赖解析。
source /path/to/pinned-esp-idf/export.sh
mosaico_workspace="$PWD"
# 使用 game prepare 输出的工具目录，不再使用引擎子模块。
mosaico_raylib_tools="/path/printed/by/game/prepare"
mosaico_game_project="$mosaico_workspace/projects/my_game"
IDF_EXTRA_ACTIONS_PATH="$mosaico_game_project/managed_components/espressif__esp_board_manager" \
  idf.py -C "$mosaico_game_project" bmgr \
  -c "$mosaico_raylib_tools/examples/boards/esp-mosaico/bmgr" \
  -b esp_mosaico
```

Registry 示例应使用其自己的 `shared/boards/esp-mosaico/bmgr` 配置目录，
替换上述空白游戏工具目录中的配置。

然后重跑原来的 `mosaico.py game build`。以上命令只生成板级源文件和配置，
设备安装仍使用 `mosaico.py`。显式加载扩展可避开 `esp-bmgr-assist` 对空白模板
间接包含 `project.cmake` 的识别限制。

使用 `--target iris` 时，将 `mosaico_game_project` 改为构建日志中的生成工程
（默认位于 `.codex-runs/mosaico/raylib-iris/<name-hash>/project`），然后重跑
带 `--target iris` 的原构建命令。生成工程需要自己的 Board Manager 配置，
仅在原始示例中生成还不够。构建时保留普通控制台输出：当前版本的
`game build --json` 成功时不输出结果对象或包路径。

## 仿真与设备检查

Host 需要 C 编译器和 Pillow，纹理/资源转换工具还需 NumPy。运行设备共用的 C 模型与 RGB565 绘制，不另写浏览器游戏逻辑。按实际玩法检查输入、计分、碰撞、暂停和重开，必要时用录制回放复验；固定帧数运行成功不能替代交互检查。

浏览器应检查实际点击、键盘和缩放后的坐标；本机/SSH 地址与连接问题见[预览排查](simulator-preview_CN.md)。当前 Host 未覆盖所有 Scene、UI、存档和音频模块；触摸、显示、音频与实际性能按[设备验收](project-gateway_CN.md#实时证据与下一步)检查，Host 耗时不作为设备 FPS。
