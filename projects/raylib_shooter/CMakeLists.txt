cmake_minimum_required(VERSION 3.16)

if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()

if(NOT SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS
        "sdkconfig.defaults;sdkconfig.application.defaults")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/raylib_lite_engine.cmake")
mosaico_game_sdk_configure_gsp_compiler()
mosaico_game_sdk_add_components(RAYLIB)
include(../../cmake/mosaico_application.cmake)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(raylib_shooter VERSION 1.0.0)

include(../../cmake/system_update.cmake)
