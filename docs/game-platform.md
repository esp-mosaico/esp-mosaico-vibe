# ESP-Mosaico Game Platform

## 定位

Game Platform 为 ESP32-S31/ESP-Mosaico 提供 480×480 的 2D 游戏运行环境。游戏逻辑面向受控的 Raylib 兼容 API；设备侧使用 RGB565 framebuffer，ESP-GSP 负责 Canvas 展示，ESP-Iris 负责管理、镜像、输入与 Recovery-first 更新。

平台不把 PNG、WAV、Tiled JSON 或通用 OpenGL 解析器带入设备运行时。源资源在构建阶段转为可 mmap 的确定性二进制。

## 分层

| 层 | 位置 | 职责 |
|---|---|---|
| 游戏应用 | `projects/<game>/main` | 玩法模型、同源 view、设备回调、Host 输入映射 |
| 设备壳 | `mosaico_game_app`、`mosaico_game_iris` | 启动、触摸任务、Action Mapper、主循环、Iris inventory |
| Raylib API | `mosaico_raylib_fast` | 常用 Raylib 2D 调用到 RGB565 快速路径的映射 |
| 游戏运行时 | `mosaico_game`、`input`、`debug`、`scene`、`ui`、`fx`、`save` | 事件队列、帧统计、场景、UI、效果与存档；core 不依赖 Iris |
| 内容运行时 | `assets`、`2d`、`tilemap`、`audio` | mmap 资源、Atlas、地图、PCM/ADPCM 混音 |
| 显示适配 | `mosaico_raylib_port` | 设备 PSRAM framebuffer、多缓冲、GSP 非阻塞提交 |
| 构建工具 | `submodule/raylib-lite-engine/tools`、`submodule/raylib-lite-engine/cmake` | 资源编译、能力组件选择、GSP 编译器发现 |
| Host 工具 | `submodule/raylib-lite-engine/host` | 同源 RGB565 仿真、回放、截图与浏览器预览 |
| 设备运维 | `mosaico.py`、ESP-Iris | 构建、Recovery、安装、监控、截图和远程输入 |

依赖方向必须保持自上而下。玩法模型不得依赖 FreeRTOS、BSP 或 ESP-Iris，使其能由 Host C 编译器直接测试。

## 项目结构

```text
projects/<game>/
├── CMakeLists.txt
├── sdkconfig.defaults
├── sdkconfig.application.defaults
├── partitions.csv
├── assets_src/              # PNG、TMJ、WAV 和生成脚本
├── assets/generated/        # .atlas、.map、.sound、稳定资源 ID
├── game.sim.json            # Host 同源编译清单
└── main/
    ├── main.c               # 调用 mosaico_game_app_run()
    ├── <game>_app.c/.h      # 仅设备：资源、触区、音频、update/render 回调
    ├── <game>.c/.h          # 无 ESP-IDF 依赖的玩法模型
    ├── <game>_view.c/.h     # Host 与设备共用的 Raylib 绘制
    ├── game_module.c        # 仅 Host：生命周期和输入映射
    ├── scene.json.in
    └── CMakeLists.txt
```

项目在顶层 CMake 中声明能力，不再逐项复制组件目录：

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/raylib_lite_engine.cmake")
mosaico_game_sdk_configure_gsp_compiler()
mosaico_game_sdk_add_components(RAYLIB AUDIO TILEMAP SCENE UI FX SAVE)
```

只请求实际使用的能力。`RAYLIB` 自动带入 core、iris、input、debug、assets、2d、
显示 port 和 `mosaico_game_app`；其余能力均为可选层。

Scene/UI/FX 使用固定容量对象，不在帧热路径分配内存。Save 将游戏自有 payload
封装为带版本号、长度与 CRC32 的 NVS blob；版本不一致时只通过项目提供的 migration
callback 升级，频繁变化的数据用 debounce 合并写入。

## 一帧的生命周期

设备上由 `mosaico_game_app` 驱动：

1. 触摸任务或 ESP-Iris RPC 写入统一事件队列。
2. Runner 取出事件，注入 Raylib 主触点兼容视图，并更新 Action Mapper。
3. 项目 `on_update` 在固定时间步内更新纯玩法状态。
4. `BeginDrawing()` 借用一个空闲 PSRAM framebuffer。
5. 共用 view 用 Raylib 兼容调用写 RGB565；Atlas 按需执行 binary-alpha/A8 混合。
6. `EndDrawing()` 非阻塞提交给 GSP；GSP 回调释放 framebuffer。
7. 屏幕镜像从最近一帧复制稳定快照，不直接占用 LCD/USB。

不要在 update/render 热路径动态分配对象、解析源资源或执行阻塞 I/O。

本地游戏闭环统一使用 `python3 mosaico.py game sim projects/<game>`。它根据
`game.sim.json` 编译同一份玩法和 view，用 Host RGB565 runner 做确定性输入、像素和
状态验证。`game sim` 不启动 GSP。设备 Canvas 提交只通过 `game build`、`iris app-update`、
ESP-Iris 日志和截图验证。协议见 [`host-simulator.md`](host-simulator.md)，兼容 API 见
[`raylib-api.md`](raylib-api.md)。

## 触摸输入边界

板载 CST9217/CST9220 系列硬件最多支持两个同时触点。BSP 已集成
`78/esp_lcd_touch_cst92xx` 0.1.0，通过标准 `esp_lcd_touch` API 返回两个触点和
track ID；游戏项目必须设置 `CONFIG_ESP_LCD_TOUCH_MAX_POINTS>=2`。Sky Hop 已将
两个触点的 down/move/up 生命周期映射为“移动 + 跳跃”并发动作。

平台事件层应保留两个触点的 ID、坐标和 down/move/up 生命周期。Raylib mouse 状态只是主触点兼容视图，不能作为多点输入的内部数据模型。这样才能可靠支持“左手移动、右手跳跃”、双指手势和触点交叉而不跳号。

## 资源和音频

构建工具将源文件转换为：

- `.atlas`：RGB565 加可选 A8，使用 FNV-1a 稳定 frame ID；
- `.map`：有限正交 Tiled 地图、对象与路径；
- `.sound`：24 kHz mono，短音效 PCM16，长音频 IMA-ADPCM；
- `game_assets.bin`：ESP-Iris system-update 写入的 mmap 分区镜像。

资源总量当前不得超过 1 MiB。项目应保留资源生成脚本与源文件，生成结果必须可重复。

## 启动与恢复约束

正常应用必须在显示初始化前启动管理面：

Raylib 游戏通过 `mosaico_game_app_run()` 走同一条启动路径，项目 `main.c` 只提供配置回调：

1. 初始化 NVS；
2. 由 `mosaico_game_iris` 注册 system inventory；
3. 调用 `iris_ota_support_start()`；
4. 初始化电源、显示、资源和游戏；
5. 首帧成功提交后调用 `esp_iris_mark_healthy()`。

正常固件保持 `CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY=y`，不包含 OTA writer。空白或未验证设备首次安装先执行 `python mosaico.py recover`，新应用、分区表或外部资源变化优先通过 `python mosaico.py iris system-update --project ...` 安装；完整分区表一致且只改代码时使用 `iris app-update`。保留 Recovery 固定分区前缀，不应为通过 app-update 而套用设备原来的可变分区布局。

## 参考项目

- `raylib_shooter`：最小 Raylib 快速绘制与固定对象池；
- `tower_defense`：Atlas、Tiled、音频、Host replay 的完整资源化项目；
- `sky_hop`：横版物理、卷轴相机、双触点 Action Mapper、动画与原创资源生成流水线。

三者都用同源 view、`game.sim.json` 和 `mosaico_game_app_run()`。

后续平台能力、交付顺序和效率指标见
[`game-platform-upgrade-plan.md`](game-platform-upgrade-plan.md)。
