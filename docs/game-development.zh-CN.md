# 游戏创建与仿真

[返回索引](README.md)

三个完整游戏由 BSP `examples/` 维护；通用运行时、绘制、资源工具与 Host 仿真
由 Raylib Lite Engine 维护。工作区保留统一命令入口：

```sh
git submodule update --init submodule/esp-mosaico-utils submodule/esp-mosaico-bsp submodule/raylib-lite-engine
python mosaico.py game create my_game --template sky-hop
python mosaico.py game sim --project projects/my_game --headless --frames 120
python mosaico.py game build --project projects/my_game
python mosaico.py iris system-update --project projects/my_game
```

可选模板为 `sky-hop`、`tower-defense`、`shooter`。`game new` 等同于 `game create`；
创建支持 `--dry-run`，拒绝覆盖。Host 需要 C 编译器与 Pillow；固件使用满足项目
约束并支持 ESP32-S31 的 ESP-IDF。交互仿真省略 `--headless`。

- [Sky Hop](../submodule/esp-mosaico-bsp/examples/sky_hop/README.md)
- [Tower Defense](../submodule/esp-mosaico-bsp/examples/tower_defense/README.md)
- [Raylib Shooter](../submodule/esp-mosaico-bsp/examples/raylib_shooter/README.md)
- [游戏开发细节](../submodule/esp-mosaico-bsp/docs/game-development.zh-CN.md)
- [引擎接口与 Host](../submodule/raylib-lite-engine/README.md)

保留 Recovery 分区和 ESP-Iris 操作流程；首次安装在空白/未验证设备上先执行
`python mosaico.py recover`。不直接照搬其他 BSP 示例的 IDF 刷写步骤。
