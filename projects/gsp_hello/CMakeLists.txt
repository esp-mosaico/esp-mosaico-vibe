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

# ESP-GSP packs scenes with a standalone gspc, not the old Python SDK.
if(NOT GSPC_EXECUTABLE AND DEFINED ENV{GSPC_EXECUTABLE}
        AND NOT "$ENV{GSPC_EXECUTABLE}" STREQUAL "")
    set(GSPC_EXECUTABLE "$ENV{GSPC_EXECUTABLE}")
endif()
if(NOT GSPC_EXECUTABLE)
    find_program(MOSAICO_GSPC_PYTHON NAMES python3 python REQUIRED)
    execute_process(
        COMMAND "${MOSAICO_GSPC_PYTHON}"
            "${CMAKE_CURRENT_LIST_DIR}/../../tools/gsp-sim/fetch_gspc.py"
        OUTPUT_VARIABLE GSPC_EXECUTABLE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE MOSAICO_GSPC_RESULT)
    if(NOT MOSAICO_GSPC_RESULT EQUAL 0 OR GSPC_EXECUTABLE STREQUAL "")
        message(FATAL_ERROR
            "Could not resolve GSPC_EXECUTABLE. Run "
            "python3 tools/gsp-sim/fetch_gspc.py or set GSPC_EXECUTABLE.")
    endif()
endif()
set(GSPC_EXECUTABLE "${GSPC_EXECUTABLE}" CACHE FILEPATH
    "Standalone gspc compatible with the pinned ESP-GSP submodule")
set(ENV{GSPC_EXECUTABLE} "${GSPC_EXECUTABLE}")

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../components/esp_mosaico_app_recovery")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gsp_hello VERSION 1.0.0)

include(../../cmake/system_update.cmake)
