include(ExternalProject)

idf_build_get_property(recovery_idf_path IDF_PATH)
idf_build_get_property(recovery_idf_target IDF_TARGET)
idf_build_get_property(recovery_python PYTHON)

if(NOT CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY)
    message(FATAL_ERROR
        "Normal firmware must retain the Gateway-controlled recovery path")
endif()
if(CONFIG_ESP_IRIS_OTA)
    message(FATAL_ERROR
        "The ESP-Iris OTA writer belongs only in the Recovery firmware")
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
set(recovery_source_build_dir "${CMAKE_BINARY_DIR}/recovery-from-source")
set(recovery_source_description "${recovery_source_build_dir}/project_description.json")
set(recovery_source_bootloader "${recovery_source_build_dir}/bootloader/bootloader.bin")
set(recovery_source_partition_table
    "${recovery_source_build_dir}/partition_table/partition-table.bin")
set(recovery_source_ota_data "${recovery_source_build_dir}/ota_data_initial.bin")
set(recovery_source_application "${recovery_source_build_dir}/factory.bin")

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

ExternalProject_Add(recovery-from-source
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.."
    BINARY_DIR "${recovery_source_build_dir}"
    CMAKE_ARGS
        "-DIDF_PATH=${recovery_idf_path}"
        "-DIDF_TARGET=${recovery_idf_target}"
        "-DBUILD_PROFILE=recovery"
        "-DSDKCONFIG=${recovery_source_build_dir}/sdkconfig"
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS
        "${recovery_source_bootloader}"
        "${recovery_source_partition_table}"
        "${recovery_source_ota_data}"
        "${recovery_source_application}"
        "${recovery_source_description}"
    BUILD_ALWAYS TRUE
    EXCLUDE_FROM_ALL TRUE)

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
add_custom_target(recovery-image ALL DEPENDS ${recovery_output_files})

add_custom_target(recovery-current-bundle
    COMMAND "${recovery_python}" "${recovery_tool}"
        ${recovery_source_inputs}
        ${recovery_layout_args}
        --output-dir "${recovery_current_output_dir}"
    BYPRODUCTS ${recovery_current_output_files}
    DEPENDS recovery-from-source "${recovery_tool}"
    COMMENT "Building an unreviewed Recovery candidate bundle"
    VERBATIM)

add_custom_target(update-recovery-prebuilt
    COMMAND "${recovery_python}" "${recovery_tool}"
        ${recovery_source_inputs}
        ${recovery_layout_args}
        --output-dir "${RECOVERY_PREBUILT_DIR}"
    DEPENDS recovery-from-source "${recovery_tool}"
    COMMENT "Atomically publishing the complete reviewed Recovery bundle"
    VERBATIM)

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

# Prepare and verify every build artifact before a device maintenance lease is
# acquired. The flash target below depends on the same target and therefore
# performs no long-running compilation while the USB endpoint is detached.
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
