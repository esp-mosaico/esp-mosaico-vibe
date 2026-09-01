cmake_minimum_required(VERSION 3.16)

# Keep the generated sdkconfig in the build directory, matching the factory
# reference project and preventing profiles from contaminating each other.
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()

if(NOT SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS
        "sdkconfig.defaults;sdkconfig.application.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_world)
