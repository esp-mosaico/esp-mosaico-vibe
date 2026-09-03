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

### Recovery 从 HTTP(S) 拉取系统更新

Recovery profile 默认编译 HTTP(S) System Update source，但不会自动访问网络。
服务器需要提供解包后的 bundle，`manifest.json` 及其 `components[].file` 必须位于
同一目录；manifest 格式与 ESP-Iris `.irisfw` bundle 相同。通过产品 CLI 触发：

```sh
python mosaico.py system-update --device-id DEVICE_ID \
  --manifest-url 'https://updates.example.com/mosaico/release/manifest.json'
```

RPC 只启动后台任务并立即返回。Recovery 等待已配置的 Wi-Fi，下载并验证所有
组件；application 写入 `ota_0`，bootloader 和 partition table 暂存在 PSRAM，
所有组件验证完成后才进入不可取消的 single-copy commit。

HTTPS 默认使用 ESP-IDF certificate bundle 验证服务器。明文 HTTP 仅用于隔离的
开发网络，需显式设置
`CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE_ALLOW_PLAIN_HTTP=y`。当前 product backend
仍是 unsigned policy；面向非受控网络发布前必须加入并启用 manifest release-key
验证。

### Recovery 从 NAND LittleFS 读取系统更新

ESP-Mosaico 的板载 NAND 与 `esp-mosaico-claw` 一致，使用 SPI NAND、wear-leveling
block device 和 LittleFS，挂载点为 `/nand`。Recovery 只读挂载已有文件系统，挂载
失败时不会格式化 NAND。将解包后的 bundle 放到同一目录，例如：

```text
/nand/system-update/manifest.json
/nand/system-update/ota_0.bin
/nand/system-update/bootloader.bin
/nand/system-update/partition-table.bin
```

Recovery 首页提供 **Update from NAND**：进入后固件会异步、只读扫描以下两种
catalog 布局，最多列出 8 个完整 bundle；点击条目可先核对 release、组件数、总
容量和 manifest 路径，再确认更新。

```text
/nand/system-update/manifest.json
/nand/system-update/<release>/manifest.json
```

每个 `manifest.json` 引用的组件必须与它位于同一目录。扫描阶段会过滤 manifest
格式错误、组件缺失或文件大小不符的条目；确认安装后，System Update backend
仍会重新执行完整 manifest、SHA-256、镜像和布局校验。也可以通过产品 CLI 直接
启动指定路径：

```sh
python mosaico.py system-update --device-id DEVICE_ID \
  --manifest-path /nand/system-update/manifest.json
```

Recovery 逐块读取组件并复用与 USB、HTTP(S) 相同的 manifest、SHA-256、镜像及
分区布局校验。application 流式写入 `ota_0`；bootloader 和 partition table 暂存
到 PSRAM，全部验证完成后统一提交。三种来源共用一个 Flash writer owner，不能
并行执行。可通过 `CONFIG_IRIS_FACTORY_NAND_SYSTEM_UPDATE_AUTO_START=y` 配置固定
路径自动启动；默认关闭，避免误用 NAND 中遗留的旧 bundle。

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
