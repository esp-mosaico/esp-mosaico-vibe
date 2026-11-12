# DFS 与显示 DMA

适用于 PSRAM 帧缓冲、异步 SPI DMA 的显示路径。先检查驱动已有 PM 支持；需要应用侧保护时使用 `ESP_PM_CPU_FREQ_MAX`，不要把 `ESP_PM_APB_FREQ_MAX` 当成 CPU 最高频率约束。

## 设置顺序

1. 用 `esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "display", &lock)` 创建显示锁，检查返回值。
2. 同一显示所有者串行处理绘图、亮度、旋转和面板模式命令。
3. 提交前 `esp_pm_lock_acquire(lock)`，完成绘制和所有分块提交。
4. 等待驱动的完成事件/fence，确认最后一段 DMA 已结束，再 `esp_pm_lock_release(lock)`。
5. 提交或排空失败时保留约束，确认传输结束后再释放；不要在 DMA 读帧缓冲时改写或释放它。

`draw_bitmap()` 返回不代表异步 DMA 完成。按驱动接口等待传输，不虚构通用排空 API。

| 现象 | 优先检查 |
| --- | --- |
| 一直高频 | 可运行任务、轮询周期、未释放 PM 锁；可用 `esp_pm_dump_locks()` 检查 |
| 降频后花屏/abort | 回退已知可用频率，检查 PSRAM/XIP、锁范围和 DMA 尾部 |
| 调用读频率总是高频 | 当前任务本身正在运行；用驻留统计判断空闲降频 |
