# ESP-Mosaico Iris Factory

这是 ESP-Mosaico 的常驻 Factory Recovery 工程。它保留原有的
ESP-Mosaico 视觉语言，同时提供离线 USB OTA、联网 TCP OTA、Factory
独立配网和签名全系统更新。

## 已实现能力

- 二级 Bootloader 在启动时将 GPIO7 配置为内部上拉输入；检测到稳定低电平时
  仅本次进入 `factory`，不修改 OTA data 或 Wi-Fi NVS。按键释放后的下一次启动
  继续遵循正常 OTA/rollback 选择。
- USB High-Speed 与 TCP `19772` 同时监听；第一个完成认证握手的链路拥有
  会话，另一条链路保持可用并在当前会话断开后重新竞争。
- USB 初始化不依赖 Wi-Fi。未配网、密码错误或 AP 不可用时，USB OTA 仍可
  独立工作。
- 屏幕扫描 Wi-Fi、输入密码、显示/隐藏密码、重新配网和忘记网络。
- 成功联网后才把 SSID/密码保存到专用 `factory_nvs` 分区；Wi-Fi driver 使用
  `WIFI_STORAGE_RAM`，不会把 Factory 凭据写入正常应用的默认 NVS。
- 下次进入 Recovery 自动连接已保存网络。
- 广播 `_esp-iris._tcp` mDNS 服务，主机名为
  `mosaico-<device-id-suffix>.local`，固定端口为 `19772`；屏幕同时显示
  mDNS、IP 和 TCP pairing token。
- 正常应用只注册只读 System Inventory 与进入 Recovery 的 RPC。只有
  Recovery 可以注册 OTA writer 和全系统 Flash backend。
- 全系统 backend 支持签名清单、`ota_0` application、bootloader 和 partition
  table；每个镜像都经过 descriptor 绑定、SHA-256、写后 readback 和镜像格式
  校验。

## 分区与更新约束

当前 16 MiB layout：

| 分区 | Offset | Size | 用途 |
|---|---:|---:|---|
| `factory_nvs` | `0x12000` | `0xe000` | 仅 Factory Wi-Fi 凭据 |
| `factory` | `0x20000` | `0x200000` | 常驻 Recovery |
| `ota_0` | `0x220000` | `0xc00000` | 正常应用 |
| `coredump` | `0xe20000` | `0xd0000` | 崩溃证据 |
| `sysmeta` | `0xef0000` | `0x10000` | 最近一次系统更新结果 |

签名系统更新可以修改分区表内容，但产品策略要求 `factory`、`ota_0`、
`factory_nvs`、`otadata` 与 `sysmeta` 的 offset 和 size 保持不变。这样可以调整
其余数据分区，同时保留 Recovery、正常应用启动路径和凭据隔离。

bootloader/partition-table 使用 ESP32-S31 单副本提交策略：bootloader 先写、
partition table 最后写，并逐段 readback。这一策略无法抵抗提交期间的断电，
因此系统更新界面会要求持续供电；量产前必须由产品安全评审正式接受这一风险。

## 构建

需要 ESP-IDF `>=6.1` 并支持 `esp32s31`。默认 application 构建会校验并打包
`prebuilt/recovery` 中已评审的 Recovery：

```sh
idf.py -B build -D BUILD_PROFILE=application build
```

主要产物：

- `build/factory.bin`：正常应用，安装到 `ota_0`。
- `build/recovery/factory.bin`：常驻 Factory Recovery。
- `build/recovery/manifest.json`：Recovery 几何与构建来源记录。

修改 Recovery 源码时，在当前 application build 中选择
`Iris Factory OTA -> Recovery image source -> Build recovery from current source`，
验证完成后再显式执行 `update-recovery-prebuilt` 发布新的预构建 Recovery。

空白或 Recovery 未验证的设备应先按
[`docs/recovery-first-workflow.md`](../../docs/recovery-first-workflow.md)
完成一次 Recovery provisioning。之后不要用正常应用的 `idf.py flash`；通过
ESP-Iris Gateway 进入 Recovery 后安装 normal firmware。

GPIO7 Recovery 选择逻辑位于二级 Bootloader。已有设备只更新 application 或
Factory app 镜像不会获得该功能；需要通过 Recovery provisioning 或签名的
full-system update 同步更新 Bootloader。

## 配置产品系统更新公钥

全系统 Flash backend 已实现，但默认不注册 writer，因为仓库中没有产品发布
公钥。先在离线发布环境生成/保管 ECDSA P-256 私钥，只把公钥放入 firmware：

```sh
openssl pkey -pubin -in release-public-key.pem -outform DER | xxd -p -c 999
```

把输出写入 `sdkconfig.recovery.defaults`：

```text
CONFIG_IRIS_FACTORY_SYSTEM_UPDATE_BACKEND=y
CONFIG_IRIS_FACTORY_SYSTEM_UPDATE_KEY_ID="product-release-1"
CONFIG_IRIS_FACTORY_SYSTEM_UPDATE_PUBLIC_KEY_DER_HEX="<SPKI-DER-HEX>"
```

可复制 [`sdkconfig.system-update-key.example`](sdkconfig.system-update-key.example)
作为配置参考。私钥和密码不得提交到本仓库，也不得放在 Gateway 主机。

Gateway 同样需要同一把公钥的 PEM 文件。签名 bundle 的基础模板见
[`system-update-manifest.template.json`](system-update-manifest.template.json)：

```sh
python "$IRIS" bundle build system-update-manifest.template.json \
  --component-root build/system-update \
  --signing-key /private/release-signing-key.pem \
  --signing-key-password-file /private/release-signing-key.password \
  --output release.irisfw

python "$IRIS" web \
  --system-update-trust-key release-public-key.pem

python "$IRIS" ctl system-update DEVICE_ID release.irisfw --wait
```

提交前必须把模板中的 `source_layout_sha256` 替换为目标设备当前 System
Inventory 报告的 partition-table SHA-256。Gateway 会在进入 Recovery 前再次
核对该值，并在重启后核对 operation ID、application、bootloader 和 partition
table 的实际身份。

## TCP 配对

设备联网后，在 Recovery 首页可看到 IP 与 mDNS 名称；`TCP pairing` 页面显示
完整 64 位 token。Gateway 可使用 mDNS 主机名和固定端口连接：

```sh
python "$IRIS" web \
  --tcp mosaico-xxxxxx.local:19772 \
  --pairing-token <64-lowercase-hex-token>
```

token 不通过 TCP 链路传输，配对使用 challenge-HMAC。首次验证硬件时，应同时
打开 Gateway Web Workbench，确认 CLI 与页面看到相同的 Device ID、Boot ID 和
操作记录。
