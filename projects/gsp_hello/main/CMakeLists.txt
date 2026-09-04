idf_component_register(
    SRCS
        "main.c"
        "board_display.c"
        "../../factory/main/iris_ota_support.c"
    INCLUDE_DIRS
        "."
        "../../factory/main"
    REQUIRES
        app_update
        esp_app_format
        esp_iris
        esp-mosaico-bsp
        esp_partition
        freertos
        nvs_flash
    PRIV_REQUIRES
        esp-gsp
        esp_lcd
)

gsp_add_bundle(${COMPONENT_LIB}
    SCENES "../ui/main.json"
    PIXEL_FORMAT rgb565)
