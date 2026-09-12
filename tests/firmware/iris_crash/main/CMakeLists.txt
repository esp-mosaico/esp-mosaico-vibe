idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_iris esp_mosaico_app_recovery esp_system freertos nvs_flash
)
