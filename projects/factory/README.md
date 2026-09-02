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
- `recover` 初始化或恢复设备，默认使用仓库内经过评审的基础包；实时显示基础包
  校验、设备检测、ESP-IDF 构建/烧录、镜像哈希校验、重连和 Recovery 就绪验证。
- `install` 构建并通过 ESP-Iris 安装普通应用；不会自动执行 `recover`。
- `install` 默认实时显示构建、Recovery 切换、传输进度、重连和固件校验阶段；
  `--json` 模式保持稳定机器输出，详细过程仍保存在运行日志中。
- `monitor` 先显示保留日志，再持续跟随；按 `Ctrl+C` 正常结束。

Recovery 屏幕在普通 OTA 写入期间显示应用镜像接收进度、传输所有者和
SHA-256 校验状态；完成后显示重启提示。System Update 继续复用同一进度页面。

## Flash 布局

16 MiB Flash 使用固定系统前缀和尾部可缩减的单应用槽：

| 分区 | Offset | Size | 用途 |
| --- | ---: | ---: | --- |
| `otadata` | `0x9000` | 8 KiB | ESP-IDF OTA 选择与回滚状态 |
| `phy_init` | `0xb000` | 4 KiB | PHY 初始化数据 |
| `sysmeta` | `0xc000` | 80 KiB | 系统专用 NVS |
| `factory` | `0x20000` | 2 MiB | 保留 Recovery |
| `coredump` | `0x220000` | 832 KiB | 崩溃证据 |
| `nvs` | `0x2f0000` | 64 KiB | 应用 NVS |
| `ota_0` | `0x300000` | 13 MiB | 普通应用；后续布局可从尾部缩减 |

`sysmeta` 中的 `esp_iris`、`wifi`、`iris_ota_demo` 和 `update` namespace
分别保存设备身份及 TCP pairing token、Factory Wi-Fi、Recovery OTA 状态和
最后一次系统更新结果。除 `ota_0` 的尾部可按受控迁移策略回收外，其他分区的
offset 和 size 均属于固定布局契约。

常用选项可通过 `python mosaico.py <command> --help` 查看。自动化环境可加
`--json`；`recover` 在唯一识别到受支持设备后直接执行，无需二次确认。

## 工程维护者

普通用户不应直接调用底层构建或写入命令。Recovery 基础包和内部写入 target
由 `mosaico.py recover` 管理；普通应用始终由 `mosaico.py install` 通过
ESP-Iris 安装。评审包包含完整哈希与布局约束，只有通过构建校验和真机验收后
才应发布。
