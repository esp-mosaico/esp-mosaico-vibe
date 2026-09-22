# 游戏开发入口

[返回文档索引](README.md)

本工作区不再托管 Sky Hop、Tower Defense、Raylib Shooter 等游戏示例。
Raylib 兼容运行时、Host 仿真、资源管线和参考实现以
[Raylib Lite Engine](../submodule/raylib-lite-engine/README.md) 为准。

```sh
git submodule update --init submodule/raylib-lite-engine
```

从引擎仓库继续：

- 概览与集成：[README](../submodule/raylib-lite-engine/README.md)
- 组件职责：[components](../submodule/raylib-lite-engine/components/README.md)
- Host 仿真与回放：`python mosaico.py game sim`（实现见引擎 `host/`、`tools/`）

工作区只保留 CMake 适配 `cmake/raylib_lite_engine.cmake`。普通应用的 Recovery
契约仍以 [GSP Hello World](../projects/hello_world/README.md) 为准；不要把
Hello World 当成游戏模板。
