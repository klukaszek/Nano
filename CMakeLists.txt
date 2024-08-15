cmake_minimum_required(VERSION 3.5)
project(Nano)

set(CMAKE_EXECUTABLE_SUFFIX ".html")

# Include directories
include_directories(
    include 
    include/cimgui 
    include/cglm
    include/cglm/include
    include/assets/fonts
    ${EMSCRIPTEN_ROOT_PATH}/cache/sysroot/include/webgpu
)

# Add the include directory so that any dependencies can be compiled
# before we find them so we can link them later for the samples
add_subdirectory(include)

# Find the shared cimgui library
find_library(CIMGUI_LIBRARY NAMES cimgui PATHS ${CMAKE_SOURCE_DIR}/include/cimgui/)

# Find the cglm library
find_library(CGLM_LIBRARY NAMES cglm PATHS ${CMAKE_SOURCE_DIR}/include/cglm/)

# Build the samples
add_subdirectory(samples)
