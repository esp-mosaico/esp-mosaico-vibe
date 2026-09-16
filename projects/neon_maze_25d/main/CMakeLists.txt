idf_component_register(
    SRCS "main.c" "neon_maze_app.c" "neon_maze_game.c" "neon_maze_view.c"
    INCLUDE_DIRS "."
    REQUIRES mosaico_game_app mosaico_game_assets mosaico_game_2d
             mosaico_raylib_fast mosaico_raylib_port mosaico_game_audio
             esp-mosaico-bsp raylib esp-gsp)

set(GAME_ASSET_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../assets_src")
set(GAME_ASSET_OUTPUT "${CMAKE_CURRENT_LIST_DIR}/../assets/generated")
add_custom_target(neon_maze_assets ALL
    COMMAND "${PYTHON}" "${GAME_ASSET_SOURCE}/prepare_sprites.py"
    COMMAND "${PYTHON}" "${GAME_ASSET_SOURCE}/generate_audio.py"
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../../submodule/raylib-lite-engine/tools/pack_game_assets.py"
            --source "${GAME_ASSET_SOURCE}" --output "${GAME_ASSET_OUTPUT}"
    BYPRODUCTS "${GAME_ASSET_OUTPUT}/enemy.atlas"
               "${GAME_ASSET_OUTPUT}/weapon.atlas"
               "${GAME_ASSET_OUTPUT}/controls.atlas"
               "${GAME_ASSET_OUTPUT}/environment.atlas"
               "${GAME_ASSET_OUTPUT}/materials.atlas"
               "${GAME_ASSET_OUTPUT}/rifle.sound"
               "${GAME_ASSET_OUTPUT}/impact.sound"
               "${GAME_ASSET_OUTPUT}/confirm.sound"
               "${GAME_ASSET_OUTPUT}/hurt.sound"
    DEPENDS "${GAME_ASSET_SOURCE}/game_assets.json"
            "${GAME_ASSET_SOURCE}/enemy.json"
            "${GAME_ASSET_SOURCE}/weapon.json"
            "${GAME_ASSET_SOURCE}/controls.json"
            "${GAME_ASSET_SOURCE}/environment.json"
            "${GAME_ASSET_SOURCE}/materials.json"
            "${GAME_ASSET_SOURCE}/enemy_run_strip_source.png"
            "${GAME_ASSET_SOURCE}/k98_rifle_source.png"
            "${GAME_ASSET_SOURCE}/tactical_panorama_source.png"
            "${GAME_ASSET_SOURCE}/tactical_materials_source.png"
            "${GAME_ASSET_SOURCE}/prepare_sprites.py"
            "${GAME_ASSET_SOURCE}/generate_audio.py")
add_dependencies(${COMPONENT_LIB} neon_maze_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/enemy.atlas" BINARY
    RENAME_TO enemy_atlas DEPENDS neon_maze_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/weapon.atlas" BINARY
    RENAME_TO weapon_atlas DEPENDS neon_maze_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/controls.atlas" BINARY
    RENAME_TO controls_atlas DEPENDS neon_maze_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/environment.atlas" BINARY
    RENAME_TO environment_atlas DEPENDS neon_maze_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/materials.atlas" BINARY
    RENAME_TO materials_atlas DEPENDS neon_maze_assets)
foreach(sound rifle impact confirm hurt)
    target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/${sound}.sound" BINARY
        RENAME_TO "neon_${sound}" DEPENDS neon_maze_assets)
endforeach()
target_include_directories(${COMPONENT_LIB} PRIVATE "${GAME_ASSET_OUTPUT}")

set(canvas_png "${CMAKE_CURRENT_BINARY_DIR}/canvas_480.png")
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/make_canvas_png.py"
    "${canvas_png}" RESULT_VARIABLE canvas_result)
if(NOT canvas_result EQUAL 0)
    message(FATAL_ERROR "Could not generate Neon Maze Canvas placeholder")
endif()
set(CANVAS_IMAGE "${canvas_png}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/scene.json.in"
               "${CMAKE_CURRENT_BINARY_DIR}/scene.json" @ONLY)
gsp_add_bundle(${COMPONENT_LIB}
    SCENES "${CMAKE_CURRENT_BINARY_DIR}/scene.json"
    PIXEL_FORMAT rgb565)
