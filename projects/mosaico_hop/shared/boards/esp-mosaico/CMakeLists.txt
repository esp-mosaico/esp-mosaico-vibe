idf_component_register(
    SRCS "board.c" "mosaico_video.c" "mosaico_audio_codec.c"
        "mosaico_hardware.c" "mosaico_setup_device.c" "mosaico_setup_imu.c"
    PRIV_INCLUDE_DIRS "."
    REQUIRES examples_common examples_audio raylib-lite-engine gen_bmgr_codes
        esp_display_present esp_board_manager esp_lcd esp_lcd_co5300
        esp_lcd_touch_cst9220 bmi270 i2c_bus esp_codec_dev
        esp_system heap nvs_flash esp_timer esp_driver_dma
    PRIV_REQUIRES freertos efuse
    WHOLE_ARCHIVE)
