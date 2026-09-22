# 工作区布局迁移

[English](workspace-migration.md) | [返回索引](README_CN.md)

本次直接切换公共接口，不保留旧路径兼容层，也不自动修改已有用户应用。

| 旧位置 | 新归属 |
| --- | --- |
| projects/hello_world | utils/mosaico-tools/templates/hello_world |
| projects/sky_hop、tower_defense、raylib_shooter | BSP/examples/ 下对应游戏 |
| components/esp_mosaico_app_recovery | utils/esp-mosaico-recovery/components/ |
| cmake/mosaico_application.cmake、mosaico_idf_project.cmake | utils/esp-mosaico-recovery/cmake/ |
| cmake/system_update.cmake、raylib_lite_engine.cmake | utils/mosaico-tools/cmake/ |
| tools/gsp-sim、GSP 分区打包、System Update 准备 | utils/mosaico-tools/tools/ |
| Hello World 资源加载与镜像实现 | utils/mosaico-tools/components/ 下可选组件 |
| Recovery 与崩溃测试固件 | utils/esp-mosaico-recovery/tests/firmware/ |

已有应用可在新工作区创建一个临时参考工程，对照其 CMake 和组件清单，
接入新的公共路径后保留自己的业务代码与分区布局。GSP 应用调用
`mosaico_gsp_add_ui_bundle`，引入所需的公共组件，不再复制 bundle loader 或镜像实现。
应用主 CMake 显式声明 utils/BSP/引擎位置，以工程目录为基准使用相对路径。

工作区保证整体移动和克隆后的创建、构建与仿真，不提供单应用独立导出。
在新位置重新构建，不复用绑定旧绝对路径的 CMake 缓存。Recovery ABI、
固定分区前缀、资源格式和设备操作方式保持原有契约。
