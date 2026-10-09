# 工作区版本与更新

[English](workspace-versions.md) | [返回索引](README_CN.md)

一个工作区发布版本由主仓库的官方 tag 和该提交记录的子模块提交 ID（Git gitlink）
共同确定。子模块不必有同名 tag。`python mosaico.py --version` 显示产品 CLI 版本，
不是工作区发布版本；无需维护另一份独立版本文件。

稳定版本 tag 使用 `vN.N.N`，例如 `v26.0.1`。最新稳定版按数字版本大小选择，
不按 tag 日期或默认分支最新提交选择。`v26.0.2-rc1` 等预发布版本需要用户明确选择。
已发布的 tag 不应移动；修复应发布新 tag，由其提交固定完整依赖集合，
维护者应在发布前验证这些依赖提交。

## 新建工程前检查

执行 `project init`、`game create` 或 `game new` 前，在工作区根目录执行以下命令
（示例使用 POSIX shell 语法；需要时将 `python` 换成 `python3`）：

```sh
mosaico_release_source=https://github.com/esp-mosaico/esp-mosaico-vibe.git
git ls-remote --tags --sort='-version:refname' "$mosaico_release_source" 'v*'
git rev-parse HEAD
git tag --points-at HEAD
git ls-tree HEAD submodule/
git submodule status --recursive
git diff --cached --submodule=short
git status --short --untracked-files=all --ignore-submodules=none
git submodule foreach --recursive 'git status --short --untracked-files=all --ignore-submodules=none'
```

即使 `origin` 指向 fork，也应查询官方仓库的实时 tag 列表。只有符合
`refs/tags/vN.N.N` 的名称才是稳定版候选。附注 tag 的 `^{}` 行才是用于与 `HEAD`
比较的提交 ID，另一行是 tag 对象 ID；轻量 tag 只有一行，直接给出提交 ID。
本地 tag 可能缺失、过时或属于私人版本；`git tag --points-at HEAD` 仅作本地参考。
不能将 `git describe` 找到的最近祖先 tag 视为当前提交恰好位于该发布版本。

`git submodule status` 与暂存区比较：前缀空格表示一致，`+` 表示提交不一致，
`-` 表示未初始化，`U` 表示冲突。还必须检查暂存区 diff，防止已暂存的 gitlink
改动掩盖与 `HEAD` 的偏差。上述 status 命令也能显示已初始化模块内的文件改动。
未初始化的模块只有已知的固定提交，尚未验证其内容；只初始化当前任务所需模块，
再检查其嵌套子模块。未使用模块可保持未初始化，但应在报告中说明。

| 检查结果 | 创建工程前 Agent 的行为 |
| --- | --- |
| `HEAD` 等于最新稳定 tag 的提交，所需依赖一致 | 报告 tag、提交和依赖状态，继续执行。 |
| `HEAD` 等于较旧的官方稳定 tag | 展示当前与最新 tag，建议更新。 |
| `HEAD` 不对应任何官方稳定 tag，包括领先于最新 tag 的分支 | 展示当前提交，建议切换到最新稳定 tag。 |
| 子模块提交不一致、gitlink 已暂存修改、冲突或脏文件 | 报告受影响路径，提出保留本地工作的版本对齐方案。 |
| 官方查询失败或尚无稳定 tag | 报告无法确认最新发布版本，不将本地 tag 当作最新版本。 |

创建工程前应与用户明确版本选择。用户已明确指定某个 tag 或开发提交时，
该选择对当前任务有效；记录这一例外，不要反复询问。仅处于功能分支不能视为用户
已作选择。版本检查本身不授权切换已有工作区。本地改动阻碍安全对齐时，使用独立
检出目录或先商定保留方式；不得自动 stash、reset、clean、强制切换或覆盖 tag。

## 更新到指定 tag

用户选定版本后，先按上述检查确认并保留主仓库和已初始化子模块中的本地改动。
保留已有分支和本地提交的可达引用；已有开发工作时优先使用独立检出目录。
以下命令要求检出目录干净，任一步失败即停止。将示例 tag 换成选定的官方 tag：

```sh
mosaico_release_source=https://github.com/esp-mosaico/esp-mosaico-vibe.git
mosaico_release_tag=v26.0.1
git fetch --no-tags --no-recurse-submodules "$mosaico_release_source" \
  "refs/tags/$mosaico_release_tag:refs/tags/$mosaico_release_tag" &&
git switch --detach "refs/tags/$mosaico_release_tag" &&
git submodule sync --recursive &&
git submodule update --checkout --recursive &&
git submodule update --init --checkout --recursive submodule/esp-mosaico-utils
```

若本地同名 tag 指向不同对象，显式 fetch 会失败；不得强制覆盖，应排查冲突或使用
独立检出目录。即使本地配置默认使用 merge 或 rebase，`--checkout` 也会要求子模块
检出记录的提交。第一次 update 对齐已初始化的模块；第二次初始化创建工程必需的
utils。固件开发再初始化 BSP，游戏开发还需引擎：

```sh
git submodule update --init --checkout --recursive submodule/esp-mosaico-bsp
# 游戏还需要初始化引擎。
git submodule update --init --checkout --recursive submodule/raylib-lite-engine
```

仅选择当前任务需要的路径。不得在子模块内执行 `git pull`，也不得使用
`git submodule update --remote`：这些命令跟随分支，无法保证发布版本配套。
任一步失败都应报告已完成和未完成的部分，解决后才能继续。

执行以下命令验证更新，并重复上面的状态检查：

```sh
git rev-parse HEAD
git rev-parse "refs/tags/$mosaico_release_tag^{commit}"
git ls-tree "refs/tags/$mosaico_release_tag" submodule/
git submodule status --recursive
```

主仓库两个 SHA 必须一致，并等于选定官方 tag 的提交。所有已初始化子模块及其嵌套
模块必须与记录的提交对应，不能有暂存的 gitlink 修改、冲突或依赖脏文件；未使用的
模块可保持未初始化。创建或构建工程前，重新读取该版本的 `AGENTS.md`、文档和
ESP-IDF 固定提交。更新源码不等于更新设备。

首次获取工作区时，也先按同一流程选择官方 tag，再克隆到新目录并仅初始化所需依赖：

```sh
git clone --branch "$mosaico_release_tag" --single-branch \
  "$mosaico_release_source" esp-mosaico-release &&
cd esp-mosaico-release &&
git submodule update --init --checkout --recursive submodule/esp-mosaico-utils
```

在新目录重复验证。需要提交应用代码时，可用 `git switch -c my-project` 从已验证
版本创建分支；单纯创建分支不会改变所处的发布提交。在任务版本报告中保留选定 tag
和子模块 SHA。

Git 命令参考：[远程 tag](https://git-scm.com/docs/git-ls-remote)、
[获取 tag](https://git-scm.com/docs/git-fetch)、
[子模块状态与检出](https://git-scm.com/docs/git-submodule)。
