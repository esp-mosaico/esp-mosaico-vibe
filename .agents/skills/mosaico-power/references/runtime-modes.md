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
3. 仅配置应用需要的唤醒源，核对板级引脚和空闲电平，避免立刻误唤醒。额外启用 IMU 或 `esp_sleep_enable_timer_wakeup()` 时单独验证，不沿用为默认配置。
4. 检查唤醒配置返回值，调用 `esp_light_sleep_start()`；失败走外设恢复路径。
5. 返回后恢复外设、用户亮度和页面，再处理输入。

保留触摸/IMU 唤醒时不能切断对应芯片供电。Deep sleep 的唤醒源需使用目标芯片支持的接口，不能直接照搬 light sleep GPIO 配置。产品息屏时间与 DFS 停动画阈值分别设置。

## Vibe 实现与版本边界

以下是 [功耗 MR !46](https://gitlab.espressif.cn:6688/ae_group/mosaico/esp-mosaico-vibe-mode/-/merge_requests/46) 的 `f84fc9a122f49ea48cf2b2697bb12412df959da1` 的历史实现参考；该 MR 已并入发布流程，以下细节仍只对应所列提交，不能替代所用固件版本的配置与实板验收。实现细节以该版本的[模式说明](https://gitlab.espressif.cn:6688/ae_group/mosaico/esp-mosaico-vibe-mode/-/blob/f84fc9a122f49ea48cf2b2697bb12412df959da1/vibe-mode/docs/power-tests/power-modes.zh-CN.md)为准。

| 状态 | 进入与配置 | 显示、通信与退出 |
| --- | --- | --- |
| 正常运行 / DFS | `CONFIG_VIBE_DFS_ENABLE=y`，40–320 MHz，自动 light sleep 关闭 | 随任务与 PM 锁调频，不能按下限推断实际驻留 |
| 在线空闲 | 输入和业务同时安静 15 秒，页面及外设准入通过 | 停动画、AOD 20%、每秒业务轮询；首次触摸执行操作，首次 AI 仅恢复当前页 |
| 待机过渡 | `CONFIG_VIBE_LIGHT_SLEEP_ENABLE=y`；首页 AI 或自动息屏超时 | 约 900 ms 动画后排空最终帧；保留 AOD，USB 主机仍活跃时延后硬件睡眠 |
| 手动 light sleep | 业务、存储、外设和唤醒源准备通过；USB 条件见[外设配置](sleep-peripherals.md#usb) | 停 Wi-Fi，面板保持供电与画面；AI、触摸或 USB host resume 唤醒同一进程，回首页并恢复用户亮度、联网 |

自动息屏默认“永不”，已有 NVS 设置优先；可选 1/2/5/10 分钟。“永不”不禁用在线空闲或首页 AI 手动睡眠。15 秒在线空闲本身不触发硬件睡眠。
该版本不初始化 IMU、不配置运动唤醒或周期定时查电量；睡前挂起未使用的 IMU。触摸维持正常扫描；睡眠唤醒的触摸/AI 消费到释放，避免回首页误操作。
板级验收范围及尚缺的验证见 [USB](sleep-peripherals.md#usb)；历史电流不能用于证明该版本功耗。
