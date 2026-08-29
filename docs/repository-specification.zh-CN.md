# ESP-Mosaico Vibe 仓库规格书

| 项目 | 规格 |
| --- | --- |
| 仓库名称 | ESP-Mosaico Vibe |
| 文档类型 | 仓库级产品与工程规格书 |
| 目标硬件 | ESP-Mosaico 开发板（ESP32-S31） |
| 软件基线 | ESP-IDF 6.1 或更高版本，并具备 ESP32-S31 目标支持 |
| 参考工程 | `projects/factory` |
| 板级能力来源 | `submodule/esp-mosaico-bsp` Git 子模块 |
| 设备运维入口 | ESP-Iris Developer Gateway 及其随组件提供的 CLI |
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

- 以 `projects/factory` 作为参考工程。
- 每个用户应用创建在独立的 `projects/<project-name>` 目录中。
- 除非明确要求修改模板，不将具体业务直接实现到 `projects/factory`。
- 参考工程固化芯片目标、设备接入和 recovery-first 契约。

### 2.5 UI 设计与真机视觉闭环

- Agent 基于设备显示与触摸约束实现 LVGL 界面。
- 参考工程通过 ESP-Iris 注册 RGB565 屏幕镜像后端，使开发者和 Agent 可在 Gateway 工作台观察真机界面。
- UI 调整与固件调试共享同一设备记录，减少人工往返。
- 专项 Skill 可扩展视觉比较能力。具体应用负责定义验收基准。

### 2.6 ESP-Iris 下载、烧录与调试闭环

- Agent 通过 ESP-Iris Developer Gateway 操作并观测设备。
- Gateway CLI 与 Web 工作台共享设备身份和操作记录。
- Agent 根据设备实时状态选择正常 OTA、首次 recovery 配置或最后恢复路径，用户无需手工拼接命令。
- 正常流程只要求用户连接 ESP-Mosaico；只有无法软件恢复时，才请求用户执行必要的按键和上下电动作。

### 2.7 Recovery-first 固件交付

- 保留 `factory` recovery 分区和正常应用 `ota_0` 分区。
- 正常固件只提供“进入 recovery”的 RPC，不携带 OTA writer。
- OTA writer 只存在于 factory recovery 固件中。
- 正常应用更新必须先进入 recovery，再由 ESP-Iris Gateway 将新固件写入 `ota_0`。
- 空白或 recovery 状态未经验证的设备，必须先配置 recovery，再执行首次应用 OTA。

### 2.8 统一设备操作与证据链

- 日常设备运维默认经 ESP-Iris Developer Gateway 完成。
- Agent 只使用 ESP-Iris 组件源码内提供的 CLI 操作设备。
- CLI 与 Web 工作台共享设备身份及操作记录。
- 固件或分区变更前，先保存有效的故障证据。

### 2.9 最小化不可恢复操作

- 正常开发不使用正常应用的 `idf.py flash`。
- `idf.py flash` 仅用于空白/未验证设备的首次 recovery 配置，或 normal 与 recovery 均不可达时的最后恢复。
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
 ├── 工程执行：projects/factory / projects/<project-name>
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
 ├── factory：保留 recovery + OTA writer
 ├── ota_0：正常 application / candidate
 ├── nvs / otadata / phy_init
 └── coredump：崩溃证据
```

### 3.3 仓库组成

| 层级 | 路径 | 职责 | 变更原则 |
| --- | --- | --- | --- |
| 仓库入口 | `README.md`、`README_CN.md` | 说明定位、创建工程和设备运维规则 | 保持中英文语义一致 |
| Agent 规则 | `AGENTS.md` | 路由开发任务、约束设备操作和恢复流程 | 规则应简洁且可执行 |
| 应用工程 | `projects/` | 容纳参考工程和用户应用 | 一个应用一个目录 |
| 参考模板 | `projects/factory` | 提供三种构建 profile、显示、ESP-Iris、屏幕镜像和 recovery-first OTA | 不承载具体用户业务 |
| 板级子模块 | `submodule/esp-mosaico-bsp` | 提供 BSP、扩展模块、交互/网络组件和示例 | 按任务初始化和检查 |
| 任务指南 | `skills/` | 提供环境安装、构建等任务化说明 | 只加载相关指南 |
| 用户文档 | `docs/` | 面向开发者说明工作流、规格和应用文档 | 不放 Agent 私有工具 |
| Agent 资产 | `.agents/` | Agent 专用文档与工具 | 不作为用户 API |

### 3.4 固件与分区架构

参考工程使用 16 MB Flash 分区布局：

| 分区 | 类型 | 偏移 | 大小 | 职责 |
| --- | --- | ---: | ---: | --- |
| `nvs` | data/nvs | `0x9000` | `0x6000` | 配置及 OTA 流程元数据 |
| `otadata` | data/ota | `0xf000` | `0x2000` | 启动分区选择与 OTA 状态 |
| `phy_init` | data/phy | `0x11000` | `0x1000` | PHY 初始化数据 |
| `factory` | app/factory | `0x20000` | `0x100000` | 保留 recovery 固件及 OTA writer |
| `ota_0` | app/ota_0 | `0x120000` | `0xc00000` | 正常 application 或 candidate |
| `coredump` | data/coredump | `0xd20000` | `0xd0000` | 崩溃转储证据 |

构建 profile 相互使用独立 build 目录和 `sdkconfig`：

| Profile | 用途 | 版本基线 | OTA writer | 默认更新方式 |
| --- | --- | --- | --- | --- |
| `application` | 正常开发固件 | `1.0.0` | 禁用 | 经 factory recovery OTA |
| `candidate` | 候选验证固件 | `1.0.1` | 禁用 | 经 factory recovery OTA |
| `recovery` | 保留恢复固件 | `1.0.0-recovery` | 启用 | 首次配置或最后恢复时烧写 |

### 3.5 设备更新状态流

```text
normal@ota_0
  │ 进入 recovery RPC（0x7fff/2）
  ▼
factory recovery + OTA writer
  │ Gateway recovery-mode OTA
  ▼
new normal@ota_0
  │ 启动、健康确认、记录新 Boot ID
  └──────────────────────────────► 可再次进入 factory recovery
```

闭环验收要求：同一 Device ID 依次完成 `normal → recovery → normal`，两次切换均产生新的 Boot ID；recovery 中可确认 OTA writer；最终固件健康运行于 `ota_0`。

## 4. 功能规格

### 4.1 Agent-Led 编排能力

| ID | 功能要求 | 验收口径 |
| --- | --- | --- |
| AL-001 | Agent 必须是正常流程的默认执行主体 | 用户连接设备并提出目标后，无需手工执行环境配置、构建、固件选择和 OTA 命令 |
| AL-002 | Agent 必须持续推进任务 | 构建或设备操作失败后，Agent 收集证据、诊断原因，并在安全和任务范围内继续修正 |
| AL-003 | Agent 必须按需编排能力 | 根据任务选择相关 Skill、组件和设备工具 |
| AL-004 | Agent 必须感知真实设备状态 | 设备操作基于实时身份、固件和分区状态 |
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
| FR-002 | 仓库必须提供可复制的参考工程 | `projects/factory` 可作为新应用基线，具体应用位于独立 `projects/<project-name>` |
| FR-003 | 仓库必须将板级实现作为子模块管理 | `submodule/esp-mosaico-bsp` 可解析到固定 Git revision，应用通过组件依赖使用 BSP |
| FR-004 | 仓库必须分离用户文档与 Agent 资产 | 用户材料进入 `docs/`，Agent 专用材料或工具进入 `.agents/` |
| FR-005 | 开发任务必须按需加载指南和子模块 | 任务只初始化所需子模块，并先检查 `skills/README.md` 与相关 `SKILL.md` |
| FR-006 | 正常开发流程必须由 Agent 主导 | 用户连接设备并描述目标后，Agent 持续推进到真机验收 |
| FR-007 | 任务过程必须可审查 | 关键决策、部署方式和验证结果具有明确记录 |

### 4.3 环境与构建

| ID | 功能要求 | 验收口径 |
| --- | --- | --- |
| FR-101 | 项目必须约束 ESP-IDF 版本 | `idf_component.yml` 声明 `idf >= 6.1` |
| FR-102 | 环境解析必须验证真实工具链 | 若根目录存在 `Environment`，仅作为不可信静态清单读取；同时验证 IDF 路径、版本、revision、Python 环境及 ESP32-S31 支持 |
| FR-103 | 参考工程必须支持三种构建 profile | `BUILD_PROFILE` 仅接受 `application`、`candidate`、`recovery` |
| FR-104 | 各 profile 配置必须隔离 | 每个 profile 使用独立 build 目录及生成的 `sdkconfig`，recovery 配置不得污染 normal 构建 |
| FR-105 | normal 构建必须校验 recovery 产物 | 缺失、被修改、超出 factory 分区、目标不兼容或 manifest 不匹配时构建失败，不静默回退到现场编译 |
| FR-106 | recovery 预构建更新必须显式执行 | 仅通过 `update-recovery-prebuilt` 目标发布新的 recovery image 与 manifest |

### 4.4 参考固件能力

| ID | 功能要求 | 验收口径 |
| --- | --- | --- |
| FR-201 | 参考固件必须初始化 NVS 和板载显示 | 启动后显示 normal 或 recovery 对应界面，并报告 480×480 显示启动状态 |
| FR-202 | 参考固件必须接入 ESP-Iris | `esp_iris_start()` 成功，Gateway 能获取设备状态和启动记录 |
| FR-203 | 参考固件必须提供屏幕镜像后端 | 将活动 LVGL RGB565 帧经 ESP-Iris screen backend 提供给工作台 |
| FR-204 | normal 固件必须提供进入 recovery RPC | 注册 service `0x7fff` / method `2`，记录计划重启并将 factory 设为启动分区 |
| FR-205 | 固件必须提供 OTA 状态与健康确认 RPC | service `0x1200` 提供状态方法 `1` 与接受方法 `2` |
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
| FR-304 | 首次应用 OTA 前必须验证 recovery | 空白、缺失或状态未知设备先配置并验证 factory recovery |
| FR-305 | normal 固件必须通过 recovery-mode OTA 安装 | 不运行正常应用的 `idf.py flash`；更新完成后确认 `ota_0`、版本、健康状态和新 Boot ID |
| FR-306 | OTA 或分区变更前必须保存故障证据 | 若存在有效 core dump，先保存结构化证据与原始日志 |
| FR-307 | 最后恢复必须限制破坏范围 | 仅在 normal/recovery 均不可达时进入 ROM 下载模式；使用确认过的端口；不执行整片擦除 |
| FR-308 | 恢复必须以产品行为为终点 | 恢复完成需要目标固件运行于预期分区，并通过功能验证 |

### 4.8 非功能规格

| ID | 非功能要求 | 验收口径 |
| --- | --- | --- |
| NFR-001 | 可恢复性 | factory recovery 保留，任何 normal OTA 失败不得以覆盖 recovery 为代价 |
| NFR-002 | 可追溯性 | 设备身份、固件版本和操作证据可关联 |
| NFR-003 | 可复现性 | 构建配置、组件 revision 和 recovery manifest 可检查 |
| NFR-004 | 资源安全 | 应用优先调用 BSP 与具体模块驱动，不直接抢占扩展槽共享引脚和总线 |
| NFR-005 | 变更隔离 | 应用、BSP 和 Agent 资产保持目录边界 |
| NFR-006 | 失败可见性 | recovery 产物不兼容、构建 profile 非法、设备状态异常等情况必须显式失败，不静默降级 |
| NFR-007 | 安全变更 | 凭据、设备身份、recovery 数据、分区和整片 Flash 的破坏性变更需要明确授权 |
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
| 新建 ESP-Mosaico 应用 | 从统一参考工程派生，继承目标、依赖、分区和恢复契约 |
| Agent 主导应用开发 | Agent 通过仓库规则、Skills、BSP 和设备证据持续推进实现与验证，用户在关键节点决策和验收 |
| UI/交互快速迭代 | 基于 480×480 LVGL 与触摸能力实现界面，并通过 Gateway 屏幕观察完成真机视觉闭环 |
| 固件候选版本验证 | 使用 candidate profile 与 recovery-mode OTA，在不覆盖保底镜像的情况下验证新版本 |
| 远程/重复设备调试 | 通过 Gateway 获取日志、屏幕、Job、状态、重启与 core dump，并用 Device ID/Boot ID 关联 |
| 更新失败恢复 | 从保留的 factory recovery 重装 `ota_0`；极端情况下再进入 ROM 下载模式 |
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
- 提供 `factory` 参考固件及 recovery-first 工作流。
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
- 当前单 normal slot 架构在更新失败时回到 factory recovery，不保留第二份旧 normal 镜像供 A/B 回退。

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
2. ESP32-S31 仍可能要求所选 ESP-IDF 版本使用 `idf.py --preview set-target esp32s31`。
3. 摄像头模块只支持左侧扩展槽；部分摄像头信号与 USB Serial/JTAG 引脚冲突，相关示例可能需要 UART 烧写与监控。
4. 两个扩展槽共享 I2C 等资源；具体模块驱动持有插槽时要求独占，应用不得绕过模块管理器抢占引脚。
5. 磁力交互校准与机械结构有关；磁铁、传感器朝向、外壳或装配公差变化后必须重新校准和验证。
6. recovery manifest 当前记录的基线包含开发版 ESP-IDF 和 dirty source 标记；发布或量产基线应从干净、可复现 revision 重新生成并验证。
7. recovery 首次配置可能需要通过编程口执行 `idf.py flash`。正常应用使用 Gateway OTA。
8. Skill 体系支持持续扩展。仓库能力以已经接入并通过验证的资源为准。

### 7.5 变更控制

以下变更视为架构级变更，实施前应获得开发负责人确认，并同步更新本规格书、参考工程和恢复流程：

- 删除或改变 `factory`、`ota_0`、`coredump` 分区及其容量。
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
4. 设备已有经过验证的 factory recovery；若没有，已先完成 recovery-first 配置。
5. normal 固件通过 Gateway recovery-mode OTA 安装到 `ota_0`，未使用 normal `idf.py flash`。
6. 同一 Device ID 完成 `normal → recovery → normal`，Boot ID 按启动变化，recovery writer 与最终 normal 固件均已确认。
7. 最终产品行为已在真机验证，必要证据已经保存。
8. Agent 已提交可复核的变更摘要和验收证据。
