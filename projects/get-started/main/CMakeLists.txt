idf_component_register(
    SRCS "main.c" "iris_ota_support.c" "iris_screen_mirror.c"
    INCLUDE_DIRS "."
    REQUIRES app_update esp_app_format esp_iris esp-mosaico-bsp esp_partition freertos lvgl__lvgl nvs_flash
)
