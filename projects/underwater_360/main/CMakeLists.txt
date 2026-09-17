idf_component_register(
    SRCS "main.c" "underwater_app.c" "underwater_world.c" "underwater_view.c" "raylib_screen_mirror.c"
    INCLUDE_DIRS "."
    REQUIRES mosaico_game_app mosaico_game_assets mosaico_game_2d esp_driver_jpeg
             mosaico_raylib_fast mosaico_raylib_port raylib esp-gsp)

set(GAME_ASSET_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../assets_src")
set(GAME_ASSET_OUTPUT "${CMAKE_CURRENT_LIST_DIR}/../assets/generated")
add_custom_target(underwater_assets ALL
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../../submodule/raylib-lite-engine/tools/pack_game_assets.py"
            --source "${GAME_ASSET_SOURCE}" --output "${GAME_ASSET_OUTPUT}" --limit 10485760
    BYPRODUCTS "${GAME_ASSET_OUTPUT}/panorama.atlas" "${GAME_ASSET_OUTPUT}/reef_fish.atlas"
               "${GAME_ASSET_OUTPUT}/marine_creatures.atlas" "${GAME_ASSET_OUTPUT}/aurora.atlas"
               "${GAME_ASSET_OUTPUT}/sunrise.atlas" "${GAME_ASSET_OUTPUT}/reindeer.atlas"
               "${GAME_ASSET_OUTPUT}/rainforest.atlas"
    DEPENDS "${GAME_ASSET_SOURCE}/game_assets.json"
            "${GAME_ASSET_SOURCE}/panorama.json"
            "${GAME_ASSET_SOURCE}/reef_panorama_clean_1600.png"
            "${GAME_ASSET_SOURCE}/reef_fish.json"
            "${GAME_ASSET_SOURCE}/reef_fish.png"
            "${GAME_ASSET_SOURCE}/marine_creatures.json"
            "${GAME_ASSET_SOURCE}/marine_creatures.png"
            "${GAME_ASSET_SOURCE}/aurora.json" "${GAME_ASSET_SOURCE}/aurora_scene.png"
            "${GAME_ASSET_SOURCE}/sunrise.json" "${GAME_ASSET_SOURCE}/sunrise_scene.png"
            "${GAME_ASSET_SOURCE}/reindeer.json" "${GAME_ASSET_SOURCE}/reindeer.png"
            "${GAME_ASSET_SOURCE}/rainforest.json" "${GAME_ASSET_SOURCE}/rainforest_scene.png")
add_dependencies(${COMPONENT_LIB} underwater_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/reef_fish.atlas" BINARY
    RENAME_TO reef_fish_atlas DEPENDS underwater_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/marine_creatures.atlas" BINARY
    RENAME_TO marine_creatures_atlas DEPENDS underwater_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/reindeer.atlas" BINARY
    RENAME_TO reindeer_atlas DEPENDS underwater_assets)
foreach(background ocean aurora sunrise rainforest)
    target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_SOURCE}/${background}.jpg" BINARY
        RENAME_TO ${background}_jpg DEPENDS underwater_assets)
endforeach()
target_include_directories(${COMPONENT_LIB} PRIVATE "${GAME_ASSET_OUTPUT}")

set(canvas_png "${CMAKE_CURRENT_BINARY_DIR}/canvas_480.png")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/make_canvas_png.py" "${canvas_png}"
    RESULT_VARIABLE canvas_result)
if(NOT canvas_result EQUAL 0)
    message(FATAL_ERROR "Could not generate Raylib Canvas placeholder")
endif()
set(CANVAS_IMAGE "${canvas_png}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/scene.json.in"
               "${CMAKE_CURRENT_BINARY_DIR}/scene.json" @ONLY)
gsp_add_bundle(${COMPONENT_LIB}
    SCENES "${CMAKE_CURRENT_BINARY_DIR}/scene.json"
    PIXEL_FORMAT rgb565)
