idf_component_register(
    SRCS
        "main.c"
        "hello_ui.c"
        "board_display.c"
        "iris_screen_mirror.c"
        "ui_bundle.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_iris
        esp-mosaico-bsp
        esp_mosaico_app_recovery
        freertos
        nvs_flash
    PRIV_REQUIRES
        esp-gsp
        esp_display_present
        esp_lcd
        esp_partition
        mbedtls
)

# Observe the renderer's little-endian RGB565 regions before the display
# presenter converts them to the panel's wire byte order.
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=esp_display_presenter_submit_buffer")

set(gsp_bundle_symbol bundle)
gsp_add_bundle(${COMPONENT_LIB}
    SCENES "../ui/main.json"
    PIXEL_FORMAT rgb565
    DEPLOYABLE
    SYMBOL ${gsp_bundle_symbol})

# ESP-GSP 1.1.0's helper always adds an embed assembly source. This project
# consumes the deployable GSPB from ui_apps instead, so keep the generated
# source as a build artifact but do not compile it into the application.
set(gsp_bundle_dir
    "${CMAKE_CURRENT_BINARY_DIR}/gsp_gen_${gsp_bundle_symbol}")
set(gsp_bundle_path
    "${gsp_bundle_dir}/${gsp_bundle_symbol}.gspb")
set(gsp_embed_source
    "${gsp_bundle_dir}/${gsp_bundle_symbol}_embed.S")
if(NOT EXISTS "${gsp_embed_source}")
    message(FATAL_ERROR
        "ESP-GSP did not generate the expected embed source: "
        "${gsp_embed_source}")
endif()
set_source_files_properties("${gsp_embed_source}"
    PROPERTIES HEADER_FILE_ONLY TRUE)

partition_table_get_partition_info(ui_apps_size
    "--partition-name ui_apps" "size")
if(NOT ui_apps_size)
    message(FATAL_ERROR "The ui_apps partition is missing from partitions.csv")
endif()

idf_build_get_property(build_python PYTHON)
set(ui_apps_image "${CMAKE_BINARY_DIR}/ui_apps.bin")
set(ui_apps_packer "${PROJECT_DIR}/../../tools/pack_gsp_partition.py")
add_custom_command(
    OUTPUT "${ui_apps_image}"
    COMMAND "${build_python}" "${ui_apps_packer}"
        --bundle "${gsp_bundle_path}"
        --output "${ui_apps_image}"
        --max-size "${ui_apps_size}"
    DEPENDS "${gsp_bundle_path}" "${ui_apps_packer}"
    COMMENT "Packing deployable GSPB into ui_apps.bin"
    VERBATIM)
add_custom_target(gsp-ui-apps-image ALL DEPENDS "${ui_apps_image}")

set_property(GLOBAL PROPERTY MOSAICO_SYSTEM_UPDATE_UI_APPS_IMAGE
    "${ui_apps_image}")
set_property(GLOBAL PROPERTY MOSAICO_SYSTEM_UPDATE_UI_APPS_TARGET
    gsp-ui-apps-image)
