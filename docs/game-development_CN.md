# 游戏创建与仿真

[English](game-development.md) | [返回索引](README_CN.md)

游戏类应用优先使用 **Raylib Lite Engine**（`submodule/raylib-lite-engine`）开发，
先在仿真器中验证画面和玩法，修复问题后再安装到设备。
三个完整游戏由 BSP `examples/` 维护；通用运行时、绘制、资源工具与 Host 仿真
由引擎维护。BSP 示例可单独克隆构建，所需固定依赖由示例声明并自动获取。

```sh
git submodule update --init submodule/esp-mosaico-utils submodule/esp-mosaico-bsp submodule/raylib-lite-engine
python mosaico.py game create my_game --template sky-hop
python mosaico.py game sim --project projects/my_game
python mosaico.py game sim --project projects/my_game --headless --frames 120
python mosaico.py game build --project projects/my_game
python mosaico.py iris system-update --project projects/my_game
```

可选模板为 `sky-hop`、`tower-defense`、`shooter`。`game new` 等同于 `game create`；
创建支持 `--dry-run`，拒绝覆盖。Host 需要 C 编译器与 Pillow；固件使用满足项目
约束并支持 ESP32-S31 的 ESP-IDF。交互仿真省略 `--headless`。

交互仿真使用与设备共用的 C 玩法模型和绘制代码，检查画面、动画、输入反馈和完整
游玩流程；按游戏实际玩法检查碰撞、计分、胜负和重开。录制输入后可通过 headless
回放重复验证。仅成功启动或运行固定帧数不能证明游戏效果正确，应查看画面、操作
结果，并在修复后复验受影响的流程。

真机阶段再验证物理按键、触摸、屏幕、音频及实际性能；Host 仿真结果不能代替这些
硬件验证。

- [Sky Hop](../submodule/esp-mosaico-bsp/examples/sky_hop/README.md)
- [Tower Defense](../submodule/esp-mosaico-bsp/examples/tower_defense/README.md)
- [Raylib Shooter](../submodule/esp-mosaico-bsp/examples/raylib_shooter/README.md)
- [游戏开发细节](../submodule/esp-mosaico-bsp/docs/game-development.zh-CN.md)
- [引擎接口与 Host](../submodule/raylib-lite-engine/README.md)
