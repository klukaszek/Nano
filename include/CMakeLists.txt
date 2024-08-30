if (NANO_NATIVE)
    message(STATUS "Building for native platform")
    add_subdirectory(dawn)
endif()

add_subdirectory(cglm)
add_subdirectory(cimgui)
