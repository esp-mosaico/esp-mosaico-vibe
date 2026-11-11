# MiniHub Panel 审查复核与整改结果

复核日期：2026-11-11
基线：`aa51ca03bb9f5e7682dc98cae0081619a7cb9033`

## 原则

本工程继续直接编译兄弟目录 `esp-minihub` 和 `esp-matter` 的官方源码。
MiniHub 已提供的 Host、Matter、Web、场景和规则实现不在面板工程中复制。
应用侧代码只负责 Mosaico 的显示、触摸、Wi-Fi 所有权、Vibe Recovery
接入，以及官方组件与 ESP-Iris 之间的构建适配。

## 复核结论

### 已确认并整改

1. **mdns 版本冲突只存在于 Thread 变体。**
   - 官方 `minihub_js` 当前没有 mdns 依赖，面板已直接引用官方组件目录，
     不再维护容易随上游源文件清单漂移的本地包装层。
   - 官方 `minihub_thread` 要求 mdns 1.13.1，ESP-Iris 0.2.0 精确要求
     1.12.0；Thread 变体通过 `variant_components/thread/minihub_thread`
     直接编译官方 Thread 源码，并把它的 mdns 清单对齐到 1.12.0。
   - Zigbee 低噪声构建通过，日志确认只解析 mdns 1.12.0。

2. **Kconfig 重复挂载成立。**
   - 删除第二套手写 MiniHub 菜单，只挂官方
     `cmake/minihub_bundle/Kconfig.projbuild` 一次。

3. **Zigbee 镜像错误暴露 BLE Mesh 成立。**
   - 默认 Zigbee 变体不再设置 `CONFIG_MINIHUB_BLE_MESH_SDK`。
   - Matter、Zigbee、MQTT、Modbus 能力仍由实际 Kconfig 生成；面板不会再
     提供一个会停掉既有 Zigbee 网络的 Mesh 入口。

4. **三个 sdkconfig 符号无效成立。**
   - 删除 `MINIHUB_WEB_MAX_OPEN_SOCKETS`、
     `MINIHUB_WEB_TASK_STACK_EXTERNAL` 和
     `COMPILER_DISABLE_DEFAULT_WARNINGS`。
   - Web 服务器继续直接采用官方 MiniHub 的 20 sockets / 16 KB 栈，
     不再用无效配置制造“已限制到 4”的假象。

5. **Matter 阶段和错误信息在面板层丢失成立。**
   - 面板按官方 `/api/matter/status` 的 `discovering`、`binding`、
     `activating` 显示三种现有文案。
   - 启动失败保留官方 Web API 返回的 `message`，状态缓冲从 64 扩至
     128 字节。
   - Matter 启动 POST 单独使用 15 秒超时；普通 REST 仍为 5 秒。
   - 队列满不再错误进入“正在发现”页。
   - 入网前显示将下发的当前 Wi-Fi SSID，不显示密码。

6. **命令队列固定占用约 11 KB 内部 RAM 成立。**
   - FreeRTOS 队列改为只保存命令指针；命令载荷优先分配到 PSRAM。
   - `hub_client` 改用 16 KB 内部 RAM 栈。
   - Matter 添加不再在 worker 栈上复制第二个完整 `hub_command_t`。
   - GOT_IP 事件不再直接提交 NVS / MiniHub provision，改由 worker 执行。
   - GSP render task 不再强制使用 PSRAM 栈。

7. **任务看门狗被完全关闭成立。**
   - 从官方 MiniHub `app_main` 直接移植 30 秒 task WDT 初始化策略。
   - 未联网时，独立 Matter、删除、重启和普通设备命令现在都会写入
     `Network is not ready`，不再从 worker 静默消失。

8. **UI 分区余量不足成立；不能通过移动持久化分区解决。**
   - 模拟器 `app.gspb` 与设备 deployable GSPB 不是同一种产物，不能用前者
     判断 Flash 分区容量。完整固件构建确认设备 GSPB 原为 2,091,856 字节，
     包装后仅比 2 MiB `ui_apps` 小 5,232 字节。
   - 保留官方 MiniHub 已有 `storage@0xc00000`，避免 System Update 在不携带
     `storage.bin` 时让已配设备、场景和规则失效；`main_app` 保持 0x9f0000，
     `ui_apps` 保持 2 MiB。
   - 合并重复的 13/12 px CJK 字库档位后，设备 GSPB 为 2,021,840 字节，
     `ui_apps.bin` 为 2,021,904 字节，余量 75,248 字节。
   - 完整应用镜像为 5,348,288 字节，小于 `main_app` 0x9f0000。

9. **Web 登录关闭是上线前缺口。**
   - 恢复官方 `CONFIG_MINIHUB_WEB_LOGIN_AUTH` 和官方首次设置 admin
     密码流程。
   - 首次启动为每台设备生成独立 256-bit REST token，保存到官方
     `minihub/api_token` NVS，并交给本机 loopback 面板使用。
   - 不使用编译期共享 token，也不记录 token。

### 原报告需要更正

1. **“Matter 300 秒等待占住 hub_client”不成立。**
   官方 `POST /api/matter/devices` 和当前面板都异步启动，worker 只轮询
   状态。300 秒是 `esp_timer` 看门狗和面板截止时间，不是阻塞等待。

2. **“屏幕离线添加 Matter 会静默丢失”不成立。**
   屏幕使用 `app_hub_add_device`，入队前和出队时都写
   `Network is not ready`。未被面板调用的独立
   `HUB_CMD_MATTER_COMMISSION` 路径仍可继续清理，但不是用户现象根因。

3. **资源风险不能反推历史配网失败根因。**
   没有当时的设备日志、socket 计数或 coredump，不能把栈、socket 或
   HTTP 超时写成已经复现的根因。

### 暂不直接修改

1. **场景/自动化超过第一页。**
   官方提供的是不限于四张卡片的场景/规则数据模型和 Web 编辑器；GSP
   触屏编辑器是本项目特有 UI。当前“业务上限错误绑定到单页四卡”的结论
   成立，但需要增加独立的可翻页 point picker，并把已选项存成 point ID，
   不能复制官方 DOM 代码或只增大数组。此项应单独实现并做六设备交互回归。

2. **圆角安全区。**
   需要实机照片或 BSP 给出的有效遮罩尺寸。没有证据时不盲目移动触摸目标。

## 验证记录

- `idf_low_noise_build doctor`：通过；IDF
  `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`，目标 `esp32s31`，
  约束 `>=6.2` 满足。
- Matter 官方 bootstrap 已补齐 GN/Pigweed。Zigbee 完整固件构建通过，
  解析 `espressif/mdns 1.12.0`；最终应用 5,348,352 字节，
  `ui_apps.bin` 2,021,904 字节。
- 无头模拟器：构建和 3 秒运行通过；GSP 报 9 条既有资源/字体告警，
  无编译错误。
- 真机 System Update 通过：设备
  `4553502d49524953010030eda0f4459a`，新 Boot ID
  `14983094770834091986`，firmware mode 为 normal，崩溃计数 0，
  目标固件与系统分区健康。
- 实机复核期间发现并修正了移动 `storage` 地址和 BLE 启动后执行硬件 SHA
  的两个启动问题。经确认写入工厂 `storage.bin` 恢复文件系统；原有配网设备、
  场景和规则已清空。
- 未修改官方 `esp-minihub` 或 `esp-matter` 工作树。当前本机
  `esp-matter` 本身有用户未提交改动，构建结果应连同其实际 SHA 和 dirty
  状态解释。
