# ESP 电源模式与设置

| 模式 | 怎么设置 | 保留能力 |
| --- | --- | --- |
| 固定高频 | `esp_pm_configure()` 的 min/max 都设 320 MHz | 正常运行，作为功耗对照 |
| DFS | 开启 `CONFIG_PM_ENABLE`，配置 40–320 MHz，`light_sleep_enable=false` | 在线通信、显示和输入；空闲降频 |
| 自动 light sleep | DFS 基础上启用 `CONFIG_FREERTOS_USE_TICKLESS_IDLE`，`light_sleep_enable=true` | 由空闲调度及 PM 锁决定进入；Wi-Fi/USB 需支持对应协同 |
| 手动 light sleep | 排空外设 → 配置唤醒源 → `esp_light_sleep_start()` → 恢复外设 | RAM 和进程保留，函数返回后继续执行 |
| Deep sleep | 持久化必要状态 → 配置唤醒源 → `esp_deep_sleep_start()` | 唤醒重新启动，普通 RAM/任务不保留 |

## DFS 示例

```c
#include "esp_pm.h"

esp_pm_config_t config = {
    .max_freq_mhz = 320,
    .min_freq_mhz = 40,
    .light_sleep_enable = false,
};
ESP_ERROR_CHECK(esp_pm_configure(&config));
```

40 MHz 是本板可用的候选下限；先完成 [显示 DMA 保护](dfs-display.md)。配置下限不等于固定在该频率，20 MHz 尚不作为默认值。需要降频时让空闲任务阻塞，检查 PM 锁是否长期持有。

## Light sleep 进退顺序

1. 停止动画、等待最后一帧及 DMA 完成；按需求保留面板画面或关闭显示。
2. 暂停使用总线的任务；手动睡眠路径先处理 Wi-Fi/USB，见 [外设配置](sleep-peripherals.md)。
3. 按板级引脚配置触摸/按键/IMU 唤醒；检查空闲电平，避免立刻误唤醒。定时唤醒使用 `esp_sleep_enable_timer_wakeup()`。
4. 检查唤醒配置返回值，调用 `esp_light_sleep_start()`；失败走外设恢复路径。
5. 返回后恢复外设、用户亮度和页面，再处理输入。

保留触摸/IMU 唤醒时不能切断对应芯片供电。Deep sleep 的唤醒源需使用目标芯片支持的接口，不能直接照搬 light sleep GPIO 配置。产品息屏时间与 DFS 停动画阈值分别设置。
