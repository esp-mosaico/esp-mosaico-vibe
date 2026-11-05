# Application-side Board composition for repository examples.
# Engine code must not include or depend on this helper.

get_filename_component(_raylib_lite_boards_root
    "${CMAKE_CURRENT_LIST_DIR}/../../boards" ABSOLUTE)

# Board Manager IDs use underscores; Board/IDF component directories use hyphens.
set(_raylib_lite_bmgr_board "esp_mosaico")
set(_raylib_lite_bmgr_metadata
    "${CMAKE_SOURCE_DIR}/components/gen_bmgr_codes/gen_board_metadata.yaml")
if(EXISTS "${_raylib_lite_bmgr_metadata}")
    file(STRINGS "${_raylib_lite_bmgr_metadata}" _raylib_lite_bmgr_board_line
        REGEX "^board:[ \\t]*[^ \\t]+" LIMIT_COUNT 1)
    if(_raylib_lite_bmgr_board_line)
        string(REGEX REPLACE "^board:[ \\t]*" ""
            _raylib_lite_bmgr_board "${_raylib_lite_bmgr_board_line}")
    endif()
endif()

string(REPLACE "_" "-" _raylib_lite_selected_board
    "${_raylib_lite_bmgr_board}")
set(_raylib_lite_board_package_dir
    "${_raylib_lite_boards_root}/${_raylib_lite_selected_board}")
set(_raylib_lite_board_dir "${_raylib_lite_board_package_dir}")
if(NOT EXISTS "${_raylib_lite_board_dir}/CMakeLists.txt" OR
        NOT EXISTS "${_raylib_lite_board_dir}/idf_component.yml")
    message(FATAL_ERROR
        "Board Manager selected '${_raylib_lite_bmgr_board}', but its Raylib Lite "
        "adapter '${_raylib_lite_selected_board}' was not found under "
        "${_raylib_lite_board_package_dir}")
endif()

set(RAYLIB_LITE_BOARD "${_raylib_lite_selected_board}" CACHE STRING
    "Raylib Lite example Board component" FORCE)
set(RAYLIB_LITE_BOARD_DIR "${_raylib_lite_board_dir}" CACHE PATH
    "Raylib Lite example Board component directory" FORCE)
set(RAYLIB_LITE_BOARD_PACKAGE_DIR "${_raylib_lite_board_package_dir}" CACHE PATH
    "Raylib Lite example Board package directory" FORCE)
message(STATUS "Raylib Lite Board adapter: ${RAYLIB_LITE_BOARD}")

list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../examples_audio"
    "${RAYLIB_LITE_BOARD_DIR}")

set(_raylib_lite_board_defaults
    "${RAYLIB_LITE_BOARD_PACKAGE_DIR}/sdkconfig.defaults")
if(EXISTS "${_raylib_lite_board_defaults}")
    list(PREPEND SDKCONFIG_DEFAULTS "${_raylib_lite_board_defaults}")
endif()

if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig" CACHE FILEPATH "Per-build config")
endif()

set(CONFIGDEP_ENABLE OFF CACHE BOOL "Use IDF configdep" FORCE)

set(_raylib_lite_board_project
    "${RAYLIB_LITE_BOARD_PACKAGE_DIR}/project.cmake")
if(EXISTS "${_raylib_lite_board_project}")
    include("${_raylib_lite_board_project}")
endif()
