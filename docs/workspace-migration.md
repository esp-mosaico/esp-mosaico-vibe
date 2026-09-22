# Workspace layout migration

[简体中文](workspace-migration_CN.md) | [Documentation index](README.md)

This migration switches directly to the public interfaces. It provides no
compatibility layer for old workspace paths and does not automatically modify
existing user applications.

| Previous location | New owner/location |
| --- | --- |
| projects/hello_world | utils/mosaico-tools/templates/hello_world |
| projects/sky_hop, tower_defense, raylib_shooter | Corresponding games under BSP/examples/ |
| components/esp_mosaico_app_recovery | utils/esp-mosaico-recovery/components/ |
| cmake/mosaico_application.cmake, mosaico_idf_project.cmake | utils/esp-mosaico-recovery/cmake/ |
| cmake/system_update.cmake, raylib_lite_engine.cmake | utils/mosaico-tools/cmake/ |
| tools/gsp-sim, GSP partition packaging, System Update preparation | utils/mosaico-tools/tools/ |
| Hello World resource loading and mirroring implementation | Optional components under utils/mosaico-tools/components/ |
| Recovery and crash-test firmware | utils/esp-mosaico-recovery/tests/firmware/ |

For an existing application, create a temporary reference project in the new
workspace and compare its CMake files and component manifest. Adopt the public
paths while retaining your business logic and partition layout. GSP applications
call `mosaico_gsp_add_ui_bundle` and include the required public components instead
of copying the bundle loader or mirroring implementation. The application's top-level
CMake file explicitly declares the utils/BSP/engine locations using paths relative
to the project directory.

The workspace supports creation, building and simulation after the entire workspace
is moved or cloned. It does not provide standalone export of individual applications.
Rebuild in the new location; do not reuse CMake caches tied to old absolute paths.
The Recovery ABI, fixed partition prefix, resource formats and device-operation
workflow retain their existing contracts.
