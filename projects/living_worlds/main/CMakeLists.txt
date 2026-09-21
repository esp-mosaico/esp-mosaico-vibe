idf_component_register(
    SRCS "main.c" "living_worlds_app.c" "living_worlds_world.c" "living_worlds_aurora.c"
         "living_worlds_ocean.c" "living_worlds_volume.c" "living_worlds_view.c"
         "raylib_screen_mirror.c"
    INCLUDE_DIRS "."
    REQUIRES mosaico_game_app mosaico_game_assets mosaico_game_2d esp_driver_jpeg
             mosaico_raylib_fast mosaico_raylib_port raylib esp-gsp)

set(GAME_ASSET_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../assets_src")
set(GAME_ASSET_OUTPUT "${CMAKE_CURRENT_LIST_DIR}/../assets/generated")
set(VOLUME_ATLASES
    sunrise_cliff_front sunrise_cliff_side sunrise_cliff_rear
    aurora_ice_front aurora_ice_side aurora_ice_rear
    ocean_reef_left_front ocean_reef_left_side ocean_reef_left_rear
    ocean_reef_right_front ocean_reef_right_side ocean_reef_right_rear)
set(BACKGROUND_ATLASES aurora ocean sunrise rainforest)

set(ASSET_BYPRODUCTS)
set(ASSET_DEPENDS
    "${GAME_ASSET_SOURCE}/game_assets.json"
    "${GAME_ASSET_SOURCE}/aurora.json" "${GAME_ASSET_SOURCE}/aurora.jpg"
    "${GAME_ASSET_SOURCE}/ocean.json" "${GAME_ASSET_SOURCE}/ocean.jpg"
    "${GAME_ASSET_SOURCE}/sunrise.json" "${GAME_ASSET_SOURCE}/sunrise.jpg"
    "${GAME_ASSET_SOURCE}/rainforest.json" "${GAME_ASSET_SOURCE}/rainforest.jpg")
foreach(name IN LISTS BACKGROUND_ATLASES)
    list(APPEND ASSET_BYPRODUCTS "${GAME_ASSET_OUTPUT}/${name}.atlas")
endforeach()
foreach(volume IN LISTS VOLUME_ATLASES)
    list(APPEND ASSET_BYPRODUCTS "${GAME_ASSET_OUTPUT}/${volume}.atlas")
    list(APPEND ASSET_DEPENDS
         "${GAME_ASSET_SOURCE}/${volume}.json"
         "${GAME_ASSET_SOURCE}/${volume}.png")
endforeach()

add_custom_target(living_worlds_assets ALL
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../../submodule/raylib-lite-engine/tools/pack_game_assets.py"
            --source "${GAME_ASSET_SOURCE}" --output "${GAME_ASSET_OUTPUT}" --limit 10485760
    BYPRODUCTS ${ASSET_BYPRODUCTS}
    DEPENDS ${ASSET_DEPENDS})
add_dependencies(${COMPONENT_LIB} living_worlds_assets)
foreach(volume IN LISTS VOLUME_ATLASES)
    target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/${volume}.atlas" BINARY
        RENAME_TO ${volume}_atlas DEPENDS living_worlds_assets)
endforeach()
foreach(background IN LISTS BACKGROUND_ATLASES)
    target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_SOURCE}/${background}.jpg" BINARY
        RENAME_TO ${background}_jpg DEPENDS living_worlds_assets)
endforeach()
target_include_directories(${COMPONENT_LIB} PRIVATE "${GAME_ASSET_OUTPUT}")
set_source_files_properties(
    living_worlds_aurora.c living_worlds_ocean.c living_worlds_view.c living_worlds_volume.c
    PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")

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
