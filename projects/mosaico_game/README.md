# mosaico_game

面向固定 480×480 ESP-Mosaico 屏幕的国际象棋应用。界面使用 ESP-GSP
1.5.1，原生模拟器和固件共用同一套 C 棋规、会话状态与 UI backend。

## 页面与流程

- S00：首页。当前只有国际象棋，不提供假分页和空设置页。
- S10：选择对战电脑或双设备对决。
- S11：选择电脑对手难度；简单使用单步战术估值，普通使用两层搜索，困难使用三层 Alpha-Beta 搜索。
- S20：PvP 等待连接；两台设备通过磁吸接触确定唯一对手并完成配对。
- S21：配对成立并显示本机阵营；双方点击“开始对局”后进入棋盘。
- S22：等待超时。
- Play：440×440 满棋盘；左上角隐藏手势入口打开对局菜单。
- S50–S52：胜、负、和棋的确认版结算页。

对局支持选中框、合法空格、可吃目标、最后一步、将军标记、认输二次
确认、断线/重连状态，以及不可跳过的四选一升变面板。产品不提供主动求和或
和棋请求；规则自动判和仍进入 S52。

双机模式复用 BSP 的双 BMM150 接触分类、`mosaico_peer_session`
`CLAIM → ACK → COMMIT` 握手和 `mosaico_peer_link` ESP-NOW 传输。
磁吸只用于确定通信对象；握手提交后物理分离不会解除棋局绑定。落子携带
ply 与前后局面哈希，支持确认重发、重复包过滤、断线提示及按着法日志重同步。
较小的稳定 Device ID 执白。

## 棋规

规则层支持标准棋子移动、将军过滤、钉住、将死、逼和、四侧王车易位、吃过路
兵、后/车/象/马升变、三次重复、50 步规则、子力不足和认输。终局后
拒绝继续落子。AI 通过同一合法着生成接口选择着法。

## 主机测试

从工作区根目录运行：

```sh
cmake -S projects/mosaico_game/tests -B projects/mosaico_game/build-tests
cmake --build projects/mosaico_game/build-tests
ctest --test-dir projects/mosaico_game/build-tests --output-on-failure
```

测试包含棋规、会话状态、PvE/PvP 流程、GSP callback/bind 合同、双机协议编解码
与特殊棋步同步，以及初始局面、Kiwipete 和 Position 3 的浅层 perft。

## 原生模拟器

```sh
python mosaico.py project sim \
  --project projects/mosaico_game --interactive --duration 120
```

命令输出 Preview URL 后，可用真实 framebuffer 回放保存流程证据：

```sh
python projects/mosaico_game/tests/run_sim_replay.py \
  --url http://127.0.0.1:3224/ \
  --output projects/mosaico_game/tests/evidence
```

端口以本次 Preview 输出为准。回放通过官方 simulator RPC 驱动共享 C backend，
不包含 Python 或网页棋规替代实现。

## 固件构建

工程不是 Raylib Lite 游戏，不能使用 `mosaico.py game build`。使用工作区固定
ESP-IDF 的低噪声构建器：

```sh
python3 submodule/esp-mosaico-utils/mosaico-tools/skills/idf-low-noise-build/scripts/idf_low_noise_build.py \
  --project projects/mosaico_game doctor
python3 submodule/esp-mosaico-utils/mosaico-tools/skills/idf-low-noise-build/scripts/idf_low_noise_build.py \
  --project projects/mosaico_game build
```

发布前需要从零验证时，在第二条命令后加 `--fullclean`。

## 设备更新

本工程保留 Vibe Mode 的 immutable partition prefix，并通过
`esp_mosaico_app_recovery` 启动 Iris OTA 支持。设备安装只能使用：

```sh
python mosaico.py iris system-update --project projects/mosaico_game
```

不得使用 `idf.py flash`、`esptool` 或直接写设备。本次源码验证未执行设备安装。
真机触控、High-Speed USB/Iris 共存、真实双设备磁吸配对、分离后同步、断线恢复
及最终显示效果仍需在目标硬件验证。

资源来源、正式素材与候选稿边界见 `ui/ASSETS.md`。
