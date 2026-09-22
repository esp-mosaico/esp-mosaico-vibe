# ESP-Mosaico Vibe

[English](README.md)

使用 AI 编程 Agent 开发 ESP-Mosaico 应用的入门工作区。仓库提供 CLI 入口、
工作区配置、固定版本子模块、文档和 Agent 指引。应用按需创建；全新克隆时没有
`projects/` 或 `components/` 目录。

## 创建应用

创建只需要 Python 3.8 或更新版本，无需提前安装 ESP-IDF、连接设备或初始化
BSP、游戏引擎。

```sh
git submodule update --init submodule/esp-mosaico-utils
python mosaico.py project init my_app
```

命令从 utils 维护的 Hello World 模板生成 `projects/my_app`，不修改默认工程。
`--dry-run` 不写文件，已有目标不会被覆盖。详见[工程创建](docs/project-init.zh-CN.md)。

## 预览和安装

```sh
git submodule update --init submodule/esp-mosaico-bsp
# 准备兼容的 ESP-IDF 环境，构建刚创建的应用。
python submodule/esp-mosaico-utils/mosaico-tools/skills/idf-low-noise-build/scripts/idf_low_noise_build.py --project projects/my_app doctor
python submodule/esp-mosaico-utils/mosaico-tools/skills/idf-low-noise-build/scripts/idf_low_noise_build.py --project projects/my_app build
python mosaico.py project sim --project projects/my_app --interactive
```

GSP 预览与设备使用相同的可移植 C UI 和 GSP 1.4.0 场景。
空白或未验证设备首次安装时，先执行 `python mosaico.py recover`，再执行
`python mosaico.py iris system-update --project projects/my_app`。
只有完整分区表和资源一致的代码更新才使用 `iris app-update`。
设备操作统一经过产品 CLI。

## 工作区职责

| 仓库 | 维护内容 |
| --- | --- |
| 本工作区 | 入口、配置、Agent 工作流、消费者集成验证 |
| [utils](submodule/esp-mosaico-utils) | CLI、Recovery、公共应用组件、Hello World 模板 |
| [BSP](submodule/esp-mosaico-bsp) | 板级支持和包含游戏在内的完整示例 |
| [Raylib Lite Engine](submodule/raylib-lite-engine) | 游戏运行时、渲染器、资源工具、Host 模拟器 |

游戏从 BSP `examples/` 创建，见[游戏入口](docs/game-development.zh-CN.md)；
仅开发游戏时初始化引擎。生成工程使用相对引用：移动或重新克隆整个工作区，
初始化固定依赖后重新构建。此布局不提供单应用独立导出或旧工作区路径兼容，
见[迁移说明](docs/workspace-migration.zh-CN.md)。

从[文档索引](docs/README.md)开始。Agent 遵循 [AGENTS.md](AGENTS.md) 和
[技能索引](.agents/skills/README.md)。持续观察时，运行
`python mosaico.py iris run --project projects/my_app`，打开输出的 Gateway Web
工作台地址；详见 [Gateway 指南](docs/project-gateway.zh-CN.md)。
