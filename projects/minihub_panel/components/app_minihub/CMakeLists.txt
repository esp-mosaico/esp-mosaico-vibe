set(_minihub_panel_requires
        adapter_ble_mesh
        adapter_matter
        adapter_modbus
        adapter_mqtt
        minihub_adapter
        minihub_auth
        minihub_config
        minihub_core
        minihub_hub
        minihub_js
        minihub_module
        minihub_matter
        minihub_net
        minihub_pack
        minihub_provision
        minihub_rules
        minihub_scenes
        minihub_web
        json
        nvs_flash
        esp_system
)

if("$ENV{MINIHUB_PANEL_VARIANT}" STREQUAL "thread")
    list(APPEND _minihub_panel_requires adapter_thread)
else()
    list(APPEND _minihub_panel_requires adapter_zigbee)
endif()

idf_component_register(
    SRCS "app_minihub.c" "adapters_setup.c" "web_seed.c"
    INCLUDE_DIRS "."
    REQUIRES ${_minihub_panel_requires}
)

# Embed factory /data/web so a blank LittleFS still gets a console without
# flashing storage.bin (which would wipe adapters.json).
idf_build_get_property(_minihub_python PYTHON)
set(_minihub_web_dir "${CMAKE_BINARY_DIR}/minihub_storage/web")
set(_minihub_web_seed "${CMAKE_BINARY_DIR}/web_seed.bin")
add_custom_command(
    OUTPUT "${_minihub_web_seed}"
    COMMAND "${_minihub_python}"
        "${CMAKE_CURRENT_LIST_DIR}/../../tools/pack_web_seed.py"
        --src "${_minihub_web_dir}"
        --dst "${_minihub_web_seed}"
    DEPENDS "${_minihub_web_dir}/.stamp"
        "${CMAKE_CURRENT_LIST_DIR}/../../tools/pack_web_seed.py"
    COMMENT "Packing MiniHub Web seed"
    VERBATIM)
target_add_binary_data(${COMPONENT_LIB} "${_minihub_web_seed}" BINARY)
