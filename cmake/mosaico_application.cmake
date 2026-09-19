# Include before ESP-IDF project.cmake. Existing sdkconfig still takes precedence;
# the recovery adapter validates the effective configuration during configure.
list(APPEND SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../components/esp_mosaico_app_recovery/sdkconfig.defaults")
