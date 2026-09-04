idf_component_register(
    SRCS "iris_ota_support.c"
    INCLUDE_DIRS "include"
    REQUIRES
        app_update
        esp_app_format
        esp_iris
        esp_partition
        freertos
        mbedtls
        nvs_flash
        spi_flash
)
