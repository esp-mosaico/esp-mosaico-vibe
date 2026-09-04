cmake_minimum_required(VERSION 3.16)

# Keep the generated sdkconfig in the build directory.
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()

if(NOT SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS
        "sdkconfig.defaults;sdkconfig.application.defaults")
endif()

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../components/esp_mosaico_app_recovery")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_world VERSION 1.0.0)

include(../../cmake/system_update.cmake)
