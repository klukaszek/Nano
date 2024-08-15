# Nano 0.4

## Current Implementations

- WGPU Compute Pipelines
    - Input Buffers
    - Output Buffers

- WGPU Render Pipelines
    - Input Buffers
    - Output Buffers
    - Vertex Buffers
    - Uniform Buffers

- WGPU CImGui

## Installation

At the moment, Nano requires Emscripten and CMake to build.
*I plan on releasing a build that does not require the Emscripten toolchain
but at the moment it is *required*.*

```bash
git clone https://github.com/klukaszek/nano.git
cd nano

# Clone cimgui and cglm for samples/demos
git submodule update --init --recursive

# Build using Emscripten's CMake
emcmake cmake .

# Compile
make
```
## Samples

Samples can be found at https://kylelukaszek.xyz/Nano/DEMO_NAME/DEMO_NAME.html
The list of demo names can be found in the Samples/ directory.

## Changelog

### 0.4

Implemented the WGPU RenderPipeline for parsed shaders in Nano. This also comes with the addition of Vertex Buffers and Uniform Buffers. With this version you can find several new demos for loading Vertex and Fragment shaders into a Nano project.

I plan on releasing a much more in-depth post regarding how Nano is put together, but I might decide to do this as part of my undergraduate thesis, so don't hold your breath. This is very much just a passion project at the moment.

For the next few versions I want to focus on refactoring the existing codebase to eliminate any redundencies that may have persisted between version changes.

My TODO is telling me to look into a better way at managing fonts in Nano, ensuring that basic audio is something that can be controlled using Nano, and to create a ShaderToy-like playground as the version 1.0 de-facto demo.

### 0.3 
Implemented shader buffer binding, pipeline layout, bind group, and pipeline generation from WGSL shader files. At the moment, simple compute shaders seem to be working as intended so I am going to take a short break and then get to work on parsing textures from WGSL and generating the appropriate bindings. Once that is complete, I can think about fully generating render pipelines from the shader.

V0.3 comes with a new sample titled "timing test". This is a simple test that times how long it takes for multiple of an operation to be applied to a buffer of floating point data (nothing crazy I know). For this test I ended up implementing single threaded and multithreaded (emscripten "pthreads") approaches to replicate the WGSL compute shader. The results of this test can be found in the console of the sample page.

On my Intel i5 9600k, I found that to perform 100000 addition operations on 65536 floats (262144 bytes) using just a single thread it takes \~12 seconds. When using my multithreaded implementation, I manage to get this down to \~3 seconds. With Nano on my Nvidia 2070 Super, the resulting buffer can be calculated in \~0.12 seconds! That is almost a \~100x increase in performance for large scale floating point number processing on the browser.

One thing I did notice while working on this test is that the emscripten pthread implementation bloats tab memory into oblivion since it seems that once a thread is joined, the memory is not freed from the tab heap. I will try to avoid using pthreads as much as I can for demos and samples in the future unless it is already part of a large demo.

### 0.2
Decided to remove all dependencies on Sokol from Nano. Instead I will try to implement all previous functionality from (basically) scratch. This will be version 0.2. This first commit contains a working demo for a single colour on browser. Still working on porting Compute Shader functionality to my new WebGPU backend.
