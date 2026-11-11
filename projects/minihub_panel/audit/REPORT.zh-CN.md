# MiniHub Panel 只读审查与修改方案

> 2026-11-11 复核说明：本报告保留原始只读审查记录。后续源码对照发现，
> “Matter 300 秒阻塞 hub_client”和“屏幕离线添加 Matter 静默丢失”并不
> 成立；已确认问题、代码整改和验证结果见
> [REVIEW-RESULT.zh-CN.md](REVIEW-RESULT.zh-CN.md)。

审查对象：`https://github.com/Xinlin-He/esp-mosaico-projects/tree/feat/minihub-panel`
目标提交：`aa51ca03bb9f5e7682dc98cae0081619a7cb9033`（2026-11-10 18:09:52 +0800，`feat: add the MiniHub touch panel`）
工作区审查前 `git status --porcelain` 为空；审查后仍为空。没有修改受跟踪文件，没有提交、推送或拉取请求。

这不是穷尽证明。下面只记录能用源码、主机复现或实际构建日志支撑的问题。未在实机上运行的行为一律标成待验证，不写成已通过。

## 1. 结论

默认 Zigbee 固件在 Mosaico 规定的 ESP-IDF 上**配置阶段就失败**，到不了编译和链接。失败原因是 ESP-Iris 0.2.0 锁定 `espressif/mdns` 1.12.0，而始终被编进依赖图的 `minihub_js` 锁定 1.13.1。工程注释已经知道这个冲突，但避开 `cmake/minihub_bundle` 并没有避开 `minihub_js`。

在此之外，屏上场景/自动化只能看见设备列表第一页、最多 4 个动作；默认镜像仍提供 BLE Mesh，并且添加时会关掉 Zigbee。这两处与 README 的产品规则不一致，不依赖实机就能从调用关系确认。

内存和任务模型也偏紧：`hub_client` 用 8192 字节的 PSRAM 栈去跑 Wi-Fi、NVS、HTTP 和 Matter 配网，命令队列每项约 1408 字节、深度 8，约占 11 KB 内部 RAM。Web 侧写在 `sdkconfig` 里的“4 个套接字、外部栈”在当前 MiniHub 里不是合法 Kconfig，实际服务器写死 20 个套接字和 16384 字节内部栈。

完整固件链接被两层挡住：未改源码的配置被 mdns 拦住；把依赖针脚只改在临时副本上之后，CHIP 的 `gn gen` 仍因 Matter 引导环境不完整而失败（`cffi` 缺少 Python 头文件，`_PW_ACTUAL_ENVIRONMENT_ROOT` 未建立）。因此**没有**未改源码的固件通过记录，也没有实机结果。

## 2. 范围、版本与架构

### 2.1 固定下来的版本

| 角色 | 仓库 | 提交 / 版本 | 说明 |
| --- | --- | --- | --- |
| 目标分支 | `Xinlin-He/esp-mosaico-projects` `feat/minihub-panel` | `aa51ca03bb9f5e7682dc98cae0081619a7cb9033` | 相对 `main` `94794a8` 增加 `minihub_panel` 等 112 个路径 |
| Vibe 工作区 | `esp-mosaico/esp-mosaico-vibe` | `d6aa4a8d3277b0c7ad3d38e54c403f2388814d1e` | 2026-11-03 |
| utils | `esp-mosaico/esp-mosaico-utils` | `be151ae047014e79a9c7ae0092dd203844499003` | Vibe 子模块 |
| BSP | `esp-mosaico/esp-mosaico-bsp` | `5f88b91e1be055d1d3674364123b2779a1758377` | Vibe 子模块 |
| 上游 MiniHub | `espressif/esp-minihub` `master` | `7cf43bf39e754a9f093937fa14c7be696c475f93` | 2026-11-10 |
| ESP-Matter | `espressif/esp-matter` `main` | `84c242d260151c144281f15d6bb9c684bf9039fb` | 2026-11-10 |
| connectedhomeip | 上述 Matter 子模块 | `99a81bd32986c5292b0b1c9245c8e247c2ad717d` | 与 Matter README 中的 CHIP 提交一致 |
| Mosaico 规定的 ESP-IDF | `espressif/esp-idf` | `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe` | `version.cmake` 为 6.2.0；`git describe` 为 `v6.1-dev-7333-g7b9cc1ac79`。doctor 判定满足 `idf: ">=6.2"` |
| MiniHub README 写的 IDF | 文档中的 `8f8481b790354212b7016282eb08f5074aa91820` | 公共 GitHub `git ls-remote` 无此对象 | 不能按上游文档针脚从 GitHub 取到 |
| Matter 对 S31 的建议 | README | `e9da155a72624fce88b8ef2cf3cde9aee2e6067f`（2026-07-02）或 IDF v6.0.2 | 与 Vibe 针脚、MiniHub 文档针脚都不同 |
| 工具链 | 本次安装 | `riscv32-esp-elf-gcc` 16.1.0（`esp-16.1.0_20260609`），`gspc` 0.6.1，`gn` 2501 | GSP 运行时针脚为 `espressif/esp-gsp` 1.5.1，LVGL `==9.5.0` |
| raylib | Vibe 子模块 | 未初始化 `99acdc58ee43380ef1c7f6eb215c06ee70af8dd8` | 游戏固件未构建 |

doctor 原文：`/opt/cursor/artifacts/minihub-audit/logs/idf-doctor.txt`。

### 2.2 仓库里实际有什么

`feat/minihub-panel` 相对 `main` 的功能增量就是 `minihub_panel`。同仓库还有 `basketball`、`snake`、`mosaico_chess`、`tof_instruments`，它们在 `main` 上已经存在。本次优先看 MiniHub 迁移和它与 Vibe/BSP/Iris/上游 MiniHub 的接缝，其它应用只做了边界核对和 `mosaico_chess` 的主机测试。

`minihub_panel` 没有 `AGENTS.md`，没有 `tests/`，没有入库的 `dependencies.lock`（根 `.gitignore` 忽略它）。质量钩子指向 Vibe 工作区里的 `check_project.py`，本仓库单独克隆时该相对路径不存在。

布局约定写在 `minihub_panel/CMakeLists.txt`：应用必须放在 `<vibe>/projects/minihub_panel`，`esp-minihub` 和 `esp-matter` 是 Vibe 仓库的兄弟目录。`components/app_minihub/Kconfig.projbuild` 用五级相对路径挂同一棵 MiniHub。README 只写了“复制到 `projects/`”，没有写这两个外部树。

### 2.3 运行时结构

`main/main.c` 是组装根，顺序是：

1. 把 cJSON 分配改到优先 PSRAM，`free()` 仍走 IDF 的堆释放（IDF 上这是合法的）。
2. `esp_flash_dispatcher_init`，Iris boot probe，NVS，`iris_ota_support_start`（Recovery 保持可达）。
3. 在 GSP/Wi-Fi 之前 `esp_bt_controller_init/enable`，失败只记录不中止。
4. 打开 GSP bundle 和 `app_display`。任一失败则停在 Recovery，不启动面板。
5. `app_hub` 独占 STA Wi-Fi 和到 `http://127.0.0.1` 的 REST/WebSocket。
6. `app_minihub_start` 在本机拉起 MiniHub Host，HTTP 端口 80。IMU 明确不启动。
7. `app_panel_create` 接 480×480 GSP 场景。

Host 启动顺序与上游 `application/minihub/main/main.c` 对齐到 rules/scenes/web/adapters，但关掉了 MiniHub 自己的 Wi-Fi STA、以太网、LCD、RainMaker、USB MSC、RTSP、MCP，也没有上游的 30 秒任务看门狗、SNTP 和工厂复位键。系统更新走 Mosaico Vibe Mode，不走 MiniHub OTA。这些是产品取舍，不是意外遗漏，除非下面单独标成缺陷。

分区前缀与 `product_contract.json` 的 `mosaico-retained-test-2m-v2` 一致：`otadata` 0x9000、`phy_init`、`sysmeta`、`vibe_mode` 0x20000、`coredump` 0x1e0000，`nvs` 在 0x200000，`main_app` 从 0x210000 起。应用把 `main_app` 缩到 `0x9f0000`，后面是 `storage` LittleFS `0x200000` 和 `ui_apps` `0x200000`，末端 0x1000000，对应 16 MB flash。不可变前缀没有被改写。

Zigbee 与 Thread 不能进同一镜像。Zigbee 镜像用 `variant_components/zigbee/minihub_thread` 桩；Thread 镜像用 Zigbee 桩。桩头文件是上游 API 的子集。Zigbee 桩缺少 `minihub_zigbee_init/start/onoff/...`，当前调用点大多包在 `CONFIG_MINIHUB_ZIGBEE_SDK` 里。Thread 镜像因此**可能**能链接，但本次没有构建 Thread 变体，不能当成已经证明。

### 2.4 明确不做的事

- 没有烧录，没有串口监视，没有操作开发板，没有 Gateway。
- 没有对 Matter、Zigbee、BLE、Wi-Fi 做互操作或渗透测试。
- 没有构建 snake、basketball、tof 的固件。tof 的 BSP 补丁只对针脚 `5f88b91` 做了 `git apply --check`（退出码 0）。文档写的基线 `7cbfb390` 不在这次浅克隆里。
- raylib 子模块未初始化，游戏模拟器未跑。
- 未改产品源码条件下，固件没有完成链接。探针构建改的是 `/tmp` 里的 MiniHub 副本，测完已 `git checkout` 还原，不是仓库改动。

## 3. 优先问题与修改方案

严重度按“不改就不能形成可安装镜像”和“装上之后会违反已写明的产品规则”来排。证据类只有三种：已复现、源码可确认、待验证。

### 问题 1 — 默认配置被 mdns 版本拆开（高，已复现）

未改源码的 Zigbee 配置在 Vibe 针脚 IDF 上失败，退出码 2，耗时 12.5 秒。

- 日志：`/opt/cursor/artifacts/minihub-audit/logs/minihub-build-unmodified-result.json`
- 清洗日志：`/opt/cursor/artifacts/minihub-audit/logs/minihub-build-unmodified-clean.log`
- 求解器原文：`esp_iris (0.2.0)` 依赖 `espressif/mdns (1.12.0)`，工程同时依赖 `espressif/mdns (1.13.1)`。

触发：把应用放到 `<vibe>/projects/minihub_panel`，兄弟目录放本次的 `esp-minihub` 与 `esp-matter`，用 IDF `7b9cc1ac` 执行 `idf.py build`（经 `idf_low_noise_build.py`）。还没进入编译。

机制：`app_minihub` 直接依赖 `minihub_hub` 和 `minihub_web`，二者的 CMake 又依赖 `minihub_js`。`minihub_js/idf_component.yml` 把 mdns 钉在 `"1.13.1"`。Iris `idf_component.yml` 钉在 `"1.12.0"`。`CONFIG_MINIHUB_DYNAMIC_MODULES` 在应用里是关闭的，JS 插件因此不会启用，但组件清单仍然参与求解。`CMakeLists.txt` 第 26–27 行写了不要导入 `cmake/minihub_bundle`，以免它把 mdns 钉到 1.13.1；这条路并没有盖住 `minihub_js`。Thread 变体还会再带上 `minihub_thread/idf_component.yml` 里的同一个 1.13.1 针脚。

影响：当前分支给不出可安装的 Zigbee 镜像，后续分区大小、告警和链接错误都还没被这条构建看见。

建议改哪里，不要在这一轮里直接改上游副本：

- 在 `minihub_panel/components/app_minihub/CMakeLists.txt` 不要把“功能关闭的 JS”当成必须链接的组件。Host 仍要 `minihub_hub` / `minihub_web` 时，应让这两个组件在 `MINIHUB_JS_PLUGINS=n` 时不要 `REQUIRES minihub_js`，或者在应用侧提供已有的 `minihub_js_run_on_worker_async` 桩并切断清单针脚。这需要上游 MiniHub 接受一个“JS 关闭时不拉取 mdns 1.13.1”的组件边界，而不是在应用里偷偷改 `components/minihub_js`。
- 若产品确实要 L2 JS，则在应用 `main/idf_component.yml` 不能靠再写一个 mdns 行来压过精确针脚。应与 Iris 维护者选定一个双方都能接受的 mdns（1.12.0 或同时升级 Iris），再改 MiniHub 清单。本次探针把**临时副本**里的 JS 针脚改成 1.12.0 之后，求解器放行，说明冲突点就是这颗针脚，不是别的组件。探针日志：`/opt/cursor/artifacts/minihub-audit/logs/minihub-build-mdns-probe-clean.log`。测完已还原该副本。

依赖与风险：改组件 `REQUIRES` 会碰到上游 `minihub_web` / `minihub_hub`。只在应用里复制一份 JS 桩，会在 Thread 变体和以后的 MiniHub 升级时再次分叉。1.12.0 是否含有 JS mDNS 用到的符号，要在真的把 JS 编进来时再验证；默认产品配置是关 JS 的。

验收：未改依赖针脚的策略下，`doctor` 仍报告 `idf_constraint_satisfied: yes`，`idf.py build` 能过组件求解并生成 `build/minihub_panel_zigbee.bin`（或 CMake 工程名对应的 app 镜像）。回归：Iris 仍链接 mdns 1.12.0；`CONFIG_MINIHUB_DYNAMIC_MODULES` 关闭时固件不含 QuickJS；打开 JS 的实验配置要么明确失败并写在文档里，要么和 Iris 使用同一 mdns。

### 问题 2 — 场景和自动化只覆盖第一页、最多 4 个设备（高，源码可确认）

触发：设备超过一页（每页 4 张卡片），或场景需要超过 4 个动作。首页可以翻页，编辑器不行。

机制：

- `update_ui` 在 `scene_building` 或 `rule_building` 时把快照页强制为 0。
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_panel/app_panel.c#L4442
- `can_create_scene` / `can_create_rule` 只扫描 `visible_points`，也就是当前这一页。第一页没有两台可控设备时，后面几页有设备也会提示“再添加一台设备”。
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_panel/app_panel.c#L3564
- `collect_unused_scene_points` 同样只看当前快照。
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_panel/app_panel.c#L3930
- `app_hub_scene_create` 拒绝 `count > APP_HUB_PAGE_SIZE`（4）。`fill_scene_actions` 展示时也在 4 个动作处截断。Hub 模型本身允许最多 64 个 point（`HUB_MAX_POINTS`），场景列表最多只保留 8 条。
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L3654
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L837

影响：第五台及以后的灯不能放进屏上场景或自动化。Web 上已有的多动作场景，屏上预览会被截断；按场景 id 执行仍可能在 Host 上跑全量，屏上显示和实际动作会不一致。这是实现限制，不是 README 写明的产品上限。README 只说至少选一个开或关。

建议：把“一页 4 张卡片”和“一条场景能包含的 point”拆成两个常量。编辑器用单独的可翻页 point 列表，索引不要绑在当前页的槽位上。`app_hub_scene_t.action_points` 的容量按产品上限（建议先定 16，并在 UI 上写出来）分配，超出时保存失败要有明确文案，而不是静默截断。`can_create_*` 应统计全部可控 point，而不是当前页。从 Web 同步超过上限的场景时，列表上标记“屏上不能完整编辑”，不要假装只有前 4 个动作。

风险：命令结构 `hub_command_t` 已经内嵌 `scene_points[4]`。加大动作数会继续加大队列项（见问题 4），应改成堆上的变长载荷，不要把更大的结构放进队列和 UI 栈。

验收：主机或模拟器里构造 6 个可控 point，确认第 5、6 个可以加入同一场景；只有 1 个可控设备在第 2 页时，创建自动化不会误报“需要另一台设备”。回归：一页 4 张卡片的首页布局、空列表、全是“跳过”不能保存，这些现有文案保持。

### 问题 3 — 默认 Zigbee 镜像仍提供 BLE Mesh，并会关掉 Zigbee（高，源码可确认）

README 写明：屏已经占用 Wi-Fi + 802.15.4 时，添加设备列表里不会出现 BLE Mesh。`sdkconfig.variant.zigbee.defaults` 却打开 `CONFIG_MINIHUB_BLE_MESH_SDK`。`app_hub_start` 只要这个宏存在就把 `APP_HUB_CAP_BLE_MESH` 放进能力位。`open_add_page` 只按能力位显示协议行，没有运行时射频互斥。

- https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/sdkconfig.variant.zigbee.defaults#L6
- https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L3084
- https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_panel/app_panel.c#L3226
- 选中 Mesh 时 `configure_add_adapter` 把 zigbee `enabled` 设为 false、ble_mesh 设为 true、thread 设为 false：
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L1638

影响：用户可以在“只显示当前固件支持的协议”的页面上点到 Mesh，并把已经在用的 Zigbee 适配器关掉。这和“Mesh 让路、保留 Zigbee”的射频规则相反。链接 Mesh 组件本身可以保留，以便以后做专门镜像；默认 UI 不应该把它当成可选项。

建议：能力位按运行时策略计算，而不是只按“组件有没有编进来”。Zigbee 镜像在 Wi-Fi STA 常开时清除 `APP_HUB_CAP_BLE_MESH`。不要从添加流程改写 `adapters.json` 里另一路 802.15.4 的 `enabled`。若将来要切换射频，做成设置里的显式模式，并写明会停止现有 Zigbee 设备。

验收：Zigbee 默认配置下添加页没有 Mesh 行；`/api/radio/provision` 仍可查询，但面板不能发起 permit。回归：Matter、Zigbee、MQTT、Modbus 行仍按各自 Kconfig 出现；Thread 镜像不出现 Zigbee 行。

### 问题 4 — `hub_client` 的栈、队列和事件任务里的 NVS（高，分配方式已确认，崩溃待验证）

`app_hub_start` 用 `xTaskCreateWithCaps(..., 8192, ..., MALLOC_CAP_SPIRAM)` 创建 `hub_client`，并用 `xQueueCreate(8, sizeof(hub_command_t))`。主机上按同样字段对齐测得 `hub_command_t` 为 1408 字节（结构里没有指针，32 位目标上数量级相同），8 项约 11264 字节，队列在内部 RAM。测量程序在 `/tmp/audit-refs/sizes.c`，不在仓库里。

- 任务与队列：
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L3060
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L3105
- Matter 添加在这条 8 KB 栈上再放一个 `hub_command_t`：
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L1934
- UI 入口同样在调用方栈上构造完整命令，例如 `app_hub_toggle`：
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L3167
- `IP_EVENT_STA_GOT_IP` 里直接 `save_wifi_profile` 和 `publish_commission_wifi` → `minihub_provision_save_credentials` → NVS commit：
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L2834

`sdkconfig.application.defaults` 还打开 `CONFIG_ESP_GSP_RENDER_TASK_STACK_PSRAM`。当前 `espressif/esp-gsp` 的 `master` Kconfig（GitHub 上没有 `v1.5.1` tag，不能把 master 的默认 12288/32768 当成 1.5.1 的已证实数值）写明：PSRAM 栈在 flash cache 关闭时不能使用，渲染回调若会碰 flash、NVS 或文件系统就应保持内部栈，或把这类调用丢给内部栈任务。面板回调会进 `app_hub_set_role` / Wi-Fi 保存 / 主题 NVS。本次没有量到渲染任务的高水位，也没有 coredump。

影响：配网、写 NVS 或关 cache 的 Wi-Fi 路径有机会在 PSRAM 栈上故障。8 KB 对 cJSON + HTTP + Matter 等待也偏小。11 KB 内部队列叠在 BLE、Wi-Fi 和 16 KB 的 httpd 栈上，内部 RAM 余量变小。这些崩溃**没有**在本次复现，因为固件没有跑起来。

建议：

- 队列项改成小消息（种类、id、长度）加上 PSRAM 里的载荷，`xQueueCreate` 不再按 1408×8 占内部 RAM。
- `hub_client` 使用内部 RAM 栈，先给 16 KB，Matter 等待不要占满这条栈；配网等待放到 Matter 自己的任务，worker 只轮询状态。
- `process_add_device` 不要再在栈上造第二个 `hub_command_t`，直接用已经在堆上的命令缓冲。
- GOT_IP 只置事件位，NVS 和 `minihub_provision_save_credentials` 由 worker 执行。
- 渲染回调里禁止直接写 NVS。主题、角色、Wi-Fi 口令都排队。在确认回调不再关 cache 之前，不要把 GSP 渲染栈放在 PSRAM。应用自己的规则是栈上聚合对象不超过 128 字节；现在的快照副本大约 1.1 KB（主机测量 `app_hub_snapshot_t` = 1168），`update_ui` 里同时有一份局部快照和一份 `previous` 副本。
  https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_panel/app_panel.c#L4441

验收：构建打开 `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` 不作为日常默认；用一次带高水位日志的启动，确认 `hub_client`、httpd、GSP 渲染任务的最小剩余栈都大于 1 KB，且内部 RAM 最小空闲在 Wi-Fi 已连接、页面已画出之后仍高于事先写明的下限（建议先定 24 KB，以实机为准）。回归：错误密码停在密码页、GOT_IP 后凭证仍在 NVS、重启后能自动重连。

### 问题 5 — 三份 sdkconfig 赋值不是当前 Kconfig，Web 实际用 20 套接字和内部 16 KB 栈（中高，已复现 + 源码可确认）

探针配置（只为越过问题 1，且 PATH 上有 `gn`）打印：

`unknown kconfig symbol 'MINIHUB_WEB_MAX_OPEN_SOCKETS'`
`unknown kconfig symbol 'MINIHUB_WEB_TASK_STACK_EXTERNAL'`
`unknown kconfig symbol 'COMPILER_DISABLE_DEFAULT_WARNINGS'`

它们写在 `sdkconfig.application.defaults` 第 25、26、95 行。日志把同一批未知符号也算到 `sdkconfig.variant.zigbee.defaults` 和 Recovery 的 defaults 上，那两个文件里没有这些行，是 kconfgen 对默认文件列表的重复归因，不要据此去改 Zigbee 文件。

上游 `minihub_web/Kconfig.minihub` 没有这两个 MiniHub 符号。`minihub_web_start` 写死 `max_open_sockets = 20`、`stack_size = 16384`，没有外部栈 caps。本次 IDF `7b9cc1ac` 里也没有 `COMPILER_DISABLE_DEFAULT_WARNINGS`。工程 CMake 另有 `-Wno-error=stringop-truncation` 等，警告策略不完全依赖这行。

- https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/sdkconfig.application.defaults#L25
- 上游实现：`esp-minihub` `7cf43bf` 的 `components/minihub_web/minihub_web.c` 约 6158–6161 行。

影响：作者以为把 httpd 限制成 4 个套接字并把栈放到 PSRAM，实际是 20 个套接字和 16 KB 内部栈。同文件把 `LWIP_MAX_SOCKETS` 调到 50，注释是为了在 httpd 拿走份额之后还给 CHIP mDNS。份额比注释假设的更大。`COMPILER_DISABLE_DEFAULT_WARNINGS=y` 不生效。

建议：删掉这三行，或改成当前树里真实存在的符号。套接字数量若产品要 4，就在应用能覆盖的 httpd 配置路径上设，不要写一个从未被读取的 `CONFIG_`。16 KB 栈若必须留在内部 RAM，就要和问题 4 的队列一起算内部 RAM 预算；若要外置，使用当前 IDF `httpd_config_t` 里真实的 caps 字段，并确认该任务不会在关 cache 时跑。

验收：配置日志不再出现这三条 unknown symbol。生成的 `sdkconfig` 里 httpd `max_open_sockets` 与设计值一致。回归：Web 控制台仍能打开，Matter 配网期间 `FindOperational` 不再因为 UDP socket 耗尽失败（此项要实机，未跑）。

### 问题 6 — 面板把局域网控制台认证关掉了（中，源码可确认；产品已写明，仍应视为上线前缺口）

`sdkconfig.application.defaults` 第 23 行 `# CONFIG_MINIHUB_WEB_LOGIN_AUTH is not set`。上游 `configs/sdkconfig/common.defaults` 和 `minihub_auth/Kconfig.minihub` 的默认都是打开。README 写了“登录认证这版默认关掉”。Panel 模式还会把用户输入的地址补成 `http://`，Bearer token 走明文。

这是刻意的版本缺口，不是无意写错。影响是：同一局域网里知道屏 IP 的人可以改设备、场景和适配器。不要把它算成“未知崩溃”，也不要在修构建之前就把它说成已经具备认证。

建议：恢复 `CONFIG_MINIHUB_WEB_LOGIN_AUTH`，首次启动强制设置本机口令或生成一次性 token，屏内 loopback 客户端使用该 token，不要为了省事关掉整个认证。Panel 模式在文档和 UI 上标明链路是明文 HTTP；若要给家庭以外使用，再加 TLS。不要把共享 token 编译进固件（上游 Kconfig 帮助已经说明这会变成产品密钥）。

验收：未带 cookie/Bearer 的 `GET /api/devices` 返回 401；屏在 Hub 模式下仍能控制本机。回归：错误口令不会被当成已登录；空口令配置不能把认证悄悄关掉。

### 问题 7 — 任务看门狗被关掉，且 Wi-Fi 未连接时部分命令被静默丢掉（中，源码可确认）

`sdkconfig.defaults` 第 65 行 `CONFIG_ESP_TASK_WDT_INIT=n`。上游 `app_main` 把任务看门狗配成 30 秒并 `trigger_panic`。Matter 配网等待默认 300 秒，所以不能照抄 30 秒去盯配网任务，但把看门狗整个关掉后，卡死的 worker 不会重启设备。

同一 worker 在队列排空时，若当时没有 `HUB_CONNECTED_BIT`，Matter 配网、删除设备和重启没有失败状态；场景/规则有“Network is not ready”。Wi-Fi 连接命令要等本轮其它命令处理完才执行，所以“连接”和“开灯”若落在同一次排空里，开灯会先被丢掉。

- https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/sdkconfig.defaults#L65
- https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L2936
- https://github.com/Xinlin-He/esp-mosaico-projects/blob/aa51ca03bb9f5e7682dc98cae0081619a7cb9033/minihub_panel/components/app_hub/app_hub.c#L3011

建议：看门狗只订空闲任务和确定不会做 300 秒阻塞的任务；配网等待不要占住唯一的 `hub_client`。每种入队命令在 Wi-Fi 未就绪时都写入对应 status。同一轮里先执行 `HUB_CMD_WIFI_CONNECT`，再处理依赖网络的命令。`sdkconfig.defaults` 里的 `CONFIG_ESP_MAIN_TASK_STACK_SIZE=20480` 会被后面的 `sdkconfig.application.defaults` 收成 10240，与上游 Matter 片段一致，这不是独立缺陷，避免以后有人以为生效的是 20480。

验收：未关联 Wi-Fi 时点灯，界面出现明确失败而不是无变化。配网超时有“timed out”而不是看门狗误杀。人为死循环只存在于测试构建时，空闲任务看门狗能重启；此项要实机。

### 问题 8 — 圆角/圆形可视区与 UI 资源分区余量（中，诊断已复现，实机与最终分区镜像待验证）

`gspc` 0.6.1 对 `main.json`：`diagnose` 0 error、16 warning；`build` 退出码 0。资源用量 `output_bytes=2059291`（字体约占 1.86 MB）。`ui_apps` 分区是 `0x200000` = 2097152 字节，差 37861 字节。这是编译器报告的场景资源，不是打进分区表之后的最终镜像，包装头还可能再占空间。

圆内安全区警告 7 条，对象包括 `home_title_light`、`wifi`（右上）、`previous`（20,382）、`next`（416,382）。BSP 热成像示例把边距写成圆角遮罩，没有证明有效区是整圆。若玻璃是圆的，翻页键和 Wi-Fi 芯片会被切掉；若只是小圆角，影响有限。本次没有上屏。

建议：用 BSP 量到的圆角或圆形安全边距（热成像示例用过 28 px）把翻页键和右上状态挪进安全区，并用实机看四角。固件能链接之后，检查 `ui_apps` 镜像大小；余量低于 64 KB 时减 CJK 字库或加大 `ui_apps`（只能从 `main_app` 尾部挪，且要先量 app 镜像，不能先砍应用槽）。

验收：`gspc diagnose` 对这 7 个对象不再报 `GSPC-LAYOUT-ROUND-SAFE`，或实机照片证明它们在圆角之内。分区打包日志里 `ui_apps` 使用量低于分区大小，并留下至少 64 KB。回归：480×480 首页四张卡片、深色/浅色主题、中文回退到 `设备 · <id>`。

### 问题 9 — Kconfig 被挂了两次（低，已复现菜单重复；没有导致这次配置失败）

`components/app_minihub/Kconfig.projbuild` 先 `rsource` 上游 `cmake/minihub_bundle/Kconfig.projbuild`，再开第二个 `menu "MiniHub"` 并重复挂入 core/auth/hub/js/matter/net/web 等片段。注释说不要依赖 bundle，因为那会拉 Thread/RainMaker；Kconfig `rsource` 本身不把 CMake 组件拉进链接，但会把符号定义两次。

在预期目录布局上用 kconfiglib 加载该片段：0 条 warning，38 个符号有两个定义节点，菜单树里有两个名为 MiniHub 的菜单。IDF 探针配置没有因为这些重复符号失败；它只对 Matter 自己的 `SEC_CERT_DAC_PROVIDER` 打了“multiple definitions”说明，并提示以后会提高严重级别。

建议：只保留一份清单。需要的 zigbee/thread/ble/modbus/mqtt 片段逐个 `rsource` 一次。不要同时挂 umbrella 和精简菜单。

验收：`menuconfig` 里只有一个 MiniHub 菜单；`MINIHUB_WEB_LOGIN_AUTH` 只有一个定义位置。回归：Zigbee/Thread 两个 `sdkconfig.variant.*.defaults` 里的符号仍然存在。

## 4. 刻意缺口（不要当成这次新发现的逻辑错误）

- IMU 不调用 `app_imu_start`。`main.c` 写明摇晃会在 `CHIPCryptoPALPSA` 里 coredump，手持分类也不够稳。崩溃本身本次没有复现，保持关闭是对的。重新打开前要有内部栈上的复现和实机分类数据。
- 无 BLE Mesh 与 Zigbee 同时工作，是射频预算。缺陷是 UI 仍把 Mesh 当作可添加项（问题 3），不是“镜像里不该出现 Mesh 目标文件”。
- 无 MiniHub 自带 OTA、RainMaker、RTSP、USB MSC、MCP、以太网。系统更新走 Vibe Mode。
- Web 登录关闭是写进 README 的本版选择，安全上仍应在问题 6 里收口。
- 动态中文名超出 `cjk.txt` 时显示 `设备 · <id>`，与 README 一致。
- `main` 任务栈最终是 10240，与上游 Matter 片段一致。

## 5. 测试矩阵

| 项 | 命令 / 位置 | 结果 |
| --- | --- | --- |
| 工作区干净 | `git status --porcelain` 审查前后 | 通过。记录在 `git-status-before.txt` 与 `git-status-after.txt` |
| IDF doctor | `idf_low_noise_build.py ... doctor`，IDF `7b9cc1ac` | 通过。`idf_version` 6.2.0，约束 `>=6.2` 满足，目标 `esp32s31` |
| 未改源码的 Zigbee 固件构建 | 同上 `build`，`ESP_MATTER_PATH` 指向 Matter `84c242d` | **失败，退出码 2**。mdns 1.12.0 与 1.13.1。没有 bin |
| mdns 探针配置 | 仅临时副本把 JS 的 mdns 改成 1.12.0 | 求解器通过，随后 `chip: 'gn' not found`。退出码非 0 |
| 带 `gn` 的探针配置 | PATH 指向 CIPD `gn` 2501，Matter `install.sh --no-host-tool` | **失败**。`gn gen` 因 `_PW_ACTUAL_ENVIRONMENT_ROOT` / `pigweed_environment.gni` 失败。bootstrap 在构建 `cffi` 时失败，系统没有 `Python.h` |
| Matter bootstrap | `/tmp/audit-refs/esp-matter/install.sh --no-host-tool` | **失败，退出码 1**。日志 `logs/matter-install.log`，`Failed building wheel for cffi` |
| GSP 场景诊断 | `gspc 0.6.1 diagnose --layout-check --round-safe-area` | 退出码 0。488 条里 472 条 information、16 条 warning、0 error |
| GSP 场景编译 | `gspc 0.6.1 build` | 退出码 0。`output_bytes=2059291` |
| Matter 配对码纯函数 | 主机程序调用 `app_hub_matter_pairing_code_ok` | 退出码 0。`"123"` 拒绝，11/21 位和 `MT:` 接受 |
| Python | `py_compile` `pack_web_seed.py`、`gen_onboard.py`、`apply_mosaico_theme.py`；`ruff check` | 通过 |
| `main.json` | `json.loads` | 通过 |
| Kconfig 双菜单 | kconfiglib 加载 `Kconfig.projbuild` | 加载成功，两个 MiniHub 菜单，38 个符号双定义 |
| chess 主机测试 | `cmake` + `ctest`，构建目录在 `/tmp/audit-refs/chess-tests` | **5/5 通过**（rules、session、gsp_contract、flows、peer_protocol） |
| snake / basketball 模拟与固件 | 文档中的 `mosaico.py game sim/build` | **未跑**。raylib 子模块未初始化 |
| tof 固件 | 文档中的 BSP 补丁 + 构建 | **未跑构建**。`git apply --check` 对 BSP `5f88b91` 退出码 0 |
| Thread 变体固件 | `MINIHUB_PANEL_VARIANT=thread` | **未跑** |
| 模拟器交互 | `mosaico.py project sim` | **未跑**。没有头显示，也没有在固件配置成功之后接 GSP 模拟器 |
| 实机：显示、触摸、Wi-Fi、Matter、Zigbee、Web、断电保持、Vibe 更新 | README 与 `AGENTS.md` 要求的板级路径 | **未跑**。禁止烧录 |

原始日志目录：`/opt/cursor/artifacts/minihub-audit/logs/`。
GSP 诊断原文在审查机 `/tmp/audit-refs/gsp-out/diagnose.json`（约 211 KB，未复制进 artifacts，避免把生成场景头文件一并带上）。结论数字以上表为准。

## 6. 实机回归矩阵（全部未跑）

| 用例 | 期望 | 状态 |
| --- | --- | --- |
| 首次引导：语言、Wi-Fi 错误密码停在密码页、Hub 模式 | 与 README 一致，重启记住 Wi-Fi | 未跑 |
| 首页 4 卡翻页、离线重连、中文缺字回退 | 不出现方块字；离线按钮有结果 | 未跑 |
| 非法 Matter 码 `123` 立即失败，不留 PAIR 卡 | 2 分钟超时只针对合法码 | 未跑 |
| 第 5 台设备加入场景 | 当前源码预期失败；修复后应成功 | 未跑 |
| Zigbee 镜像添加页无 BLE Mesh | 修复后 | 未跑 |
| 局域网无认证访问 `http://<ip>/` | 当前版本按 README 可打开；问题 6 修复后应为 401 | 未跑 |
| 摇晃设备 | IMU 关闭，不应重启 | 未跑 |
| `system-update` 不带 `storage.bin` | 已配对设备还在 | 未跑 |
| 带 `storage.bin` 的整包更新 | 设备被清空；这是 README 已说明的行为 | 未跑 |
| 看门狗与 coredump 分区 | 空闲任务卡死能重启并留下 coredump | 未跑 |
| 圆角处翻页键和 Wi-Fi 芯片 | 可见且可点 | 未跑 |

## 7. 构建阻断与未覆盖

阻断，分开看：

1. **产品依赖缺陷（已复现）**：mdns 1.12.0 与 1.13.1。不改依赖图就没有固件。
2. **环境阻断（已复现，不是应用 C 代码写错）**：Matter `install.sh` 在缺少 `python3-dev`（无 `Python.h`）时编不过 `cffi`，Pigweed 环境没激活，`gn gen` 找不到 `pigweed_environment.gni`。公共 GitHub 上也没有 MiniHub README 写的 IDF `8f8481b…`。三套 IDF 针脚（Vibe `7b9cc1ac`、MiniHub 文档、Matter 对 S31 的 `e9da155`）没有被任何一次成功链接验证过可以一起用。
3. **范围未覆盖**：Thread 镜像、实机、游戏工程、tof 实机、WebSocket/规则引擎/Matter CASE 的协议级测试、安全扫描。

## 8. 分阶段修改顺序

1. **先让未改产品行为的 Zigbee 配置能过求解并完成链接。** 处理问题 1 的组件边界，补上 Matter 引导（`python3-dev` 属于环境，写进应用 README 的外部依赖，而不是改业务代码）。用链接产物核对 `main_app` 与 `ui_apps` 的实际大小（问题 8 的容量部分）。删掉或改写问题 5 的无效 `CONFIG_`。
2. **再改用户看得见的规则。** 问题 3 的 Mesh 能力位，问题 2 的场景/自动化数据模型。命令结构同步改成堆载荷，避免问题 4 恶化。
3. **然后收内存和任务。** 问题 4 的栈和 NVS，问题 7 的看门狗与命令丢失。用高水位和一次实机 coredump 策略验收。
4. **最后收认证和字库/圆角。** 问题 6、问题 8 的安全边距。认证不要和“先编过”绑成同一次行为变更。
5. **补测试。** 配对码、场景分页、能力位可以做成不依赖 IDF 的主机测试。GSP `diagnose` 进预提交的 manual 阶段。固件构建以 Vibe 针脚 IDF 为门禁，并锁一份 `dependencies.lock` 或等价的完整针脚，不要每次重新求解间接依赖。

## 9. 仓库状态

审查结束时 `git -C /workspace status --porcelain` 为空，HEAD 仍是 `aa51ca03bb9f5e7682dc98cae0081619a7cb9033`。没有提交，没有推送，没有拉取请求，没有 issue，没有联系他人。构建和探针都在 `/tmp/audit-refs` 与 `/opt/cursor/artifacts/minihub-audit`。临时改过的 MiniHub JS 清单已还原。
