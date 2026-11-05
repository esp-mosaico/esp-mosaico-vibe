# Mosaico Hop

基于 Raylib Lite Engine 的 `sky-hop` 模板，面向 ESP-Mosaico 480×480 圆屏触控的四关横版跳跃游戏。关卡进度、分数和生命会带入下一关，通过第 4 关后本局结束。

## 玩法

- 标题画面任意点击开始。
- 屏幕底部 **LEFT** / **RIGHT** 移动，**JUMP** 跳跃。可同时按住移动和跳跃。
- 右上角暂停按钮可暂停或继续。
- 收集金币、从上方踩掉紫色巡逻怪，并走到关卡最右侧。
- 掉落或撞到怪物会损失生命；生命耗尽后可点击重开。

Host 与设备共用同一套 C 玩法和 RGB565 绘制。音频、LCD 时序和物理触摸需要在真机上确认。

## 在本工作区中运行

在 `esp-mosaico-vibe` 根目录：

```sh
python3 mosaico.py game sim --project projects/mosaico_hop
python3 mosaico.py game sim --project projects/mosaico_hop --headless --frames 120 \
  --scenario projects/mosaico_hop/scenarios/touch_run.json \
  --state-output /tmp/mosaico_hop_state.json
python3 mosaico.py game build --project projects/mosaico_hop --target iris
```

浏览器仿真默认地址是 `http://127.0.0.1:8460/`。键盘：`A`/`D` 或方向键移动，空格跳跃，`P` 暂停，回车开始或进入下一关。

设备安装只使用 `mosaico.py`，不要用 `idf.py flash` 或 `esptool` 写设备。本机已有 Claw 基线固件时，先按需进入 Vibe Mode，再用构建打印出的 System Update 包更新：

```sh
python3 mosaico.py recover
python3 mosaico.py iris system-update --project projects/mosaico_hop --bundle /path/to/mosaico_hop-system-update.irisfw
```

`--bundle` 使用 `game build --target iris` 实际打印的路径。
