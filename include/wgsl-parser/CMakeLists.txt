cmake_minimum_required(VERSION 3.12)
project(wgsl-parser)

# Create the wgsl_parser library
add_library(wgsl-parser
    wgsl-parser.c
    wgsl-parser.h
    slre.c
    slre.h
    webgpu.h
)

# Create the test executable
add_executable(test
    test.c
)

# Link the wgsl_parser library to the test executable
target_link_libraries(test PRIVATE wgsl-parser)
