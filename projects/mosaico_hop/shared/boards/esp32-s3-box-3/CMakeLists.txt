idf_component_register(
    SRCS "board.c" "box3_video.c" "box3_audio_codec.c"
    PRIV_INCLUDE_DIRS "."
    REQUIRES esp_display_present examples_common examples_audio raylib-lite-engine esp_board_manager esp_codec_dev esp_lcd esp_lcd_touch esp_timer heap
    PRIV_REQUIRES freertos
    # The shared audio worker calls a backend factory provided only by this
    # selected Board. Keep the provider object independent of archive order.
    WHOLE_ARCHIVE)
