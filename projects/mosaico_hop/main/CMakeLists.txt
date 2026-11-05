file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(
    SRCS ${example_sources}
    INCLUDE_DIRS "."
    REQUIRES examples_common ${RAYLIB_LITE_BOARD}
)
# Resolve the Engine only after IDF has registered/resolved dependencies.
if(TARGET idf::espressif2022__raylib-lite-engine)
    idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT
        espressif2022__raylib-lite-engine COMPONENT_DIR)
else()
    idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT
        raylib-lite-engine COMPONENT_DIR)
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_GAME_NATIVE=1)
include("${RAYLIB_LITE_ENGINE_ROOT}/tools/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/..")
