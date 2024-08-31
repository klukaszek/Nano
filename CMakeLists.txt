cmake_minimum_required(VERSION 3.5)
project(Nano)

if (EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(NANO_WEB 1)
else()
    set(NANO_NATIVE 1)
    set(CMAKE_EXECUTABLE_SUFFIX "")
    message("Building for native platform")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    include 
    include/cimgui 
    include/cglm
    include/cglm/include
    include/glfw/include
    include/webgpu
    include/glfw3webgpu
    include/assets/fonts
    # ${EMSCRIPTEN_ROOT_PATH}/cache/sysroot/include/webgpu
)

# Add the include directory so that any dependencies can be compiled
# before we find them so we can link them later for the samples
add_subdirectory(include)

# Find the shared cimgui library
find_library(CIMGUI_LIBRARY NAMES cimgui HINTS ${CMAKE_SOURCE_DIR}/include/cimgui/)

# Find the cglm library
find_library(CGLM_LIBRARY NAMES cglm HINTS ${CMAKE_SOURCE_DIR}/include/cglm/)

if (NANO_NATIVE)

    # Find the native WGPU library if we are building for the native platform
    find_library(WGPU_LIBRARY NAMES wgpu_native PATHS ${CMAKE_SOURCE_DIR}/include/webgpu/)
    
    # Find the GLFW3 library
    find_library(GLFW3_LIBRARY NAMES glfw HINTS ${CMAKE_SOURCE_DIR}/include/glfw/)

    find_library(GLFW3WGPU NAMES glfw3webgpu HINTS ${CMAKE_SOURCE_DIR}/include/glfw3webgpu/)

    message("WGPU_LIBRARY: ${WGPU_LIBRARY}")
    message("GLFW3_LIBRARY: ${GLFW3_LIBRARY}")
    message("GLFW3WGPU: ${GLFW3WGPU}")
endif()

# Build the samples
add_subdirectory(samples)
