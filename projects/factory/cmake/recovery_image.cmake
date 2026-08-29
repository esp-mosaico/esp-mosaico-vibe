include(ExternalProject)

idf_build_get_property(recovery_idf_path IDF_PATH)
idf_build_get_property(recovery_idf_target IDF_TARGET)
idf_build_get_property(recovery_python PYTHON)

# Normal firmware must always retain the Gateway-controlled path back to the
# factory recovery writer. Fail during configuration instead of producing an
# application image that cannot be safely updated.
if(NOT CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY)
    message(FATAL_ERROR
        "Normal application/candidate profiles must set "
        "CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY=y")
endif()

if(CONFIG_ESP_IRIS_OTA)
    message(FATAL_ERROR
        "Normal application/candidate profiles must not enable the ESP-Iris "
        "OTA writer; it belongs only in the factory recovery image")
endif()

partition_table_get_partition_info(
    recovery_partition_offset "--partition-name factory" "offset")
partition_table_get_partition_info(
    recovery_partition_size "--partition-name factory" "size")
partition_table_get_partition_info(
    normal_partition_offset "--partition-name ota_0" "offset")

if(NOT recovery_partition_offset OR NOT recovery_partition_size)
    message(FATAL_ERROR
        "The partition table must contain a factory partition for recovery")
endif()

if(NOT normal_partition_offset)
    message(FATAL_ERROR
        "The partition table must contain ota_0 for normal firmware")
endif()

set(recovery_tool "${CMAKE_CURRENT_LIST_DIR}/../tools/prepare_recovery.py")
set(recovery_prebuilt_image
    "${RECOVERY_PREBUILT_DIR}/factory.bin")
set(recovery_prebuilt_manifest
    "${RECOVERY_PREBUILT_DIR}/manifest.json")
set(recovery_output_dir "${CMAKE_BINARY_DIR}/recovery")
set(recovery_output_image "${recovery_output_dir}/factory.bin")
set(recovery_output_manifest "${recovery_output_dir}/manifest.json")
set(recovery_source_build_dir "${CMAKE_BINARY_DIR}/recovery-from-source")
set(recovery_source_image
    "${recovery_source_build_dir}/factory.bin")
set(recovery_source_description
    "${recovery_source_build_dir}/project_description.json")

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

# Keep the recovery compiler/configuration state completely separate from the
# application build. EXCLUDE_FROM_ALL ensures the sub-build runs only when the
# selected source is "build" or update-recovery-prebuilt is explicitly invoked.
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
        "${recovery_source_image}"
        "${recovery_source_description}"
    # Always enter the child build so source changes are noticed. Ninja still
    # keeps this incremental when the recovery inputs have not changed.
    BUILD_ALWAYS TRUE
    EXCLUDE_FROM_ALL TRUE)

set(recovery_common_args
    --target "${recovery_idf_target}"
    --partition-offset "${recovery_partition_offset}"
    --partition-size "${recovery_partition_size}"
    --secure-boot "${recovery_secure_boot}"
    --flash-encryption "${recovery_flash_encryption}"
    --output-image "${recovery_output_image}"
    --output-manifest "${recovery_output_manifest}")

if(CONFIG_GET_STARTED_RECOVERY_IMAGE_PREBUILT)
    set(recovery_image_source "prebuilt")
    if(NOT EXISTS "${recovery_prebuilt_image}")
        message(FATAL_ERROR
            "Prebuilt recovery image not found: ${recovery_prebuilt_image}. "
            "Select 'Build recovery from current source' in menuconfig or run "
            "update-recovery-prebuilt.")
    endif()
    if(NOT EXISTS "${recovery_prebuilt_manifest}")
        message(FATAL_ERROR
            "Prebuilt recovery manifest not found: ${recovery_prebuilt_manifest}. "
            "Select 'Build recovery from current source' in menuconfig or run "
            "update-recovery-prebuilt.")
    endif()

    add_custom_command(
        OUTPUT "${recovery_output_image}" "${recovery_output_manifest}"
        COMMAND "${recovery_python}" "${recovery_tool}"
            --image "${recovery_prebuilt_image}"
            --manifest "${recovery_prebuilt_manifest}"
            ${recovery_common_args}
        DEPENDS
            "${recovery_tool}"
            "${recovery_prebuilt_image}"
            "${recovery_prebuilt_manifest}"
        COMMENT "Validating checked-in recovery image"
        VERBATIM)
elseif(CONFIG_GET_STARTED_RECOVERY_IMAGE_BUILD)
    set(recovery_image_source "build")
    add_custom_command(
        OUTPUT "${recovery_output_image}" "${recovery_output_manifest}"
        COMMAND "${recovery_python}" "${recovery_tool}"
            --image "${recovery_source_image}"
            --project-description "${recovery_source_description}"
            --source-root "${CMAKE_CURRENT_LIST_DIR}/.."
            ${recovery_common_args}
        DEPENDS recovery-from-source "${recovery_tool}"
        COMMENT "Staging freshly built recovery image"
        VERBATIM)
else()
    message(FATAL_ERROR
        "No recovery image source selected. Run menuconfig and select one "
        "under 'Iris Factory OTA > Recovery image source'.")
endif()

add_custom_target(recovery-image ALL
    DEPENDS "${recovery_output_image}" "${recovery_output_manifest}")

# This target is intentionally explicit because it modifies a checked-in
# artifact. It is available even when normal builds use the prebuilt image.
add_custom_target(update-recovery-prebuilt
    COMMAND "${recovery_python}" "${recovery_tool}"
        --image "${recovery_source_image}"
        --project-description "${recovery_source_description}"
        --source-root "${CMAKE_CURRENT_LIST_DIR}/.."
        --target "${recovery_idf_target}"
        --partition-offset "${recovery_partition_offset}"
        --partition-size "${recovery_partition_size}"
        --secure-boot "${recovery_secure_boot}"
        --flash-encryption "${recovery_flash_encryption}"
        --output-image "${recovery_prebuilt_image}"
        --output-manifest "${recovery_prebuilt_manifest}"
    DEPENDS recovery-from-source "${recovery_tool}"
    COMMENT "Rebuilding and updating checked-in recovery firmware"
    VERBATIM)

message(STATUS
    "Recovery image source: ${recovery_image_source}; output: ${recovery_output_image}")
