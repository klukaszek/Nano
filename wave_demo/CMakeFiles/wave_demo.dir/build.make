# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.30

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/runner/work/Nano/Nano

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/runner/work/Nano/Nano

# Include any dependencies generated for this target.
include samples/wave_demo/CMakeFiles/wave_demo.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include samples/wave_demo/CMakeFiles/wave_demo.dir/compiler_depend.make

# Include the progress variables for this target.
include samples/wave_demo/CMakeFiles/wave_demo.dir/progress.make

# Include the compile flags for this target's objects.
include samples/wave_demo/CMakeFiles/wave_demo.dir/flags.make

samples/wave_demo/CMakeFiles/wave_demo.dir/wave.c.o: samples/wave_demo/CMakeFiles/wave_demo.dir/flags.make
samples/wave_demo/CMakeFiles/wave_demo.dir/wave.c.o: samples/wave_demo/CMakeFiles/wave_demo.dir/includes_C.rsp
samples/wave_demo/CMakeFiles/wave_demo.dir/wave.c.o: samples/wave_demo/wave.c
samples/wave_demo/CMakeFiles/wave_demo.dir/wave.c.o: samples/wave_demo/CMakeFiles/wave_demo.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/runner/work/Nano/Nano/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object samples/wave_demo/CMakeFiles/wave_demo.dir/wave.c.o"
	cd /home/runner/work/Nano/Nano/samples/wave_demo && /home/runner/work/Nano/Nano/emsdk/upstream/emscripten/emcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT samples/wave_demo/CMakeFiles/wave_demo.dir/wave.c.o -MF CMakeFiles/wave_demo.dir/wave.c.o.d -o CMakeFiles/wave_demo.dir/wave.c.o -c /home/runner/work/Nano/Nano/samples/wave_demo/wave.c

samples/wave_demo/CMakeFiles/wave_demo.dir/wave.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing C source to CMakeFiles/wave_demo.dir/wave.c.i"
	cd /home/runner/work/Nano/Nano/samples/wave_demo && /home/runner/work/Nano/Nano/emsdk/upstream/emscripten/emcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/runner/work/Nano/Nano/samples/wave_demo/wave.c > CMakeFiles/wave_demo.dir/wave.c.i

samples/wave_demo/CMakeFiles/wave_demo.dir/wave.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling C source to assembly CMakeFiles/wave_demo.dir/wave.c.s"
	cd /home/runner/work/Nano/Nano/samples/wave_demo && /home/runner/work/Nano/Nano/emsdk/upstream/emscripten/emcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/runner/work/Nano/Nano/samples/wave_demo/wave.c -o CMakeFiles/wave_demo.dir/wave.c.s

# Object files for target wave_demo
wave_demo_OBJECTS = \
"CMakeFiles/wave_demo.dir/wave.c.o"

# External object files for target wave_demo
wave_demo_EXTERNAL_OBJECTS =

samples/wave_demo/wave_demo.html: samples/wave_demo/CMakeFiles/wave_demo.dir/wave.c.o
samples/wave_demo/wave_demo.html: samples/wave_demo/CMakeFiles/wave_demo.dir/build.make
samples/wave_demo/wave_demo.html: include/cimgui/cimgui.a
samples/wave_demo/wave_demo.html: samples/wave_demo/CMakeFiles/wave_demo.dir/linkLibs.rsp
samples/wave_demo/wave_demo.html: samples/wave_demo/CMakeFiles/wave_demo.dir/objects1.rsp
samples/wave_demo/wave_demo.html: samples/wave_demo/CMakeFiles/wave_demo.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/runner/work/Nano/Nano/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable wave_demo.html"
	cd /home/runner/work/Nano/Nano/samples/wave_demo && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/wave_demo.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
samples/wave_demo/CMakeFiles/wave_demo.dir/build: samples/wave_demo/wave_demo.html
.PHONY : samples/wave_demo/CMakeFiles/wave_demo.dir/build

samples/wave_demo/CMakeFiles/wave_demo.dir/clean:
	cd /home/runner/work/Nano/Nano/samples/wave_demo && $(CMAKE_COMMAND) -P CMakeFiles/wave_demo.dir/cmake_clean.cmake
.PHONY : samples/wave_demo/CMakeFiles/wave_demo.dir/clean

samples/wave_demo/CMakeFiles/wave_demo.dir/depend:
	cd /home/runner/work/Nano/Nano && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/runner/work/Nano/Nano /home/runner/work/Nano/Nano/samples/wave_demo /home/runner/work/Nano/Nano /home/runner/work/Nano/Nano/samples/wave_demo /home/runner/work/Nano/Nano/samples/wave_demo/CMakeFiles/wave_demo.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : samples/wave_demo/CMakeFiles/wave_demo.dir/depend
