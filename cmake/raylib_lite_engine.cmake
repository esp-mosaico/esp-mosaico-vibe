# Workspace adapter for the pinned Raylib Lite Engine submodule.
set(RAYLIB_LITE_ENGINE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../submodule/raylib-lite-engine"
    CACHE PATH "Raylib Lite Engine source root")

if(NOT EXISTS "${RAYLIB_LITE_ENGINE_ROOT}/cmake/mosaico_game_sdk.cmake")
    message(FATAL_ERROR
        "Raylib Lite Engine is unavailable. Run: git submodule update --init "
        "submodule/raylib-lite-engine")
endif()

set(MOSAICO_GAME_GSPC_FETCHER
    "${CMAKE_CURRENT_LIST_DIR}/../tools/gsp-sim/fetch_gspc.py"
    CACHE FILEPATH "Workspace GSP compiler resolver")
set(MOSAICO_GAME_RECOVERY_COMPONENT_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../components/esp_mosaico_app_recovery"
    CACHE PATH "Workspace application recovery component")

include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/mosaico_game_sdk.cmake")
