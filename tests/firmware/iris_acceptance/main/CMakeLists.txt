set(acceptance_sources "main.c")
set(acceptance_requires esp_iris esp_mosaico_app_recovery freertos nvs_flash)

# Credentials remain outside the repository and are not copied into sdkconfig.
# Set the environment variable only for an explicitly enabled Wi-Fi test build.
if(DEFINED ENV{IRIS_ACCEPTANCE_WIFI_HEADER} AND
   NOT "$ENV{IRIS_ACCEPTANCE_WIFI_HEADER}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{IRIS_ACCEPTANCE_WIFI_HEADER}" acceptance_wifi_header)
    if(NOT IS_ABSOLUTE "${acceptance_wifi_header}" OR
       NOT EXISTS "${acceptance_wifi_header}")
        message(FATAL_ERROR "IRIS_ACCEPTANCE_WIFI_HEADER must name an existing absolute private header")
    endif()
    list(APPEND acceptance_sources "wifi.c")
    list(APPEND acceptance_requires esp_event esp_netif esp_wifi)
endif()

idf_component_register(SRCS ${acceptance_sources} INCLUDE_DIRS "."
                       REQUIRES ${acceptance_requires})

if(acceptance_wifi_header)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        IRIS_ACCEPTANCE_WIFI_ENABLED=1
        IRIS_ACCEPTANCE_WIFI_HEADER="${acceptance_wifi_header}")
endif()
