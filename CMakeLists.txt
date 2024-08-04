cmake_minimum_required(VERSION 3.5)
project(Nano)

set(CMAKE_EXECUTABLE_SUFFIX ".html")

# Define the source files
set(SOURCES
    nano.h
    include/nano_cimgui.h
)

# Add the include directory so that any dependencies can be compiled
# before we link the main executable. Ultimately, Nano should be a plug
# and play WebGPU header file, but for now we need to compile it to test it.
add_subdirectory(include)

# Include directories
include_directories(
    include 
    include/cimgui 
    include/wgsl-parser
    include/cglm
    include/cglm/include
    include/assets/fonts
    ${EMSCRIPTEN_ROOT_PATH}/cache/sysroot/include/webgpu
)

# Find the shared cimgui library
find_library(CIMGUI_LIBRARY NAMES cimgui PATHS ${CMAKE_SOURCE_DIR}/include/cimgui/)

# Find the wgsl-parser library
find_library(WGSL_PARSER_LIBRARY NAMES wgsl-parser PATHS ${CMAKE_SOURCE_DIR}/include/wgsl-parser/)

# Find the cglm library
find_library(CGLM_LIBRARY NAMES cglm PATHS ${CMAKE_SOURCE_DIR}/include/cglm/)

# Create a library using Nano, WGPU Entry, CGLM, and CImGui
add_library(Nano STATIC ${SOURCES})

# Link the Nano library with necessary dependencies
add_dependencies(Nano cimgui wgsl-parser)
target_link_libraries(Nano
    cimgui
    wgsl-parser
    # cglm
)

add_subdirectory(samples)
