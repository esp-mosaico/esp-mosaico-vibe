# Raylib 兼容 API

`mosaico_raylib_fast` 将常用 Raylib 2D API 直接写入 480×480 RGB565 framebuffer。
Host 仿真和设备固件编译同一份实现，因此颜色混合、裁剪、几何光栅化和文字像素一致。

当前支持：

- 窗口与时间：`InitWindow`、`CloseWindow`、`WindowShouldClose`、尺寸查询、
  `SetTargetFPS`、`GetFrameTime`、`GetTime`、`GetFPS`；
- 帧与相机：`BeginDrawing/EndDrawing`、`BeginMode2D/EndMode2D`、世界/屏幕转换、
  `BeginScissorMode/EndScissorMode`；
- 图元：pixel、line/line strip/dashed、circle/ellipse、rectangle/gradient/rounded、
  triangle/fan/strip、poly，以及对应的 outline/thickness 变体；
- 纹理：`LoadTexture`、`UnloadTexture`、`DrawTexture`、`V`、`Rec`、`Ex`、`Pro`；
  2.5D 内核另有 `Mosaico2DDrawColumn`、`Mosaico2DDrawSpan`、`Mosaico2DDrawFloorRow`，
  在 RGB565 内做竖条/扫描线采样和距离光照，不走 Wasm；
- 文本：`DrawText`、`MeasureText`、`TextFormat`；
- 碰撞与颜色：矩形、圆、点、三角形查询，`GetCollisionRec`、`Fade`、
  `ColorAlpha`、`ColorTint`、`ColorBrightness`；
- 输入：key pressed/down/released/up、主触点 mouse 视图、双触点位置/ID/count；
  `MosaicoFastGetImuAcceleration()` 返回平台注入的三轴值。

平台输入桥通过 `MosaicoFastInjectAction/Key/Pointer/Imu` 更新这些查询状态。Host
模块已经统一注入；设备的触摸或 IMU 任务也应在写入游戏事件队列时调用对应入口。
`GetTime()` 使用已提交帧数和目标 FPS 计算，保证 headless 回放确定，不读取主机墙钟。

当前不支持 Raylib 的 3D、OpenGL、shader、render texture、模型、通用图片解码和
自定义字体加载。设备音频使用 `mosaico_game_audio`，资源由构建期 Atlas/声音管线生成。
未列出的 Raylib 调用会在链接阶段失败，避免意外带入另一套渲染后端。
