# 工作区验证

[返回索引](README.md)

自动 CI 只初始化 utils，验证 CLI 入口、空工作区创建、dry-run、拒绝覆盖、
自定义模板、工程选择和含空格路径下整个工作区移动：

```sh
python3 -m unittest discover -s tests/mosaico_cli_integration -v
```

手动 `Hello World integration` 工作流从模板生成临时工程，再执行固件构建、
GSP sim_bridge、480×480 渲染和实际交互验证；不使用预置 Hello World。

实现验证在所属仓库运行：utils 的 `mosaico-tools/tests` 和
`esp-mosaico-recovery/tests`、BSP 的 `tests/games`、引擎的 `tests`。
固件夹具位于 utils 的 `esp-mosaico-recovery/tests/firmware/`。
真机测试通过工作区 `mosaico.py`，证据保存同一 Device ID、各阶段 Boot ID、
Recovery ready、目标固件身份、healthy 和原始日志。
