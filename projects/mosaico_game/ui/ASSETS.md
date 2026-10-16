# UI 资源清单

## 正式资源

- `pieces/set-illustrate/{w,b}{K,Q,R,B,N,P}.png`：当前棋盘、升变棋子池与
  升变面板使用的 3D 棋子组。
- `pieces/art_{win,loss,draw}.png`：B 版横幅纹章结算插画。
- `pieces/result_ribbon_{win,loss,draw}.png`：由 `render_assets.py` 生成的结算横幅。
- `pieces/heat_bg.png`、`pieces/anim_war.gif`：由 `render_assets.py` 生成的背景与“战”字动画。
- `pieces/anim_wk.gif`：原始首页动画。
- `fonts/cjk.ttf`：Droid Sans Fallback（上游文件名
  `DroidSansFallbackFull.ttf`），Copyright © 2006–2010 Google Corp.，
  Apache License 2.0。
- `fonts/DejaVuSans.ttf`：仅用于设计 mockup，Bitstream Vera/DejaVu 许可；
  完整许可见 `fonts/LICENSE`。

`render_assets.py` 的“战”字使用系统 AR PL UKai 字体烘焙。字体上游为
Arphic Technology，许可为 Arphic Public License；字体本体未随工程分发。

棋子、首页动画和结算插画是工程已有的定制素材，目录中没有保存其原始生成记录或
授权文件。它们可用于本工程内部验证，但在对外发布或商业分发前，项目负责人仍须
补录来源、作者/生成方式和可分发许可。不得将本说明误作已经取得授权的证明。

## 设计候选

- `pieces/candidates/`
- `pieces/set-v2/`

这些目录不被 `main.json` 或 `generate_ui.py` 引用，仅作为候选稿归档，不进入 GSP
bundle。正式资源与候选稿不得混用。

已删除不再引用的 `mark_*.png`、`war.png` 和旧 HUD mockup。
