cmake_minimum_required(VERSION 3.5)
project(Nano)

set(CMAKE_EXECUTABLE_SUFFIX ".html")

# Copy the assets to the build directory
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ASSERTIONS --preload-file ${CMAKE_SOURCE_DIR}/include/assets/shaders@/")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s INITIAL_MEMORY=100mb -s STACK_SIZE=64mb -s ALLOW_MEMORY_GROWTH")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_WEBGPU=1 -s FULL_ES2=1 -O3 -s WASM=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -sPTHREAD_POOL_SIZE=5 -s WASM_WORKERS")

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add the clear_demo executable 
add_executable(clear_demo clear_demo.c)
target_link_libraries(clear_demo Nano)

# Compiler and linker flags for Emscripten
set_target_properties(clear_demo PROPERTIES
    COMPILE_FLAGS "${EMCC_COMPILER_FLAGS}"
    LINK_FLAGS "${EMCC_LINKER_FLAGS} -o clear_demo.html --shell-file ../shell.html"
)
