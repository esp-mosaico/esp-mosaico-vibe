cmake_minimum_required(VERSION 3.16)

# Keep every firmware profile's generated sdkconfig inside its own build
# directory. This prevents a recovery build from contaminating a normal build.
set(BUILD_PROFILE "application" CACHE STRING
    "Firmware profile: application, candidate, or recovery")
set_property(CACHE BUILD_PROFILE PROPERTY STRINGS application candidate recovery)

if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()

if(NOT SDKCONFIG_DEFAULTS)
    if(BUILD_PROFILE STREQUAL "application")
        set(SDKCONFIG_DEFAULTS
            "sdkconfig.defaults;sdkconfig.application.defaults")
    elseif(BUILD_PROFILE STREQUAL "candidate")
        set(SDKCONFIG_DEFAULTS
            "sdkconfig.defaults;sdkconfig.application.defaults;sdkconfig.candidate.defaults")
    elseif(BUILD_PROFILE STREQUAL "recovery")
        set(SDKCONFIG_DEFAULTS
            "sdkconfig.defaults;sdkconfig.recovery.defaults")
    else()
        message(FATAL_ERROR
            "Unknown BUILD_PROFILE='${BUILD_PROFILE}'. Use application, candidate, or recovery.")
    endif()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(iris_get_started)

