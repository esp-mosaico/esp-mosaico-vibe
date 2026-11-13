---
name: mosaico-power
description: >
  Configure ESP-Mosaico power modes and look up ESP32-S31, OLED, touch and
  Wi-Fi power settings and measured board currents. Use for 功耗、DFS、
  light sleep、显示待机与触摸唤醒 requests.
---

# ESP-Mosaico 功耗配置

适用 ESP32-S31、CO5300 OLED、CST9220 触摸；其他硬件先核对驱动。

| 要查什么 | 入口 |
| --- | --- |
| 运行、DFS、light sleep、deep sleep 怎么设置 | [ESP 电源模式](references/runtime-modes.md) |
| DFS 下显示传输怎么保护 | [DFS 与 DMA](references/dfs-display.md) |
| 显示、触摸、Wi-Fi、USB 怎么省电和恢复 | [外设配置](references/sleep-peripherals.md) |
| 各模式电流、亮度与颜色对照 | [功耗数据](references/measurements.md) |

先选择需要保留的显示、联网与唤醒能力，再配置对应模块。临时调暗或睡眠退出后恢复用户亮度；没有画面变化不提交帧，后台任务尽量等待事件。
遵循工作区固定 SDK、构建和 `mosaico.py` 设备操作规则。
