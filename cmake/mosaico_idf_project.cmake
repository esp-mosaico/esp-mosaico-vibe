# Include after EXTRA_COMPONENT_DIRS is set. Call project() afterwards.
get_filename_component(_mosaico_boot_splash
    "${CMAKE_CURRENT_LIST_DIR}/../submodule/esp-mosaico-bsp/components/mosaico_boot_splash"
    ABSOLUTE)
if(EXISTS "${_mosaico_boot_splash}/CMakeLists.txt")
    list(APPEND EXTRA_COMPONENT_DIRS "${_mosaico_boot_splash}")
    list(REMOVE_DUPLICATES EXTRA_COMPONENT_DIRS)
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include("${CMAKE_CURRENT_LIST_DIR}/../submodule/esp-mosaico-bsp/cmake/register_boot_splash.cmake")
unset(_mosaico_boot_splash)
