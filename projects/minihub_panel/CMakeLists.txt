cmake_minimum_required(VERSION 3.16)
set(MOSAICO_UTILS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../submodule/esp-mosaico-utils")
set(MOSAICO_BSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../submodule/esp-mosaico-bsp")
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()
if(NOT SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.application.defaults")
endif()

set(MINIHUB_PANEL_VARIANT "zigbee" CACHE STRING
    "MiniHub radio variant: zigbee or thread")
set_property(CACHE MINIHUB_PANEL_VARIANT PROPERTY STRINGS zigbee thread)
if(NOT MINIHUB_PANEL_VARIANT STREQUAL "zigbee" AND
   NOT MINIHUB_PANEL_VARIANT STREQUAL "thread")
    message(FATAL_ERROR
        "MINIHUB_PANEL_VARIANT must be 'zigbee' or 'thread', got '${MINIHUB_PANEL_VARIANT}'")
endif()
# ESP-IDF expands component requirements in a child CMake process that does
# not inherit arbitrary cache variables. Export the selected variant so
# component CMake files resolve the same radio graph during regeneration.
set(ENV{MINIHUB_PANEL_VARIANT} "${MINIHUB_PANEL_VARIANT}")
list(APPEND SDKCONFIG_DEFAULTS
    "sdkconfig.variant.${MINIHUB_PANEL_VARIANT}.defaults")

# MiniHub Host components only. Do not import cmake/minihub_bundle — that
# yml pulls Thread/RainMaker and pins mdns 1.13.1 against Iris 1.12.0.
get_filename_component(MINIHUB_REPO_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../esp-minihub" ABSOLUTE)
if(NOT DEFINED ENV{ESP_MATTER_PATH})
    get_filename_component(_LOCAL_ESP_MATTER
        "${CMAKE_CURRENT_LIST_DIR}/../../../esp-matter" ABSOLUTE)
    if(EXISTS "${_LOCAL_ESP_MATTER}/components")
        set(ENV{ESP_MATTER_PATH} "${_LOCAL_ESP_MATTER}")
    endif()
endif()
if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "ESP_MATTER_PATH is required for MiniHub Matter support")
endif()
set(ESP_MATTER_PATH "$ENV{ESP_MATTER_PATH}")
set(MATTER_SDK_PATH "${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip")
list(APPEND EXTRA_COMPONENT_DIRS
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components")

list(APPEND EXTRA_COMPONENT_DIRS
    "${MINIHUB_REPO_ROOT}/components/json"
    "${MINIHUB_REPO_ROOT}/components/minihub_adapter"
    "${MINIHUB_REPO_ROOT}/components/minihub_auth"
    "${MINIHUB_REPO_ROOT}/components/minihub_ble_gatt"
    "${MINIHUB_REPO_ROOT}/components/minihub_ble_mesh"
    "${MINIHUB_REPO_ROOT}/components/minihub_config"
    "${MINIHUB_REPO_ROOT}/components/minihub_core"
    "${MINIHUB_REPO_ROOT}/components/minihub_device_widget"
    "${MINIHUB_REPO_ROOT}/components/minihub_hub"
    "${MINIHUB_REPO_ROOT}/components/minihub_js"
    "${MINIHUB_REPO_ROOT}/components/minihub_lcd"
    "${MINIHUB_REPO_ROOT}/components/minihub_matter"
    "${MINIHUB_REPO_ROOT}/components/minihub_module"
    "${MINIHUB_REPO_ROOT}/components/minihub_net"
    "${MINIHUB_REPO_ROOT}/components/minihub_ota"
    "${MINIHUB_REPO_ROOT}/components/minihub_pack"
    "${MINIHUB_REPO_ROOT}/components/minihub_product_params"
    "${MINIHUB_REPO_ROOT}/components/minihub_provision"
    "${MINIHUB_REPO_ROOT}/components/minihub_rainmaker"
    "${MINIHUB_REPO_ROOT}/components/minihub_rules"
    "${MINIHUB_REPO_ROOT}/components/minihub_scenes"
    "${MINIHUB_REPO_ROOT}/components/minihub_store"
    "${MINIHUB_REPO_ROOT}/components/minihub_update"
    "${MINIHUB_REPO_ROOT}/components/minihub_web"
    "${MINIHUB_REPO_ROOT}/components/minihub_protocol_adapters/adapter_ble_mesh"
    "${MINIHUB_REPO_ROOT}/components/minihub_protocol_adapters/adapter_matter"
    "${MINIHUB_REPO_ROOT}/components/minihub_protocol_adapters/adapter_modbus"
    "${MINIHUB_REPO_ROOT}/components/minihub_protocol_adapters/adapter_mqtt"
    "${MINIHUB_REPO_ROOT}/components/minihub_protocol_adapters/adapter_wifi")

if(MINIHUB_PANEL_VARIANT STREQUAL "thread")
    list(APPEND EXTRA_COMPONENT_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/variant_components/thread/minihub_thread"
        "${MINIHUB_REPO_ROOT}/components/minihub_protocol_adapters/adapter_thread"
        "${CMAKE_CURRENT_LIST_DIR}/variant_components/thread/minihub_zigbee")
else()
    # The real Thread component unconditionally resolves OpenThread BR
    # dependencies. Keep the lightweight API-compatible stub out of the
    # application's ordinary components/ tree so the Thread variant can select
    # the real component without a duplicate-name collision.
    list(APPEND EXTRA_COMPONENT_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/variant_components/zigbee/minihub_thread"
        "${MINIHUB_REPO_ROOT}/components/minihub_zigbee"
        "${MINIHUB_REPO_ROOT}/components/minihub_protocol_adapters/adapter_zigbee")
endif()

include("${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/gsp_compiler.cmake")
list(APPEND EXTRA_COMPONENT_DIRS
    "${MOSAICO_UTILS_ROOT}/esp-mosaico-vibe-mode/components/esp_mosaico_app_recovery"
    "${MOSAICO_UTILS_ROOT}/mosaico-tools/components/esp_mosaico_gsp_bundle"
    "${MOSAICO_UTILS_ROOT}/mosaico-tools/components/esp_mosaico_gsp_iris")
include("${MOSAICO_UTILS_ROOT}/esp-mosaico-vibe-mode/cmake/mosaico_idf_project.cmake")
idf_build_set_property(COMPILE_OPTIONS
    "-Wno-error=stringop-truncation;-Wno-error=format-truncation;-Wno-error=format;-Wno-error=unused-result" APPEND)
project(minihub_panel_${MINIHUB_PANEL_VARIANT} VERSION 1.0.0)
include("${MINIHUB_REPO_ROOT}/cmake/matter_no_rtti.cmake")
if(CONFIG_MINIHUB_MATTER_ADAPTER AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
    foreach(_comp esp_matter_controller esp_matter esp_matter_ota_provider esp_matter_console minihub_matter)
        idf_build_get_property(_build_components BUILD_COMPONENTS)
        if(${_comp} IN_LIST _build_components)
            idf_component_get_property(_lib ${_comp} COMPONENT_LIB)
            get_target_property(_lib_type ${_lib} TYPE)
            if(NOT _lib_type STREQUAL "INTERFACE_LIBRARY")
                target_compile_options(${_lib} PRIVATE
                    $<$<COMPILE_LANGUAGE:CXX>:-fpermissive;-Wno-error>)
            endif()
        endif()
    endforeach()
endif()
include("${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/system_update.cmake")
