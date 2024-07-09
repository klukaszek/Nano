cmake_minimum_required(VERSION 3.5)
project(Nano)

set(CMAKE_EXECUTABLE_SUFFIX ".html")
set(EMCC_COMPILER_FLAGS "-s USE_WEBGPU=1 -s FULL_ES2=1 -O3")
set(EMCC_LINKER_FLAGS "-s NO_DISABLE_EXCEPTION_CATCHING=1 -s USE_WEBGPU=1 -s FULL_ES2=1 -s WASM=1")

# Copy the assets to the build directory
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s --preload-file ${CMAKE_SOURCE_DIR}/include/assets@/")

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add the clear_demo executable 
add_executable(clear_demo clear_demo.c)
target_link_libraries(clear_demo Nano)

# Compiler and linker flags for Emscripten
set_target_properties(clear_demo PROPERTIES
    COMPILE_FLAGS "${EMCC_COMPILER_FLAGS}"
    LINK_FLAGS "${EMCC_LINKER_FLAGS} -o clear_demo.html --shell-file shell.html"
)
