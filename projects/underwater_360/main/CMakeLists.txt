idf_component_register(
    SRCS "main.c" "underwater_app.c" "underwater_world.c" "underwater_aurora.c"
         "underwater_ocean.c" "underwater_volume.c" "underwater_view.c"
         "raylib_screen_mirror.c"
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
               "${GAME_ASSET_OUTPUT}/ocean.atlas"
               "${GAME_ASSET_OUTPUT}/sunrise.atlas" "${GAME_ASSET_OUTPUT}/reindeer.atlas"
               "${GAME_ASSET_OUTPUT}/sunrise_cliff_front.atlas"
               "${GAME_ASSET_OUTPUT}/sunrise_cliff_side.atlas"
               "${GAME_ASSET_OUTPUT}/sunrise_cliff_rear.atlas"
               "${GAME_ASSET_OUTPUT}/aurora_ice_front.atlas"
               "${GAME_ASSET_OUTPUT}/aurora_ice_side.atlas"
               "${GAME_ASSET_OUTPUT}/aurora_ice_rear.atlas"
               "${GAME_ASSET_OUTPUT}/ocean_reef_left_front.atlas"
               "${GAME_ASSET_OUTPUT}/ocean_reef_left_side.atlas"
               "${GAME_ASSET_OUTPUT}/ocean_reef_left_rear.atlas"
               "${GAME_ASSET_OUTPUT}/ocean_reef_right_front.atlas"
               "${GAME_ASSET_OUTPUT}/ocean_reef_right_side.atlas"
               "${GAME_ASSET_OUTPUT}/ocean_reef_right_rear.atlas"
               "${GAME_ASSET_OUTPUT}/rainforest.atlas"
    DEPENDS "${GAME_ASSET_SOURCE}/game_assets.json"
            "${GAME_ASSET_SOURCE}/panorama.json"
            "${GAME_ASSET_SOURCE}/reef_panorama_clean_1600.png"
            "${GAME_ASSET_SOURCE}/reef_fish.json"
            "${GAME_ASSET_SOURCE}/reef_fish.png"
            "${GAME_ASSET_SOURCE}/marine_creatures.json"
            "${GAME_ASSET_SOURCE}/marine_creatures.png"
            "${GAME_ASSET_SOURCE}/aurora.json" "${GAME_ASSET_SOURCE}/aurora_scene.png"
            "${GAME_ASSET_SOURCE}/ocean.json" "${GAME_ASSET_SOURCE}/ocean_scene.png"
            "${GAME_ASSET_SOURCE}/sunrise.json" "${GAME_ASSET_SOURCE}/sunrise_scene.png"
            "${GAME_ASSET_SOURCE}/sunrise_cliff_front.json" "${GAME_ASSET_SOURCE}/sunrise_cliff_front.png"
            "${GAME_ASSET_SOURCE}/sunrise_cliff_side.json" "${GAME_ASSET_SOURCE}/sunrise_cliff_side.png"
            "${GAME_ASSET_SOURCE}/sunrise_cliff_rear.json" "${GAME_ASSET_SOURCE}/sunrise_cliff_rear.png"
            "${GAME_ASSET_SOURCE}/aurora_ice_front.json" "${GAME_ASSET_SOURCE}/aurora_ice_front.png"
            "${GAME_ASSET_SOURCE}/aurora_ice_side.json" "${GAME_ASSET_SOURCE}/aurora_ice_side.png"
            "${GAME_ASSET_SOURCE}/aurora_ice_rear.json" "${GAME_ASSET_SOURCE}/aurora_ice_rear.png"
            "${GAME_ASSET_SOURCE}/ocean_reef_left_front.json" "${GAME_ASSET_SOURCE}/ocean_reef_left_front.png"
            "${GAME_ASSET_SOURCE}/ocean_reef_left_side.json" "${GAME_ASSET_SOURCE}/ocean_reef_left_side.png"
            "${GAME_ASSET_SOURCE}/ocean_reef_left_rear.json" "${GAME_ASSET_SOURCE}/ocean_reef_left_rear.png"
            "${GAME_ASSET_SOURCE}/ocean_reef_right_front.json" "${GAME_ASSET_SOURCE}/ocean_reef_right_front.png"
            "${GAME_ASSET_SOURCE}/ocean_reef_right_side.json" "${GAME_ASSET_SOURCE}/ocean_reef_right_side.png"
            "${GAME_ASSET_SOURCE}/ocean_reef_right_rear.json" "${GAME_ASSET_SOURCE}/ocean_reef_right_rear.png"
            "${GAME_ASSET_SOURCE}/reindeer.json" "${GAME_ASSET_SOURCE}/reindeer.png"
            "${GAME_ASSET_SOURCE}/rainforest.json" "${GAME_ASSET_SOURCE}/rainforest_scene.png")
add_dependencies(${COMPONENT_LIB} underwater_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/reef_fish.atlas" BINARY
    RENAME_TO reef_fish_atlas DEPENDS underwater_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/marine_creatures.atlas" BINARY
    RENAME_TO marine_creatures_atlas DEPENDS underwater_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/reindeer.atlas" BINARY
    RENAME_TO reindeer_atlas DEPENDS underwater_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/sunrise_cliff_front.atlas" BINARY
    RENAME_TO sunrise_cliff_front_atlas DEPENDS underwater_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/sunrise_cliff_side.atlas" BINARY
    RENAME_TO sunrise_cliff_side_atlas DEPENDS underwater_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/sunrise_cliff_rear.atlas" BINARY
    RENAME_TO sunrise_cliff_rear_atlas DEPENDS underwater_assets)
foreach(volume aurora_ice_front aurora_ice_side aurora_ice_rear
               ocean_reef_left_front ocean_reef_left_side ocean_reef_left_rear
               ocean_reef_right_front ocean_reef_right_side ocean_reef_right_rear)
    target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/${volume}.atlas" BINARY
        RENAME_TO ${volume}_atlas DEPENDS underwater_assets)
endforeach()
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
