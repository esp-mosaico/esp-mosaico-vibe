# Build the local, unsigned ESP-Iris System Update bundle used by
# `python mosaico.py system-update --project projects/<application>`.
# The application supplies its partition layout and binary; this module wires
# the shared validation and staging step into the ESP-IDF build graph.

set(system_update_preparer
    "${CMAKE_CURRENT_LIST_DIR}/../tools/prepare_system_update.py")
set(system_update_partition_csv "${PROJECT_SOURCE_DIR}/partitions.csv")
set(system_update_iris_root
    "${CMAKE_CURRENT_LIST_DIR}/../submodule/esp-mosaico-tools/submodule/esp-iris")
set(system_update_iris_tool
    "${system_update_iris_root}/components/esp_iris/tools/esp_iris.py")
set(system_update_stage_dir "${CMAKE_BINARY_DIR}/system-update")
set(system_update_bundle
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-system-update.irisfw")

# The ESP-Iris bundle builder imports Gateway runtime dependencies (for
# example zeroconf), so mosaico.py passes its prepared host Python explicitly.
if(DEFINED ESP_IRIS_PYTHON AND EXISTS "${ESP_IRIS_PYTHON}")
    set(system_update_python "${ESP_IRIS_PYTHON}")
elseif(DEFINED ENV{ESP_IRIS_PYTHON} AND EXISTS "$ENV{ESP_IRIS_PYTHON}")
    set(system_update_python "$ENV{ESP_IRIS_PYTHON}")
else()
    set(system_update_python "")
endif()

if(system_update_python)
    add_custom_target(system-update-bundle
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${system_update_stage_dir}"
        COMMAND "${system_update_python}" "${system_update_preparer}"
            --partition-csv "${system_update_partition_csv}"
            --partition-table
                "${CMAKE_BINARY_DIR}/partition_table/partition-table.bin"
            --application "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin"
            --bootloader
                "${CMAKE_BINARY_DIR}/bootloader/bootloader.bin"
            --stage-dir "${system_update_stage_dir}"
            --release "${PROJECT_VERSION}"
        COMMAND "${system_update_python}" "${system_update_iris_tool}" bundle build
            "${system_update_stage_dir}/manifest.json"
            --component-root "${system_update_stage_dir}"
            --output "${system_update_bundle}"
        DEPENDS "${system_update_preparer}" "${system_update_partition_csv}"
                "${system_update_iris_tool}" app bootloader partition_table_bin
        BYPRODUCTS "${system_update_bundle}"
        COMMENT "Building application + bootloader + partition-table System Update bundle"
        VERBATIM)
else()
    add_custom_target(system-update-bundle
        COMMAND "${CMAKE_COMMAND}" -E echo
            "ESP-Iris host environment unavailable; use mosaico.py system-update"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM)
endif()

message(STATUS "ESP-Iris System Update bundle: ${system_update_bundle}")
