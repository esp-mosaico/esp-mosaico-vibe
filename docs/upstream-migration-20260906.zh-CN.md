# 三仓库远端更新分析与本地迁移

日期：2026-09-06。三个仓库已获取远端并将本地提交 rebase 到下列最新基线；当前检出分支均为 `codex/upstream-migration-20260906`。未推送远端，旧分支仍保留，每个仓库另有 `codex/backup-pre-upstream-20260906` 备份引用。

| 仓库 | 迁移前本地 HEAD | 本次获取的远端基线 |
| --- | --- | --- |
| esp-mosaico-vibe | `127b56d834fc` | `origin/main` / `eb060d9a38e1` |
| esp-mosaico-tools | `2f9f07b760f0` | `origin/main` / `6737d145525c` |
| esp-iris | `1fc38b5cbfd1` | `origin/master` / `ad1b2f5fadb7` |

## 远端更新内容

- **esp-mosaico-vibe，4 个提交：** GSP UI bundle 独立 `ui_apps` 分区及校验加载；准备工具支持一步生成目标系统布局；采用产品后端布局策略的 System Update v1；固定系统前缀调整到 2 MiB。
- **esp-mosaico-tools，6 个提交：** Recovery backend 从固定整表/源 hash 白名单转为检查当前与目标的受保护分区契约；应用及数据按已验证目标表的描述符写入，分区表最后提交；预编译 reviewed Recovery 更新到 2.5.0，新分区表缩小 Recovery 与 Core Dump。
- **esp-iris，2 个提交：** v1 manifest 移除 `source_layout_sha256`；Gateway 不再在 BEGIN 前按该字段授权源表，由产品 backend 决定源到目标兼容性。目标分区表摘要、更新 receipt、应用身份与健康校验继续存在；旧字段及废弃 v2 schema 被明确拒绝。

## 合并与适配

Iris 的 13 个本地提交（11 个指定问题和 2 个支持修复）顺序重放成功。`range-diff` 表明除 P03 周围上游已删除的源布局白名单上下文外，各修复补丁保持一致，未用上游版本覆盖掉本地实现。P05 操作身份绑定、P06 未知结果/只读对账、P03 明确角色/产品要求仍保留。

两处 Git 冲突均为 gitlink：先完成 Iris，再让 tools 指向迁移后的 Iris，最后让根仓库指向迁移后的 tools。原本每个问题单独提交的结构保留；迁移补充使用独立提交。

新增适配：

1. 三个普通应用、Recovery、Gateway expectation 统一新 `layout_id=mosaico-retained-recovery-2m-v1`，保留实际 Inventory 分区摘要校验；避免新旧不兼容固定布局共享旧标识。acceptance fixture 分区同步新版 HelloWorld，并以提交内容和工作区内容双重核对。
2. 上游 GSP 打包器的 `os.fchmod` 在 Windows 不可用，补充 Windows 路径并确保 fdopen/权限/写入/fsync/replace 失败时关闭描述符、清理临时文件、保留旧输出；POSIX 保持 0644 和原子替换。
3. 移除上游与 Python 3.8 lint target 冲突的无效 `noqa`；不替换为 Python 3.10 才有的 pairwise。

| 分区 | 旧 offset / size | 新 offset / size |
| --- | --- | --- |
| factory | `0x20000 / 0x200000` | `0x20000 / 0x1c0000` |
| coredump | `0x220000 / 0xd0000` | `0x1e0000 / 0x20000` |
| nvs | `0x2f0000 / 0x10000` | `0x200000 / 0x10000` |
| ota_0 | `0x300000 / 0xd00000` | `0x210000 / 0xdf0000` |

2 MiB 指固定系统前缀，Recovery 自身容量是 1.75 MiB；otadata、phy_init、sysmeta 的原偏移及大小保留。

## 验证结果

| 检查 | 结果 |
| --- | --- |
| Iris Python 3.8 / Windows | 271 passed，2 个 POSIX 专用 skipped |
| Iris Python 3.12 / Windows | 271 passed，2 个 POSIX 专用 skipped |
| Ruff / mypy | 通过，33 个源码文件 |
| Iris 源码及现有前端产物预算 | 通过；前端源码本次未改动 |
| 根仓库 GSP / System Update 准备 / 分区契约 | 10 passed |
| 根仓库 CLI 集成（原生 unittest 入口） | 4 passed |
| GSP 工具 Windows / Linux | 各 4 passed，含失败清理回归 |
| tools OTA / 分区摘要定向回归 | 4 + 7 passed |
| tools CLI 完整文件 | 88 passed，3 failed + 1 error：既有 Windows 路径/IDF Python fixture 假设；不作为全套通过 |
| HelloWorld / Recovery 重新构建 | 均通过；BIN 1,150,768 B / 1,654,208 B，Recovery 在新 1.75 MiB 容量内 |

根目录直接 pytest 全收集会使 `tests/mosaico_cli` 与产品同名包冲突，因此 CLI 集成使用仓库现有的 `python tests/mosaico_cli/test_cli.py` unittest 入口通过；没有删除失败测试或放宽产品检查。

证据位于本地 `.agents/iris-audit-20260905/`：`migration-iris38.xml`、`migration-iris312.xml`、`migration-workspace.xml`、`migration-budgets.json`。构建沿用已核对的 ESP-IDF `7b9cc1ac79f8` / v6.1-dev、IDF Python 3.12.10，未 fullclean；旧配置与验收产物另存 `migration-config-backups/`。

构建记录：HelloWorld `20260906-150145-build-56640`（199.8 秒，2 warnings），Recovery `20260906-150210-build-34908`（215.0 秒，5 warnings）。Recovery 告警包括 LTO 串行处理及 `esp_iris_files.c` 的 `strnlen` bound 告警；本轮未修改该文件，也不把构建成功写成无告警。产物摘要、实际配置与启动链文件见 [新固件索引](../.agents/iris-audit-20260905/migration-firmware.json)，根仓库与 tools 源码记录见 [迁移源码索引](../.agents/iris-audit-20260905/migration-workspace-sources.json)。

## 设备与历史证据边界

本次只迁移仓库、适配并构建，未执行任何设备写入。现有板仍是上轮旧布局；[原验收报告](esp-iris-fix-acceptance.zh-CN.md) 的 commit/BIN/Boot ID 是历史证据，不能转记为新布局实机通过。

旧板不会通过新版普通 install 的分区 hash 校验，新 System Update backend 也要求当前和目标均满足新的固定系统前缀，不能借由放宽检查自动迁移。新 NVS 与应用位置涉及旧 Recovery、Core Dump、NVS 区域；设备迁移须单独安排数据保留及受控恢复流程。本轮没有抹除或搬移这些数据。

远端预编译 reviewed Recovery 2.5.0 是上游产物，不包含尚未发布的本地 P03/A02 等改动；本轮保留其原始二进制，没有将本地构建冒充 reviewed 发布包。后续验证本地修复须使用匹配的 current-source 固件，并独立完成新布局实机验收。

## 原提交映射

### esp-mosaico-vibe

| 原提交 | 迁移后提交 | 变更 |
| --- | --- | --- |
| `64eadf3` | `cac0c6f` | docs: record Iris reviews and planned A01 P01 I04 I05 scope |
| `5d84de8` | `883bf1f` | test(IRIS): add retained-recovery hardware acceptance fixture |
| `a65d41f` | `7261de9` | fix(IRIS-P03): declare normal application compatibility |
| `0c75816` | `354c12d` | docs: record Iris fix acceptance and pending target-device validation |
| `127b56d` | `44a00e1` | docs: accept Iris fixes on corrected Serial JTAG device and pin validated tools |
### esp-mosaico-tools

| 原提交 | 迁移后提交 | 变更 |
| --- | --- | --- |
| `bcd5e93` | `ae9bac1` | feat(recover): reserve an explicit independent USB Serial/JTAG port |
| `8719556` | `9193441` | fix(IRIS-P03): declare and require the Mosaico update contract |
| `2f9f07b` | `f707365` | build: pin independently committed Iris fixes and validated workbench support |
### esp-iris

| 原提交 | 迁移后提交 | 变更 |
| --- | --- | --- |
| `e856181` | `af5b43c` | fix(IRIS-I01): bound RPC callback response lengths before copying |
| `abab942` | `fcdcb62` | fix(IRIS-I02): preserve coalesced receive tails under TX backpressure |
| `902e47b` | `f921410` | fix(IRIS-P05): bind operation IDs to durable request and payload fingerprints |
| `521ec3c` | `0e5ef84` | fix(IRIS-P06): preserve uncertain writes and append read-only reconciliation |
| `18af720` | `2bdaf78` | fix(IRIS-P03): declare firmware roles and enforce update compatibility |
| `cb6c592` | `abc4845` | fix(IRIS-P02): expire unhandshaken single-transport owners |
| `78acded` | `a678828` | fix(IRIS-P04): reject session replays and negotiate bounded host reopen |
| `e76f30d` | `315f348` | fix(IRIS-I03): wrap host sequence counters in serialized wire order |
| `eb5e77c` | `71c9d0d` | fix(IRIS-T01): make host validation and shutdown portable across Python runtimes |
| `c63615f` | `8720cc9` | fix(IRIS-A02): bound service execution and define build profiles with concurrent control |
| `5a2b85f` | `ede2d88` | fix(IRIS-T02): enforce portable release and firmware regression gates |
| `1363ee6` | `a640490` | fix(gateway): retry maintenance observation through USB reenumeration |
| `1fc38b5` | `e38755c` | fix(workbench): preserve exact uint64 boot identity display |
