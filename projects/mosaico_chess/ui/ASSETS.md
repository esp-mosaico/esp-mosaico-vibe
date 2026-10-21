# UI 资源清单

## 正式资源

- `designs/pieces_redesigned/{w,b}{K,Q,R,B,N,P}.png`：已确认的棋子
  定稿，由棋盘、升变棋子池与升变面板直接共用。
- `designs/s00_home_480.png`、
  `designs/s10_mode_select_480.png`、
  `designs/s11_difficulty_480.png`、
  `designs/s20_wait_connect_480_v2.png`、
  `designs/s21_pair_success_480.png`、
  `designs/s22_pair_fail_480.png`、
  `designs/s30_play_board_480.png`、
  `designs/s30_board_highlights_480.png`、
  `designs/s30_{promotion,game_menu,resign_confirm,connection_lost,sync_error}_480.png`、
  `designs/s5{0_win,1_loss,2_draw}_480.png`：已确认的页面、棋盘交互和
  对局浮层设计基线。S00–S22 与 S50–S52 直接作为 GSP 页面视觉层，并叠加原生
  透明热区；S30 保持动态棋盘和棋子，外框与浮层面板由
  `tools/render_assets.py` 从确认稿无损派生到 `runtime/overlays/`，按钮、状态
  和棋局仍由原生 GSP/C 驱动。
- `runtime/screens/`：实际打包的圆角中英文页面和配对动画。
- `runtime/overlays/localized/`：由 `tools/render_assets.py` 调用双语资源生成器，
  生成器从中文确认稿派生英文对局浮层；目录同时保存已确认的英文等待对手页面。
  生成过程不修改 `designs/` 中的原始定稿。
- `fonts/cjk.ttf`：仅供资源生成工具渲染中文，字体为 Droid Sans Fallback（上游文件名
  `DroidSansFallbackFull.ttf`），Copyright © 2006–2010 Google Corp.，
  Apache License 2.0；完整条款和归属见
  `fonts/LICENSE.DroidSansFallback` 与 `fonts/NOTICE.DroidSansFallback`。
- `fonts/DejaVuSans.ttf`：用于 GSP 原生 ASCII 文本及生成英文运行时资源，
  Bitstream Vera/DejaVu 许可；
  完整许可见 `fonts/LICENSE`。

棋子是工程已有的定制素材，目录中没有保存其原始生成记录或授权文件。它们可用于
本工程内部验证，但在对外发布或商业分发前，项目负责人仍须补录来源、作者/生成
方式和可分发许可。不得将本说明误作已经取得授权的证明。

## 设计候选与定稿管理规则

- 临时候选稿不保存在正式 UI 目录。
- 仅当用户明确批准候选图后，才将其加入 `designs/`。`designs/` 仅允许新增
  已批准定稿，严禁直接修改或覆盖已有定稿。

已删除不再引用的旧版动画、结算素材和 HUD mockup。
