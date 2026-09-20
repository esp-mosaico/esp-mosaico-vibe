# VS Code ESP-IDF 扩展配置

命令行安装与 bin 验证通过后，**若用户要用 VS Code ESP-IDF 扩展编译**，或主动询问 IDE 配置，按本节输出三个环境变量及 `idf.customExtraVars` 配置。纯命令行用户可跳过。

官方文档：[在 VS Code 的 ESP-IDF 扩展中手动配置 ESP-IDF 与工具](https://docs.espressif.com/projects/vscode-esp-idf-extension/zh_CN/latest/installation.html#vs-code-esp-idf-esp-idf)

---

## 需要配置的三个变量

| 变量 | 含义 |
|------|------|
| `IDF_PATH` | ESP-IDF 源码目录（即 `YOUR_INSTALL_PATH` 的实际值） |
| `IDF_TOOLS_PATH` | 工具链根目录（默认 `~/.espressif` / `%USERPROFILE%\.espressif`，或用户指定的 `YOUR_TOOLS_PATH`） |
| `IDF_PYTHON_ENV_PATH` | IDF Python 虚拟环境目录（由 `install` + `export` 自动创建，路径因 IDF 版本和 Python 版本而异） |

> **注意：** 若系统或 shell 中已全局设置上述变量，扩展会**优先使用环境变量**而非 `idf.currentSetup` 所选配置。使用扩展内「选择 ESP-IDF 版本」时，需清除冲突的全局变量。详见官方文档说明。

> **路径格式：** VS Code 设置中 `~`、`%VARNAME%`、`$VARNAME` **均无效**。须写绝对路径，或使用 `${env:HOME}` 等形式。

---

## 获取实际路径（编译验证同一终端内执行）

必须在 **已执行 export 的同一 session** 中读取。自定义工具链路径时，export 前先设 `IDF_TOOLS_PATH`（与编译步骤相同）。

Linux / macOS：
```bash
echo "IDF_PATH=$IDF_PATH"
echo "IDF_TOOLS_PATH=$IDF_TOOLS_PATH"
echo "IDF_PYTHON_ENV_PATH=$IDF_PYTHON_ENV_PATH"
```

也可用官方推荐命令一次性导出（需先 source export）：
```bash
python "$IDF_PATH/tools/idf_tools.py" export --format key-value
```

Windows CMD：
```bat
echo IDF_PATH=%IDF_PATH%
echo IDF_TOOLS_PATH=%IDF_TOOLS_PATH%
echo IDF_PYTHON_ENV_PATH=%IDF_PYTHON_ENV_PATH%
```

Windows PowerShell：
```powershell
echo "IDF_PATH=$env:IDF_PATH"
echo "IDF_TOOLS_PATH=$env:IDF_TOOLS_PATH"
echo "IDF_PYTHON_ENV_PATH=$env:IDF_PYTHON_ENV_PATH"
```

将命令输出的**实际路径**整理后交给用户，不要猜测或使用占位符。

---

## `idf.customExtraVars` 配置示例

用上方命令得到的实际路径替换示例中的路径。

Linux / macOS：
```json
{
  "idf.customExtraVars": {
    "IDF_PATH": "/home/user/esp/esp-idf-YOUR_IDF_VERSION",
    "IDF_TOOLS_PATH": "/home/user/.espressif",
    "IDF_PYTHON_ENV_PATH": "/home/user/.espressif/python_env/idfYOUR_MAJOR_MINOR_py3.12_env"
  }
}
```

Windows（反斜杠须写成 `\\`）：
```json
{
  "idf.customExtraVars": {
    "IDF_PATH": "C:\\esp\\esp-idf-YOUR_IDF_VERSION",
    "IDF_TOOLS_PATH": "C:\\Users\\user\\.espressif",
    "IDF_PYTHON_ENV_PATH": "C:\\Users\\user\\.espressif\\python_env\\idfYOUR_MAJOR_MINOR_py3.12_env"
  }
}
```

配置入口：命令面板 → `Preferences: Open Settings (JSON)`，或项目 `.vscode/settings.json`。
