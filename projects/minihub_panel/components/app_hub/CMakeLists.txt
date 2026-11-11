idf_component_register(
    SRCS "app_hub.c"
    INCLUDE_DIRS "include"
    REQUIRES cjson esp_event esp_http_client esp_netif esp_websocket_client
        esp_wifi nvs_flash minihub_provision
)
