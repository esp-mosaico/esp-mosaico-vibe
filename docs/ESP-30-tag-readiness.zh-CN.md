# ESP-30：首次 tag 发布准备评估

评估日期：2026-09-14。对应 [ESP-30](https://linear.app/loop233/issue/ESP-30)。
本报告检查当前主仓库、两个固定子模块及其远端差异，列出打 tag 和版本切换指南的剩余工作。
它不是发布验收通过声明。初次评估未更新子模块指针、创建 tag、发布 Release 或操作设备；
评估后的版本修改见文末记录，基线与 CI 表仍记录初次检查的提交。

## 结论

当前代码已有通过的主仓库 CI 和主机测试，可以作为发布候选基线。
正式发布前应先确定子模块范围，补齐版本说明、构建依赖记录和目标版本的验收证据。
若本次只保存开发快照，可以保留当前指针，但发布说明必须明确其功能边界；
若目标是 ESP-30 所属的“v1.0：稳定发布”，不能仅凭创建 Git tag 宣称完成。

最值得优先纳入的是 BSP 已合并的交互扩展板类型修复；内存观测和未合并 PR
应按发布范围决定，不能把远端全部最新内容自动视为发布依赖。

## 已核实的版本与 CI

初始主仓库和两个子模块均无未提交变更；子模块 HEAD 与主仓库 gitlink 一致。
通过 `git ls-remote` 查询时，三个 origin 均没有 tag。

| 仓库 | 本次检查的提交 | 远端默认分支 | CI 证据 |
| --- | --- | --- | --- |
| Vibe | `7c4a41c3bcab0572a935ed61421201b993521c01` | `main`，与本地相同 | [该提交 CI 成功](https://github.com/esp-mosaico/esp-mosaico-vibe/actions/runs/34814760455) |
| BSP | `3d5c451598aaffbda6f41895a0414dba4bac3505` | `master`：`1bb8457a8ad2ce7310c146903fe906fb33a9bbbd` | [固定提交成功](https://github.com/esp-mosaico/esp-mosaico-bsp/actions/runs/34477793043)；[远端提交成功](https://github.com/esp-mosaico/esp-mosaico-bsp/actions/runs/34817358339) |
| Utils | `024055d665f54b5e5f271b140633579215d26b0a` | `main`：`2ea54547541915dd5a818c22235c7260b44a090f` | [固定提交所查运行已取消](https://github.com/esp-mosaico/esp-mosaico-utils/actions/runs/34813344771)；[远端提交成功](https://github.com/esp-mosaico/esp-mosaico-utils/actions/runs/34820068367) |

Utils 固定提交的运行取消不等于测试失败。主仓库 CI 覆盖该组合的 Tools 测试和应用构建，
但不替代 Utils 的完整质量、Workbench、固件矩阵检查；保持此指针时需补齐对应证据。
`ESP-Iris/` 是 Utils 内部目录，不是第三个独立 Git 子模块。

| 版本对象 | 当前值 | 权威来源 |
| --- | --- | --- |
| Vibe 发布 tag | 尚无 | 主仓库 Git tag，需制定命名规则 |
| BSP 组件 | `0.2.0` | `submodule/esp-mosaico-bsp/components/esp-mosaico-bsp/idf_component.yml` |
| ESP-Iris / Gateway / Workbench | `0.1.0` | Utils 中组件 manifest、Gateway `__init__.py`、前端 package 文件 |
| Mosaico Tools | `0.1.0` | Utils 中 `esp-mosaico-recovery/tools/mosaico_cli/__init__.py`；`python mosaico.py --version` |
| 保留 Recovery 固件 | `2.8.5-recovery` | Recovery 默认配置和 `prebuilt/recovery/manifest.json` |
| Hello World / GSP 示例固件 | `1.0.0` | 两个项目的 CMakeLists 和 sdkconfig.defaults |
| 构建基线 | ESP-IDF `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`，ESP32-S31 | 主仓库及 Utils CI；应用、Recovery、BSP 均声明 IDF `>=6.2` |

`mosaico.py --version` 和示例显示的 `1.0.0` 都不能确认 Vibe tag。
建议 Vibe 使用 `vX.Y.Z`，候选使用 `vX.Y.Z-rc.N`；具体首个版本号由本次发布范围决定。
Utils 已规定 `esp-iris-vX.Y.Z` 和 `esp-mosaico-tools-vX.Y.Z`，两个产品独立演进，
无需跟随 Vibe 统一为 1.0。BSP 的 tag 命名需补文档；组件版本不是现成的 Git tag。
子模块不必都有 tag 才能复现 Vibe，但其固定 SHA 必须长期可获取。

## 剩余工作及优先级

### P0：冻结发布内容及验收口径

1. **决定 BSP 升级范围。** 当前固定版本将 `MOSAICO_BOARD_TYPE_INTERACT` 定义为
   `0x15`，远端 `55a2033` 已修正为 `0x16`。若发布支持对应交互扩展板，应纳入修复并
   验证识别与交互。远端还包含 `e65035c` 的摄像头默认分辨率支持；升级到
   `1bb8457` 会同时纳入它，需要回归摄像头默认配置和显式分辨率。
2. **决定是否包含 ESP-24 内存观测。** Utils 新增 `64139cc` 的协议、固件、Gateway
   和 CLI 能力，以及预算修正，均已合并到 `2ea5454`。它是功能增量，未发现必须为打 tag
   引入的依据。纳入后需重新运行 Vibe CI，不能直接复用子仓库绿色结果。
3. **记录应用与 Recovery 的 BSP 组合。** Recovery 的
   `firmware/recovery/main/idf_component.yml` 单独固定 BSP `3d5c451`。
   更新 Vibe BSP 指针不会自动修改它。可以保留经过验证的不同组合，但必须明确记录；
   若同步升级 Recovery，需做相应固件验收。
4. **冻结正式候选后做真机发布验收。** 至少覆盖 Hello World、`init` 生成工程、
   GSP 场景及资源更新、normal → Recovery → normal。操作使用 `mosaico.py`，
   检查同一 Device ID、各阶段新的 Boot ID、Recovery ready、应用健康及固件 hash；
   CLI 与 Web 工作台记录应对应。保留原始日志、操作记录和有价值的 crash evidence。
   本地存在 2026-09-11 的 crash acceptance `passed` 历史记录，但它不证明最终 tag
   或新增 BSP 功能已验收，本次也没有把历史设备状态作为实时状态。
5. **对外分发前处理许可缺口。** Utils 的 `LICENSES.md` 明确记录 Recovery/Tools
   尚缺发布所需许可；Vibe 也没有仓库级 LICENSE。由维护者确认并补齐授权文本。
   这是对外发布准备项，不妨碍内部保存 Git 快照；不能把 BSP/Iris 的 Apache-2.0
   自动当作整个工作区的许可。

### P1：完成 ESP-30 的版本确认和切换能力

1. **补发布说明与兼容矩阵。** Vibe 缺少 tag 使用指南和 changelog。建议新增
   `docs/version-switching.zh-CN.md`，从中英文 README 链接；发布记录保存主仓库、
   两个 gitlink、IDF SHA、工具/Recovery 版本、硬件版本、Recovery ABI、分区布局、
   依赖解析结果、固件 hash 和验收链接。发布记录中的信息应与实际 tag 自动或人工核对。
2. **修复 Utils 说明断链。** `README.md` 引用的 `VERSIONING.md`、`MIGRATION.md`
   在固定提交和远端 main 均不存在。应恢复权威内容，或重写指向现有资料的入口。
3. **消除 IDF 基线矛盾。** Vibe `docs/repository-specification.zh-CN.md` 仍写
   `>=6.1`，实际三个主要 component manifest 均为 `>=6.2`；Utils 的 AGENTS
   也残留 Recovery 6.1 描述。按源代码约束修正文档，同时保留主机 Python 与 IDF
   bootstrap Python 分开解析的说明。
4. **说明并验证 Gateway 换版本流程。** `gateway.py` 比较的是整个 Utils Git SHA，
   不是显示的 `0.1.0`。切换 Utils 后，旧 Gateway 会触发 revision mismatch，CLI
   明确不自动终止它。指南必须给出经过验证的安全退出、重启和状态目录兼容方案，
   包括等待其它设备操作结束。当前 `mosaico.py` 未提供显式 Gateway stop 子命令，
   需在 Tools 中完善产品入口或记录既有受支持操作；不能写一个不存在的命令。
5. **定义降级边界。** 切回旧 tag 只改变 PC 源码，不改变已运行 Gateway、设备固件、
   NVS、Recovery 或布局。只承诺验证过的兼容版本回退；布局或 Recovery ABI 不兼容时，
   给出迁移方案或明确不支持。不能使用整片擦除或无条件 `recover` 代替兼容性判断。

### P1：保证构建及发布证据可复现

1. **确定 ESP-IDF 依赖锁策略。** 根 `.gitignore` 忽略所有 `dependencies.lock`，
   本地虽有生成文件，Vibe 的 Git 树不包含它们；manifest 中也存在版本范围。
   固定 tag 和 IDF SHA 仍不能固定所有远程组件。至少为发布应用和 Recovery 保存
   验证过的依赖锁与组件来源，或交付具有同等约束的发布构建清单。
   Utils 已有 Python `requirements.lock` 和 npm `package-lock.json`，无需重复发明机制。
2. **固定 BSP CI 工具链。** BSP workflow 使用 `espressif/idf:latest`，无法保证未来
   重跑同一 tag 时的工具链一致。应固定经过验证的 IDF revision 或镜像 digest，
   并保留构建输出；与 Vibe/Utils 的 IDF 基线对齐可减少组合差异。
3. **明确 tag 验证机制。** 三个 CI workflow 都没有 tag push 触发。可增加受控 tag
   模式，也可在发布流程中强制复用“同一 commit”已通过的完整 CI；不是必须重复跑一遍。
   目前 Vibe/Utils artifacts 只保留 14 天，应将发布需要的固件、ELF、hash、锁文件和
   验收证据归档到长期位置，并核对 required check 的保护配置。
4. **补齐 Recovery 二进制来源说明。** 预置包为 `2.8.5-recovery`，manifest 的 source
   commit 是 `b009032e10bdfb492fceddc9f6828f63a841c6cb`，IDF 字符串包含 `-dirt`。
   四份镜像的长度和 SHA-256 本次均核对通过；这证明文件符合 manifest，不能证明其
   可由当前源码完整重建。明确记录沿用已审定旧包的原因、来源与兼容证据。
   只有替代包通过布局、hash 和真机验收后才能替换，不能为统一版本号重写它。
5. **让版本检查支持下一版。** Utils `tools/check_repository_layout.py` 把 Iris、
   Gateway、Workbench、Tools 都硬编码为 `0.1.0`。首次沿用此版本不受影响；后续升级前
   应改为检查各产品内部版本一致性，并允许 Iris 与 Tools 独立升级，补充发布 changelog。

### P2：明确可以延后的功能

- [Vibe PR #2](https://github.com/esp-mosaico/esp-mosaico-vibe/pull/2) 是 GSP sim backend
  工作，仍未合并；当前主线已有 PC bridge。只有本次承诺对应模拟器行为才需要纳入。
- [BSP PR #5](https://github.com/esp-mosaico/esp-mosaico-bsp/pull/5) 修改双槽扫描、占用期间
  扫描和拔出检测。其 CI 运行显示 `action_required`，PR 中硬件测试清单尚未勾选。
  若首版承诺双槽持续热插拔，需先评审并验收；否则列为已知限制，不能当成已支持行为。

## 版本切换指南应落实的操作

以下为指南的实施清单，不表示已有 tag 或 Gateway 迁移流程已经完成。

1. 从官方 origin 获取 tags；用 `git tag --list` 查可用版本，
   `git describe --tags --exact-match HEAD` 确认精确 tag。无输出/报错时明确显示未处于 tag，
   不能把最近祖先 tag 当作当前发行版。必要时用 `git rev-parse HEAD` 给出完整提交。
2. 用 `git status --short` 检查主仓库，并用
   `git submodule foreach --recursive 'git status --short'` 检查子仓库改动；
   单独说明用户应用、未跟踪文件、被忽略配置的备份办法，避免切换覆盖开发成果。
3. 无需在当前工作目录切换时，优先以独立 clone 检出目标 tag 做验证。
   在当前目录操作前先完成上述备份；以实际选择的 tag 替换下列占位符：

   ```sh
   git fetch origin --tags
   git switch --detach <实际的目标tag>
   git submodule sync --recursive
   git submodule update --init --recursive
   git submodule status --recursive
   ```

   子模块状态前缀 `-` 表示未初始化，`+` 表示与 gitlink 不一致，`U` 表示冲突；
   这些都不能作为正式组合验收通过。不要用 `git submodule update --remote`，它会选择
   分支版本而偏离目标 tag。若要开发新应用，从 tag 创建个人开发分支。
4. 初次获取项目应使用 recursive clone；说明 GitHub 自动源码 ZIP 不包含完整子模块
   内容，而且当前启动器会解析 Git revision。核实 HTTPS/SSH 获取方式：Vibe 的 Utils
   URL 当前为 SSH，BSP 为 HTTPS，首次使用者可能需要额外 SSH 配置。
5. 分别检查 `python mosaico.py --version`、子模块 SHA 和目标 tag 的兼容矩阵；
   根据目标版本处理已启动 Gateway，再运行 `python mosaico.py doctor` 和 live `list`。
6. 隔离旧 build、sdkconfig 和 managed_components 等生成状态，保留个人配置副本，
   用目标版本依赖重新构建。不能沿用工作目录残留的旧依赖锁宣称复现成功。
7. 若还要同步设备，按目标版本契约选择 `install`、`system-update` 或确有需要的
   `recover`，并执行真机闭环；源码切换成功与设备升级成功分别验收。
8. 至少测试首次 clone → tag、开发分支 → tag，以及两个已验证 tag 之间切换。
   目前没有两个历史 tag，首次发布不能宣称已验证跨历史版本降级；应给出适用范围，
   第二版发布时补充双向兼容结果。

## 本次执行的检查及边界

在本机 Linux / Python 3.12.3，复用已有隔离测试环境执行：

| 检查 | 结果 |
| --- | --- |
| `pytest -q tests --ignore=tests/firmware` | 25 passed |
| `pytest -q submodule/esp-mosaico-utils/esp-mosaico-recovery/tests` | 168 passed |
| ESP-Iris tools 目录：`pytest -q --ignore=tests/e2e` | 294 passed |
| `python3 mosaico.py --version` | `mosaico.py 0.1.0` |
| Recovery manifest 四份镜像 size / SHA-256 | 全部一致 |
| origin refs、近期 CI、未合并 PR | 在线查询；状态以本次评估时为准 |

合计 487 项主机测试通过，适用于上表固定提交。未在本次重跑 ESP-IDF 构建、Workbench
测试或真机操作；固件 CI 状态来自对应 GitHub 运行，未将其等同于硬件验收。

建议执行顺序：确定 BSP/Utils 范围 → 完成子仓库变更与验证 → 更新 Vibe gitlink →
补版本指南和兼容/依赖清单 → 最终组合 CI 与真机验收 → 冻结提交 → 创建 annotated tag
并归档发布证据。产品需要独立发布时先发布子仓库 tag，再发布引用它们的 Vibe tag；
已发布 tag 不移动，修正通过新版本交付。

## 评估后实施：Recovery 源码版本调整

按维护者要求，将新构建 Recovery 固件的版本从 `2.8.5-recovery` 调整为精确的
`0.1`（不附加 `-recovery`），并更新 Utils 的版本说明和 changelog。
主仓库通过 gitlink 固定这次 Utils 修改；ESP-Iris、CLI、BSP、Recovery ABI 和分区布局
未随此调整改变。

源码版本由 `firmware/recovery/sdkconfig.recovery.defaults` 中的
`CONFIG_APP_PROJECT_VER` 定义。已经审定的 `prebuilt/recovery` 包仍保持
`2.8.5-recovery` 及原始 manifest/hash，默认 `mosaico.py recover` 继续使用旧包；
`recover --source current` 使用当前源码构建。新镜像尚需真机验收后才能替换发布基础包，
不能将修改 manifest 字符串视为升级了预置固件。本次未运行设备写入命令。

修改后验证：主仓库与 Recovery/CLI 合计 193 项测试通过；Recovery low-noise build
成功、0 个警告，`factory.bin` 为 1,706,176 字节，镜像内 app descriptor 版本确认为
`0.1`。预置旧包四份镜像长度与 SHA-256 仍符合原始 manifest。
构建使用 Python 3.12.3 和 IDF `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`；
该 IDF 工作目录有已有本地修改，因此此结果仅为本机构建验证，不作为干净发布构建证据。
原始日志保存在 Recovery 项目内
`.codex-runs/idf-low-noise-build/20260914-210648-build-3779630/raw.log`。

## 后续实施：预置 Recovery 0.1 与预编译 Workbench

维护者进一步要求更新预置包，并允许中断在线设备完成验证。当前预置包实际包含
`0.1` Recovery 镜像；上一节“预置包仍为旧版本”的描述仅记录第一次源码修改阶段。

- 新 Recovery 来自 Utils `c4316c6608402b1db375ddccd8d5b821b21b6c8b`，
  镜像为 1,706,304 字节，SHA-256 为
  `e0d48922eea6b15eab194a6e466b12b04f01180c72bc422d14c8dc545d334cb9`。
  manifest 重新生成；旧包的 bootloader、分区表和初始 OTA 数据逐字节保留。
- 修复版本检查只接受三段版本号的问题：`0.1` 等价于 `0.1.0`。
  本次重命名保留旧 2.x 最低版本要求至 `2.8.5` 的兼容性，更高要求仍被拒绝；
  针对边界、无效字符串、溢出和旧包兼容增加了 C 主机回归验证。
- 自动 `recover --source current` 因设备未进入 ROM 模式而未能写入；
  后续使用 `mosaico.py system-update` 的 Recovery 自更新路径完成验证。
  最终 Recovery 版本、ELF hash 和健康状态均通过 Gateway 确认。
  再使用原始 `cyber_ride` 更新包恢复应用，原包的 `2.5.0-recovery` 最低要求通过，
  应用 ELF hash 与操作前完全一致，同一设备在各阶段获得新的 Boot ID。
  有效 core dump 由 Gateway 保存，未擦除用户凭据或 NVS。
- 本次验证实际运行于设备已有应用布局；完整基础包的 ROM 首次部署未重测。
  构建仍使用带已有本地修改的 IDF，不宣称完成干净的可复现发布构建。
- ESP-Iris 新增跟踪的 `tools/frontend/dist`（298,247 字节），Gateway 可直接提供
  Workbench，无需用户安装 Node.js。修正两层 Git 忽略规则，CI 构建后检查 dist
  与 Git 一致，防止源码和预编译前端不同步。
- 修改后 194 项主仓库/Recovery 测试通过；前端 7 项单元测试、5 项浏览器测试通过，
  1 项依赖专用硬件 fixture 的测试按条件跳过。实际设备另做只读 Web 验证。
  Web 显示的 Device ID、Boot ID 和恢复操作记录与 CLI 一致，浏览器无运行错误，
  截图保存为 `.codex-runs/esp-30-prebuilt/workbench-final.png`。
  前端重复构建与提交文件一致，298,247 字节低于 409,600 字节预算，npm audit
  报告 0 个漏洞。修正后的 Recovery 构建成功、0 个警告。

本次原始日志、备份与结构化证据保存在 `.codex-runs/esp-30-prebuilt/`。
该目录为本地证据，不随 Git 提交；正式发布仍需做长期归档。
