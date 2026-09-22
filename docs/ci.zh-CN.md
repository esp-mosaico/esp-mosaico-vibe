# 入口检查与按需集成验证

[返回文档索引](README.md)

本仓库是开发入口。自动 CI 只检查工作区与固定版本工具的连接；组件自身的
跨平台兼容性、单元测试和固件构建应由所属组件仓库维护，本仓库不再运行全量
主机测试矩阵、Recovery 构建或 ESP-Iris 验收固件构建。

## 自动入口检查

[CI 工作流](../.github/workflows/ci.yml)在面向 `main` 的 Pull Request、
`main` push 和手动触发时运行一个 Linux / Python 3.12 任务：

- 检查 `mosaico.py --version` 能加载固定版本的工具。
- 检查工作区配置、默认应用和资源路径。
- 从 Hello World 模板初始化项目，检查生成内容和引用。

只初始化 `esp-mosaico-utils` 顶层子模块，不安装 ESP-IDF、GSP 或额外 Python
依赖，不构建固件。保留原有 `CI / required` 检查名称供分支保护使用；它现在只
代表入口检查通过，不代表固件构建或设备验收通过。

本地运行同样的检查：

```sh
git submodule update --init submodule/esp-mosaico-utils
python mosaico.py --version
python -m unittest discover -s tests/mosaico_cli_integration -v
```

## 手动 Hello World 集成验证

更新子模块版本、参考应用或构建与仿真集成后，可在 GitHub Actions 中选择
**Hello World integration → Run workflow**，选择需要验证的分支后运行。
[手动工作流](../.github/workflows/integration.yml)只通过 `workflow_dispatch`
触发，不随 push 或 Pull Request 自动运行，也不作为自动入口检查的依赖。

该工作流初始化 BSP 与工具依赖，安装固定 revision 的 ESP-IDF，然后：

- 检查 ESP-IDF 环境并构建 `projects/hello_world`。
- 编译 GSP PC bridge 并运行交互验证。
- 运行无界面仿真，检查输出图像为 480×480。

ESP-IDF revision 和构建参数维护在手动工作流中。构建日志、成功生成的固件、
交互证据和截图保留 14 天。

## 其他本地验证

现有 `tests/`、验收固件与 `requirements-ci.txt` 保留供按需验证使用，不属于
自动 CI。运行全量工作区测试时，初始化相关子模块并准备对应测试依赖：

```sh
git submodule update --init --recursive
python -m pip install -r requirements-ci.txt
python -m pytest -q tests --ignore=tests/firmware
```

涉及 C 编译、GSP 或游戏仿真的测试还需要各自的编译器、资源和主机依赖；上述
Python 依赖安装不代替这些准备。组件内部测试按所属组件的文档运行，参见
[组件资料](README.md#专项与组件资料)。

CI 不发现或操作真机，也不发布正式 Release。构建通过不代表设备验收通过；需要
真机交付时，按[CLI 更新流程](mosaico-cli.zh-CN.md#选择更新方式)保存设备身份、
新 Boot ID、固件身份、健康状态及产品行为证据。
