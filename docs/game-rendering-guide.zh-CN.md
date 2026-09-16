# ESP-Mosaico 游戏渲染与画质优化指南

本文面向 ESP32-S31、480×480 RGB565 游戏。目标是在 Host 与设备使用同一份游戏
模型和 view 的前提下，稳定达到 30 FPS，并保持墙体、地板、精灵和 HUD 的视觉质量。

本文依据 Neon Maze 的实测整理：原始路径约 17–20 FPS、单帧渲染 50–58 ms；第一轮
批量墙体和双行地板达到 30.3 FPS、约 27.9 ms，但出现墙脚和透明素材回归；修复画面后
因逐列小绘制和逐像素除法回退到约 18.2 FPS、54.6 ms。这说明性能和画质必须用同一组
场景、截图与设备日志联合验收。

## 目标和边界

设备验收目标：

- 逻辑与显示帧率稳定在 30 FPS；复杂场景不低于 25 FPS。
- 单帧渲染预算不超过 30 ms，建议为显示与系统任务保留 3 ms 以上余量。
- 不通过明显降低墙体清晰度、删除反馈、破坏透视或引入闪烁来换取帧率。
- Host 与设备使用相同的地图、射线、深度、资源和绘制顺序。
- 平台实现可以针对 S31 使用 PIE SIMD，但标量实现必须保留给 Host 和其他目标。
- 每项优化都必须分别验证像素结果、动态观感和设备耗时。

画质调整分为三类：

| 类型 | 例子 | 验收方式 |
|---|---|---|
| 无损 | 连续内存写、缓存查询结果、定点增量、SIMD fill/copy | 像素完全一致 |
| 近似无损 | 亮度量化、合并相邻 span、预缩放尺寸档位 | 截图差异阈值 + 实机观察 |
| 有损 | 半分辨率世界、隔行地板、减少射线数量 | 必须显式配置并做动态画质确认 |

默认先做无损优化。只有无损路径仍无法满足预算时，才启用可控的有损档位。

## 统一渲染路径

游戏只保留一条 view：

```text
game state
  -> shared view
  -> Raylib-compatible RGB565 renderer
  -> Surface
       Host: RGB565 frame -> browser/PNG
       Device: PSRAM framebuffer -> GSP Canvas -> LCD
```

应用不得直接依赖 LCD、GSP、FreeRTOS 或 ESP-Iris。设备差异只放在 framebuffer 获取、
提交、输入和音频适配层。Host 仿真用于玩法、布局、像素和确定性验证；设备用于 CPU、
PSRAM、LCD 时序、触摸、声音和振动验证。

## 应用侧技巧

### 固定时间步与渲染解耦

- 游戏逻辑使用固定 tick，不以实际显示帧率改变移动速度和攻击间隔。
- update 中不加载资源、不分配内存、不解析 JSON、不执行 NVS 或日志批量输出。
- 低帧时可以丢弃过期画面，但不能跳过逻辑 tick 或累积输入延迟。
- 动画按照 tick 或明确的动画时间推进，不依赖某次 render 是否成功提交。

### 先做可见性，再绘制

- 地图对象先做视锥、距离和屏幕边界裁剪。
- 精灵先用 depth buffer 判断遮挡，再合并连续可见区间。
- 完全可见的精灵只调用一次缩放绘制；部分遮挡时只提交少量连续区间。
- 屏幕外、墙后的粒子、尸体、拾取物和特效不进入像素路径。
- 透明对象按需求排序；binary alpha 对象不进入通用 alpha blend。

### 射线和透视

- 使用网格 DDA，步数跟穿过的格子数量相关，禁止固定小步长推进射线。
- 每列缓存距离、材质、纹理 U、墙顶、墙底、侧面标记和亮度档位。
- 避免在像素循环中调用 `sinf`、`cosf`、`sqrtf`、`floorf` 和除法。
- 一帧只计算一次方向向量、相机平面、地平线和材质 frame。
- 垂直纹理坐标使用定点增量：起点加固定 step，禁止逐像素 64-bit 除法。
- 墙顶和墙底先裁剪到 viewport，再由同一组值驱动墙体、地板遮挡和精灵遮挡。

### 地板和天空

- 地板使用水平扫描线或 tile，不使用覆盖大区域的通用三角形。
- 相邻材质区间先合并为 span，再调用底层批量接口。
- 世界坐标到纹理坐标使用 16.16 定点增量。
- 远处可以使用两行共用一次采样，但这是有损档位，必须确认移动时没有明显横纹。
- 天空只绘制未被墙体覆盖的范围；相机不动时可以复用缓存。
- 地板、天空和墙体必须共享精确的交界坐标，避免黑缝、白线或覆盖到错误区域。

### 墙体

- 材质本身不能包含贯穿整张 tile 的高亮列，否则透视拉伸后会形成明显白线。
- 墙脚使用材质内置 skirting 或合并后的水平区段；禁止每个 2 像素墙柱调用多个
  `DrawRectangle`。
- 相邻墙柱如果材质、亮度和高度接近，应合并批量处理。
- 亮度优先量化为 8 或 16 档，避免每个像素使用任意乘法系数。
- 可预生成正面、侧面、远处和阴影材质，直接采样 RGB565。
- 门、窗等特殊墙体保持单独语义，但仍应批量输出顶部、窗洞、窗台和墙脚。
- 批量光栅前后必须做像素对比，特别检查画面四角、近墙、负 top、底部裁剪和窗口。

### 精灵、武器和特效

- 素材构建阶段裁剪透明边界，设备端不扫描 alpha bounds。
- 不透明、binary alpha、smooth alpha 分开走快路径。
- 常用缩放尺寸可以预生成 variant，减少运行时缩放与采样。
- 敌人奔跑、瞄准、开火、受击和倒地使用明确帧；不使用平移单张图片代替动画。
- 武器和 HUD 保持原生 480×480 清晰度，即使世界区域使用较低质量档位。
- 枪口闪光、命中标记和震动使用短生命周期，避免大面积多层 alpha 混合。
- 粒子使用固定容量对象池，并限制同屏数量、尺寸和 alpha 层数。

### HUD 和触摸控件

- 静态 HUD 背景缓存，只在状态变化时重绘文字、血量和弹药。
- 触摸图标优先使用 binary alpha atlas；透明角必须在构建后测试为 alpha 0。
- 避免用多个重叠半透明圆绘制摇杆和按钮。
- 调试统计默认隐藏，不与正式 HUD 一起占用每帧预算。
- 文本内容和布局不变时缓存字形结果或整块 HUD surface。

### 游戏区域和脏矩形

- 第一人称转向会改变整个世界区域，脏矩形不能显著减少 3D 世界渲染。
- HUD、菜单、塔防、横版地图和静止背景适合脏矩形。
- 3D 世界建议整块更新，HUD 与控制层局部更新。
- 不要为 240 根墙柱生成 240 个 LCD 更新区域；过多 transaction 会抵消收益。

## 通用渲染 SDK 技巧

### 快路径分层

底层至少区分：

```text
opaque 1:1 copy
opaque scale
binary-alpha copy
binary-alpha scale
smooth-alpha blend
solid fill
horizontal span
raycast wall batch
```

调用方在资源加载时确定 alpha 类型，热路径不得重复扫描像素判断是否透明。

### 连续访问优先

- framebuffer 是行优先布局，优先按行或小 tile 连续写。
- 逐列计算的射线结果应先存入 column descriptor，再按行/tile 输出。
- tile 建议从 8×8 或 16×8 开始测量，避免每行遍历全部无关列。
- 尽量让每次底层调用输出至少 8 个连续 RGB565 像素。
- PSRAM 上避免稀疏竖写、读改写和重复覆盖同一像素。

### 减少调用和分支

- 合并相邻同色矩形、墙脚、地板 span 和精灵可见区间。
- 裁剪、材质选择和亮度选择放在外层，内层循环只做采样和写入。
- 禁止在像素内层调用通用绘制 API。
- 常用宽度 2、4、8 提供专门 fill；长区间走批量实现。
- 使用增量采样器替代逐像素除法和 `%`；2 的幂纹理可用 mask wrap。

### RGB565 与亮度

- 白色 tint 直接复制，不拆 RGB565。
- 固定亮度使用查表或预烘焙材质。
- 动态亮度量化后再 shade，避免产生大量细微但肉眼无意义的颜色变化。
- alpha=0 跳过，alpha=255 直接写，只有中间 alpha 进入 blend。
- 禁止为了方便将 binary alpha 素材送入 smooth-alpha 路径。

### S31 PIE SIMD

S31 与 P4 同类 PIE 能力适合 128-bit 批量处理。一次可处理 8 个 RGB565 像素。
提供带标量回退的内部接口：

```c
void mosaico_fill_rgb565(uint16_t *dst, uint16_t color, size_t count);
void mosaico_copy_rgb565(uint16_t *dst, const uint16_t *src, size_t count);
void mosaico_shade_rgb565(uint16_t *dst, const uint16_t *src,
                          size_t count, unsigned light256);
```

实现要求：

- `#if SOC_CPU_HAS_PIE` 选择 PIE 汇编，其他目标使用标量 C。
- 16 字节对齐时走最快路径，处理未对齐头部和不足 8 像素的尾部。
- 使用 128-bit load/store、broadcast 和硬件零开销循环。
- 先用于 fill、copy 和长 span；短 2 像素墙柱不能有效利用 8 个 SIMD lane。
- SIMD 与标量版本必须逐像素一致，并在设备上用 cycle/time benchmark 验证。
- 先消除除法和小 draw call，再接 SIMD，不能用 SIMD 掩盖错误的数据布局。

### API 边界

- 游戏语义留在项目中，例如天空、敌人和 HUD。
- SDK 提供通用的 fill、copy、span、column、sprite 和 raycast batch。
- 通用统计结构不应出现 `enemy_us`、`wall_us` 等特定游戏字段；项目可保存具名阶段，
  SDK 只提供通用 phase slot 或计时工具。
- 未使用的公开接口及时删除，避免为一次实验永久扩展 API。

## 端侧 Surface、GSP 和 LCD 技巧

### framebuffer

- 保留可随机访问的完整 RGB565 framebuffer。射线、depth test、精灵和 HUD 都需要它。
- CPU 直接绘制到最终提交 buffer，禁止每帧再复制一张完整 framebuffer。
- 多 buffer 用于让 CPU 与 LCD 并行；buffer 只能在 GSP release callback 后复用。
- 屏幕截图引用最近完成的帧，按需复制；不能在每帧维持第三张截图副本。
- framebuffer 放 PSRAM，频繁使用的 column descriptor、depth、查表和 tile scratch
  优先放内部 RAM。

### 提交和防撕裂

- 使用 `esp_gsp_canvas_try_push()` 非阻塞提交完整帧。
- `acquire`、`submit`、`release` 和 `in_flight` 分开统计。
- 只要渲染超过 33 ms，LCD/TE 不是首要瓶颈；先优化 CPU 光栅。
- 不因性能问题直接取消防撕裂。普通 GRAM 模式无法解决 50 ms 的 CPU 渲染。
- 当渲染低于 25 ms 后，再比较 TE、buffer 数、LCD 时钟和队列策略。
- `busy` 或 `superseded` 表示生产和消费节奏不匹配，不能只看平均 FPS。

### 内存和 cache

- 热循环代码和常用只读表按平台能力放入快速可执行/只读区域。
- 小型中间 tile、深度数组和 SIMD 对齐缓冲放内部 RAM。
- 大 atlas 和完整 framebuffer 放 PSRAM，访问应连续并尽量只读/只写一次。
- 一帧内缓存 atlas frame descriptor，禁止对每根射线重复查表。
- 不在热路径申请、释放或扩容容器。

### 任务和日志

- 游戏任务保持稳定优先级，音频、触摸和 Iris 不应长时间阻塞渲染。
- 性能日志按固定帧间隔输出，禁止逐帧打印。
- 音频混音使用固定块和专用任务；音效触发只写轻量队列。
- 震动、NVS、截图和远程输入均不得在 render 内同步等待。

## 素材构建技巧

- PNG/WAV/JSON 在构建阶段转换，设备只读取 `.atlas`、`.sound` 和二进制地图。
- opaque tile 必须 full-bleed，不能被 sprite 的背景去除逻辑吃掉边缘。
- sprite 保留透明 padding；tile 不增加透明 padding。
- atlas 尺寸和 frame 边界必须测试，防止采样进入相邻 frame 或黑色空白区。
- 墙体 tile 的左右边缘应可平铺，高亮和接缝不能贯穿整列。
- 墙脚可以烘焙进材质；如果运行时绘制，应按连续区段批量输出。
- 为资源输出生成确定性 hash，同一源文件重复构建必须字节一致。
- 对透明角、亮边、底部材质、动画 frame ID 和 atlas 越界编写构建测试。

## 仿真与设备闭环

### Host 验证

统一命令：

```bash
python3 mosaico.py game sim projects/<game>
```

Host 检查：

- 固定场景截图和像素 hash。
- 快速转向、近墙、窗口、墙脚、地图边界和透明 HUD。
- 输入 replay、state hash、通关和失败流程。
- 标量与优化路径逐像素比较。
- AddressSanitizer/UndefinedBehaviorSanitizer 可用时检查越界和整数问题。

Host 的毫秒数不能代表 S31 性能，只用于比较算法和发现画面回归。

### 设备验证

固件只通过：

```bash
python3 mosaico.py install --project projects/<game>
python3 mosaico.py monitor --grep '<game-tag>'
```

每次记录：

- commit/submodule SHA、firmware SHA、Device ID 和 Boot ID。
- logic FPS、display FPS、render、acquire、submit、release。
- dropped、busy、superseded、errors、overflow 和 in-flight。
- 世界、地板、墙体、精灵、HUD 等项目阶段耗时。
- 初始场景、快速转向、近墙、最多敌人、开火特效和结算画面。
- 同场景 Host 与设备截图。

## 性能预算

30 FPS 的总周期约为 33.3 ms。建议设备侧预算：

| 阶段 | 建议预算 |
|---|---:|
| update 与输入 | 1 ms |
| 射线与天空 | 3 ms |
| 地板 | 6 ms |
| 墙体 | 10 ms |
| 精灵与特效 | 4 ms |
| 武器和 HUD | 5 ms |
| acquire/submit 与余量 | 4 ms |

预算是复杂场景上限，不是初始静止画面的结果。任一阶段超预算，先减少计算、分支、
调用和非连续访问，再考虑降低画质。

## 优化顺序

按以下顺序推进，每一步单独测量并保留前后截图：

1. 修复越界、错误 alpha、墙地接缝和材质边缘。
2. 移除热路径分配、日志、查表、三角函数、除法和取模。
3. 增加裁剪、遮挡和连续区间合并。
4. 将竖向稀疏写改为按行或 tile 连续写。
5. 缓存静态 HUD、frame descriptor、亮度材质和常用缩放 variant。
6. 用 PIE SIMD 优化长 fill、copy 和 RGB565 span。
7. 调整 framebuffer 数、GSP/TE 和 LCD 参数。
8. 最后才考虑减少射线、两行共用采样或降低世界分辨率。

## 禁止的优化方式

- 只在简单初始画面测 FPS，不测动态复杂场景。
- 通过删除敌人、特效、反馈或材质细节获得性能数字。
- 没有截图对比就合入批量光栅实现。
- 在像素循环里做除法、浮点三角函数、动态分配或通用 API 调用。
- 每个墙柱、地板格或粒子独立提交 LCD transaction。
- 直接关闭防撕裂来掩盖 CPU 渲染不足。
- 设备和 Host 维护两套游戏 view。
- 在通用 SDK 暴露单个游戏专属的统计和行为接口。

## 提交门槛

渲染优化进入主分支前必须满足：

- Host 单元测试和资源测试通过。
- 固定截图无黑块、白线越界、atlas 泄漏和透明背景回归。
- 动态转向时墙体边缘稳定，地板没有明显隔行闪烁。
- 设备复杂场景达到性能目标，且日志中没有 display error 或 queue overflow。
- 子模块实现先独立提交，父仓库再更新 submodule SHA。
- 文档记录画质取舍、实测数据和可回退的标量路径。

