# 内存观测

参考应用 `projects/hello_world` 启用 ESP-Iris 的任务栈观测。应用可以调用
`esp_iris_memory_get_heap()` 查询内部 RAM 和 SPIRAM 的当前、历史最低空闲字节，
调用 `esp_iris_memory_get_tasks()` 查询当前所有任务的历史最低栈余量。
后者由调用方提供数组；容量不足时返回 `ESP_ERR_INVALID_SIZE`，并在 `count`
中返回当前需要的容量。配置项 `CONFIG_FREERTOS_USE_TRACE_FACILITY=y` 和
`CONFIG_ESP_IRIS_TASK_MEMORY_OBSERVATION=y` 是全任务查询的前提。

连接设备后运行：

```sh
python mosaico.py memory
python mosaico.py memory --follow --interval 10
python mosaico.py --json memory --follow
```

`--follow` 在 PC 端轮询，默认每 5 秒一次；设备不会因此创建常驻采样任务。
JSON 模式每次采样输出一行。Gateway 的只读接口为
`GET /v1/devices/{device_id}/memory`。数据含 Device ID、Boot ID、两次查询的
设备运行时间、两个 heap 的字节数，以及每个任务的编号和历史最低栈余量。
任务编号只在当前 Boot ID 内有意义。任务创建或删除时，相邻两次采样的列表可能不同。

内部 RAM 和 SPIRAM 的历史最低空闲值是 ESP-IDF 分别记录的各 heap 区域
最低值之和；各区域触底可能并非同一时刻。它适合作为保守余量指标，
不能换算成精确的全局历史峰值。SPIRAM 总量为 0 表示当前固件没有可分配的
SPIRAM。全任务查询会临时分配状态数组并扫描任务栈；对时延敏感的应用应
加大轮询间隔，并测量查询时的最大停顿。超过 128 个任务时查询明确报错，
不会返回被截断的列表。
