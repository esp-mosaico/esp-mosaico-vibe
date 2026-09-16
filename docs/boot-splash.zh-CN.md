# 静态开机 Logo

ESP-Mosaico 的保留 Recovery bootloader 在应用分区选择前，通过 SPI2 QSPI
向 CO5300 绘制黑底橙色点阵 `mosaico`。Logo 由紧凑字形数据生成，不依赖应用的
LVGL/GSP、PSRAM 或外部 UI 资源。eFuse 标识的 v1.0、v1.1、v1.2 板有对应
GPIO 分支；未知板型或屏幕传输失败不会阻断启动。

## 改动边界

- `esp-mosaico-utils` 子模块拥有 bootloader 绘制代码、Recovery 配置和完整
  `firmware/recovery/prebuilt/recovery` 基础包。
- `esp-mosaico-bsp` 子模块不作改动。进入 Recovery 或普通应用时，BSP 照常
  reset 和完整初始化屏幕，允许短暂黑屏；不保证 Logo 保留至应用首次刷新。
- 当前 workspace 仅更新 utils 子模块，用 `tests/test_boot_splash_contract.py`
  检查启动顺序、显示失败不阻断启动和 BSP 完整初始化契约。

bootloader 不使用 LP STORE 寄存器或跨启动阶段交接协议。分区布局、OTA 选择、
Recovery Boot 按键、ESP-Iris USB 归属和普通应用安装流程均不改变。

## 使用与更新

初始化两个子模块后，通过产品命令操作设备：

```sh
git submodule update --init submodule/esp-mosaico-bsp submodule/esp-mosaico-utils
python3 mosaico.py list
python3 mosaico.py recover --device-id DEVICE_ID
python3 mosaico.py install --project projects/hello_world --device-id DEVICE_ID
```

默认 `recover` 使用提交的预编译包，因此该包必须与 Logo 源码一起更新。普通
应用的 `install` 和示例的 System Update 不替换保留 bootloader。GSP 首次部署
或 UI 资源变化时遵循其项目 README，不能把应用上传成功当作 UI 资源已验证。

## 验证范围

2026-09-16，ESP32-S31 v1.2 测试板完成真实开机 Logo 观察和同一 Device ID 的
normal → Recovery → normal 往返。开发者确认“看到了点阵logo 显示都很正常”。
最初版本包含 BSP 接管，之后按开发者要求撤销 BSP 改动并移除交接协议，接受
应用初始化时短暂黑屏。旧版本的接管日志不能作为撤销后的候选验收证据；当前
候选的构建环境、哈希与验收范围见预编译包 README 和 manifest。

bootloader 固定可用空间为 `0x6000` 字节，使用 ERROR 日志配置。v1.0/v1.1 的
分支已编译，但尚未实机验收。运行原始日志留在 `.codex-runs/`，不提交设备凭据。
