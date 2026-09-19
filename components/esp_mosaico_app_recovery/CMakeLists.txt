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

# Defaults do not override a previously generated sdkconfig. Fail here rather
# than producing firmware that can install once but cannot return to Recovery.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults" mosaico_contract
    REGEX "^CONFIG_.*=")
foreach(entry IN LISTS mosaico_contract)
    string(REGEX MATCH "^([^=]+)=(.*)$" unused "${entry}")
    set(key "${CMAKE_MATCH_1}")
    string(REPLACE "\"" "" expected "${CMAKE_MATCH_2}")
    if(expected STREQUAL "y")
        if(NOT ${key})
            message(FATAL_ERROR "Mosaico application requires ${key}=y; update sdkconfig and rebuild")
        endif()
    elseif(NOT "${${key}}" STREQUAL "${expected}")
        message(FATAL_ERROR "Mosaico application requires ${key}=${expected}; update sdkconfig and rebuild")
    endif()
endforeach()
if(CONFIG_ESP_IRIS_OTA)
    message(FATAL_ERROR "Mosaico application must disable CONFIG_ESP_IRIS_OTA; Recovery owns the writer")
endif()
