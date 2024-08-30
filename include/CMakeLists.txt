if (NANO_NATIVE)
    message(STATUS "Building for native platform")
    set(DAWN_FETCH_DEPENDENCIES ON)
    add_subdirectory("dawn" EXCLUDE_FROM_ALL)
endif()

add_subdirectory(cglm)
add_subdirectory(cimgui)
