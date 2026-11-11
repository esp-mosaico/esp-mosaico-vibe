idf_component_register(SRCS "app_panel.c"
    INCLUDE_DIRS "include"
    REQUIRES app_hub esp-gsp nvs_flash)
include("${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/gsp_bundle.cmake")
mosaico_gsp_add_ui_bundle(${COMPONENT_LIB} "ui/main.json")
