idf_component_register(
    SRCS
        "game_audio.c"
        "platform_esp_audio.c"
        "platform_audio_write_all.c"
    INCLUDE_DIRS "."
    REQUIRES raylib-lite-engine
    PRIV_REQUIRES freertos)
