# Compile the official MiniHub Thread component directly while sharing the
# mdns version selected by ESP-Iris.
get_filename_component(MINIHUB_THREAD_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../../esp-minihub/components/minihub_thread"
    ABSOLUTE)

if(CONFIG_MINIHUB_THREAD_ADAPTER)
    set(SRCS "${MINIHUB_THREAD_ROOT}/thread_br.c")
else()
    set(SRCS "${MINIHUB_THREAD_ROOT}/thread_stub.c")
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "${MINIHUB_THREAD_ROOT}/include" "${MINIHUB_THREAD_ROOT}"
    REQUIRES
        minihub_config
        espressif__cjson
        nvs_flash
        openthread
        esp_netif
        esp_event
        esp_coex
        esp_wifi
        mdns
)
