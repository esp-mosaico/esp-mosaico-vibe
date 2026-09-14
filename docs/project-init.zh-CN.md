# 使用 mosaico.py init 创建工程

在已有 ESP-Mosaico 工作区中运行：

```sh
python mosaico.py init my_app
```

命令以 `projects/hello_world` 为参考，创建 `projects/my_app`。可以从工作区的
任意子目录调用根目录启动器；创建位置始终由工作区配置决定，与当前目录无关。
在工作区以外调用时，使用全局参数 `--workspace PATH` 指定工作区或配置文件。

工程名为 1–31 个 ASCII 字符，以字母开头，其余字符只允许字母、数字和下划线。
不接受路径和 Windows 保留名称。目标已经存在时，无论是文件、空目录还是符号
链接，命令都会报错，不会覆盖或合并已有内容。

## 预览与自动化

```sh
python mosaico.py init my_app --dry-run
python mosaico.py init my_app --json
```

`--dry-run` 完成与实际创建相同的模板和路径校验，列出目标目录与文件，但不创建
目录、工程文件或运行日志。`--verbose` 在正常创建时也显示模板路径和文件清单。

`--json` 成功时在标准输出返回一个 JSON 对象，包含 `ok`、`command`、`status`、
`name`、`project`、`template`、`files` 和 `install_command`。
`status` 为 `created` 或 `dry_run`；`files` 使用相对工程目录的路径。
失败时在标准错误输出返回现有 CLI 错误格式。退出码：成功为 0，名称或目标冲突
为 2，配置或模板问题为 3，写入失败或写入中断为 5。

## 工程内容

只生成以下八个源文件：

```text
projects/my_app/
├── CMakeLists.txt
├── README.md
├── partitions.csv
├── sdkconfig.defaults
├── sdkconfig.application.defaults
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml
    └── main.c
```

生成时调整 CMake 工程名、日志 TAG、USB 产品名称和 README 安装路径；初始界面
继续显示 `Hello World!`，应用版本沿用模板。分区表、ESP-IDF 版本约束、ESP-Iris
接入和保留 Recovery 配置继续使用参考工程内容。普通应用保留
`esp_mosaico_app_recovery` 和 `iris_ota_support_start()`，OTA writer 留在 Recovery。

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
    "init_template": "projects/hello_world"
  }
}
```

以上仅展示相关字段，保留配置中的其他字段。`init_template` 可省略，默认值为
`projects/hello_world`，独立于 `default_project`。相对路径从配置文件所在目录
解析。自定义模板需要保持 Hello World 的八文件结构以及工程名、TAG、README
命令等待替换标记；不支持将 Recovery 固件作为普通应用模板。

`projects_dir` 必须在工作区内部，可以包含多级目录。命令重新计算 BSP、
ESP-Iris、`components/esp_mosaico_app_recovery` 和 `cmake/system_update.cmake`
的引用路径。本地 BSP 和 ESP-Iris 路径来自配置中的 `dependencies`。
共享组件与构建脚本仍由工作区维护，不会复制进新应用。

## 构建与设备安装

创建工程只需要 Python 3.8 或更新版本，以及已经初始化的工具子模块；不要求
配置 ESP-IDF、下载 BSP 依赖、启动 Gateway 或连接设备，也不会自动执行这些步骤。
构建前应初始化所需子模块，并配置满足生成工程 `main/idf_component.yml` 约束的
ESP-IDF 环境。

创建成功后，从工作区根目录显式选择新工程安装：

```sh
python mosaico.py install --project projects/my_app
```

空白或未经验证的设备须先执行 `python mosaico.py recover` 并验证 Recovery 就绪。
安装后可用 `python mosaico.py monitor --timeout 20` 观察日志。默认工程不会因
`init` 改变，因此在工作区根目录使用 `install` 时应保留 `--project` 参数。
