# 持续集成与主机检查

[返回文档索引](README.md)

## 本地检查

初始化测试涉及的子模块后，在工作区根目录执行：

```sh
python -m pip install -r requirements-ci.txt
python -m pytest -q tests --ignore=tests/firmware
python -m pytest -q submodule/esp-mosaico-utils/esp-mosaico-recovery/tests
python mosaico.py --version
```

工具测试依赖与产品实现由固定版本的 `esp-mosaico-utils` 维护。涉及 Recovery C
代码的主机测试还使用 cJSON、LVGL 和 OpenSSL；依赖版本、环境变量及平台准备步骤
以 [CI 工作流](../.github/workflows/ci.yml)为准。

Windows 上，Recovery HTTP 授权主机测试不适用，应添加：

```text
--ignore=submodule/esp-mosaico-utils/esp-mosaico-recovery/tests/test_http_update_authorization_host.py
```

CI 的 Windows 命令还明确排除 4 个硬编码 POSIX 路径显示的测试，完整选择器见工作流。
这些测试仍在 Linux 和 macOS 上执行。

## CI 覆盖范围

GitHub Actions 在面向 `main` 的 Pull Request、`main` push 和手动触发时运行：

- 主机矩阵：原生 Linux、macOS、Windows，Python 3.8 与 3.12；执行工作区和工具测试。
- 固件矩阵：GSP Hello World、由 `project init` 生成的应用、ESP-Iris 验收固件
  和保留 Recovery。
- GSP 检查：编译 PC bridge，并渲染一张 480×480 的无界面帧。

固件任务使用 GitHub 托管的 Ubuntu runner，安装工作流固定 revision 的 ESP-IDF
及 ESP32-S31 preview 工具链，再通过 low-noise runner 检查环境与构建。
ESP-IDF revision、矩阵和参数统一维护在工作流中。测试报告、构建日志和成功生成的
固件产物保留 14 天；分支保护可将 `CI / required` 配置为必选检查。

CI 不发现或操作真机，也不发布正式 Release。构建通过不代表设备验收通过；需要
真机交付时，按[CLI 更新流程](mosaico-cli.zh-CN.md#选择更新方式)保存设备身份、
新 Boot ID、固件身份、健康状态及产品行为证据。
