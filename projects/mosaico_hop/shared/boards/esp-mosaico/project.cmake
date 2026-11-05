# ESP Board Manager generates these defaults next to the selected application's
# CMakeLists.txt when `idf.py bmgr` creates components/gen_bmgr_codes.
set(_mosaico_bmgr_profile_dir "${CMAKE_CURRENT_LIST_DIR}/bmgr")
set(_mosaico_board_manager_defaults
    "${CMAKE_SOURCE_DIR}/components/gen_bmgr_codes/board_manager.defaults")
if(EXISTS "${_mosaico_board_manager_defaults}")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/components/gen_bmgr_codes/gen_board_device_custom.h")
        message(FATAL_ERROR
            "ESP-Mosaico requires the Raylib Lite custom Board Manager profile. "
            "Regenerate with: idf.py bmgr -c ${_mosaico_bmgr_profile_dir} -b esp_mosaico")
    endif()
    list(APPEND SDKCONFIG_DEFAULTS "${_mosaico_board_manager_defaults}")
else()
    message(STATUS
        "ESP-Mosaico: run 'idf.py bmgr -c ${_mosaico_bmgr_profile_dir} -b esp_mosaico' before building")
endif()
