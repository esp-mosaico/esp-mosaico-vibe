# 使用 mosaico.py project init 创建工程

在已有 ESP-Mosaico 工作区中运行：

```sh
python mosaico.py project init my_app
```

命令以 `projects/hello_world` 为参考，创建 `projects/my_app`。可以从工作区的
任意子目录调用根目录启动器；创建位置始终由工作区配置决定，与当前目录无关。
在工作区以外调用时，使用全局参数 `--workspace PATH` 指定工作区或配置文件。

工程名为 1–31 个 ASCII 字符，以字母开头，其余字符只允许字母、数字和下划线。
不接受路径和 Windows 保留名称。目标已经存在时，无论是文件、空目录还是符号
链接，命令都会报错，不会覆盖或合并已有内容。

## 预览与自动化

```sh
python mosaico.py project init my_app --dry-run
python mosaico.py project init my_app --json
```

`--dry-run` 完成与实际创建相同的模板和路径校验，列出目标目录与文件，但不创建
目录、工程文件或运行日志。`--verbose` 在正常创建时也显示模板路径和文件清单。

`--json` 成功时在标准输出返回一个 JSON 对象，包含 `ok`、`command`、`status`、
`name`、`project`、`template`、`files` 和 `install_command`。
`status` 为 `created` 或 `dry_run`；`files` 使用相对工程目录的路径。
失败时在标准错误输出返回现有 CLI 错误格式。退出码：成功为 0，名称或目标冲突
为 2，配置或模板问题为 3，写入失败或写入中断为 5。

## 工程内容

本工作区的 Hello World 描述生成完整的 GSP 应用、PC 后端和字体资源：

```text
projects/my_app/
├── CMakeLists.txt
├── README.md
├── partitions.csv
├── sdkconfig.defaults
├── sdkconfig.application.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   ├── main.c
│   ├── hello_ui.c / hello_ui.h
│   ├── board_display.c / board_display.h
│   ├── iris_screen_mirror.c / iris_screen_mirror.h
│   └── ui_bundle.c / ui_bundle.h
├── pc/
│   ├── CMakeLists.txt
│   └── platform_pc.c
└── ui/
    ├── main.json
    └── fonts/  (DejaVu regular、bold 和许可证)
```

生成时调整 CMake 工程名、日志 TAG、USB 产品名称、工具依赖路径和 README 安装路径；
初始界面为橙黑暖白风格的 Hello World，点击 Say hello 后计数递增（99 后回到 00）。
场景与交互 C 代码由 PC 仿真和真机共用，应用版本沿用模板。分区表、ESP-IDF 版本约束、
ESP-Iris 接入和保留 Recovery 配置继续使用参考工程内容。普通应用保留
`esp_mosaico_app_recovery` 和 `iris_ota_support_start()`，OTA writer 留在 Recovery。

在应用目录完成 `idf.py reconfigure` 后，可在工作区根目录预览：

```sh
python3 tools/gsp-sim/run.py --interactive projects/my_app/ui/main.json
```

首次安装或修改场景、字体、图片时必须使用 `iris system-update`，同时安装 `ui_apps`。
只有分区表和 UI 资源匹配时，C 代码变更才可使用 `iris app-update`。

命令不会复制构建目录、下载的组件、`sdkconfig` 或 `dependencies.lock`。模板
源文件和工作区的 `default_project` 保持不变。写入失败会清理本次创建的文件和
空目录；如果有无法清理的内容，错误详情会列出对应路径。

## 工作区配置

`.mosaico.json` 中相关字段为：

```json
{
  "workspace": {
    "projects_dir": "projects",
    "default_project": "projects/hello_world",
    "init_template": "projects/hello_world/mosaico-template.json"
  }
}
```

以上仅展示相关字段，保留配置中的其他字段。`init_template` 指向 JSON 描述文件，
相对路径从配置文件所在目录解析。子仓库没有默认模板；未配置此字段时，其他命令
正常使用，`project init` 会提示补充配置。

Hello World 源码和 `projects/hello_world/mosaico-template.json` 均由主仓库维护。
描述文件声明复制哪些文件、如何替换工程名和文案，以及共享资源的位置。主仓库
改名、增加源文件或调整生成规则时，同步修改描述即可，无需改动工具子仓库。
生成结果不会复制描述文件本身，也不需要将可编译的 Hello World 源码改成占位符模板。

工具子仓库只维护通用描述格式、变量展开、路径与内容校验、文件创建和失败清理。
描述中的 `files` 决定文件清单，`replacements`/`append` 决定文本变换，`paths`
声明工作区或依赖目录中的资源。它不执行模板提供的脚本，也不假定 `main/main.c`
或任何 README 原文。详细字段和示例见[模板格式规范](../submodule/esp-mosaico-utils/esp-mosaico-recovery/docs/project-template.md)。

`projects_dir` 必须在工作区内部，可以包含多级目录。描述中的路径变量相对于
每个生成文件的父目录计算；BSP 和 ESP-Iris 路径来自 `dependencies` 配置。
Windows 上生成目录与引用的本地依赖须位于同一盘符。共享组件与构建脚本仍由
工作区维护，不会复制进新应用。Recovery 固件不能作为普通应用模板。

## 构建与设备安装

创建工程只需要 Python 3.8 或更新版本，以及已经初始化的工具子模块；不要求
配置 ESP-IDF、下载 BSP 依赖、启动 Gateway 或连接设备，也不会自动执行这些步骤。
构建前应初始化所需子模块，并配置满足生成工程 `main/idf_component.yml` 约束的
ESP-IDF 环境。

创建成功后，从工作区根目录显式选择新工程安装：

```sh
python mosaico.py iris system-update --project projects/my_app
```

空白或未经验证的设备须先执行 `python mosaico.py recover` 并验证 Recovery 就绪。
安装后可用 `python mosaico.py iris logs --timeout 20` 观察日志。默认工程不会因
`project init` 改变，因此在工作区根目录使用 `iris system-update` 时应保留 `--project` 参数。

后续仅修改代码且完整分区表与设备一致时可用 `iris app-update`。角色及产品契约来自共享应用配置，已有 `sdkconfig` 的实际值仍需通过构建检查。
