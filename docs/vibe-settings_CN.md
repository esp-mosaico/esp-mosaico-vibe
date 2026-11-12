# 应用复用 Vibe Mode 设置

[English](vibe-settings.md) | [返回索引](README_CN.md)

Vibe Mode 保存设置，不代表另一个应用启动后已自动应用。开发 UI 或游戏时，先确认
需要继承哪些设置，再在应用的设备适配层读取；便携模型不直接访问 NVS 或 Wi-Fi 驱动。

## 当前存储与职责

以下是当前 utils 源码的存储实现，不是新的稳定公共 API。升级 utils 时复核类型与
默认值；优先使用已有公共读取接口，没有接口时使用集中、只读的应用适配器。
分区名取自 `mosaico_recovery_contract.h` 的 `MOSAICO_SYSMETA_PARTITION`，当前为 `sysmeta`。

| 内容 | namespace / key | 类型与语义 |
| --- | --- | --- |
| Wi-Fi 名称、密码 | `shared` / `wifi_ssid`、`wifi_password` | NVS 字符串；读取成对凭据并验证长度，开放网络的空密码有效 |
| 亮度 | `vibe_prefs` / `brightness` | `u32`，当前有效范围 20–100，缺失/越界默认 80；面板百分比，不是软件 RGB 增益 |
| 语言 | `vibe_prefs` / `lang` | `u32`，1 英文、2 简体中文；Vibe 有旧 `settings/language` 迁移逻辑，不在应用中重复写迁移 |
| 息屏时间 | `vibe_prefs` / `timeout` | `u32`，秒，0 表示不自动息屏；当前上限 600，缺失/越界默认 0 |
| 自动旋转、触觉 | `vibe_prefs` / `rotate`、`haptics` | `u32` 布尔；当前默认分别为关、开 |
| 自定义共享条目 | `shared` / 用户键 | NVS 字符串；只读取应用声明需要的键 |

`open_confirm`、`confirm_cnt` 是 Vibe 打开应用的确认策略，不直接当作游戏或应用内
交互设置。旧 `wifi` namespace 由 Vibe 的网络模块迁移到 `shared`；应用不反向迁移或
创建另一套全局凭据。凭据存在也不代表当前已经联网，连接状态需从应用自己的网络服务获取。

## 应用适配

- 按需初始化已有 NVS 分区并以 `NVS_READONLY` 打开 namespace。初始化/读取失败时
  关闭句柄、返回可辨识错误并使用应用约定的回退；不能擦除或格式化 `sysmeta` 来修复。
- 用正确的 NVS 类型读取，检查字符串容量、结束符、返回码和数值范围。Wi-Fi 缺失时
  呈现未配置状态，不把错误或超长密码当成开放网络。不要硬编码或输出密码、token。
- 在面板初始化后通过板级亮度接口应用用户百分比。暂时调暗、AOD 或睡眠时保存原值，
  恢复后还原；不要为临时省电覆盖持久设置。息屏超时与停动画/DFS 空闲阈值分别处理。
- 模拟器注入相同含义的设置快照；设备端再验证“Vibe 修改设置 → 启动应用”的继承，
  以及缺失值、范围错误、联网失败和唤醒恢复。验证全局设置保持不变，不以模拟器替代设备验收。

实现依据：utils 中的
[`vibe_settings.c`](https://github.com/esp-mosaico/esp-mosaico-utils/blob/709ed2f295a10dbcd8306ca960699b96b709193d/esp-mosaico-vibe-mode/firmware/vibe-mode/components/app_settings/src/vibe_settings.c)、
[`factory_network.c`](https://github.com/esp-mosaico/esp-mosaico-utils/blob/709ed2f295a10dbcd8306ca960699b96b709193d/esp-mosaico-vibe-mode/firmware/vibe-mode/components/app_network/src/factory_network.c) 和
[`mosaico_recovery_contract.h`](https://github.com/esp-mosaico/esp-mosaico-utils/blob/709ed2f295a10dbcd8306ca960699b96b709193d/esp-mosaico-vibe-mode/include/mosaico_recovery_contract.h)。
Vibe 私有 `app_settings` / `app_network` 组件管理自身任务与状态，不应整个链接到普通应用来获取配置。
设备配网命令见 [CLI 文档](mosaico-cli_CN.md#命令职责)，设备写入仍仅通过 `mosaico.py`。
