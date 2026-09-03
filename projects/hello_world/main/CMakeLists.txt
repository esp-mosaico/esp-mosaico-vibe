idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp-mosaico-bsp
        esp_mosaico_app_recovery
        freertos
        lvgl__lvgl
        nvs_flash
)
