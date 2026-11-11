idf_component_register(SRCS "main.c"
    PRIV_REQUIRES app_display app_hub app_minihub app_panel bt esp_iris
        cjson esp_flash_dispatcher esp_mosaico_app_recovery esp_mosaico_gsp_bundle
        esp_mosaico_gsp_iris nvs_flash log)

set(MINIHUB_STORAGE_DIR "${CMAKE_BINARY_DIR}/minihub_storage")
set(MINIHUB_WEB_SRC "${MINIHUB_REPO_ROOT}/components/minihub_web/web")
idf_build_get_property(MINIHUB_BUILD_PYTHON PYTHON)
file(REMOVE_RECURSE "${MINIHUB_STORAGE_DIR}")
file(MAKE_DIRECTORY "${MINIHUB_STORAGE_DIR}")
file(MAKE_DIRECTORY "${MINIHUB_STORAGE_DIR}/config")
file(COPY
    "${MINIHUB_REPO_ROOT}/application/minihub/configs/adapters.json"
    "${MINIHUB_REPO_ROOT}/configs/rules.json"
    "${MINIHUB_REPO_ROOT}/configs/scenes.json"
    "${MINIHUB_REPO_ROOT}/components/minihub_device_widget/model/devices.csv"
    DESTINATION "${MINIHUB_STORAGE_DIR}/config")
file(GLOB_RECURSE MINIHUB_WEB_FILES CONFIGURE_DEPENDS
    "${MINIHUB_WEB_SRC}/*")

add_custom_command(
    OUTPUT "${MINIHUB_STORAGE_DIR}/web/.stamp"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${MINIHUB_STORAGE_DIR}/web"
    COMMAND "${MINIHUB_BUILD_PYTHON}"
        "${MINIHUB_REPO_ROOT}/tools/minify_web_assets.py"
        --src "${MINIHUB_WEB_SRC}"
        --dst "${MINIHUB_STORAGE_DIR}/web"
        --gzip
        -q
    COMMAND "${CMAKE_COMMAND}" -E touch
        "${MINIHUB_STORAGE_DIR}/web/.stamp"
    DEPENDS ${MINIHUB_WEB_FILES}
        "${MINIHUB_REPO_ROOT}/tools/minify_web_assets.py"
    COMMENT "Preparing MiniHub Web console"
    VERBATIM)

littlefs_create_partition_image(storage "${MINIHUB_STORAGE_DIR}"
    FLASH_IN_PROJECT
    DEPENDS "${MINIHUB_STORAGE_DIR}/web/.stamp")

# Do not put storage.bin in system-update. MiniHub persists commissioned
# devices, adapters, scenes and rules on this LittleFS partition; rewriting
# the factory image on every UI/app resource update wipes them.
# Web console is seeded from the application image; see app_minihub.
