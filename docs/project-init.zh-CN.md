# 工程创建与选择

[返回索引](README.md)

全新工作区没有预置应用。只初始化 utils 即可创建，Python 3.8+ 足够：

```sh
git submodule update --init submodule/esp-mosaico-utils
python mosaico.py project init my_app --dry-run
python mosaico.py project init my_app
```

模板归 `esp-mosaico-utils/mosaico-tools/templates/hello_world/`，新工程默认生成到
`projects/my_app`。创建不需要 BSP、引擎、ESP-IDF 或设备，不启动 Gateway；
失败会清理由此次创建写入的文件，已有目标拒绝覆盖，不自动修改默认工程。

`.mosaico.json` 的 `workspace.init_template` 可指向自定义描述；
`workspace.projects_dir` 可修改生成位置，仍须位于工作区内。模板格式及路径变量见
[应用集成](../submodule/esp-mosaico-utils/mosaico-tools/docs/application-integration.md)。

工程选择顺序：显式 `--project`、当前所在工程、用户配置的有效默认工程、
唯一已创建工程。零工程提示创建，多工程要求选择；内部 Recovery 不参与选择。
显式 `--project` 相对调用目录解析。可从工作区内任意嵌套目录调用入口，
或通过 `--workspace PATH` 指定工作区。

初始化 BSP、准备兼容的 ESP-IDF 后，构建生成工程并执行：

```sh
python mosaico.py project sim --project projects/my_app --interactive
python mosaico.py recover
python mosaico.py iris system-update --project projects/my_app
```

`recover` 用于空白或未验证设备首次安装。新应用、布局变化或外部资源变化用
`system-update`；完整分区表和资源相同的代码更新才用 `app-update`。

生成源码不保存开发机绝对路径。移动或重新克隆整个工作区后，初始化固定版本
依赖并重新构建；旧构建缓存不作为可移植产物。
