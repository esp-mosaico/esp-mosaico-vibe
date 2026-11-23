# 工程创建与选择

[English](project-init.md) | [返回索引](README_CN.md)

全新工作区没有预置应用。本工作区要求 Python 3.10+、Git 2.28+，即使个别公共工具
支持更低版本也须遵守。只初始化 utils 即可创建应用：

```sh
git submodule update --init submodule/esp-mosaico-utils
python mosaico.py project init my_app --dry-run
python mosaico.py project init my_app
```

模板归 `esp-mosaico-utils/mosaico-tools/templates/hello_world/`，新工程默认生成到
`projects/my_app`。创建不需要 BSP、引擎、ESP-IDF 或设备，不启动 Gateway；
失败会清理由此次创建写入的文件，已有目标拒绝覆盖，不自动修改默认工程。

工作区 `.mosaico.json` 使用 `schema_version: 2`，模板描述仍使用 schema 1。
旧工作区见[迁移说明](workspace-migration_CN.md)。`workspace.init_template` 可指向自定义描述；
`workspace.projects_dir` 可修改生成位置，仍须位于工作区内。模板格式及路径变量见
[应用集成](../submodule/esp-mosaico-utils/mosaico-tools/docs/application-integration.md)。

创建时会初始化分支为 `main` 的独立 Git 仓库，补齐 Git、编辑器和质量检查配置，
保留模板已有设置；不会自动暂存、提交、设置远端或安装 hooks。在生成工程中按照
`tools/quality/README.md` 安装并启用固定版本检查。检查器由 utils 统一维护，
工程只保存策略及相对引用。

Hello World 按职责组织：

| 路径 | 职责 |
| --- | --- |
| `main/main.c` | 设备启动与依赖组装 |
| `components/app_hello/` | 可移植 C UI 控制器与公共接口 |
| `components/app_hello/ui/` | 场景源文件和字体 |
| `components/app_hello/host/` | 复用控制器的原生桌面适配器 |
| `components/app_display/` | BSP 显示与触摸策略 |

`project sim` 自动发现组件场景及 Host 后端，也兼容旧工程根目录的 `ui/main.json`
和 `pc/`。多场景时可用 `--scene PATH` 选择，但此时仅渲染场景、不执行 C 回调；
验证应用行为时应省略该参数。

工程选择顺序：显式 `--project`、当前所在工程、用户配置的有效默认工程、
唯一已创建工程。零工程提示创建，多工程要求选择。应用选择排除 Recovery 暂存工程。
显式 `--project` 相对调用目录解析。可从工作区内任意嵌套目录调用入口，
或通过 `--workspace PATH` 指定工作区。

初始化 BSP，准备 ESP-IDF `master` 固定提交
`7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`（目标 `esp32s31`）后，构建生成工程并执行。
用 `git -C "$IDF_PATH" rev-parse HEAD` 核对完整 SHA；不要直接跟随最新 `master`：

```sh
python mosaico.py project sim --project projects/my_app --interactive
```

UI 类应用优先使用 GSP。设计确认后，先在仿真器中暴露并修复布局、裁切、资源显示、
点击反馈、页面切换、计时器和状态变化问题，复验受影响的流程后再进入真机验证。
仿真应运行与设备共用的原生 C UI 和交互逻辑；静态截图不能代替交互验证。
只有依赖真实硬件的问题或仿真器未覆盖的行为才优先在真机排查，并说明验证缺口。

仿真验证完成后，通过产品 CLI 安装，并验证实际屏幕、输入和硬件交互。
当前 Hello World 模板支持 Iris 截图，但未注册可选的指针 RPC；触摸需在设备屏幕上
实测。通用 RPC/input 能力字段不能证明已实现指针处理器：

```sh
python mosaico.py recover
python mosaico.py iris system-update --project projects/my_app
```

`recover` 用于空白或未验证设备首次安装，会写入基础固件；仅进入已有
Vibe Mode 的方式见[设备模式](device-modes_CN.md)。新应用、布局变化或外部资源变化用
`system-update`；完整分区表和资源相同的代码更新才用 `app-update`。

Hello World 生成源码不保存开发机绝对路径。移动或重新克隆整个工作区后，初始化固定版本
依赖并重新构建；旧构建缓存不作为可移植产物。
