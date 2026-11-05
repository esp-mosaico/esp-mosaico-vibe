set(_native_services)
if(RAYLIB_LITE_NATIVE_SERVICE_COMPONENT)
    list(APPEND _native_services "${RAYLIB_LITE_NATIVE_SERVICE_COMPONENT}")
endif()
idf_component_register(
    SRCS
        "native_module_main.c"
        "native_feedback.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        raylib-lite-engine
        examples_audio
        esp_timer
        heap
        log
    PRIV_REQUIRES ${_native_services})

if(DEFINED RAYLIB_LITE_EXAMPLE_TARGET_FPS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        RAYLIB_LITE_NATIVE_TARGET_FPS=${RAYLIB_LITE_EXAMPLE_TARGET_FPS})
endif()

if(RAYLIB_LITE_NATIVE_SERVICE_COMPONENT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_NATIVE_SERVICES_ENABLED=1)
endif()
