cmake_minimum_required(VERSION 3.20)
project(minihub_panel_backend LANGUAGES C)

if(NOT ESP_GSP_COMPONENT_DIR AND DEFINED ENV{ESP_GSP_COMPONENT_DIR})
    set(ESP_GSP_COMPONENT_DIR "$ENV{ESP_GSP_COMPONENT_DIR}")
endif()
if(NOT ESP_GSP_COMPONENT_DIR)
    set(ESP_GSP_COMPONENT_DIR
        "${CMAKE_CURRENT_LIST_DIR}/../../../managed_components/espressif__esp-gsp")
    get_filename_component(ESP_GSP_COMPONENT_DIR "${ESP_GSP_COMPONENT_DIR}" ABSOLUTE)
endif()
if(NOT EXISTS "${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge/CMakeLists.txt")
    message(FATAL_ERROR "Install espressif/esp-gsp first")
endif()

option(MINIHUB_HOST_LIVE "Connect the simulator to a real MiniHub" OFF)
if(MINIHUB_HOST_LIVE)
    find_package(CURL REQUIRED)
    find_package(Threads REQUIRED)
    set(CJSON_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../managed_components/espressif__cjson")
    set(HUB_SOURCE host_hub.c "${CJSON_DIR}/cJSON/cJSON.c")
    set(HUB_INCLUDE "${CJSON_DIR}/cJSON")
else()
    set(HUB_SOURCE mock_hub.c)
endif()

add_subdirectory("${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge" sim_bridge)
gsp_add_backend(minihub_panel_backend
    SOURCES ../app_panel.c ${HUB_SOURCE} platform_pc.c
    INCLUDE_DIRECTORIES ../include ../../app_hub/include ${HUB_INCLUDE}
    SCENES ../ui/main.json)

if(MINIHUB_HOST_LIVE)
    target_link_libraries(minihub_panel_backend PRIVATE CURL::libcurl Threads::Threads)
endif()
