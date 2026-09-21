# Standard ESP-IDF project() entry for every normal Mosaico application.
# Include after EXTRA_COMPONENT_DIRS is set. Call project() afterwards.
#
# Recovery owns the retained second-stage bootloader (0x2000-0x8000) and the
# first-frame logo. system-update never replaces that bootloader, and the
# immutable prefix (otadata through coredump) cannot grow it. Keep the splash
# component on the application side when the workspace BSP provides it; do not
# compile its LCD/SPI renderer into this application's bootloader.
include("${CMAKE_CURRENT_LIST_DIR}/mosaico_application.cmake")

get_filename_component(_mosaico_boot_splash
    "${CMAKE_CURRENT_LIST_DIR}/../submodule/esp-mosaico-bsp/components/mosaico_boot_splash"
    ABSOLUTE)
if(EXISTS "${_mosaico_boot_splash}/CMakeLists.txt")
    list(APPEND EXTRA_COMPONENT_DIRS "${_mosaico_boot_splash}")
    list(REMOVE_DUPLICATES EXTRA_COMPONENT_DIRS)
endif()

if(NOT DEFINED MOSAICO_BOOT_SPLASH_IN_BOOTLOADER)
    set(MOSAICO_BOOT_SPLASH_IN_BOOTLOADER OFF)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(_mosaico_register_boot_splash
    "${CMAKE_CURRENT_LIST_DIR}/../submodule/esp-mosaico-bsp/cmake/register_boot_splash.cmake")
if(MOSAICO_BOOT_SPLASH_IN_BOOTLOADER AND EXISTS "${_mosaico_register_boot_splash}")
    include("${_mosaico_register_boot_splash}")
endif()
unset(_mosaico_register_boot_splash)
unset(_mosaico_boot_splash)
