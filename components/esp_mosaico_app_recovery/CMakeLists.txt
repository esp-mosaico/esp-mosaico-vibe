idf_component_register(
    SRCS "iris_ota_support.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../submodule/esp-mosaico-utils/esp-mosaico-recovery/include"
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
