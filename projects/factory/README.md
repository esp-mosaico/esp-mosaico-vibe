# ESP-Mosaico Factory Reference

`factory` 是 ESP-Mosaico 用户工程的参考模板，同时提供统一恢复流程所需的基础
固件。日常使用不需要了解其启动链路和 Flash 布局。

## 用户命令

在仓库根目录运行：

```sh
python mosaico.py list
python mosaico.py recover
python mosaico.py install --project projects/<project>
python mosaico.py monitor
```

- `list` 列出仓库适配的设备型号，不查询当前连接设备。
- `recover` 初始化或恢复设备，默认使用仓库内经过评审的基础包。
- `install` 构建并通过 ESP-Iris 安装普通应用；不会自动执行 `recover`。
- `monitor` 先显示保留日志，再持续跟随；按 `Ctrl+C` 正常结束。

常用选项可通过 `python mosaico.py <command> --help` 查看。自动化环境可加
`--json`；`recover` 在唯一识别到受支持设备后直接执行，无需二次确认。

## 工程维护者

普通用户不应直接调用底层构建或写入命令。Recovery 基础包和内部写入 target
由 `mosaico.py recover` 管理；普通应用始终由 `mosaico.py install` 通过
ESP-Iris 安装。评审包包含完整哈希与布局约束，只有通过构建校验和真机验收后
才应发布。
