# Sky Hop

Sky Hop is a four-level scrolling platform game. Level data is kept in the
host-testable gameplay model: progress, score, and remaining lives carry into
the next level, while completing level 4 finishes the run.

原创横版平台跳跃 Demo，用于验证 Mosaico Raylib Game SDK。

## 操作

- 点击标题卡开始。
- 屏幕底部左侧 `<` 向左移动，中间 `>` 向右移动，右侧 `JUMP` 跳跃。
- 点击右上角暂停按钮暂停/继续；Button、Joystick 和 IMU 事件会映射到相同 Action。
- 收集金币、踩掉紫色巡逻怪，并抵达关卡最右侧。

本版本使用平台 `Camera2D` 世界坐标渲染，加入场景滑入 Tween、固定容量粒子、
暂停场景与 NVS 最高分存档。游戏更新模型仍可脱离 ESP-IDF 在 Host 上测试。

当前同时嵌入一份只读 Atlas/音频作为资源分区不可用时的恢复兜底。正常情况下仍
优先读取 `game_assets` 分区；修复 Recovery system-update 通道后应取消整包嵌入，
以恢复约 300 KiB 应用空间。

## 构建与安装

```bash
python mosaico.py game run --project projects/sky_hop
python mosaico.py game run --project projects/sky_hop --headless --frames 300
python mosaico.py game build --project projects/sky_hop
python mosaico.py recover  # 空白或未验证设备首次安装前
python mosaico.py iris app-update --project projects/sky_hop
python mosaico.py iris logs
```

浏览器模拟器地址为 `http://127.0.0.1:8460/`。键盘使用 `A/D` 或方向键移动、
空格跳跃、`P` 暂停、回车开始/进入下一关；触屏设备可同时按住底部移动键和跳跃键。
模拟器直接编译并调用设备相同的 `platform_game.c`，所以关卡、碰撞、分数和状态切换
不需要在网页端重复实现。网页使用轻量 Host 绘制来快速验玩法，最终 Atlas、音频和 GSP
效果仍以真机为准。

应用保留 factory Recovery，并通过 `iris_ota_support_start()` 暴露进入 Recovery 的 RPC。
