# Host simulator protocol v1

本地仿真直接用 Host C 编译器编译游戏的玩法和绘制源码，并把设备使用的
`mosaico_raylib_fast` 与 `mosaico_game_2d` 一起链接。它不依赖 Emscripten、WASM、
SDL 或系统 Raylib，最终帧仍是 480×480 RGB565。`game sim` 统一使用确定性 Host
runner，显示共享 C view 的像素结果，不是 Python/HTML 重画的设计原型。

项目用根目录 `game.sim.json` 声明同源文件：

```json
{
  "schema": "mosaico-game-sim/v1",
  "sources": ["main/game_module.c", "main/game.c", "main/game_view.c"]
}
```

`game.c` 和 `game_view.c` 同时进入设备固件与 Host。`game_module.c` 只描述 Host
生命周期和输入映射，通过 `mosaico_game_module_v1()` 返回固定 ABI 描述；通用
`host_module_bridge.c` 负责分配状态、注入输入和提供 RGB565 framebuffer。新增项目
不需要再编写完整的 `host_adapter.c`。Sky Hop、Tower Defense 与 Raylib Shooter 已迁到
`game.sim.json` 同源 view；设备侧 `main.c` 只调用 `mosaico_game_app_run()`。

常用命令（在仓库根目录执行；三个参考游戏同一套）：

```bash
python3 mosaico.py game sim projects/raylib_shooter
python3 mosaico.py game sim projects/sky_hop --headless --frames 300
python3 mosaico.py game sim projects/tower_defense --headless --frames 300
python3 mosaico.py game sim projects/raylib_shooter --headless \
  --scenario my_replay.json --state-output artifacts/state.json
```

`--scenario` 需要已有回放文件；仓库不自带 `scenario.json`。可在 GUI 里 Record
后下载，或按下面的事件格式手写。

`game sim` 始终走确定性 Host runner，不编译、不链接、不启动 ESP-GSP。它用于暂停、
单步、变速、重置、截图、录制和输入回放。Host runner 默认监听 `127.0.0.1:8460`，
HTTP 接口为：

- `GET /api/v1/info`、`GET /api/v1/state`、`GET /api/v1/frame`；
- `POST /api/v1/input`，传 action、最多两个 pointer 和可选 `{x,y,z}` IMU；
- `POST /api/v1/control`，执行 pause、resume、step、reset、speed 和 recording。

Headless 场景按固定帧处理以下事件：

- `action`：`code` 为 `left/right/jump/pause/restart` 或数值，带 `pressed`；
- `pointer`：`track`、`x`、`y` 和 `pressed`；
- `tap`：一帧内产生 down/up；
- `imu`：`x`、`y`、`z` 浮点加速度；
- `pause`、`resume`、`step`、`reset`：控制仿真时钟。

精确坐标、多点触摸与 IMU 均通过 Host runner 验证。ESP-GSP 设备调度、TE 时序、
LCD 总线吞吐和物理 IMU 噪声仍需用设备 benchmark、日志与 ESP-Iris 截图确认。
