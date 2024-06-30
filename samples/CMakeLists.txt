cmake_minimum_required(VERSION 3.5)
project(Nano)

set(CMAKE_EXECUTABLE_SUFFIX ".html")
set(EMCC_COMPILER_FLAGS "-s USE_WEBGPU=1 -s FULL_ES2=1 -O3")
set(EMCC_LINKER_FLAGS "-s USE_WEBGPU=1 -s FULL_ES2=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1")

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add the clear_demo executable 
add_executable(clear_demo clear_demo.c)
target_link_libraries(clear_demo Nano)

# Compiler and linker flags for Emscripten
set_target_properties(clear_demo PROPERTIES
    COMPILE_FLAGS "${EMCC_COMPILER_FLAGS} -o ${PROJECT_NAME}.html --shell-file ${CMAKE_SOURCE_DIR}/shell.html"
    LINK_FLAGS "${EMCC_LINKER_FLAGS}"
)
