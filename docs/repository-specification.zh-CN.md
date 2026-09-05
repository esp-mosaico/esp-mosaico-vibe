# ESP-Mosaico Vibe 仓库规格书

| 项目 | 规格 |
| --- | --- |
| 仓库名称 | ESP-Mosaico Vibe |
| 文档类型 | 仓库级产品与工程规格书 |
| 目标硬件 | ESP-Mosaico 开发板（ESP32-S31） |
| 软件基线 | ESP-IDF 6.1 或更高版本，并具备 ESP32-S31 目标支持 |
| 参考应用 | `projects/hello_world` |
| Recovery 工程 | `submodule/esp-mosaico-tools/firmware/recovery` |
| 板级能力来源 | `submodule/esp-mosaico-bsp` Git 子模块 |
| 设备运维入口 | 根目录 `mosaico.py`、固定版本的 `submodule/esp-mosaico-tools` 和 ESP-Iris Developer Gateway |
| 文档状态 | 产品定义与当前工程基线 |

**仓库定义：ESP-Mosaico 定制的 Agent 主导（Agent-Led）人机协同开发统一入口。**

ESP-Mosaico Vibe 是一套 Agent-Led 开发工作区。Agent 在其中理解目标、调用能力并操作真实硬件，再根据设备反馈持续迭代。

用户定义目标，完成必要授权并验收实物。Agent 作为默认执行主体，推进从方案到真机验证的完整流程。仓库通过 `AGENTS.md`、Skills、板级资源和 ESP-Iris 提供执行基础。

本规格书定义该工作区的目标、架构、功能契约和边界。仓库示例仅提供技术参考。

## 1. 问题

传统嵌入式开发缺少统一的执行主体。用户需要亲自衔接开发环境、应用代码和真实设备，工程操作占用了大量创作时间。

从环境部署到真机运行，开发过程跨越多个工具和系统。主要问题如下：

1. **启动成本高**：环境配置和依赖排错需要大量人工操作。
2. **资源分散**：板级资料、组件和专项框架缺少统一入口。
3. **UI 链路断裂**：设计、代码与真机效果之间需要反复搬运。
4. **部署步骤繁琐**：构建、固件选择和设备写入由多套工具完成。
5. **Debug 反馈慢**：日志、画面和设备状态难以关联。
6. **更新风险高**：普通烧录可能破坏恢复路径。
7. **Agent 难以持续执行**：缺少统一规则、实时状态和明确的完成条件。

本仓库让 Agent 依据用户目标编排开发流程。正常情况下，用户表达目标并连接设备，Agent 持续推进到真机验证，从而缩短“想法 → 可运行实物”的路径。

### 1.1 人机职责

| 角色 | 核心职责 |
| --- | --- |
| 用户 | 定义目标，提供必要授权，验收最终实物 |
| Agent | 规划并执行开发流程，依据真机反馈持续修正 |
| 仓库 | 提供执行规则、工程能力和安全边界 |
| ESP-Iris | 提供统一的设备操作与观测通道 |
| ESP-Mosaico | 承载真实运行结果，作为功能与体验的最终验证对象 |

Agent-Led 的默认主导关系是：**Agent 持续推进，用户在关键节点介入。**这些节点包括产品决策、必要授权和物理操作。

## 2. 方法

仓库采用以下方法解决上述问题：

### 2.1 `AGENTS.md` 统一入口

- 根目录 `AGENTS.md` 是 Agent 进入仓库后的统一任务路由入口。
- 它规定工程开发与设备操作的共同约束。
- Agent 按同一套规则执行，用户可以审查其路径与结果，避免不同会话采用互相冲突的操作方式。

### 2.2 Skill 按需加载

- `skills/` 将专项流程封装为任务化指南。
- Agent 先识别任务所需能力，再加载相关 `SKILL.md`，不把全部资料一次性塞入上下文。
- Skill 体系可持续接入新的开发能力。
- 每个 Skill 应明确使用条件、执行路径和验收方式。

### 2.3 软硬件资料本地统一接入

- `submodule/esp-mosaico-bsp` 集中提供板级能力与可运行示例。
- 音频、视觉 AI 和多设备交互均有受控入口。Agent 优先复用这些已验证路径。
- Agent 只加载当前任务需要的资源。

### 2.4 模板化应用孵化

- 以 `projects/hello_world` 作为参考应用。
- 每个用户应用创建在独立的 `projects/<project-name>` 目录中。
- `projects/` 仅承载参考应用和用户应用；保留 Recovery 是
  `esp-mosaico-tools` 中只由 `mosaico.py recover` 使用的内部固件资源。
- 参考应用通过 `components/esp_mosaico_app_recovery` 固化设备接入和
  recovery-first 契约。

### 2.5 UI 设计与真机视觉闭环

- Agent 基于设备显示与触摸约束实现 LVGL 界面。
- Recovery 固件通过 ESP-Iris 注册 RGB565 屏幕镜像后端，使开发者和 Agent 可在 Gateway 工作台观察恢复界面；具体应用按自身 UI 架构注册对应后端。
- 使用 GSP 的应用在 PC 上通过 `tools/gsp-sim` 预览 480×480 场景；仿真运行时与 `submodule/esp-gsp` 中的 **espressif/esp-gsp 1.1.0** 配套。
- UI 调整与固件调试共享同一设备记录，减少人工往返。
- 专项 Skill 可扩展视觉比较能力。具体应用负责定义验收基准。

### 2.6 ESP-Iris 下载、烧录与调试闭环

- Agent 通过 ESP-Iris Developer Gateway 操作并观测设备。
- Gateway CLI 与 Web 工作台共享设备身份和操作记录。
- Agent 根据设备实时状态选择正常 OTA、首次 recovery 配置或最后恢复路径，用户无需手工拼接命令。
- 正常流程只要求用户连接 ESP-Mosaico；只有无法软件恢复时，才请求用户执行必要的按键和上下电动作。

### 2.7 Recovery-first 固件交付

- 保留兼容的 Recovery 与正常应用交付路径。
- 正常固件只提供“进入 recovery”的 RPC，不携带 OTA writer。
- 写入能力只在 Recovery 中提供。
- 正常应用更新必须先进入 Recovery，再由 ESP-Iris Gateway 完成安装。
- 空白或 recovery 状态未经验证的设备，必须先配置 recovery，再执行首次应用 OTA。
- 固定的 `sysmeta` 系统 NVS 保存设备身份、TCP pairing token、Factory Wi-Fi、
  Recovery OTA 状态和系统更新结果；应用 `nvs` 与这些系统数据分离。
- `ota_0` 固定起始地址，后续分区表只能在同时提供可装入目标尺寸的应用镜像时
  从其尾部回收空间；其余已声明分区保持 offset 和 size 不变。

### 2.8 统一设备操作与证据链

- 日常设备运维默认经 ESP-Iris Developer Gateway 完成。
- Agent 只使用 ESP-Iris 组件源码内提供的 CLI 操作设备。
- CLI 与 Web 工作台共享设备身份及操作记录。
- 固件或分区变更前，先保存有效的故障证据。

### 2.9 最小化不可恢复操作

- 普通应用只通过 `python mosaico.py install` 安装。
- 空白/未验证设备和最后恢复只通过 `python mosaico.py recover` 处理，底层实现不作为用户接口。
- 不以恢复连接为由擦除整片 Flash。
- 未经明确授权，不覆盖凭据、设备身份、recovery 数据或相关分区。

## 3. 系统架构

### 3.1 端到端协同链路

```text
用户提出目标并连接 ESP-Mosaico
              │
              ▼
Agent 理解目标、识别约束和完成条件
              │
              ▼
读取 AGENTS.md，按需选择 Skill、BSP、组件与示例
              │
              ▼
规划 → UI/交互设计 → 工程创建 → 代码实现 → 构建
              │
              ▼
ESP-Iris 发现设备 → recovery-mode OTA → 启动
              │
              ▼
读取多模态设备证据
              │
              ▼
Agent 判断是否满足目标
 ├── 否：诊断、修改并继续执行 ───────┐
 ├── 需授权/物理操作：请求用户后继续 ┤
 └── 是：整理验证证据 → 用户验收实物 │
                                      │
                ◄─────────────────────┘
```

Agent 按可审查规则持续执行闭环，直到功能通过真机验证。

### 3.2 逻辑架构

```text
用户
 ├── 目标与产品决策
 ├── 必要授权或物理操作
 └── 实物验收
        │
        ▼
Agent Orchestrator（统一控制面）
 ├── 规则与上下文：AGENTS.md / docs/
 ├── 能力路由：skills/
 ├── Recovery 执行：submodule/esp-mosaico-tools/firmware/recovery
 ├── 设备侧公共能力：submodule/esp-mosaico-tools/submodule/esp-iris
 ├── 应用执行：projects/hello_world / projects/<project-name>
 ├── 板级知识：submodule/esp-mosaico-bsp
 └── 设备操作：ESP-Iris CLI
        │
        ▼
ESP-Iris Developer Gateway
 ├── 设备发现 / 状态 / Job
 ├── 日志 / 屏幕 / core dump
 └── 控制 / OTA / 重启 / recovery
        │ USB High-Speed（独占设备会话）
        ▼
ESP-Mosaico 真实设备
 ├── 板级能力：显示 / 触摸 / 音频 / 传感 / 扩展
 ├── 正常应用
 ├── 保留恢复能力
 └── 崩溃证据
```

### 3.3 仓库组成

| 层级 | 路径 | 职责 | 变更原则 |
| --- | --- | --- | --- |
| 仓库入口 | `README.md`、`README_CN.md` | 说明定位、创建工程和设备运维规则 | 保持中英文语义一致 |
| Agent 规则 | `AGENTS.md` | 路由开发任务、约束设备操作和恢复流程 | 规则应简洁且可执行 |
| 应用工程 | `projects/` | 容纳参考应用和用户应用 | 一个应用一个目录 |
| 参考应用 | `projects/hello_world` | 提供显示、ESP-Iris 和 recovery-first 接入 | 可复制为具体用户应用 |
| GSP 参考应用 | `projects/gsp_hello` | 提供可在 PC 仿真和真机运行的 GSP Hello World | 作为 GSP 应用起点 |
| Recovery 工程 | `submodule/esp-mosaico-tools/firmware/recovery` | 提供固定的保留 Recovery、OTA writer 和系统恢复能力 | 与 `mosaico.py recover` 同版本维护，不承载普通应用代码 |
| 应用恢复组件 | `components/esp_mosaico_app_recovery` | 提供正常应用进入 Recovery 和健康确认能力 | 仅供正常应用使用，不包含 OTA writer |
| GSP 运行时 | `submodule/esp-gsp/` | 固定 espressif/esp-gsp 1.1.0 Git 子模块 | 固件与仿真共用同一 pin |
| GSP 主机仿真 | `tools/gsp-sim/` | 用独立 `sim` 预览场景 JSON | 不引入 claw hub/runtime |
| 板级子模块 | `submodule/esp-mosaico-bsp` | 提供 BSP、扩展模块、交互/网络组件和示例 | 按任务初始化和检查 |
| 工具子模块 | `submodule/esp-mosaico-tools` | 提供统一 CLI、构建 runner、Recovery 固件，并递归锁定 ESP-Iris | 主仓库只固定 tools；Iris 由 tools 的嵌套 gitlink 唯一固定 |
| 任务指南 | `skills/` | 提供环境安装、构建等任务化说明 | 只加载相关指南 |
| 用户文档 | `docs/` | 面向开发者说明工作流、规格和应用文档 | 不放 Agent 私有工具 |
| 产品工具 | `mosaico.py`、`.mosaico.json` | 启动固定的工具子模块并提供工作区配置 | 不依赖 `.agents/` 私有资产 |
| Agent 资产 | `.agents/` | Agent 专用文档与工具 | 不作为用户 API |

### 3.4 统一产品命令

| 命令 | 用户语义 | 默认行为 |
| --- | --- | --- |
| `python mosaico.py doctor` | 检查主机开发环境 | 只检查 Python、ESP-IDF、ESP-Iris、状态目录和 USB 枚举，不构建或写设备 |
| `python mosaico.py list` | 查看设备清单 | 连接 Gateway 并列出在线及缓存离线设备的 Device ID、在线状态、固件身份、模式、连接方式和 Boot ID；`--details` 展开端点及能力信息 |
| `python mosaico.py recover` | 初始化或恢复设备 | 使用评审基础包，完成后停留在 Recovery 就绪状态 |
| `python mosaico.py install` | 安装普通应用 | 构建工程并通过 ESP-Iris 安装，不自动触发恢复 |
| `python mosaico.py monitor` | 查看设备日志 | 先显示保留日志，再持续跟随至用户结束 |

构建 profile、启动基础产物和设备布局均属于 `mosaico.py` 的内部实现，不作为
用户参数或正常工作流的一部分。

### 3.5 设备更新状态流

```text
设备未知或 Recovery 未验证 ── recover ──► Recovery 就绪
                                              │
                                              └── install ──► 正常应用健康运行
                                                                     │
                                                                     └── monitor
```

闭环验收要求：保持同一 Device ID，各次启动产生新的 Boot ID，Recovery 服务
可用，最终固件身份匹配且产品行为健康。

## 4. 功能规格

### 4.1 Agent-Led 编排能力

| ID | 功能要求 | 验收口径 |
| --- | --- | --- |
| AL-001 | Agent 必须是正常流程的默认执行主体 | 用户连接设备并提出目标后，无需手工执行环境配置、构建、固件选择和 OTA 命令 |
| AL-002 | Agent 必须持续推进任务 | 构建或设备操作失败后，Agent 收集证据、诊断原因，并在安全和任务范围内继续修正 |
| AL-003 | Agent 必须按需编排能力 | 根据任务选择相关 Skill、组件和设备工具 |
| AL-004 | Agent 必须感知真实设备状态 | 设备操作基于实时身份、固件和健康状态 |
| AL-005 | Agent 必须以实物行为作为完成依据 | 任务完成需要真机功能通过验收 |
| AL-006 | Agent 必须提供可审查证据 | 交付记录变更、部署结果和真机证据 |
| AL-007 | Agent 必须正确处理人工接管 | 需要用户决策、授权或物理操作时暂停 |
| AL-008 | Agent 必须支持任务恢复 | 构建失败、设备重启、进入 recovery 或会话中断后，能够依据持久化记录继续同一目标 |
| AL-009 | Agent 必须最小化用户操作 | 对可由软件安全完成的步骤，不要求用户复制命令、选择端口或人工转抄设备状态 |

### 4.2 仓库与工程管理

| ID | 功能要求 | 验收口径 |
| --- | --- | --- |
| FR-000 | 仓库必须作为 Agent 主导的人机协同开发统一入口 | Agent 从根目录 `AGENTS.md` 获取任务路由，作为默认执行主体推进开发闭环，过程可由用户复核 |
| FR-001 | 仓库必须提供中英文入口说明 | 根目录存在 `README.md` 与 `README_CN.md`，且核心开发、调试、恢复规则一致 |
| FR-002 | 仓库必须提供可复制的参考应用 | `projects/hello_world` 可作为新应用基线，具体应用位于独立 `projects/<project-name>`；`projects/` 不包含 Recovery 工程 |
| FR-003 | 仓库必须将板级实现作为子模块管理 | `submodule/esp-mosaico-bsp` 可解析到固定 Git revision，应用通过组件依赖使用 BSP |
| FR-004 | 仓库必须分离产品工具与 Agent 资产 | 产品 CLI 进入 `tools/`，用户材料进入 `docs/`，Agent 私有资产进入 `.agents/` |
| FR-005 | 开发任务必须按需加载指南和子模块 | 任务只初始化所需子模块，并先检查 `skills/README.md` 与相关 `SKILL.md` |
| FR-006 | 正常开发流程必须由 Agent 主导 | 用户连接设备并描述目标后，Agent 持续推进到真机验收 |
| FR-007 | 任务过程必须可审查 | 关键决策、部署方式和验证结果具有明确记录 |

### 4.3 环境与构建

| ID | 功能要求 | 验收口径 |
| --- | --- | --- |
| FR-101 | 项目必须约束 ESP-IDF 版本 | `idf_component.yml` 声明 `idf >= 6.1` |
| FR-102 | 环境解析必须验证真实工具链 | 若根目录存在 `Environment`，仅作为不可信静态清单读取；同时验证 IDF 路径、版本、revision、Python 环境及 ESP32-S31 支持 |
| FR-107 | 主机 CLI 必须支持 Python 3.8 或更新版本 | Gateway 隔离环境与当前解释器 major/minor 一致，单个条件锁自动选择兼容依赖 |
| FR-108 | ESP-IDF Python 必须独立解析 | CLI 使用 Python 3.8/3.9 时，为 ESP-IDF 6.1 bootstrap 解析并验证 Python 3.10 或更新解释器；缺失时明确区分 CLI 与 ESP-IDF 的版本要求 |
| FR-103 | `install` 必须解析和构建用户工程 | 当前工程优先；无法唯一选择时列出候选并退出 |
| FR-104 | `--skip-build` 必须显式标记复用 | 只复用完整 BIN/ELF/MAP，并在结果中返回 `reused_build=true` |
| FR-105 | `recover` 必须校验评审基础包 | 文件缺失、哈希不匹配或目标不兼容时停止，不写设备 |
| FR-106 | 当前源码恢复必须显式选择 | 仅 `recover --source current` 构建候选包，并显示未经评审警告 |

### 4.4 参考固件能力

| ID | 功能要求 | 验收口径 |
| --- | --- | --- |
| FR-201 | 参考固件必须初始化 NVS 和板载显示 | 启动后显示 normal 或 recovery 对应界面，并报告 480×480 显示启动状态 |
| FR-202 | 参考固件必须接入 ESP-Iris | `esp_iris_start()` 成功，Gateway 能获取设备状态和启动记录 |
| FR-203 | 参考固件必须提供屏幕镜像后端 | 将活动 LVGL RGB565 帧经 ESP-Iris screen backend 提供给工作台 |
| FR-204 | normal 固件必须提供进入 Recovery 的能力 | Gateway 可以将同一设备切换到 Recovery，用户无需了解内部调用 |
| FR-205 | 固件必须提供安装状态与健康确认 | Gateway 可以判定写入、重启、固件身份和健康结果 |
| FR-206 | recovery 界面必须反映 Gateway 连接状态 | 至少区分启动中、等待连接、协商中、已就绪和失败 |
| FR-207 | normal 固件不得包含 OTA writer | normal 配置启用 `CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY`，同时禁用 `CONFIG_ESP_IRIS_OTA` |
| FR-208 | recovery 固件必须包含 OTA writer | recovery profile 启用 `CONFIG_ESP_IRIS_OTA` 和 recovery 模式配置 |

### 4.5 UI 与多模态调试

| ID | 功能要求 | 验收口径 |
| --- | --- | --- |
| FR-221 | 应用 UI 必须基于真实板级显示约束实现 | 明确使用 480×480、RGB565、LVGL 和对应触摸能力，不套用未经适配的通用前端尺寸 |
| FR-222 | UI 必须可通过 Gateway 观察 | ESP-Iris screen backend 可返回活动 LVGL 帧，工作台能够显示与设备一致的画面 |
| FR-223 | UI 迭代必须进入真机闭环 | 每个关键界面完成真机显示和交互验证 |
| FR-224 | 调试证据必须支持多模态关联 | 同一次启动的日志、画面和设备状态可以关联 |
| FR-225 | GSP 场景必须可在 PC 上用 pinned ESP-GSP 仿真 | `python3 tools/gsp-sim/run.py --headless --dump-ppm` 使用 `submodule/esp-gsp` 1.1.0 配套的独立 `sim` |

### 4.6 板级与扩展能力

以下能力由 BSP 子模块提供，具体应用需显式集成并验证，不能仅因子模块存在就视为应用已经启用。

| 能力域 | 已提供的 BSP/组件能力 |
| --- | --- |
| 人机界面 | 480×480 CO5300 QSPI 显示、CST9217 电容触摸、LVGL 接入 |
| 音频 | ES8311 麦克风与扬声器 codec |
| 传感 | BMI270 IMU、两颗 BMM150 磁力计、BQ27220 电量计 |
| 存储与输入反馈 | SPI NAND、AI/BOOT 按键、状态 LED、振动马达 |
| 扩展 | 左右热插拔模块槽、模块发现、描述符校验和独占生命周期管理 |
| 扩展模块 | 左槽 OV3640 摄像头；左右槽按键灯、摇杆模块 |
| 多设备 | 磁吸交互分类、拓扑、游戏原语、ESP-NOW peer link、路由和应用消息 |

### 4.7 设备运维与恢复

| ID | 功能要求 | 验收口径 |
| --- | --- | --- |
| FR-301 | 日常设备操作必须经 Gateway | ESP-Iris CLI 执行设备操作，Web 工作台观察同一记录 |
| FR-302 | 每次操作必须查询实时设备身份 | 不使用缓存 Device ID/Boot ID 作为当前证据，操作前查询 live device/status |
| FR-303 | Gateway 拥有 USB High-Speed 会话时不得直连串口 | normal 与 recovery 均由 ESP-Iris 使用唯一 High-Speed USB，会话不可并发占用 |
| FR-304 | 首次应用安装前必须验证 Recovery | 空白、缺失、版本不匹配或状态未知设备先运行 `recover` |
| FR-305 | normal 固件必须通过 recovery-mode OTA 安装 | 运行 `python mosaico.py install`；更新完成后确认固件身份、健康状态和新 Boot ID |
| FR-306 | 安装或恢复前必须保存故障证据 | 若存在有效 core dump，先保存结构化证据与原始日志 |
| FR-307 | 最后恢复必须限制破坏范围 | 仅在 normal/Recovery 均不可达时进入恢复模式；锁定唯一目标设备；不执行整片擦除 |
| FR-308 | 恢复必须以产品行为为终点 | 恢复完成需要目标固件身份匹配并通过功能验证 |
| FR-309 | Recovery 必须使用设备/物理 endpoint 级维护租约 | 即使目标尚未完成 HELLO，也只 detach 该 endpoint；Gateway 进程及其他设备操作保持运行 |
| FR-310 | Recovery 仅允许 Gateway 本机执行 | 远程 profile 在获取租约或写入前明确失败 |
| FR-311 | 本地 Gateway 必须匹配固定 ESP-Iris revision | revision 不一致时拒绝操作且不得终止现有 Gateway |

### 4.8 非功能规格

| ID | 非功能要求 | 验收口径 |
| --- | --- | --- |
| NFR-001 | 可恢复性 | 保留 Recovery 能力，任何普通应用安装失败不得破坏恢复路径 |
| NFR-002 | 可追溯性 | 设备身份、固件版本和操作证据可关联 |
| NFR-003 | 可复现性 | 构建配置、组件 revision 和 recovery manifest 可检查 |
| NFR-004 | 资源安全 | 应用优先调用 BSP 与具体模块驱动，不直接抢占扩展槽共享引脚和总线 |
| NFR-005 | 变更隔离 | 应用、BSP 和 Agent 资产保持目录边界 |
| NFR-006 | 失败可见性 | recovery 产物不兼容、构建 profile 非法、设备状态异常等情况必须显式失败，不静默降级 |
| NFR-007 | 安全变更 | 凭据、设备身份、Recovery 数据和整片 Flash 的破坏性变更需要明确授权 |
| NFR-008 | 最短路径 | Agent 优先使用仓库内已验证的 Skill、组件、示例与设备工具，避免不必要的云端检索和重复环境探索 |
| NFR-009 | 人工负担最小化 | 正常链路仅要求用户连接设备并确认目标/结果；只有权限、产品决策或不可软件完成的物理操作才转交用户 |
| NFR-010 | 闭环连续性 | Agent 在未满足完成定义且仍有安全可行路径时持续推进，不把中间产物作为最终结果交付 |
| NFR-011 | 可观察性 | 用户能够了解执行阶段、设备状态和验证结果 |

## 5. 技术优势

1. **Agent-Led 闭环**：Agent 持续推进开发任务，直至真机结果满足目标。
2. **真实设备感知**：ESP-Iris 向 Agent 提供统一的设备状态与多模态证据。
3. **能力按需编排**：Agent 根据任务选择 Skill 和组件，减少上下文与环境探索。
4. **软硬件资料接入**：仓库集中提供板级支持、专项框架和参考示例。
5. **UI 真机闭环**：Agent 依据设备约束实现界面，并通过屏幕镜像检查效果。
6. **部署与 Debug 一体化**：ESP-Iris 统一设备操作、观测和故障取证。
7. **安全且可追溯**：Recovery-first 保留恢复路径，操作记录支持复核。

## 6. 应用场景

### 6.1 仓库工作流适用场景

| 场景 | 本仓库提供的价值 |
| --- | --- |
| 从创意快速制作实物原型 | Agent 将自然语言目标转化为工程、UI 和设备功能，持续构建部署到真机并根据反馈迭代 |
| 新建 ESP-Mosaico 应用 | 从统一参考工程派生，继承目标、依赖和恢复契约 |
| Agent 主导应用开发 | Agent 通过仓库规则、Skills、BSP 和设备证据持续推进实现与验证，用户在关键节点决策和验收 |
| UI/交互快速迭代 | 基于 480×480 LVGL 与触摸能力实现界面，并通过 Gateway 屏幕观察完成真机视觉闭环 |
| 固件候选版本验证 | 使用 candidate profile 与 recovery-mode OTA，在不覆盖保底镜像的情况下验证新版本 |
| 远程/重复设备调试 | 通过 Gateway 获取日志、屏幕、Job、状态、重启与 core dump，并用 Device ID/Boot ID 关联 |
| 更新失败恢复 | 运行 `python mosaico.py recover`，就绪后再运行 `install` |
| BSP 与应用协同开发 | BSP 能力保留在子模块，应用工程保留在 `projects/`，分别演进和审查 |

### 6.2 BSP 示例证明的产品方向

以下方向具有仓库内示例作为技术参考，但仍需在具体用户应用中完成需求定义、集成和产品级验证：

- 显示、触摸、按键、LED、振动和设备设置类 HMI 应用。
- 音频回环、MP3 播放、中文 TTS、唤醒词与语音命令应用。
- 摄像预览、拍照存储、Wi-Fi 图传和端侧视觉模型展示。
- IMU 手势识别、磁吸交互、多设备拓扑和跨屏游戏。
- Windows 扩展屏、HID 触摸与 UAC 音频等 USB 复合设备。
- 左右扩展槽上的摄像头、按键灯和摇杆模块应用。

## 7. 边界

### 7.1 仓库承诺范围

- 作为 ESP-Mosaico 定制的 Agent 主导人机协同开发统一入口，覆盖从想法到真机验证的工程链路。
- 正常流程由 Agent 持续推进到真机验证。用户负责目标和验收。
- 提供 ESP-Mosaico 开发版的统一工程起点和目录规范。
- 提供 Recovery-only 的 `factory` 固件、普通应用参考工程及 recovery-first 工作流。
- 通过 BSP 子模块提供板级能力和示例来源。
- 规定通过 ESP-Iris Gateway 执行日常设备操作并保留证据。
- 规定固件交付与恢复的安全路径。

### 7.2 非承诺范围

- Agent 的默认执行权适用于任务范围内的低风险工程步骤。
- “用户只需连接设备”描述设备可识别时的正常体验。授权与物理操作仍由用户完成。
- 仓库面向开发阶段。量产需要独立的产品验证。
- BSP 示例只证明对应技术路径。
- 第三方扩展模块需要匹配的驱动与资源仲裁。
- 硬件合规和生产测试属于独立工作范围。
- 云服务与配套客户端需要按产品需求另行建设。
- 当前更新策略不承诺保留第二份旧应用镜像供 A/B 回退。

### 7.3 人工接管与授权边界

Agent 遇到以下情况必须暂停，并向用户说明风险及所需输入：

1. 产品目标或交互方向存在会显著改变结果的歧义。
2. 需要用户提供新的外部授权。
3. 需要用户执行物理操作。
4. 操作可能破坏持久化数据或恢复路径。
5. 任务范围或既定架构需要改变。

用户完成介入动作后，Agent 重新查询实时状态并继续推进。

### 7.4 已知技术约束

1. ESP-Mosaico 只有一个 USB High-Speed 接口；Gateway 占用时，其他工具不得并发打开同一会话。
2. ESP32-S31 需要兼容的 ESP-IDF 预览目标支持，由统一命令解析和验证环境。
3. 摄像头模块只支持左侧扩展槽，启用前必须核对扩展槽资源占用。
4. 两个扩展槽共享 I2C 等资源；具体模块驱动持有插槽时要求独占，应用不得绕过模块管理器抢占引脚。
5. 磁力交互校准与机械结构有关；磁铁、传感器朝向、外壳或装配公差变化后必须重新校准和验证。
6. Recovery manifest 记录完整基础包的来源、布局和哈希；发布或量产基线应从干净、可复现 revision 生成并验证。
7. Recovery 首次配置由 `python mosaico.py recover` 完成；正常应用由 `python mosaico.py install` 安装。
8. Skill 体系支持持续扩展。仓库能力以已经接入并通过验证的资源为准。
9. `mosaico.py` 与 ESP-Iris Gateway 支持 Python 3.8 或更新版本；ESP-IDF 6.1
   的 bootstrap 解释器仍须为 Python 3.10 或更新版本，两套解释器由统一命令
   分别解析和检查。

### 7.5 变更控制

以下变更视为架构级变更，实施前应获得开发负责人确认，并同步更新本规格书、参考工程和恢复流程：

- 删除或改变保留 Recovery、正常应用或崩溃证据能力。
- 将 OTA writer 移入 normal 固件，或取消默认经 recovery 更新。
- 改变进入 recovery RPC、健康确认或 OTA 状态协议。
- 更换目标芯片、最低 ESP-IDF 版本、USB 传输所有权或 Gateway 运维入口。
- 改变 Agent 默认执行权、人工授权点或任务完成定义。
- 改变应用、BSP 子模块、用户文档和 Agent 资产的目录职责。
- 引入 Secure Boot、Flash Encryption 等安全机制并重新生成 recovery 产物。

### 7.6 仓库级完成定义

一次面向具体应用的交付只有同时满足以下条件，才可视为完成：

1. Agent 已明确目标和完成条件。
2. 应用位于独立的 `projects/<project-name>`，并只依赖已检查的板级或组件 API。
3. 在兼容的 ESP-IDF/ESP32-S31 环境中完成对应 profile 构建，构建日志和产物可追溯。
4. 设备已有经过验证的 Recovery；若没有，已先运行 `python mosaico.py recover`。
5. normal 固件已通过 `python mosaico.py install` 完成 Gateway recovery-mode OTA。
6. 同一 Device ID 完成 `normal → recovery → normal`，Boot ID 按启动变化，recovery writer 与最终 normal 固件均已确认。
7. 最终产品行为已在真机验证，必要证据已经保存。
8. Agent 已提交可复核的变更摘要和验收证据。
