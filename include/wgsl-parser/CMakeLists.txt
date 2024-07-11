cmake_minimum_required(VERSION 3.12)
project(wgsl-parser)

set(CMAKE_C_COMPILER "clang")

# Create the wgsl_parser library
add_library(wgsl-parser
    wgsl-parser.c
    wgsl-parser.h
    slre.c
    slre.h
    webgpu.h
)

add_library(wgsl-parserNew
    wgsl-parserNew.c
    wgsl-parserNew.h
    webgpu.h
)

# # Create the test executable
# # Make sure it creates a binary called test
# add_executable(test
#     testNew.c
# )
#
#
# # # Link the wgsl_parser library to the test executable
# target_link_libraries(test PRIVATE wgsl-parserNew)

set(CMAKE_C_COMPILER "emcc")
