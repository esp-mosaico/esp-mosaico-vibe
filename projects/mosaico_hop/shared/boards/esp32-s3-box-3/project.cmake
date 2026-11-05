# Application-side Board Manager sdkconfig composition. The generated defaults
# enable the peripherals/devices described by the official selected board pack.
# The project selector has already placed the Board's own defaults first.
set(_rle_box3_bmgr_defaults
    "${CMAKE_SOURCE_DIR}/components/gen_bmgr_codes/board_manager.defaults")
if(EXISTS "${_rle_box3_bmgr_defaults}")
    list(APPEND SDKCONFIG_DEFAULTS "${_rle_box3_bmgr_defaults}")
else()
    message(STATUS
        "ESP32-S3-BOX-3: run 'idf.py bmgr -c <boards-dir> -b esp32_s3_box_3 -a <amend-dir>' before building")
endif()
