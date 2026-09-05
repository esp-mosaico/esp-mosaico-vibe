idf_component_register(
    SRCS
        "main.c"
        "board_display.c"
        "iris_screen_mirror.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_iris
        esp-mosaico-bsp
        esp_mosaico_app_recovery
        freertos
        nvs_flash
    PRIV_REQUIRES
        esp-gsp
        esp_display_present
        esp_lcd
)

# Observe the renderer's little-endian RGB565 regions before the display
# presenter converts them to the panel's wire byte order.
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=esp_display_presenter_submit_buffer")

gsp_add_bundle(${COMPONENT_LIB}
    SCENES "../ui/main.json"
    PIXEL_FORMAT rgb565)
