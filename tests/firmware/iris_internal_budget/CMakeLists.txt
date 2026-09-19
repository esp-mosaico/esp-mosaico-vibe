cmake_minimum_required(VERSION 3.16)
set(mosaico_root "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
set(SDKCONFIG_DEFAULTS
    "${mosaico_root}/projects/gsp_hello/sdkconfig.defaults;${mosaico_root}/projects/gsp_hello/sdkconfig.application.defaults;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
set(EXTRA_COMPONENT_DIRS
    "${mosaico_root}/projects/gsp_hello/main"
    "${mosaico_root}/components/esp_mosaico_app_recovery")
set(MOSAICO_IRIS_BUDGET_FIXTURE ON)
if(NOT GSPC_EXECUTABLE)
    execute_process(COMMAND python3 "${mosaico_root}/tools/gsp-sim/fetch_gspc.py"
        OUTPUT_VARIABLE GSPC_EXECUTABLE OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE gspc_result)
    if(NOT gspc_result EQUAL 0)
        message(FATAL_ERROR "Could not resolve the pinned gspc")
    endif()
endif()
set(GSPC_EXECUTABLE "${GSPC_EXECUTABLE}" CACHE FILEPATH "Pinned gspc")
set(ENV{GSPC_EXECUTABLE} "${GSPC_EXECUTABLE}")
include(../../../cmake/mosaico_application.cmake)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(iris_internal_budget VERSION 1.0.0)
include("${mosaico_root}/cmake/system_update.cmake")
