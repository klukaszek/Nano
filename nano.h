// ---------------------------------------------------------------------
//  nano.h
//  version 0.2.0 (2024-06-30)
//  author: Kyle Lukaszek
//  --------------------------------------------------------------------
//  CIMGUI + WebGPU Application Framework
//  --------------------------------------------------------------------
//  C Application framework for creating 2D/3D applications
//  using WebGPU and CImGui. This header file includes the necessary
//  functions and data structures to create a simple application.
//
//  --------------------------------------------------------------------
//  To create a new application, simply include this header file in your
//  main.c file and define the following functions and macros:
//
//  #include "nano.h"
//
//  static void init() {
//      // Make sure to call nano_basic_init() to initialize the sokol app
//      nano_basic_init();
//
//      // Add any additional initialization code here
//      ...
//  }
//
//  static void cleanup() {
//      // Add any cleanup code here
//      ...
//      nano_basic_cleanup();
//  }
//
//  static void event(const sapp_event *e) {
//      nano_basic_event(e);
//      // Add any event handling code here
//      ...
//  }
//
//  static void frame() - This function is called every frame and is where you
//  will
//            update your application logic and render your scene.
//
//  // Define the sokol app description and return it to start the app
//  sapp_desc sokol_main(int argc, char *argv[]) {
//      // Define the sokol app description
//      return (sapp_desc){
//      .init_cb = nano_init,
//      .frame_cb = frame,
//      .cleanup_cb = nano_cleanup,
//      .event_cb = nano_event,
//      .icon.sokol_default = true,
//      .logger.func = slog_func,
//      .width = 800,
//      .height = 600,
//      .window_title = "nano app",
//      .high_dpi = true,
//      }
//  }
// ------------------------------------------------------------------

// TODO: Fix font rendering resolution on WebGPU
// TODO: Look into ligature support for fonts
//       -> This is a bit tricky because ImGui does not support ligatures.
//       -> We would have to implement this ourselves by writing a custom method
//       to preprocess strings and replace ligatures with the correct unicode
//       characters.

#define CIMGUI_WGPU
#include "wgpu_entry.h"
#include <stdint.h>
#include <unistd.h>
#include <webgpu/webgpu.h>
#include <wgsl-parser.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>

// Include cglm for matrix and vector operations
#include <cglm/struct.h>

#ifndef NANO_DEBUG
    #define NANO_DEBUG_UI 0
#else
    #define NANO_DEBUG_UI 1
#endif

// File IO
// -----------------------------------------------
// Function to read a shader into a string
char *nano_read_file(const char *filename) {
    assert(filename != NULL);

    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "NANO: nano_read_file() -> Could not open file %s\n",
                filename);
        return NULL;
    }

    // Get the length of the file so we can allocate the correct amount of
    // memory for the shader string buffer
    fseek(file, 0, SEEK_END);
    size_t length = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    // It is important to free the buffer after using it, it might be a good
    // idea to implement an arena structure into the Nano STL for better
    // handling of dynamic memory allocations.
    char *buffer = (char *)malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "NANO: nano_read_file() -> Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    return buffer;
}

// -------------------------------------------------
// Nano Font Data
// -------------------------------------------------

// Include fonts as header files
#include "JetBrainsMonoNerdFontMono-Bold.h"
#include "LilexNerdFontMono-Medium.h"
#include "Roboto-Regular.h"

// Total number of fonts included in nano
#define NANO_NUM_FONTS 3

// -------------------------------------------------
//  NANO
// -------------------------------------------------

#define NANO_FAIL -1
#define NANO_OK 0

// Maximum number of compute shaders that can be stored in the shader pool
#define NANO_MAX_COMPUTE 16

// Maximum size of the hash table for each of the buffer pools.
#define NANO_MAX_BUFFERS 256

typedef struct nano_buffer_t {
    WGPUBuffer buffer;
    bool in_use;
    WGPUBufferUsageFlags usage;
    uint32_t shader_id;
    uint8_t group;
    uint8_t binding;
    size_t size;
} nano_buffer_t;

typedef struct nano_pipeline_layout_t {
    WGPUBindGroupLayout bg_layouts[MAX_GROUPS];
    size_t num_layouts;
} nano_pipeline_layout_t;

typedef struct BufferNode {
    nano_buffer_t buffer_entry;
    bool occupied;
} BufferNode;

typedef struct nano_buffer_pool_t {
    BufferNode buffers[NANO_MAX_BUFFERS];
    size_t buffer_count;
    size_t total_size;
} nano_buffer_pool_t;

typedef struct nano_shader_t {
    uint32_t id;
    WGPUComputePipeline pipeline;
    nano_pipeline_layout_t pipeline_layout;
    bool in_use;
} nano_shader_t;

typedef struct ShaderNode {
    nano_shader_t shader_entry;
    bool occupied;
} ShaderNode;

typedef struct nano_shader_pool_t {
    ShaderNode shaders[NANO_MAX_COMPUTE];
    size_t shader_count;
} nano_shader_pool_t;

typedef WGPURenderPassDescriptor nano_pass_action_t;
typedef wgpu_state_t nano_wgpu_state_t;

typedef struct nano_t {
    nano_wgpu_state_t wgpu;
    bool show_debug;
    vec2s dimensions;
    float frametime;
    float fps;
    int frame_count;
    nano_pass_action_t pass_action;
    int font_index;
    float font_size;
    nano_buffer_pool_t buffer_pool;
    nano_buffer_pool_t staging_pool;
    nano_shader_pool_t shader_pool;
    ImFont *fonts[NANO_NUM_FONTS];
} nano_t;

// Initialize a static nano_t struct to hold the running application data
static nano_t nano_app = {
    .wgpu = {0},
    .show_debug = NANO_DEBUG_UI,
    .dimensions = {0},
    .frametime = 0.0f,
    .fps = 0.0f,
    .frame_count = 0.0f,
    .pass_action = 0,
    .font_index = 0,
    .font_size = 16.0f,
    .buffer_pool = {0},
    .staging_pool = {0},
    .shader_pool = {0},
    .fonts = {0},
};

// Buffer Pool Functions
// -------------------------------------------------

// Initialize the buffer pool with the given table
void nano_init_buffer_pool(nano_buffer_pool_t *table) {
    assert(table != NULL);
    for (int i = 0; i < NANO_MAX_BUFFERS; i++) {
        table->buffers[i].occupied = false;
    }
    table->buffer_count = 0;
    table->total_size = 0;
}

// Hash function to hash the buffer using the shader id, group, and binding
uint8_t _nano_hash_buffer(uint32_t shader_id, uint8_t group, uint8_t binding) {
    return (shader_id * 31 + group * 31 + binding) % NANO_MAX_BUFFERS;
}

// Helper function to insert a buffer into the buffer pool
bool _nano_insert_buffer(nano_buffer_pool_t *table, nano_buffer_t buffer) {
    assert(table != NULL);
    assert(buffer.buffer != NULL);

    uint8_t hash =
        _nano_hash_buffer(buffer.shader_id, buffer.group, buffer.binding);
    uint8_t original_hash = hash;

    do {
        if (!table->buffers[hash].occupied) {
            table->buffers[hash].buffer_entry = buffer;
            table->buffers[hash].occupied = true;
            table->buffer_count++;
            table->total_size += buffer.size;
            printf("NANO: Buffer Added To Pool | shader id: %d, group: %d, "
                   "binding: %d\n",
                   buffer.shader_id, buffer.group, buffer.binding);
            return true;
        }
        hash = (hash + 1) % NANO_MAX_BUFFERS;
    } while (hash != original_hash);

    printf("Error: Buffer pool is full\n");
    return false;
}

// Get a buffer from the buffer pool using the shader id, group, and binding
// These values are used to hash the buffer and find the correct buffer in the
// buffer pool.
nano_buffer_t *nano_get_buffer(nano_buffer_pool_t *table, uint32_t shader_id,
                               uint8_t group, uint8_t binding) {
    assert(table != NULL);

    uint8_t hash = _nano_hash_buffer(shader_id, group, binding);
    uint8_t original_hash = hash;

    do {
        if (table->buffers[hash].occupied &&
            table->buffers[hash].buffer_entry.shader_id == shader_id &&
            table->buffers[hash].buffer_entry.group == group &&
            table->buffers[hash].buffer_entry.binding == binding) {
            return &table->buffers[hash].buffer_entry;
        }
        hash = (hash + 1) % NANO_MAX_BUFFERS;
    } while (hash != original_hash && table->buffers[hash].occupied);

    return NULL;
}

// Create a Nano WGPU buffer object and add the buffer to the buffer pool.
// Returns a nano_buffer_t struct with the buffer object and metadata.
nano_buffer_t nano_create_buffer(WGPUDevice device,
                                 nano_buffer_pool_t *buffer_pool,
                                 uint32_t shader_id, uint8_t group,
                                 uint8_t binding, WGPUBufferUsageFlags usage,
                                 size_t size) {
    // Align the buffer size to the GPU cache line size of 32 bytes
    size_t gpu_cache_line_size = 32;
    size_t cache_aligned_size =
        (size + (gpu_cache_line_size - 1)) & ~(gpu_cache_line_size - 1);

    // Create the buffer descriptor
    WGPUBufferDescriptor desc = {
        .usage = usage,
        .size = cache_aligned_size,
        .mappedAtCreation = false,
    };

    // Create the buffer (implementation details omitted)
    WGPUBuffer wgpu_buffer = wgpuDeviceCreateBuffer(device, &desc);
    if (wgpu_buffer == NULL) {
        fprintf(stderr,
                "NANO: nano_create_buffer() -> Could not create buffer\n");
        return (nano_buffer_t){0};
    }

    // Create nano buffer struct to hold the buffer and metadata
    nano_buffer_t buffer = {
        .buffer = wgpu_buffer,
        .in_use = true,
        .shader_id = shader_id,
        .usage = usage,
        .group = group,
        .binding = binding,
        .size = cache_aligned_size,
    };

    // Insert the buffer into the buffer pool
    // if the insertion fails, release the buffer and return an empty buffer
    if (!_nano_insert_buffer(buffer_pool, buffer)) {
        // Handle insertion failure
        // Release the created buffer
        wgpuBufferRelease(wgpu_buffer);
        return (nano_buffer_t){0};
    }

    return buffer;
}

// Release a buffer from the buffer pool
void nano_release_buffer(nano_buffer_pool_t *buffer_pool,
                         nano_buffer_t buffer) {
    assert(buffer_pool != NULL);

    uint8_t hash =
        _nano_hash_buffer(buffer.shader_id, buffer.group, buffer.binding);
    uint8_t original_hash = hash;

    do {
        if (buffer_pool->buffers[hash].occupied &&
            buffer_pool->buffers[hash].buffer_entry.buffer == buffer.buffer) {
            buffer_pool->buffers[hash].occupied = false;
            wgpuBufferRelease(buffer.buffer);
            buffer_pool->buffer_count--;
            buffer_pool->total_size -= buffer.size;
            return;
        }
        hash = (hash + 1) % NANO_MAX_BUFFERS;
    } while (hash != original_hash);
}

// Destroy a buffer from the buffer pool
void nano_destroy_buffer(nano_buffer_pool_t *buffer_pool,
                         nano_buffer_t buffer) {
    assert(buffer_pool != NULL);

    uint8_t hash =
        _nano_hash_buffer(buffer.shader_id, buffer.group, buffer.binding);
    uint8_t original_hash = hash;

    do {
        if (buffer_pool->buffers[hash].occupied &&
            buffer_pool->buffers[hash].buffer_entry.buffer == buffer.buffer) {
            buffer_pool->buffers[hash].occupied = false;
            wgpuBufferDestroy(buffer.buffer);
            buffer_pool->buffer_count--;
            buffer_pool->total_size -= buffer.size;
            return;
        }
        hash = (hash + 1) % NANO_MAX_BUFFERS;
    } while (hash != original_hash);
}

// Shader Pool Functions
// -------------------------------------------------

// Simple hash function to hash the shader id to find the correct slot in the
// shader pool
static uint32_t nano_hash_shader_id(uint32_t shader_id) {
    return shader_id % NANO_MAX_COMPUTE;
}

// Initialize the shader pool with the correct default values
// This function should be called before using the shader pool
void nano_init_shader_pool(nano_shader_pool_t *table) {
    assert(table != NULL);
    for (int i = 0; i < NANO_MAX_COMPUTE; i++) {
        table->shaders[i].occupied = false;
    }
    table->shader_count = 0;
}

// Find a shader slot in the shader pool using the shader id
static int nano_find_shader_slot(nano_shader_pool_t *table,
                                 uint32_t shader_id) {
    uint32_t hash = nano_hash_shader_id(shader_id);
    uint32_t index = hash;

    // Linear probing
    for (int i = 0; i < NANO_MAX_COMPUTE; i++) {
        nano_shader_t *shader = &table->shaders[index].shader_entry;
        if (!shader->in_use || shader->id == shader_id) {
            return (int)index;
        }
        index = (index + 1) % NANO_MAX_COMPUTE;
    }

    // If we get here, the shader pool is full
    return -1;
}

// Retrieve a shader from the shader pool using the shader id
nano_shader_t *nano_get_shader(nano_shader_pool_t *table, uint32_t shader_id) {
    int index = nano_find_shader_slot(table, shader_id);
    if (index < 0) {
        return NULL;
    }

    return &table->shaders[index].shader_entry;
}

void nano_release_shader(nano_shader_pool_t *table, uint32_t shader_id) {
    int index = nano_find_shader_slot(table, shader_id);
    if (index < 0) {
        return;
    }

    table->shaders[index].occupied = false;
    wgpuComputePipelineRelease(table->shaders[index].shader_entry.pipeline);
    table->shaders[index].shader_entry = (nano_shader_t){0};
    table->shader_count--;
}

// String hashing function using the FNV-1a algorithm
// Used for hashing strings to create unique IDs for shaders
uint32_t fnv1a_32(const char *key) {
    uint32_t hash = 2166136261u;
    for (; *key; ++key) {
        hash ^= (uint32_t)*key++;
        hash *= 16777619;
    }
    return hash;
}

/*
 * Hash a shader string to create a unique ID for the shader
 * @param shader - The shader string to hash (can be filename or shader code)
 * @return uint32_t - The 32-bit unsigned integer hash value
 */
uint32_t nano_hash_shader(const char *shader) { return fnv1a_32(shader); }

// Toggle flag to show debug UI
void nano_toggle_debug() { nano_app.show_debug = !nano_app.show_debug; }

// Font Data and Methods
// -----------------------------------------------

// Font struct to hold the TTF data and length
typedef struct nano_font_t {
    unsigned char *ttf;
    int ttf_len;
} nano_font_t;

// Font 0
nano_font_t jetbrains_mono_nerd_font = {
    .ttf = JetBrainsMonoNerdFontMono_Bold_ttf,
    .ttf_len = sizeof(JetBrainsMonoNerdFontMono_Bold_ttf)};

// Font 1
nano_font_t lilex_nerd_font = {.ttf = LilexNerdFontMono_Medium_ttf,
                               .ttf_len = sizeof(LilexNerdFontMono_Medium_ttf)};

// Font 2
nano_font_t roboto_font = {.ttf = Roboto_Regular_ttf,
                           .ttf_len = sizeof(Roboto_Regular_ttf)};

// 0 => JetBrains Mono Nerd, 1 => Lilex Nerd Font, 2 => Roboto
static nano_font_t *fonts[] = {&jetbrains_mono_nerd_font, &lilex_nerd_font,
                               &roboto_font};

// Function to set the font for the ImGui context
void nano_set_font(int index) {
    if (&(nano_app.fonts[index]) == NULL) {
        fprintf(stderr, "NANO: nano_set_font() -> Font is NULL\n");
        return;
    }

    ImGuiIO *io = igGetIO();

    // Set the font as the default font
    io->FontDefault = nano_app.fonts[index];
    // IMPORTANT: FIGURE OUT DPI SCALING FOR WEBGPU
    // io->FontGlobalScale = sapp_dpi_scale();
}

// Function to set the font size for the ImGui context
void nano_set_font_size(float size) {
    ImGuiIO *io = igGetIO();
    for (int i = 0; i < NANO_NUM_FONTS; i++) {
        nano_app.fonts[i] = ImFontAtlas_AddFontFromMemoryTTF(
            io->Fonts, fonts[i]->ttf, fonts[i]->ttf_len, size, NULL, NULL);
    }
    nano_set_font(nano_app.font_index);
}

// Stats / Telemetry
// -----------------------------------------------

// Calculate current frames per second
static void nano_calc_fps() {

    // Get the frame time and calculate the frames per second with delta time
    nano_app.frametime = wgpu_frametime();
    nano_app.dimensions = (vec2s){{wgpu_width(), wgpu_height()}};
    nano_app.fps = 1000 / nano_app.frametime;
}

// Shader Parsing (using the wgsl-parser library)
// -----------------------------------------------

// Aliases for the wgsl-parser structs
typedef ComputeInfo nano_compute_info_t;
typedef GroupInfo nano_group_info_t;
typedef BindingInfo nano_binding_info_t;
typedef WGPUBufferUsageFlags nano_buffer_usage_t;
typedef WGPUBufferDescriptor nano_buffer_desc_t;

nano_compute_info_t *nano_parse_compute_shader(char *shader) {
    if (shader == NULL) {
        fprintf(stderr,
                "NANO: nano_parse_compute_shader() -> Shader is NULL\n");
        return NULL;
    }

    // Initialize the compute info struct
    ComputeInfo *info = &((ComputeInfo){
        .workgroup_size = {1, 1, 1},
        .groups = {{0}},
        .entry = {0},
    });

    // Use my wgsl-parser library to parse the compute shader
    int result = parse_wgsl_compute(shader, info);
    if (result < 0) {
        fprintf(stderr,
                "NANO: nano_parse_compute_shader() -> Error parsing shader\n");
        return NULL;
    }

    return (nano_compute_info_t *)info;
}

// Validate the compute shader using the wgsl-parser library
int nano_validate_compute_shader(nano_compute_info_t *info) {
    if (info == NULL) {
        fprintf(
            stderr,
            "NANO: nano_validate_compute_shader() -> Compute info is NULL\n");
        return -1;
    }

    // Use my wgsl-parser library to validate the compute shader
    int result = validate_compute((ComputeInfo *)info);
    if (result < 0) {
        fprintf(stderr, "NANO: nano_validate_compute_shader() -> Error "
                        "validating shader\n");
        return -1;
    }

    return 0;
}

// Return the buffer usage flags for a binding info struct
nano_buffer_usage_t nano_get_wgpu_buffer_usage(nano_binding_info_t *binding) {
    if (binding == NULL) {
        fprintf(stderr,
                "NANO: nano_get_wgpu_buffer_usage() -> Binding is NULL\n");
        return WGPUBufferUsage_None;
    }

    WGPUBufferUsageFlags usage = WGPUBufferUsage_None;

    // Make copy of the usage string so we can tokenize it without ruining the
    // pointer
    char usage_copy[51];
    strncpy(usage_copy, binding->usage, strlen(binding->usage));

    // Tokenize the usage string and check if all usages are valid
    char *token = strtok(usage_copy, ", ");
    while (token != NULL) {
        if (strcmp(token, "storage") == 0) {
            usage |= WGPUBufferUsage_Storage;
        } else if (strcmp(token, "uniform") == 0) {
            usage |= WGPUBufferUsage_Uniform;
        } else if (strcmp(token, "read_write") == 0) {
            usage |= WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
        } else if (strcmp(token, "read") == 0) {
            usage |= WGPUBufferUsage_CopySrc;
        }
        token = strtok(NULL, ", ");
    }

    return (nano_buffer_usage_t)usage;
}

// Generate the buffers and bind group layouts necessary for the pipeline layout
int nano_build_pipeline_layout(nano_compute_info_t *info, uint32_t shader_id,
                               size_t buffer_size,
                               nano_pipeline_layout_t *out_layout) {
    if (info == NULL) {
        fprintf(stderr,
                "Nano: nano_build_pipeline_layout() -> Compute info is NULL\n");
        return NANO_FAIL;
    }

    size_t num_groups = 0;
    WGPUBindGroupLayout bg_layouts[MAX_GROUPS];

    // Iterate over the groups and bindings to create the buffers to be used
    // for the shader These buffers should be created in the buffer pool
    for (int i = 0; i < MAX_GROUPS; i++) {
        nano_group_info_t group = info->groups[i];
        if (group.num_bindings >= MAX_BINDINGS) {
            fprintf(stderr, "NANO: Shader %d: Group %d has too many bindings\n",
                    shader_id, i);
            break;
        }

        // If a group has no bindings, we can break out of the loop early since
        // no other groups should have bindings.
        if (group.num_bindings == 0) {
            break;
        }

        printf("NANO: Shader %d: Group %d has %d bindings\n", shader_id, i,
               group.num_bindings);

        // If a group has bindings, we must create the buffers for each binding.
        // Once the bindings are created, we can create the bind group layout
        // entry. This will be used to create the bind group layout descriptor.
        // From here we can create the pipeline layout descriptor and the
        // compute pipeline descriptor.

        WGPUBindGroupLayoutEntry bgl_entries[MAX_BINDINGS];

        // BINDING AND BGL CREATION
        for (int j = 0; j < group.num_bindings; j++) {
            nano_binding_info_t binding = group.bindings[j];
            // Check if the binding has a usage to be considered valid
            if (strlen(binding.usage) > 0) {
                WGPUBufferUsageFlags buffer_usage =
                    nano_get_wgpu_buffer_usage(&binding);

                printf("NANO: Shader %d: Creating new buffer for binding %d "
                       "with usage %s\n",
                       shader_id, binding.binding, binding.usage);

                // Create the buffer in the buffer pool
                // Think about moving all of this to a separate function just
                // for understanding and readability
                nano_buffer_t buffer = nano_create_buffer(
                    nano_app.wgpu.device, &nano_app.buffer_pool, shader_id, i,
                    j, buffer_usage, buffer_size);

                // Set the according bindgroup layout entry for the binding
                bgl_entries[j] = (WGPUBindGroupLayoutEntry){
                    .binding = binding.binding,
                    .visibility = WGPUShaderStage_Compute,
                    // We use a ternary operator to determine the buffer type by
                    // inferring the type from binding usage. If it is a
                    // uniform, we know the binding type is a uniform buffer.
                    // Otherwise, we assume it is a storage buffer.
                    .buffer = {.type = (buffer_usage ==
                                        WGPUBufferBindingType_Uniform)
                                           ? WGPUBufferBindingType_Uniform
                                           : WGPUBufferBindingType_Storage},
                };

                printf("NANO: Shader %d: Bindgroup layout entry created for "
                       "binding %d\n",
                       shader_id, binding.binding);
            } else {
                break;
            }
        }

        // BINDGROUP LAYOUT CREATION
        WGPUBindGroupLayoutDescriptor bg_layout_desc = {
            .entryCount = (size_t)group.num_bindings,
            .entries = bgl_entries,
        };

        printf("NANO: Shader %d: Creating bind group layout for group %d\n",
               shader_id, i);

        // Assign the bind group layout to the bind group layout array
        // so that we can create the WGPUPipelineLayoutDescriptor later.
        bg_layouts[i] = wgpuDeviceCreateBindGroupLayout(nano_app.wgpu.device,
                                                        &bg_layout_desc);
        if (bg_layouts[i] == NULL) {
            fprintf(stderr,
                    "NANO: Shader %d: Could not create bind group "
                    "layout for group %d\n",
                    shader_id, i);
            return NANO_FAIL;
        }

        // Increment the number of groups
        num_groups++;
    }

    // Copy the bind group layouts to the output layout
    out_layout->num_layouts = num_groups;
    // Copy the bind group layouts to the nano_pipeline_layout_t struct
    memcpy(out_layout->bg_layouts, bg_layouts, sizeof(bg_layouts));

    printf("NANO: Shader %d: Bind group layout created with %zu groups\n",
           shader_id, out_layout->num_layouts);

    return NANO_OK;
}

// Create our nano_shader_t struct to hold the compute shader and pipeline,
// return shader id on success, 0 on failure
uint32_t nano_create_shader(const char *shader_path, nano_compute_info_t *info,
                            size_t buffer_size) {
    if (shader_path == NULL) {
        fprintf(stderr, "NANO: nano_create_shader() -> Shader path is NULL\n");
        return 0;
    }

    // Get shader id hash for the compute shader by hashing the shader path
    uint32_t shader_id = nano_hash_shader(shader_path);

    // Find a slot in the shader pool to store the shader
    int slot = nano_find_shader_slot(&nano_app.shader_pool, shader_id);
    if (slot < 0) {
        fprintf(stderr,
                "NANO: Shader pool is full. Could not insert shader %d\n",
                shader_id);
        return 0;
    }

    // Read the shader code from the file
    char *shader_source = nano_read_file(shader_path);
    if (!shader_source) {
        fprintf(stderr, "Could not read file %s\n", shader_path);
        return 0;
    }

    // Parse the compute shader to get the workgroup size as well and group
    // layout requirements. These are stored in the info struct.
    memcpy(info, nano_parse_compute_shader(shader_source),
           sizeof(nano_compute_info_t));
    if (!info) {
        fprintf(stderr, "Could not parse compute shader %s\n", shader_path);
        return 0;
    } else {
        printf("NANO: Parsed compute shader: %s\n", shader_path);
        printf("NANO: Shader workgroup size: (%d, %d, %d)\n",
               info->workgroup_size[0], info->workgroup_size[1],
               info->workgroup_size[2]);
    }

    // Validate the compute shader
    int status = nano_validate_compute_shader(info);
    if (status != NANO_OK) {
        fprintf(stderr, "Failed to validate compute shader %s\n", shader_path);
        return 0;
    }

    // Create the compute shader module for WGSL shaders
    WGPUShaderModuleWGSLDescriptor wgsl_desc = {
        .chain =
            {
                .next = NULL,
                .sType = WGPUSType_ShaderModuleWGSLDescriptor,
            },
        .code = shader_source,
    };

    // This is the main descriptor that will be used to create the compute
    // shader. We must include the WGSL descriptor in the nextInChain field.
    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = (WGPUChainedStruct *)&wgsl_desc,
        .label = "Compute Shader",
    };

    // Create the shader module for the compute shader and free the source
    WGPUShaderModule compute_shader =
        wgpuDeviceCreateShaderModule(nano_app.wgpu.device, &shader_desc);

    // Free the shader source after creating the shader module
    free(shader_source);

    // Create a programmable stage descriptor for the compute shader
    // We essentially declare the compute shader as the module and the
    // entry point as the main function. We can pass this descriptor to
    // the compute pipeline to act as the compute stage.
    WGPUProgrammableStageDescriptor compute_stage_desc = {
        .module = compute_shader,
        .entryPoint = "main",
    };

    printf("NANO: Creating compute shader with id %d\n", shader_id);

    // Create the pipeline layout for the shader
    nano_pipeline_layout_t pipeline_layout;
    status = nano_build_pipeline_layout(info, shader_id, buffer_size,
                                        &pipeline_layout);
    if (status != NANO_OK) {
        fprintf(stderr, "NANO: Failed to build pipeline layout for shader %d\n",
                shader_id);
        return 0;
    }

    char label[64];
    snprintf(label, 64, "Shader %d", shader_id);
    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .bindGroupLayoutCount = pipeline_layout.num_layouts,
        .bindGroupLayouts = pipeline_layout.bg_layouts,
        .label = label,
    };

    WGPUPipelineLayout pipeline_layout_obj = wgpuDeviceCreatePipelineLayout(
        nano_app.wgpu.device, &pipeline_layout_desc);

    WGPUComputePipelineDescriptor pipeline_desc = {
        .layout = pipeline_layout_obj,
        .compute = compute_stage_desc,
    };

    // Create the compute pipeline
    WGPUComputePipeline pipeline =
        wgpuDeviceCreateComputePipeline(nano_app.wgpu.device, &pipeline_desc);
    if (pipeline != NULL) {
        printf("NANO: Successfully compiled shader %d\n", shader_id);

        // Check if the shader is already in use, if it is, we need to free the
        // old pipeline
        if (nano_app.shader_pool.shaders[slot].occupied) {
            // Free the shader module if it is already in use
            fprintf(stderr, "NANO: Replacing shader %d with %d\n",
                    nano_app.shader_pool.shaders[slot].shader_entry.id,
                    shader_id);
            nano_release_shader(&nano_app.shader_pool, shader_id);
        }

        // Create the shader struct to hold the pipeline and layout
        nano_shader_t shader = {
            .id = shader_id,
            .pipeline = pipeline,
            .pipeline_layout = pipeline_layout,
            .in_use = true,
        };

        // Copy the shader to the shader pool
        memcpy(&nano_app.shader_pool.shaders[slot].shader_entry, &shader,
               sizeof(nano_shader_t));
        nano_app.shader_pool.shader_count++;
    } else {
        fprintf(stderr, "NANO: Failed to create compute pipeline\n");
        wgpuShaderModuleRelease(compute_shader);
        return 0;
    }

    // Release the shader module after creating the compute pipeline
    wgpuShaderModuleRelease(compute_shader);

    return shader_id;
}

// Core Application Functions (init, event, cleanup)
// -------------------------------------------------

// Function called by sokol_app to initialize the application with WebGPU
// sokol_gfx sglue & cimgui
static void nano_default_init(void) {
    printf("Initializing NANO WGPU app...\n");

    // Get the WGPU device from sokol_gfx
    nano_app.wgpu.device = wgpu_get_device();
    nano_app.dimensions = (vec2s){{nano_app.wgpu.width, nano_app.wgpu.width}};

    // Inject CImGui into the WGPU context so we can use it for UI

    // // Initialize sokol_imgui so that we can make calls to cimgui
    // simgui_setup(&(simgui_desc_t){.no_default_font = true});
    //
    // // Get the ImGui IO pointer so we can access the fontatlas
    // ImGuiIO *io = igGetIO();

    // // Iterate through the fonts and add them to the font atlas
    // for (int i = 0; i < NANO_NUM_FONTS; i++) {
    //     nano_app.fonts[i] = ImFontAtlas_AddFontFromMemoryTTF(
    //         io->Fonts, fonts[i]->ttf, fonts[i]->ttf_len, nano_app.font_size,
    //         NULL, NULL);
    // }
    //
    // // Set the default font to the first font in the list (JetBrains Mono
    // Nerd) nano_set_font(nano_app.font_index);

    printf("NANO: Initialized\n");
}

// Free any resources that were allocated
static void nano_default_cleanup(void) {
    // simgui_shutdown();
    wgpu_stop();
}

// The event function is called by sokol_app whenever an event occurs
// and it passes the event to simgui_handle_event to handle the event
static void nano_default_event(const void *e) { /* simgui_handle_event(e); */ }
