idf_build_get_property(recovery_idf_target IDF_TARGET)
idf_build_get_property(recovery_python PYTHON)

if(NOT CONFIG_FACTORY_RECOVERY_FIRMWARE)
    message(FATAL_ERROR "projects/factory must build the retained Recovery firmware")
endif()
if(NOT CONFIG_ESP_IRIS_OTA)
    message(FATAL_ERROR "The retained Recovery firmware requires the ESP-Iris OTA writer")
endif()

partition_table_get_partition_info(
    recovery_partition_offset "--partition-name factory" "offset")
partition_table_get_partition_info(
    recovery_partition_size "--partition-name factory" "size")
partition_table_get_partition_info(
    normal_partition_offset "--partition-name ota_0" "offset")
partition_table_get_partition_info(
    ota_data_partition_offset "--partition-name otadata" "offset")

if(NOT recovery_partition_offset OR NOT recovery_partition_size)
    message(FATAL_ERROR "The partition table must contain the Recovery slot")
endif()
if(NOT normal_partition_offset OR NOT ota_data_partition_offset)
    message(FATAL_ERROR "The partition table does not support the retained Recovery workflow")
endif()

set(recovery_tool "${CMAKE_CURRENT_LIST_DIR}/../tools/prepare_recovery.py")
set(recovery_output_dir "${CMAKE_BINARY_DIR}/recovery")
set(recovery_current_output_dir "${CMAKE_BINARY_DIR}/recovery-current")
set(recovery_source_description "${CMAKE_BINARY_DIR}/project_description.json")
set(recovery_source_bootloader "${CMAKE_BINARY_DIR}/bootloader/bootloader.bin")
set(recovery_source_partition_table
    "${CMAKE_BINARY_DIR}/partition_table/partition-table.bin")
set(recovery_source_ota_data "${CMAKE_BINARY_DIR}/ota_data_initial.bin")
set(recovery_source_application "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin")

set(recovery_bundle_files
    bootloader.bin partition-table.bin ota_data_initial.bin factory.bin manifest.json)

if(CONFIG_SECURE_BOOT)
    set(recovery_secure_boot 1)
else()
    set(recovery_secure_boot 0)
endif()
if(CONFIG_SECURE_FLASH_ENC_ENABLED)
    set(recovery_flash_encryption 1)
else()
    set(recovery_flash_encryption 0)
endif()

set(recovery_layout_args
    --target "${recovery_idf_target}"
    --bootloader-offset "${CONFIG_BOOTLOADER_OFFSET_IN_FLASH}"
    --partition-table-offset "${CONFIG_PARTITION_TABLE_OFFSET}"
    --ota-data-offset "${ota_data_partition_offset}"
    --recovery-offset "${recovery_partition_offset}"
    --recovery-size "${recovery_partition_size}"
    --normal-offset "${normal_partition_offset}"
    --secure-boot "${recovery_secure_boot}"
    --flash-encryption "${recovery_flash_encryption}")

set(recovery_prebuilt_inputs
    --bootloader "${RECOVERY_PREBUILT_DIR}/bootloader.bin"
    --partition-table "${RECOVERY_PREBUILT_DIR}/partition-table.bin"
    --ota-data "${RECOVERY_PREBUILT_DIR}/ota_data_initial.bin"
    --recovery "${RECOVERY_PREBUILT_DIR}/factory.bin"
    --manifest "${RECOVERY_PREBUILT_DIR}/manifest.json")

set(recovery_source_inputs
    --bootloader "${recovery_source_bootloader}"
    --partition-table "${recovery_source_partition_table}"
    --ota-data "${recovery_source_ota_data}"
    --recovery "${recovery_source_application}"
    --project-description "${recovery_source_description}"
    --source-root "${CMAKE_CURRENT_LIST_DIR}/..")

set(recovery_prebuilt_dependencies "${recovery_tool}")
foreach(bundle_file IN LISTS recovery_bundle_files)
    list(APPEND recovery_prebuilt_dependencies "${RECOVERY_PREBUILT_DIR}/${bundle_file}")
endforeach()

set(recovery_output_files "")
set(recovery_current_output_files "")
foreach(bundle_file IN LISTS recovery_bundle_files)
    list(APPEND recovery_output_files "${recovery_output_dir}/${bundle_file}")
    list(APPEND recovery_current_output_files "${recovery_current_output_dir}/${bundle_file}")
endforeach()

add_custom_command(
    OUTPUT ${recovery_output_files}
    COMMAND "${recovery_python}" "${recovery_tool}"
        ${recovery_prebuilt_inputs}
        ${recovery_layout_args}
        --output-dir "${recovery_output_dir}"
    DEPENDS ${recovery_prebuilt_dependencies}
    COMMENT "Validating the reviewed Recovery bundle"
    VERBATIM)
add_custom_target(recovery-image DEPENDS ${recovery_output_files})

add_custom_target(recovery-current-bundle
    COMMAND "${recovery_python}" "${recovery_tool}"
        ${recovery_source_inputs}
        ${recovery_layout_args}
        --output-dir "${recovery_current_output_dir}"
    BYPRODUCTS ${recovery_current_output_files}
    DEPENDS "${recovery_tool}"
    COMMENT "Packaging an unreviewed Recovery candidate from the current build"
    VERBATIM)
add_dependencies(recovery-current-bundle app bootloader partition_table_bin blank_ota_data)

add_custom_target(update-recovery-prebuilt
    COMMAND "${recovery_python}" "${recovery_tool}"
        ${recovery_source_inputs}
        ${recovery_layout_args}
        --output-dir "${RECOVERY_PREBUILT_DIR}"
    DEPENDS "${recovery_tool}"
    COMMENT "Atomically publishing the complete reviewed Recovery bundle"
    VERBATIM)
add_dependencies(update-recovery-prebuilt app bootloader partition_table_bin blank_ota_data)

set(MOSAICO_RECOVERY_SOURCE "reviewed" CACHE STRING
    "Internal source used by mosaico-recover-flash: reviewed or current")
set_property(CACHE MOSAICO_RECOVERY_SOURCE PROPERTY STRINGS reviewed current)
if(MOSAICO_RECOVERY_SOURCE STREQUAL "reviewed")
    set(mosaico_recovery_bundle_dir "${recovery_output_dir}")
    set(mosaico_recovery_bundle_target recovery-image)
elseif(MOSAICO_RECOVERY_SOURCE STREQUAL "current")
    set(mosaico_recovery_bundle_dir "${recovery_current_output_dir}")
    set(mosaico_recovery_bundle_target recovery-current-bundle)
    message(WARNING "mosaico-recover-flash will use an unreviewed current-source bundle")
else()
    message(FATAL_ERROR "MOSAICO_RECOVERY_SOURCE must be reviewed or current")
endif()

# Prepare and verify every artifact before a device maintenance lease is
# acquired. The flash target depends on the same bundle and does not compile
# while the USB endpoint is detached.
add_custom_target(mosaico-recover-prepare
    DEPENDS ${mosaico_recovery_bundle_target})

# This is the only low-level write target used by mosaico.py. ESP-IDF owns the
# flasher arguments; the Python product CLI never assembles an esptool command.
esptool_py_custom_target(
    mosaico-recover-flash mosaico_recover ${mosaico_recovery_bundle_target})
esptool_py_flash_target_image(
    mosaico-recover-flash recovery_bootloader
    "${CONFIG_BOOTLOADER_OFFSET_IN_FLASH}"
    "${mosaico_recovery_bundle_dir}/bootloader.bin")
esptool_py_flash_target_image(
    mosaico-recover-flash recovery_partition_table
    "${CONFIG_PARTITION_TABLE_OFFSET}"
    "${mosaico_recovery_bundle_dir}/partition-table.bin")
esptool_py_flash_to_partition(
    mosaico-recover-flash otadata
    "${mosaico_recovery_bundle_dir}/ota_data_initial.bin")
esptool_py_flash_to_partition(
    mosaico-recover-flash factory
    "${mosaico_recovery_bundle_dir}/factory.bin")

message(STATUS
    "Recovery bundle source for mosaico-recover-flash: ${MOSAICO_RECOVERY_SOURCE}")
