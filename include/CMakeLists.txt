if (NANO_NATIVE)
    message(STATUS "Building for native platform")
    add_subdirectory(glfw3webgpu)
    # set(DAWN_FETCH_DEPENDENCIES ON)
    # add_subdirectory("dawn" EXCLUDE_FROM_ALL)
endif()

add_subdirectory(cglm)
add_subdirectory(cimgui)
