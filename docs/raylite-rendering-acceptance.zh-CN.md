# raylib-lite 渲染优化验收（2026-09-16）

本轮完成不透明纹理缩放优化与正确性、构建、设备启动验收。
动态战斗最差帧率及与 MicroPixel 的同负载对比尚未验收。

## 改动范围

`submodule/raylib-lite-engine/components/mosaico_game_2d/mosaico_game_2d.c`
的不透明、白色 tint、无旋转缩放路径，以 64 个源 X 坐标为一块复用查表，
避免每一行重新计算水平采样。工作区为 256 字节栈空间，无堆分配。
保留原有最近邻采样、翻转、裁剪和越出源纹理时跳过写入的语义。
没有更改分辨率、射线数量或游戏画质设置。

工作区既有的护甲、油桶和关卡修改随当前固件一起构建安装；本轮未提交代码。

## 验证结果

- Game SDK 27 项回归通过，包括当前 Neon Maze 模型测试。
- 100 组墙柱、400 组地板/span、100 组不透明缩放参考像素对照通过。
  缩放覆盖整数目标坐标、双轴翻转、越界源区域、裁剪、跨 64 项分块和 stride padding。
- 同一套参考测试分别运行于引擎 HEAD 基线与修改版，均通过。
- AddressSanitizer 和 UndefinedBehaviorSanitizer 检查通过。
- Host 120 帧仿真通过，已检查生成的 480×480 截图。
- ESP-IDF `v6.2-dev-1691-g8e7e0312303-dirty`、Python 3.10.12、ESP32-S31
  构建通过；固件 2653680 字节。构建仍有 factory 容量警告，安装使用保留 Recovery 的 OTA 路径。
- Iris 安装完成，应用连接且健康；设备 `4553502d49524953010030eda0f460c0`
  的 Boot ID 从 `3173115907640405528` 变为 `5144027171301442675`。

## 性能证据与边界

Host 基准使用相同编译参数，把 8×8 不透明纹理缩放到 480×205，循环 500 次：
基线约 0.176 ms/次，修改版约 0.052 ms/次。这个小纹理测试主要测采样开销，
不能据此推算实际纹理的 PSRAM/cache 表现或设备 FPS。

设备安装后出生位置 `(2.50, 3.50)`、开始提示界面，采集到：

| 指标 | 实测 |
| --- | --- |
| 显示帧率 | 27.8 FPS |
| 逻辑速率 | 29.8–30.8 Hz（日志统计窗口值） |
| render | 35.55–35.75 ms |
| sky | 1.58–1.67 ms |
| floor | 13.73–13.94 ms |
| wall | 13.13–13.29 ms |
| enemy | 0.114–0.115 ms |
| HUD | 6.61–6.98 ms |
| dropped / busy / superseded / errors / overflow | 均为 0 |

现有 wall 计时包含 BeginDrawing 与射线计算，HUD 计时包含 EndDrawing；
这些不能当作独立纯光栅内核耗时相加分析。阶段计时目前仍混在 SDK 统计结构中，
应后续迁回游戏 view，并分开 acquire、raycast、raster、submit。

升级前设备在另一位置 `(3.07, 8.87)` 的游戏中，约 23.5–23.6 FPS，
与升级后出生位置负载不同，不能作为本轮速度提升的对照证据。
当前设备数据只证明此界面稳定运行；未完成移动、开阔视野、多敌人与特效叠加的同场景 A/B。
本轮没有取得设备截图，也没有进行实体屏幕和声音的人工验收。

## 复现及原始记录

```sh
python3 -m unittest discover -s tests/game_sdk -v
python3 submodule/raylib-lite-engine/tests/test_columns.py
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 submodule/raylib-lite-engine/tests/test_columns.py
python3 mosaico.py game sim projects/neon_maze_25d --headless --frames 120
```

本地构建记录：`.codex-runs/mosaico/20260916T133737Z-game-build/raw.log`。
本地安装记录：`.codex-runs/mosaico/20260916T133818Z-install/raw.log`。
本轮便于对照的监控输出为 `/tmp/raylite-before.log` 与 `/tmp/raylite-after.log`；
临时文件不是长期保存的验收附件，原始 monitor 日志由 `.codex-runs/mosaico/` 保留。
