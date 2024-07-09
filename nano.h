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
// #include "nano.h"
//
// static void init(void) { nano_default_init(); }
//
// static void frame(void) {
//
//     // Update necessary Nano app state at beginning of frame
//     // Get the current command encoder (this is nano_app.wgpu->cmd_encoder)
//     // A new command encoder is created with each frame
//     WGPUCommandEncoder cmd_encoder = nano_start_frame();
//
//     // Change Nano app state at end of frame
//     nano_end_frame();
// }
//
// static void shutdown(void) { nano_default_cleanup(); }
//
// int main(int argc, char *argv[]) {
//     wgpu_start(&(wgpu_desc_t){
//         .title = "Solid Color Demo",
//         .width = 640,
//         .height = 480,
//         .init_cb = init,
//         .frame_cb = frame,
//         .shutdown_cb = shutdown,
//         .sample_count = 1,
//     });
//     return 0;
// }
// ------------------------------------------------------------------

// TODO: Add a button to reload the shader in the shader pool once it has
//       been successfully validated. This will allow for quick iteration
//       on the shader code.
// TODO: Implement an event queue for input event handing in the future
// TODO: Store the buffer indices in the shader struct for easy access
// TODO: Rewrite the parser to actually use the provided grammar in the WebGPU
//       spec
// TODO: Implement render pipeline shaders into the shader pool

// -------------------------------------------------
//  NANO
// -------------------------------------------------

#define CIMGUI_WGPU
#include "wgpu_entry.h"
#include <stdint.h>
#include <unistd.h>
#include <webgpu/webgpu.h>
#include <wgsl-parser.h>

#ifdef CIMGUI_WGPU
    #define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    #include <cimgui.h>
#endif

#ifdef NANO_CGLM
    // Include cglm for matrix and vector operations
    #include <cglm/struct.h>
#endif

#ifndef NANO_DEBUG
    #define NANO_DEBUG_UI 0
#else
    #define NANO_DEBUG_UI 1
#endif

#define NANO_FAIL -1
#define NANO_OK 0

// Nano Pipeline Declarations
// ---------------------------------------
typedef struct nano_pipeline_layout_t {
    WGPUBindGroupLayout bg_layouts[MAX_GROUPS];
    size_t num_layouts;
} nano_pipeline_layout_t;

// Nano Buffer Declarations
// ---------------------------------------

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

typedef struct BufferNode {
    nano_buffer_t buffer_entry;
    bool occupied;
} BufferNode;

typedef struct nano_buffer_pool_t {
    BufferNode buffers[NANO_MAX_BUFFERS];
    size_t buffer_count;
    size_t total_size;
} nano_buffer_pool_t;

// Nano Shader Declarations
// ----------------------------------------

// Maximum number of compute shaders that can be stored in the shader pool
#define NANO_MAX_SHADERS 16

enum nano_shader_type_t { NANO_SHADER_COMPUTE = 0, NANO_SHADER_RENDER = 1 };

typedef struct nano_shader_t {
    uint32_t id;
    enum nano_shader_type_t type;
    union {
        WGPUComputePipeline compute;
        WGPURenderPipeline render;
    } pipeline;
    nano_pipeline_layout_t pipeline_layout;
    bool in_use;
    char label[64];
    char *source;
    const char *path;
} nano_shader_t;

typedef struct ShaderNode {
    nano_shader_t shader_entry;
    bool occupied;
} ShaderNode;

typedef struct nano_shader_pool_t {
    ShaderNode shaders[NANO_MAX_SHADERS];
    int shader_count;
    // One string for all shader labels used for ImGui combo boxes
    // Only updated when a shader is added or removed from the pool
    char shader_labels[64 * NANO_MAX_SHADERS];
} nano_shader_pool_t;

// Nano Font Declarations
// -------------------------------------------

// Include fonts as header files
#include "JetBrainsMonoNerdFontMono-Bold.h"
#include "LilexNerdFontMono-Medium.h"
#include "Roboto-Regular.h"

// Total number of fonts included in nano
#define NANO_NUM_FONTS 3

typedef struct nano_font_t {
    const unsigned char *ttf;
    size_t ttf_len;
    const char *name;
    ImFont *imfont;
} nano_font_t;

typedef struct nano_font_info_t {
    nano_font_t fonts[NANO_NUM_FONTS];
    uint32_t font_count;
    uint32_t font_index;
    float font_size;
    bool update_font;
} nano_font_info_t;

// Static Nano Font Info Struct to hold GUI fonts
// I will probably implement my own font system in the future but this covers
// imgui for now.
// Leave the imfont pointer for now, we will set this in the init method
// for our imgui fonts
static nano_font_info_t nano_fonts = {
    // Add the fonts we wish to read from our font header files
    .fonts =
        {
            {
                .ttf = JetBrainsMonoNerdFontMono_Bold_ttf,
                .ttf_len = sizeof(JetBrainsMonoNerdFontMono_Bold_ttf),
                .name = "JetBrains Mono Nerd",
            },
            {
                .ttf = LilexNerdFontMono_Medium_ttf,
                .ttf_len = sizeof(LilexNerdFontMono_Medium_ttf),
                .name = "Lilex Nerd Font",
            },
            {
                .ttf = Roboto_Regular_ttf,
                .ttf_len = sizeof(Roboto_Regular_ttf),
                .name = "Roboto",
            },
        },
    .font_count = NANO_NUM_FONTS,
    // Default font index
    .font_index = 0,
    // Default font size
    .font_size = 16.0f,
};

// Nano State Declarations & Static Definition
// -------------------------------------------

typedef WGPURenderPassDescriptor nano_pass_action_t;
typedef wgpu_state_t nano_wgpu_state_t;

// This is the struct that will hold all of the running application data
// In simple terms, this is the state of the application
typedef struct nano_t {
    nano_wgpu_state_t *wgpu;
    bool show_debug;
    float frametime;
    float fps;
    nano_font_info_t font_info;
    nano_pass_action_t pass_action;
    nano_buffer_pool_t buffer_pool;
    nano_buffer_pool_t staging_pool;
    nano_shader_pool_t shader_pool;
} nano_t;

// Initialize a static nano_t struct to hold the running application data
static nano_t nano_app = {
    .wgpu = 0,
    .show_debug = NANO_DEBUG_UI,
    .frametime = 0.0f,
    .fps = 0.0f,
    .font_info = {0},
    .pass_action = {0},
    .buffer_pool = {0},
    .staging_pool = {0},
    .shader_pool = {0},
};

// Misc Functions
// -------------------------------------------------

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
            LOG("NANO: Buffer Added To Pool | shader id: %d, group: %d, "
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
    return shader_id % NANO_MAX_SHADERS;
}

// Initialize the shader pool with the correct default values
// This function should be called before using the shader pool
void nano_init_shader_pool(nano_shader_pool_t *table) {
    assert(table != NULL);
    for (int i = 0; i < NANO_MAX_SHADERS; i++) {
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
    for (int i = 0; i < NANO_MAX_SHADERS; i++) {
        nano_shader_t *shader = &table->shaders[index].shader_entry;
        if (!shader->in_use || shader->id == shader_id) {
            return (int)index;
        }
        index = (index + 1) % NANO_MAX_SHADERS;
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

// Return a string of shader labels for ImGui combo boxes
static int _nano_update_shader_labels() {
    if (nano_app.shader_pool.shader_count == 0) {
        fprintf(stderr, "NANO: nano_get_shader_labels() -> No shaders found\n");
        return NANO_FAIL;
    }

    char labels[64 * NANO_MAX_SHADERS] = "";

    LOG("NANO: Updating shader labels\n");

    // Concatenate all shader labels into a single string
    for (int i = 0; i < NANO_MAX_SHADERS; i++) {
        if (nano_app.shader_pool.shaders[i].occupied) {
            char *label = nano_app.shader_pool.shaders[i].shader_entry.label;
            strncat(labels, label, strlen(label));
            strncat(labels, "\0", 1);
        }
    }

    // Add an extra null terminator to the end of the string
    strncat(labels, "\0", 1);

    // Set the shader labels in the shader pool
    strncpy(nano_app.shader_pool.shader_labels, labels, strlen(labels));

    return NANO_OK;
}

// Empty a shader slot in the shader pool and properly release the shader
void nano_release_shader(nano_shader_pool_t *table, uint32_t shader_id) {
    int index = nano_find_shader_slot(table, shader_id);
    if (index < 0) {
        return;
    }

    nano_shader_t *shader = &table->shaders[index].shader_entry;

    // Make sure the node in the table knows it is no longer occupied
    table->shaders[index].occupied = false;

    // Release the pipeline
    if (shader->pipeline.compute || shader->pipeline.render) {

        if (shader->type == NANO_SHADER_COMPUTE) {
            // Release the compute pipeline
            wgpuComputePipelineRelease(shader->pipeline.compute);
        } else if (shader->type == NANO_SHADER_RENDER) {
            // Release the render pipeline
            wgpuRenderPipelineRelease(shader->pipeline.render);
        }
    }

    // Release the shader shader_source
    if (shader->source) {
        free(shader->source);
    }

    // Create a new empty shader entry at the shader slot
    table->shaders[index].shader_entry = (nano_shader_t){0};

    // Then decrement the shader count
    table->shader_count--;

    _nano_update_shader_labels();
}

// Font Methods
// -----------------------------------------------

// Function to set the font for the ImGui context
void nano_set_font(int index) {
    if (index < 0 || index >= NANO_NUM_FONTS) {
        fprintf(stderr, "NANO: nano_set_font() -> Invalid font index\n");
        return;
    }

    if (&(nano_app.font_info.fonts[index].imfont) == NULL) {
        fprintf(stderr, "NANO: nano_set_font() -> Font is NULL\n");
        return;
    }

    ImGuiIO *io = igGetIO();

    // Set the font as the default font
    io->FontDefault = nano_app.font_info.fonts[index].imfont;

    LOG("NANO: Set font to %s\n",
        nano_app.font_info.fonts[index].imfont->ConfigData->Name);

    // IMPORTANT: FIGURE OUT DPI SCALING FOR WEBGPU
    // io->FontGlobalScale = sapp_dpi_scale();
}

// Function to initialize the fonts for the ImGui context
void nano_init_fonts(nano_font_info_t *font_info, float font_size) {

    ImGuiIO *io = igGetIO();

    // Set the app state font info to the static nano_fonts struct
    // if no font info is provided
    memcpy(&nano_app.font_info, font_info, sizeof(nano_font_info_t));

    // Iterate through the fonts and add them to the font atlas
    for (int i = 0; i < NANO_NUM_FONTS; i++) {
        nano_font_t *cur_font = &nano_app.font_info.fonts[i];
        cur_font->imfont = ImFontAtlas_AddFontFromMemoryTTF(
            io->Fonts, (void *)cur_font->ttf, cur_font->ttf_len, font_size,
            NULL, NULL);
        strncpy((char *)&cur_font->imfont->ConfigData->Name, cur_font->name,
                strlen(cur_font->name));
        LOG("NANO: Added ImGui Font: %s\n", cur_font->imfont->ConfigData->Name);
    }

    // Whenever we reach this point, we can assume that the font size has been
    // updated
    nano_app.font_info.update_font = false;

    // Set the default font to the first font in the list (JetBrains Mono Nerd)
    nano_set_font(nano_app.font_info.font_index);
}

// Function to set the font size for the ImGui context
void nano_set_font_size(float size) {
    nano_init_fonts(&nano_app.font_info, size); // They do the same thing
}

// Stats / Telemetry
// -----------------------------------------------

static int _nano_find_shader_slot_with_index(int index) {
    int count = 0;

    for (int i = 0; i < NANO_MAX_SHADERS; i++) {
        if (nano_app.shader_pool.shaders[i].occupied) {
            count++;
        }

        if (index == (count - 1)) {
            return i;
        }
    }

    return -1;
}

// Aliases for the wgsl-parser structs
typedef ComputeInfo nano_compute_info_t;
typedef GroupInfo nano_group_info_t;
typedef BindingInfo nano_binding_info_t;
typedef WGPUBufferUsageFlags nano_buffer_usage_t;
typedef WGPUBufferDescriptor nano_buffer_desc_t;

// Shader Parsing (using the wgsl-parser library)
// -----------------------------------------------

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
        fprintf(stderr, "NANO: nano_validate_compute_shader() -> Compute "
                        "info is NULL\n");
        return NANO_FAIL;
    }

    // Use my wgsl-parser library to validate the compute shader
    int result = validate_compute((ComputeInfo *)info);
    if (result < 0) {
        fprintf(stderr, "NANO: nano_validate_compute_shader() -> Error "
                        "validating shader\n");
        return NANO_FAIL;
    }

    return NANO_OK;
}

// Return the buffer usage flags for a binding info struct
nano_buffer_usage_t nano_get_wgpu_buffer_usage(nano_binding_info_t *binding) {
    if (binding == NULL) {
        fprintf(stderr,
                "NANO: nano_get_wgpu_buffer_usage() -> Binding is NULL\n");
        return WGPUBufferUsage_None;
    }

    WGPUBufferUsageFlags usage = WGPUBufferUsage_None;

    // Make copy of the usage string so we can tokenize it without ruining
    // the pointer
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

// Generate the buffers and bind group layouts necessary for the pipeline
// layout
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

        // If a group has no bindings, we can break out of the loop early
        // since no other groups should have bindings.
        if (group.num_bindings == 0) {
            break;
        }

        LOG("NANO: Shader %d: Group %d has %d bindings\n", shader_id, i,
            group.num_bindings);

        // If a group has bindings, we must create the buffers for each
        // binding. Once the bindings are created, we can create the bind
        // group layout entry. This will be used to create the bind group
        // layout descriptor. From here we can create the pipeline layout
        // descriptor and the compute pipeline descriptor.

        WGPUBindGroupLayoutEntry bgl_entries[MAX_BINDINGS];

        // BINDING AND BGL CREATION
        for (int j = 0; j < group.num_bindings; j++) {
            nano_binding_info_t binding = group.bindings[j];
            // Check if the binding has a usage to be considered valid
            if (strlen(binding.usage) > 0) {
                WGPUBufferUsageFlags buffer_usage =
                    nano_get_wgpu_buffer_usage(&binding);

                LOG("NANO: Shader %d: Creating new buffer for binding %d "
                    "with usage %s\n",
                    shader_id, binding.binding, binding.usage);

                // Create the buffer in the buffer pool
                // Think about moving all of this to a separate function
                // just for understanding and readability
                nano_buffer_t buffer = nano_create_buffer(
                    nano_app.wgpu->device, &nano_app.buffer_pool, shader_id, i,
                    j, buffer_usage, buffer_size);

                // Set the according bindgroup layout entry for the binding
                bgl_entries[j] = (WGPUBindGroupLayoutEntry){
                    .binding = binding.binding,
                    .visibility = WGPUShaderStage_Compute,
                    // We use a ternary operator to determine the buffer
                    // type by inferring the type from binding usage. If it
                    // is a uniform, we know the binding type is a uniform
                    // buffer. Otherwise, we assume it is a storage buffer.
                    .buffer = {.type = (buffer_usage ==
                                        WGPUBufferBindingType_Uniform)
                                           ? WGPUBufferBindingType_Uniform
                                           : WGPUBufferBindingType_Storage},
                };

                LOG("NANO: Shader %d: Bindgroup layout entry created for "
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

        LOG("NANO: Shader %d: Creating bind group layout for group %d\n",
            shader_id, i);

        // Assign the bind group layout to the bind group layout array
        // so that we can create the WGPUPipelineLayoutDescriptor later.
        bg_layouts[i] = wgpuDeviceCreateBindGroupLayout(nano_app.wgpu->device,
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

    LOG("NANO: Shader %d: Bind group layout created with %zu groups\n",
        shader_id, out_layout->num_layouts);

    return NANO_OK;
}

// Create our nano_shader_t struct to hold the compute shader and pipeline,
// return shader id on success, 0 on failure
// Label optional
uint32_t nano_create_compute_shader(const char *shader_path,
                                    nano_compute_info_t *info,
                                    size_t buffer_size, char *label) {
    if (shader_path == NULL) {
        fprintf(stderr,
                "NANO: nano_create_compute_shader() -> Shader path is NULL\n");
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
        LOG("NANO: Parsed compute shader: %s\n", shader_path);
        LOG("NANO: Shader workgroup size: (%d, %d, %d)\n",
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

    // Shader label
    char default_label[64];
    snprintf(default_label, 64, "Shader %d", shader_id);

    // If the label is NULL, use the default label
    if (label == NULL) {
        label = default_label;
        LOG("NANO: Using default label for shader %d\n", shader_id);
        LOG("NANO: Default label: %s\n", label);
    }

    // This is the main descriptor that will be used to create the compute
    // shader. We must include the WGSL descriptor in the nextInChain field.
    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = (WGPUChainedStruct *)&wgsl_desc,
        .label = (const char *)label,
    };

    LOG("NANO: Creating compute shader with id %d\n", shader_id);

    // Create the shader module for the compute shader and free the source
    WGPUShaderModule compute_shader =
        wgpuDeviceCreateShaderModule(nano_app.wgpu->device, &shader_desc);

    // Create a programmable stage descriptor for the compute shader
    // We essentially declare the compute shader as the module and the
    // entry point as the main function. We can pass this descriptor to
    // the compute pipeline to act as the compute stage.
    WGPUProgrammableStageDescriptor compute_stage_desc = {
        .module = compute_shader,
        .entryPoint = "main",
    };

    // Create the pipeline layout for the shader
    nano_pipeline_layout_t pipeline_layout;
    status = nano_build_pipeline_layout(info, shader_id, buffer_size,
                                        &pipeline_layout);
    if (status != NANO_OK) {
        fprintf(stderr, "NANO: Failed to build pipeline layout for shader %d\n",
                shader_id);
        return 0;
    }

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .bindGroupLayoutCount = pipeline_layout.num_layouts,
        .bindGroupLayouts = pipeline_layout.bg_layouts,
    };

    WGPUPipelineLayout pipeline_layout_obj = wgpuDeviceCreatePipelineLayout(
        nano_app.wgpu->device, &pipeline_layout_desc);

    WGPUComputePipelineDescriptor pipeline_desc = {
        .layout = pipeline_layout_obj,
        .compute = compute_stage_desc,
    };

    // Create the compute pipeline
    WGPUComputePipeline pipeline =
        wgpuDeviceCreateComputePipeline(nano_app.wgpu->device, &pipeline_desc);
    if (pipeline != NULL) {
        LOG("NANO: Successfully compiled shader %d\n", shader_id);

        // Check if the shader is already in use, if it is, we need to free
        // the old pipeline
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
            .type = NANO_SHADER_COMPUTE,
            .pipeline.compute = pipeline,
            .pipeline_layout = pipeline_layout,
            .in_use = true,
            .source = shader_source,
            .path = shader_path,
        };

        // Set the label for the shader
        strncpy(shader.label, label, strlen(label) + 1);

        // Copy the shader to the shader pool node
        memcpy(&nano_app.shader_pool.shaders[slot].shader_entry, &shader,
               sizeof(nano_shader_t));
        nano_app.shader_pool.shaders[slot].occupied = true;
        LOG("NANO: Shader pool slot %d occupied\n", slot);
        nano_app.shader_pool.shader_count++;
    } else {
        fprintf(stderr, "NANO: Failed to create compute pipeline\n");
        wgpuShaderModuleRelease(compute_shader);
        return 0;
    }

    // Release the shader module after creating the compute pipeline
    wgpuShaderModuleRelease(compute_shader);

    // Update the shader labels for the ImGui combo boxes
    _nano_update_shader_labels();

    return shader_id;
}

// Core Application Functions (init, event, cleanup)
// -------------------------------------------------

// Function called by sokol_app to initialize the application with WebGPU
// sokol_gfx sglue & cimgui
static void nano_default_init(void) {
    LOG("Initializing NANO WGPU app...\n");

    // Retrieve the WGPU state from the wgpu_entry.h file
    // This state is created and ready to use when running an
    // application with wgpu_start().
    nano_app.wgpu = wgpu_get_state();

    // Once the device is created, we can create the default pipeline for
    // rendering a simple screen quad with a texture

    // TODO: The buffers and shaders for this pipeline should eventually be
    // added to added to our buffer and shader pools accordingly
    wgpu_init_default_pipeline();

    // Set the fonts
    nano_init_fonts(&nano_fonts, 16.0f);

    LOG("NANO: Initialized\n");
}

// Free any resources that were allocated
static void nano_default_cleanup(void) {

    if (nano_app.shader_pool.shader_count > 0) {
        for (int i = 0; i < NANO_MAX_SHADERS; i++) {
            if (nano_app.shader_pool.shaders[i].occupied) {
                nano_release_shader(
                    &nano_app.shader_pool,
                    nano_app.shader_pool.shaders[i].shader_entry.id);
            }
        }

        // Free the shader shader
        free(nano_app.shader_pool.shader_labels);
    }

    wgpu_stop();
}

// The event function is called by sokol_app whenever an event occurs
// and it passes the event to simgui_handle_event to handle the event
static void nano_default_event(const void *e) { /* simgui_handle_event(e); */ }

// A function that draws a CImGui frame of the current nano_app state
// Include collapsibles for all nested structs
static void nano_draw_debug_ui() {
    bool visible = true;
    bool closed = false;
    WGPUCommandEncoder cmd_encoder = nano_app.wgpu->cmd_encoder;
    static bool show_demo = false;

    // Necessary to call before starting a new frame
    // Will be refactored into a proper nano_cimgui_* function
    ImGui_ImplWGPU_NewFrame();
    igNewFrame();

    igSetNextWindowSize((ImVec2){400, 450}, ImGuiCond_FirstUseEver);
    igBegin("Nano Debug", &nano_app.show_debug, 0);
    if (igButton("Open ImGui Demo Window", (ImVec2){200, 0})) {
        show_demo = !show_demo;
    }

    // Show the ImGui demo window based on the show_demo flag
    if (show_demo)
        igShowDemoWindow(&show_demo);

    if (igCollapsingHeader_BoolPtr("About Nano", &visible,
                                   ImGuiTreeNodeFlags_CollapsingHeader)) {
        igTextWrapped(
            "Nano is a simple solution for starting a new WebGPU based"
            " application. Nano is designed to use "
            "C as its primary programming language. The only exception is "
            "CImGui's bindings to the original C++ implementation of Dear "
            "ImGui, but CImGui works fine. Nano is currently being rebuilt "
            "from the ground up so it is not ready for anything yet.");
        igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);
    }

    // Nano Render Information
    if (igCollapsingHeader_BoolPtr("Nano Render Information", &visible,
                                   ImGuiTreeNodeFlags_CollapsingHeader)) {
        igText("Nano Render Information");
        igSeparator();
        igText("Frame Time: %.2f ms", nano_app.frametime);
        igText("Frames Per Second: %.2f", nano_app.fps);
        igText("Render Resolution: (%d, %d)", nano_app.wgpu->width,
               nano_app.wgpu->height);
        igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);
    }

    // Nano Font Information
    if (igCollapsingHeader_BoolPtr("Nano Font Information", &visible,
                                   ImGuiTreeNodeFlags_CollapsingHeader)) {
        nano_font_info_t *font_info = &nano_app.font_info;
        igText("Font Index: %d", font_info->font_index);
        igText("Font Size: %.2f", font_info->font_size);
        if (igCombo_Str("Select Font", (int *)&font_info->font_index,
                        "JetBrains Mono Nerd Font\0Lilex Nerd Font\0Roboto\0\0",
                        3)) {
            nano_set_font(font_info->font_index);
        }
        igSliderFloat("Font Size", (float *)&font_info->font_size, 8.0f, 32.0f,
                      "%.2f", 1.0f);
        // Once the slider is released, set the flag for editing the font
        // size This requires our render pass to basically be completed
        // before we can do this however. This is a workaround for now.
        if (igIsItemDeactivatedAfterEdit()) {
            nano_app.font_info.update_font = true;
        }
        igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);
    }

    // Shader Pool Information
    if (igCollapsingHeader_BoolPtr("Nano Shader Pool Information", &visible,
                                   ImGuiTreeNodeFlags_CollapsingHeader)) {

        igText("Shader Pool Information");
        igText("Shader Count: %zu", nano_app.shader_pool.shader_count);

        igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);

        if (nano_app.shader_pool.shader_count == 0) {
            igText("No shaders found.\nAdd a shader to inspect it.");
        } else {
            static int shader_index = 0;
            igCombo_Str("Select Shader", &shader_index,
                        (const char *)nano_app.shader_pool.shader_labels, 5);

            int slot = _nano_find_shader_slot_with_index(shader_index);
            if (slot < 0) {
                igText("Error: Shader not found");
            } else {
                char *source =
                    nano_app.shader_pool.shaders[slot].shader_entry.source;
                char *label =
                    nano_app.shader_pool.shaders[slot].shader_entry.label;
                
                // Calculate the size of the input text box based on the
                // visual text size of the shader source
                ImVec2 size = {200, 0};
                igCalcTextSize(&size, source, NULL, false, 0.0f);

                // Add some padding to the size
                size.x += 20;
                size.y += 20;
                
                // Display the shader source in a text box
                igInputTextMultiline(label, source, strlen(source) * 2, size,
                                     ImGuiInputTextFlags_AllowTabInput, NULL,
                                     NULL);

                // Display shader type
                igText("Shader Type: %s",
                        nano_app.shader_pool.shaders[slot].shader_entry.type ==
                                NANO_SHADER_COMPUTE
                            ? "Compute"
                            : "Render");

                if (igButton("Validate Shader", (ImVec2){200, 0})) {
                    nano_compute_info_t *info =
                        nano_parse_compute_shader(source);
                    if (info) {
                        if (nano_validate_compute_shader(info) == NANO_OK) {
                            LOG("NANO: Shader %s has been validated\n", label);
                        } else {
                            LOG("NANO: Shader %s is invalid. Try again.\n",
                                label);
                        }
                    }
                }

                if (igButton("Remove Shader", (ImVec2){200, 0})) {
                    nano_release_shader(&nano_app.shader_pool, shader_index);
                }
            }
        }
        igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);
    }

    // Buffer Pool Information
    if (igCollapsingHeader_BoolPtr("Nano Buffer Pool Information", &visible,
                                   ImGuiTreeNodeFlags_CollapsingHeader)) {
        igText("Shader Buffer Pool Information");
        igText("Buffer Count: %zu", nano_app.buffer_pool.buffer_count);
        igText("Total Size: %zu", nano_app.buffer_pool.total_size);
        igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);
        igText("Staging Buffer Pool Information");
        igText("Buffer Count: %zu", nano_app.staging_pool.buffer_count);
        igText("Total Size: %zu", nano_app.staging_pool.total_size);
        igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);
    }

    igSliderFloat4("RBGA Colour",
                   (float *)&nano_app.wgpu->default_pipeline_info.clear_color,
                   0.0f, 1.0f, "%.2f", 0);

    igEnd();
    igRender();

    float *clear_color =
        (float *)&nano_app.wgpu->default_pipeline_info.clear_color;

    // Write the uniform buffer with the clear color for the frame
    wgpuQueueWriteBuffer(
        wgpuDeviceGetQueue(nano_app.wgpu->device),
        nano_app.wgpu->default_pipeline_info.uniform_buffer, 0,
        &nano_app.wgpu->default_pipeline_info.clear_color,
        sizeof(nano_app.wgpu->default_pipeline_info.clear_color));

    // Set the ImGui encoder to our current encoder
    // This is necessary to render the ImGui draw data
    ImGui_ImplWGPU_SetEncoder(cmd_encoder);

    // --------------------------
    // End of the Dear ImGui frame

    // Get the current swapchain texture view
    WGPUTextureView back_buffer_view = wgpu_get_render_view();
    if (!back_buffer_view) {
        fprintf(stderr, "Failed to get current swapchain texture view.\n");
        return;
    }

    // Create a generic render pass color attachment
    WGPURenderPassColorAttachment color_attachment = {
        .view = back_buffer_view,
        // We set the depth slice to 0xFFFFFFFF to indicate that the depth
        // slice is not used.
        .depthSlice = ~0u,
        // If our view is a texture view (MSAA Samples > 1), we need to
        // resolve the texture to the swapchain texture
        .resolveTarget =
            state.desc.sample_count > 1
                ? wgpuSwapChainGetCurrentTextureView(nano_app.wgpu->swapchain)
                : NULL,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = {.r = clear_color[0],
                       .g = clear_color[1],
                       .b = clear_color[2],
                       .a = clear_color[3]},
    };

    WGPURenderPassDescriptor render_pass_desc = {
        .label = "Nano Debug Render Pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &color_attachment,
        .depthStencilAttachment = NULL,
    };

    WGPURenderPassEncoder render_pass =
        wgpuCommandEncoderBeginRenderPass(cmd_encoder, &render_pass_desc);
    if (!render_pass) {
        fprintf(stderr, "NANO: Failed to begin default render pass encoder.\n");
        wgpuCommandEncoderRelease(cmd_encoder);
        return;
    }

    // Set our render pass encoder to use our pipeline and
    wgpuRenderPassEncoderSetBindGroup(
        render_pass, 0, nano_app.wgpu->default_pipeline_info.bind_group, 0,
        NULL);
    wgpuRenderPassEncoderSetPipeline(
        render_pass, nano_app.wgpu->default_pipeline_info.pipeline);
    wgpuRenderPassEncoderDraw(render_pass, 3, 1, 0, 0);

    // Render ImGui Draw Data
    // This will be refactored into a nano_cimgui_render function
    // I am thinking of making all nano + cimgui functionality optional using
    // macros
    ImGui_ImplWGPU_RenderDrawData(igGetDrawData(), render_pass);

    wgpuRenderPassEncoderEnd(render_pass);
}

// -------------------------------------------------------------------------------
// Nano Frame Update Functions
// nano_start_frame() - Called at the beginning of the frame callback method
// nano_end_frame() - Called at the end of the frame callback method
// These functions are used to update the application state to ensure that the
// app is ready to render the next frame.
// -------------------------------------------------------------------------------

// Calculate current frames per second
static WGPUCommandEncoder nano_start_frame() {

    // Update the dimensions of the window
    nano_app.wgpu->width = wgpu_width();
    nano_app.wgpu->height = wgpu_height();

    // Set the display size for ImGui
    ImGuiIO *io = igGetIO();
    io->DisplaySize =
        (ImVec2){(float)nano_app.wgpu->width, (float)nano_app.wgpu->height};

    // Get the frame time and calculate the frames per second with delta
    // time
    nano_app.frametime = wgpu_frametime();
    // Calculate the frames per second
    nano_app.fps = 1000 / nano_app.frametime;

    // Start the frame and return the command encoder
    // This command encoder should be passed around from
    // function to function to perform multiple render passes
    // We can then present the frame in nano_end_frame() by
    // telling the swapchain to present the frame.
    WGPUCommandEncoderDescriptor cmd_encoder_desc = {
        .label = "Nano Frame Command Encoder",
    };
    // Set the command encoder for the app state
    nano_app.wgpu->cmd_encoder = wgpuDeviceCreateCommandEncoder(
        nano_app.wgpu->device, &cmd_encoder_desc);

    return nano_app.wgpu->cmd_encoder;
}

static void nano_end_frame() {

    // Draw the debug UI
    if (nano_app.show_debug) {
        nano_draw_debug_ui();
        // Release the buffer since we no longer need itebug_ui();
    }

    // Update the font size if the flag is set
    if (nano_app.font_info.update_font) {
        nano_init_fonts(&nano_app.font_info, nano_app.font_info.font_size);
    }

    // Create a command buffer so that we can submit the command encoder
    WGPUCommandBufferDescriptor cmd_buffer_desc = {
        .label = "Command Buffer",
    };

    // Finish the command encoder and submit the command buffer
    WGPUCommandBuffer cmd_buffer =
        wgpuCommandEncoderFinish(nano_app.wgpu->cmd_encoder, &cmd_buffer_desc);
    wgpuQueueSubmit(wgpuDeviceGetQueue(nano_app.wgpu->device), 1, &cmd_buffer);

    // Release the buffer since we no longer need it
    wgpuCommandBufferRelease(cmd_buffer);

    // Release the command encoder after the frame is done
    wgpuCommandEncoderRelease(nano_app.wgpu->cmd_encoder);
}
