# 游戏开发入口

[返回文档索引](README.md)

本工作区不再托管 Sky Hop、Tower Defense、Raylib Shooter 等游戏示例。
Raylib 兼容运行时、Host 仿真、资源管线和参考实现以
[Raylib Lite Engine](../submodule/raylib-lite-engine/README.md) 为准。

```sh
git submodule update --init submodule/raylib-lite-engine
```

Host 仿真在**引擎仓库根目录**运行，不要用本仓库的 `python mosaico.py game`
（那是旧包装，指向子模块里过期的 CLI）：

```sh
cd submodule/raylib-lite-engine
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/sky_hop
python3 tools/game_cli.py sim examples/last_zone_extraction --headless --frames 90
```

浏览器预览为 `http://127.0.0.1:8460/`。这不是 [GSP 仿真](../tools/gsp-sim/README.md)。
完整契约见引擎 [游戏开发指南](../submodule/raylib-lite-engine/docs/game-development.zh-CN.md)。

从引擎仓库继续：

- 概览与集成：[README](../submodule/raylib-lite-engine/README.md)
- 组件职责：[components](../submodule/raylib-lite-engine/components/README.md)
- 绘制总表：[game-drawing-inventory](../submodule/raylib-lite-engine/docs/game-drawing-inventory.zh-CN.md)

工作区只保留 CMake 适配 `cmake/raylib_lite_engine.cmake`。普通应用的 Recovery
契约仍以 [GSP Hello World](../projects/hello_world/README.md) 为准；不要把
Hello World 当成游戏模板。
