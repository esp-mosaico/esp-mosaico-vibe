# ESP-Iris Crash 诊断专项固件

本夹具只用于实机破坏性测试。它保留产品的 Recovery 分区契约，通过
`mosaico.py rpc` 注入计划重启、断言、非法内存访问、任务看门狗以及连续三次
启动崩溃。USB High-Speed 始终归 ESP-Iris 所有，不能同时打开串口会话。

Service ID 为 `0x6A03`，方法如下：

| Method | 行为 |
| --- | --- |
| 1 | 返回当前 Boot ID、崩溃计数和健康状态 |
| 2 | 延迟触发断言 |
| 3 | 延迟触发非法内存写 |
| 4 | 延迟触发任务看门狗 panic |
| 5 | 标记并执行计划重启 |
| 6 | 标记计划重启，并在随后三次启动中断言崩溃 |
| 7 | 显式清除崩溃循环计数 |

方法 2–6 的请求为一个小端 u64 测试 token。自动验收入口为：

```text
python3 tests/iris_crash_acceptance/run.py
```

运行器将证据写入 `.codex-runs/iris-crash/`，最后重新安装
`projects/hello_world`。测试报告必须保留 Device ID、每次 Boot ID、安装 operation、
原始 Core Dump、精确 ELF SHA-256、解码器输出和源码位置。
