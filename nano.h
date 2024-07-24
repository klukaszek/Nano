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

// TODO: Handle binding/buffer initialization.
// TODO: Test compute pipeline creation in the shader pool
// TODO: Test render pipeline creation in the shader pool
// TODO: Move default shader for debug UI into the shader pool
// TODO: Add a button to reload the shader in the shader pool once it has
//       been successfully validated. This will allow for quick iteration
//       on the shader code.
// TODO: Implement an event queue for input event handing in the future

// -------------------------------------------------
//  NANO
// -------------------------------------------------

#include "webgpu.h"
#define CIMGUI_WGPU
#include "wgpu_entry.h"
#include <stdint.h>
#include <stdlib.h>
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
typedef PipelineLayout nano_pipeline_layout_t;

// Nano Buffer Declarations
// ---------------------------------------

// Maximum size of the hash table for each of the buffer pools.
#define NANO_GROUP_MAX_BINDINGS 8

typedef BindingInfo nano_binding_info_t;
typedef WGPUBufferDescriptor nano_buffer_desc_t;

// Nano Entry Point Declarations
// ----------------------------------------
typedef EntryPoint nano_entry_t;

// Nano Shader Declarations
// ----------------------------------------

// Maximum number of compute shaders that can be stored in the shader pool
#define NANO_MAX_SHADERS 16

typedef ShaderType nano_shader_type_t;
typedef ShaderInfo nano_shader_t;

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

// Buffer Functions
// -------------------------------------------------

// Create a Nano WGPU buffer object within a
int nano_create_buffer(nano_binding_info_t *binding, size_t size) {
    // Align the buffer size to the GPU cache line size of 32 bytes
    size_t gpu_cache_line_size = 32;
    size_t cache_aligned_size =
        (size + (gpu_cache_line_size - 1)) & ~(gpu_cache_line_size - 1);

    // Create the buffer descriptor
    WGPUBufferDescriptor desc = {
        .usage = binding->usage_flags,
        .size = cache_aligned_size,
        .mappedAtCreation = false,
    };

    // Create the buffer as part of the binding object
    binding->buffer = wgpuDeviceCreateBuffer(nano_app.wgpu->device, &desc);
    if (binding->buffer == NULL) {
        fprintf(stderr,
                "NANO: nano_create_buffer() -> Could not create buffer\n");
        return NANO_FAIL;
    }

    return NANO_OK;
}

// Get a buffer from the shader info struct using the group and binding
WGPUBuffer* nano_get_buffer(nano_shader_t *info, int group, int binding) {
    if (info == NULL) {
        fprintf(stderr, "NANO: nano_get_buffer() -> Shader info is NULL\n");
        return NULL;
    }
    
    // Retrieve binding index from the group_indices array
    int index = info->group_indices[group][binding];
    if (index == -1) {
        fprintf(stderr, "NANO: nano_get_buffer() -> Binding not found\n");
        return NULL;
    }

    // Return the buffer from the binding info struct as a pointer
    return &info->bindings[index].buffer;
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
        fprintf(stderr,
                "NANO: _nano_update_shader_labels() -> No shaders found\n");
        return NANO_FAIL;
    }

    char labels[64 * NANO_MAX_SHADERS] = "";

    LOG("NANO: Updating shader labels\n");

    // Concatenate all shader labels into a single string
    for (int i = 0; i < NANO_MAX_SHADERS; i++) {
        if (nano_app.shader_pool.shaders[i].occupied) {
            char *label =
                (char *)nano_app.shader_pool.shaders[i].shader_entry.label;
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

    if (shader->source)
        free((void *)shader->source);
    if (shader->label)
        free((void *)shader->label);
    if (shader->path)
        free((void *)shader->path);

    // Release the pipelines if they exist
    if (shader->compute_pipeline)
        wgpuComputePipelineRelease(shader->compute_pipeline);
    if (shader->render_pipeline)
        wgpuRenderPipelineRelease(shader->render_pipeline);

    // Release the shader modules
    if (shader->layout.num_layouts > 0) {
        for (int i = 0; i < shader->layout.num_layouts; i++) {
            wgpuBindGroupLayoutRelease(shader->layout.bg_layouts[i]);
        }
    }

    // Release the buffer objects in the shader bindings
    for (int i = 0; i < shader->binding_count; i++) {
        nano_binding_info_t binding = shader->bindings[i];
        if (binding.buffer)
            wgpuBufferRelease(binding.buffer);
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

// Shader Parsing (using the wgsl-parser library)
// -----------------------------------------------

int nano_parse_shader(char *source, nano_shader_t *shader) {
    if (shader == NULL) {
        fprintf(stderr, "NANO: nano_parse_shader() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    // Initialize the parser with the shader source code
    Parser parser;
    init_parser(&parser, source);

    // Parse the shader to get the entry points and binding information
    // This will populate the shader struct with the necessary information
    // This shader struct will ultimately be used to store the ComputePipeline
    // and PipelineLayout objects.
    parse_shader(&parser, shader);

    // Make sure the shader has at least one entry point
    if (shader->entry_point_count == 0) {
        fprintf(stderr, "NANO: nano_parse_shader() -> Shader parsing failed\n");
        return NANO_FAIL;
    }

    return NANO_OK;
}

// Build the bindings for the shader and populate the group_indices array
// group_indices[group][binding] = index @ binding_info[]
int nano_build_bindings(nano_shader_t *info, size_t buffer_size) {
    if (info == NULL) {
        fprintf(stderr, "NANO: nano_build_bindings() -> Shader info is NULL\n");
        return NANO_FAIL;
    }

    // Iterate through the bindings and assign the index of the binding
    // to the group_indices array
    int binding_count = info->binding_count;

    // This allows us to index our list of bindings by group and binding
    for (int i = 0; i < binding_count; i++) {
        nano_binding_info_t binding = info->bindings[i];
        info->group_indices[binding.group][binding.binding] = i;
    }

    // Iterate over the groups and bindings to create the buffers to be
    // used for the pipeline layout
    for (int i = 0; i < MAX_GROUPS; i++) {
        for (int j = 0; j < MAX_BINDINGS; j++) {

            // If the group index is not -1, we can create the buffer
            // Otherwise, we can break out of the loop early
            if (info->group_indices[i][j] != -1) {
                int index = info->group_indices[i][j];
                nano_binding_info_t *binding = &info->bindings[index];

                // If the buffer usage is not none, create the buffer
                if (binding->usage_flags != WGPUBufferUsage_None) {

                    LOG("NANO: Shader %d: Creating new buffer for "
                        "binding %d "
                        "with type %s\n",
                        info->id, binding->binding, binding->type);

                    // Create the buffer within the shader
                    int status = nano_create_buffer(binding, buffer_size);

                    if (status != NANO_OK) {
                        fprintf(stderr,
                                "NANO: Shader %d: Could not create buffer "
                                "for binding %d\n",
                                info->id, binding->binding);
                        return NANO_FAIL;
                    }
                    // If the buffer usage is none, we can break out of the loop
                } else {
                    fprintf(stderr,
                            "NANO: Shader %d: Binding %d has no usage\n",
                            info->id, binding->binding);
                    return NANO_FAIL;
                }
            }
            // We should technically assume that if we hit -1 then there
            // should be no more bindings left in the group. This means we
            // can break out of the loop early and move on to the next
            // group. This isn't perfect, but I won't have any stupid layouts.
        }
    }

    return NANO_OK;
}

// Get the binding information from the shader info struct as a
// nano_binding_info_t
nano_binding_info_t nano_get_shader_binding(nano_shader_t *info, int group,
                                            int binding) {
    if (info == NULL) {
        fprintf(stderr,
                "NANO: nano_get_shader_binding() -> Shader info is NULL\n");
        return (nano_binding_info_t){0};
    }

    int index = info->group_indices[group][binding];
    if (index == -1) {
        fprintf(stderr,
                "NANO: nano_get_shader_binding() -> Binding not found\n");
        return (nano_binding_info_t){0};
    }

    return info->bindings[index];
}

// Build a pipeline layout for the shader using the bindings in the shader
int nano_build_pipeline_layout(nano_shader_t *info, size_t buffer_size) {
    if (info == NULL) {
        fprintf(stderr,
                "Nano: nano_build_pipeline_layout() -> Compute info is NULL\n");
        return NANO_FAIL;
    }

    int num_groups = 0;

    int binding_count = info->binding_count;

    // If our nano_shader_t struct has no bindings, we can return early
    if (info->binding_count <= 0)
        return NANO_FAIL;

    // Create an array of bind group layouts for each group
    WGPUBindGroupLayout bg_layouts[MAX_GROUPS];
    if (info->binding_count >= MAX_BINDINGS) {
        fprintf(stderr, "NANO: Shader %d: Too many bindings\n", info->id);
        return NANO_FAIL;
    }

    // Initialize the group binding indices array to -1
    for (int i = 0; i < MAX_GROUPS; i++) {
        for (int j = 0; j < MAX_BINDINGS; j++) {
            info->group_indices[i][j] = -1;
        }
    }

    // If a group has bindings, we must create the buffers for each
    // binding. Once the bindings are created, we can create the bind
    // group layout entry. This will be used to create the bind group
    // layout descriptor. From here we can create the pipeline layout
    // descriptor and the compute pipeline descriptor.
    int status = nano_build_bindings(info, buffer_size);
    if (status != NANO_OK) {
        fprintf(stderr, "NANO: Shader %d: Could not build bindings\n",
                info->id);
        return NANO_FAIL;
    }

    // Iterate through the groups and create the bind group layout
    for (int i = 0; i < MAX_GROUPS; i++) {

        // Bind group layout entries for the group
        WGPUBindGroupLayoutEntry bgl_entries[MAX_BINDINGS];
        int num_bindings = 0;

        // Iterate through the bindings in the group and create the bind
        // group layout entries
        for (int j = 0; j < MAX_BINDINGS; j++) {
            int index = info->group_indices[i][j];

            // If the index is -1, we can break out of the loop early since we
            // can assume that there are no more bindings in the group
            if (index == -1) {
                break;
            }

            // Get the binding information from the shaderinfo struct
            nano_binding_info_t binding =
                info->bindings[info->group_indices[i][j]];
            WGPUBufferUsageFlags buffer_usage = binding.usage_flags;

            // Set the according bindgroup layout entry for the
            // binding
            bgl_entries[j] = (WGPUBindGroupLayoutEntry){
                .binding = (uint32_t)binding.binding,
                .visibility = WGPUShaderStage_None,
                // We use a ternary operator to determine the
                // buffer type by inferring the type from
                // binding usage. If it is a uniform, we know
                // the binding type is a uniform buffer.
                // Otherwise, we assume it is a storage buffer.
                .buffer = {.type =
                               (buffer_usage == WGPUBufferBindingType_Uniform)
                                   ? WGPUBufferBindingType_Uniform
                                   : WGPUBufferBindingType_Storage},
            };

            // Set the visibility of the binding based on the
            // available entry point types in the shader
            if (info->entry_indices.compute != -1) {
                bgl_entries[j].visibility |= WGPUShaderStage_Compute;
            } else if (info->entry_indices.vertex != -1) {
                bgl_entries[j].visibility |= WGPUShaderStage_Vertex;
            } else if (info->entry_indices.fragment != -1) {
                bgl_entries[j].visibility |= WGPUShaderStage_Fragment;
            }

            num_bindings++;
        } // Middle level (j) for-loop

        // BINDGROUP LAYOUT CREATION
        WGPUBindGroupLayoutDescriptor bg_layout_desc = {
            .entryCount = (size_t)num_bindings,
            .entries = bgl_entries,
        };

        if (num_bindings != 0) {
            LOG("NANO: Shader %d: Creating bind group layout for group "
                "%d with %d entries\n",
                info->id, num_groups, num_bindings);

            // Assign the bind group layout to the bind group layout
            // array so that we can create the
            // WGPUPipelineLayoutDescriptor later.
            bg_layouts[i] = wgpuDeviceCreateBindGroupLayout(
                nano_app.wgpu->device, &bg_layout_desc);
            if (bg_layouts[i] == NULL) {
                fprintf(stderr,
                        "NANO: Shader %d: Could not create bind group "
                        "layout for group %d\n",
                        info->id, num_groups);
                return NANO_FAIL;
            }

            num_groups++;
        } else {
            // If there are no bindings in the group, we can break out of the
            // loop early
            break;
        }

    } // Top level (i) for-loop

    // Assign the number of layouts to the output layout
    info->layout.num_layouts = num_groups;

    // Copy the bind group layouts to the output layout
    memcpy(info->layout.bg_layouts, &bg_layouts, sizeof(bg_layouts));

    return NANO_OK;
}

// Build the shader pipelines for the shader
// If the shader has multiple entry points, we can build multiple pipelines
// The renderpipeline depends on the vertex and fragment entry points
// The computepipeline depends on the compute entry point
int nano_build_shader_pipelines(nano_shader_t *info) {

    int retval = NANO_OK;
    int compute_index = info->entry_indices.compute;
    int vertex_index = info->entry_indices.vertex;
    int fragment_index = info->entry_indices.fragment;

    // Create the pipeline layout descriptor
    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .bindGroupLayoutCount = info->layout.num_layouts,
        .bindGroupLayouts = info->layout.bg_layouts,
    };

    // Create the pipeline layout object for the wgpu pipeline descriptor
    WGPUPipelineLayout pipeline_layout_obj = wgpuDeviceCreatePipelineLayout(
        nano_app.wgpu->device, &pipeline_layout_desc);

    // Create the compute shader module for WGSL shaders
    WGPUShaderModuleWGSLDescriptor wgsl_desc = {
        .chain =
            {
                .next = NULL,
                .sType = WGPUSType_ShaderModuleWGSLDescriptor,
            },
        .code = info->source,
    };

    // This is the main descriptor that will be used to create the
    // compute shader. We must include the WGSL descriptor in the
    // nextInChain field.
    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = (WGPUChainedStruct *)&wgsl_desc,
        .label = (const char *)info->label,
    };

    // Declare shader modules for the different shader types
    WGPUShaderModule vertex_shader = NULL;
    WGPUShaderModule fragment_shader = NULL;
    WGPUShaderModule compute_shader = NULL;

    // Create the shader module for the different shader types if a valid
    // index exists in the entry indices
    if (compute_index != -1) {
        compute_shader =
            wgpuDeviceCreateShaderModule(nano_app.wgpu->device, &shader_desc);
    } else if (vertex_index != -1) {
        vertex_shader =
            wgpuDeviceCreateShaderModule(nano_app.wgpu->device, &shader_desc);
    } else if (vertex_index != -1) {
        fragment_shader =
            wgpuDeviceCreateShaderModule(nano_app.wgpu->device, &shader_desc);
    }

    // Create the compute pipeline if the compute entry index is valid
    if (compute_index != -1) {
        WGPUComputePipelineDescriptor pipeline_desc = {
            .layout = pipeline_layout_obj,
            .compute =
                {
                    .module = compute_shader,
                    .entryPoint = info->entry_points[compute_index].entry,
                },
        };

        // Set the WGPU pipeline object in our shader info struct
        info->compute_pipeline = wgpuDeviceCreateComputePipeline(
            nano_app.wgpu->device, &pipeline_desc);
    }

    // If both the vertex and fragment entry indices are valid, we can create
    // a render pipeline for the shader
    if (vertex_index != -1 && fragment_index != -1) {

        WGPURenderPipelineDescriptor renderPipelineDesc = {
            .layout = pipeline_layout_obj,
            .vertex =
                {
                    .module = vertex_shader,
                    .entryPoint = info->entry_points[vertex_index].entry,
                    // TODO: Add other vertex attributes here
                },
            .fragment =
                &(WGPUFragmentState){
                    .module = fragment_shader,
                    .entryPoint = info->entry_points[fragment_index].entry,
                    // TODO: Add other fragment attributes here
                },
            // TODO: Add other render pipeline attributes here
        };

        // Assign the render pipeline to the shader info struct
        info->render_pipeline = wgpuDeviceCreateRenderPipeline(
            nano_app.wgpu->device, &renderPipelineDesc);

        // If the vertex or fragment entry indices are valid, but the other is
        // not, we can't create a render pipeline
    } else if (vertex_index != -1 || fragment_index != -1) {
        fprintf(stderr,
                "NANO: Shader %d: Could not create render pipeline. Missing "
                "paired vertex/fragment shader\n",
                info->id);
        retval = NANO_FAIL;
    }

    // Release the shader modules if they are not null
    if (vertex_shader)
        wgpuShaderModuleRelease(vertex_shader);
    if (fragment_shader)
        wgpuShaderModuleRelease(fragment_shader);
    if (compute_shader)
        wgpuShaderModuleRelease(compute_shader);

    return retval;
}

// Find the indices of the entry points in the shader info struct
ShaderIndices nano_precompute_entry_indices(const nano_shader_t *info) {

    // -1 means no entry point found
    ShaderIndices indices = {-1, -1, -1};

    // Iterate through the entry points and find the index of the
    // entry point in the shader info struct
    for (int i = 0; i < info->entry_point_count; i++) {
        switch (info->entry_points[i].type) {
            case VERTEX:
                indices.vertex = i;
                break;
            case FRAGMENT:
                indices.fragment = i;
                break;
            case COMPUTE:
                indices.compute = i;
                break;
            default:
                break;
        }
    }

    return indices;
}

// Create our nano_shader_t struct to hold the compute shader and
// pipeline, return shader id on success, 0 on failure Label
// optional. This only supports WGSL shaders for the time being.
// I'll explore SPIRV at a later time
uint32_t nano_create_shader(const char *shader_path, size_t buffer_size,
                            char *label) {
    if (shader_path == NULL) {
        fprintf(stderr, "NANO: nano_create_shader() -> "
                        "Shader path is NULL\n");
        return 0;
    }

    // Get shader id hash for the compute shader by hashing the
    // shader path
    uint32_t shader_id = nano_hash_shader(shader_path);

    // Find a slot in the shader pool to store the shader
    int slot = nano_find_shader_slot(&nano_app.shader_pool, shader_id);
    if (slot < 0) {
        fprintf(stderr,
                "NANO: Shader pool is full. Could not insert "
                "shader %d\n",
                shader_id);
        return NANO_FAIL;
    }

    // Read the shader code from the file
    char *shader_source = nano_read_file(shader_path);
    if (!shader_source) {
        fprintf(stderr, "Could not read file %s\n", shader_path);
        return NANO_FAIL;
    }

    // If the label is NULL, use the default label
    if (label == NULL) {
        // Shader label
        char default_label[64];
        snprintf(default_label, 64, "Shader %d", shader_id);
        label = default_label;
        LOG("NANO: Using default label for shader %d\n", shader_id);
        LOG("NANO: Default label: %s\n", label);
    }

    // Initialize the shader info struct
    nano_shader_t info = {
        .id = shader_id,
        .source = shader_source,
        .path = shader_path,
        .label = strdup(label),
    };

    // Parse the compute shader to get the workgroup size as well
    // and group layout requirements. These are stored in the info
    // struct.
    int status = nano_parse_shader(shader_source, &info);
    if (status != NANO_OK) {
        fprintf(stderr, "NANO: Failed to parse compute shader: %s\n",
                info.path);
        return NANO_FAIL;
    }

    // Get the entry indices for the shader
    info.entry_indices = nano_precompute_entry_indices(&info);
    int compute_index = info.entry_indices.compute;
    int vertex_index = info.entry_indices.vertex;
    int fragment_index = info.entry_indices.fragment;

    LOG("NANO: Parsed shader: %s\n", info.path);

    // Log entry point information if it exists
    if (compute_index != -1) {
        nano_entry_t *entry = &info.entry_points[compute_index];
        LOG("NANO: Compute shader %u entry point: %s\n", info.id, entry->entry);
        LOG("NANO: Compute shader %u workgroup size: (%d, %d, %d)\n", info.id,
            entry->workgroup_size.x, entry->workgroup_size.y,
            entry->workgroup_size.z);
    }
    if (vertex_index != -1) {
        nano_entry_t *entry = &info.entry_points[vertex_index];
        LOG("NANO: Vertex shader %u entry point: %s\n", info.id, entry->entry);
    }
    if (fragment_index != -1) {
        nano_entry_t *entry = &info.entry_points[fragment_index];
        LOG("NANO: Fragment shader %u entry point: %s\n", info.id,
            entry->entry);
    }

    // Build the pipeline layout
    status = nano_build_pipeline_layout(&info, buffer_size);
    if (status != NANO_OK) {
        fprintf(stderr, "NANO: Failed to build pipeline layout for shader %d\n",
                info.id);
        return 0;
    }

    // Build the shader pipelines
    status = nano_build_shader_pipelines(&info);
    if (status != NANO_OK) {
        fprintf(stderr,
                "NANO: Failed to build shader pipelines for shader %d\n",
                info.id);
        return 0;
    }

    // Add the shader to the shader pool
    memcpy(&nano_app.shader_pool.shaders[slot].shader_entry, &info,
           sizeof(nano_shader_t));
    // Set the slot as occupied
    nano_app.shader_pool.shaders[slot].occupied = true;
    // Increment the shader count
    nano_app.shader_pool.shader_count++;

    // Update the shader labels for the ImGui combo boxes
    _nano_update_shader_labels();

    return shader_id;
}

// Get the compute pipeline from the shader info struct if it exists
WGPUComputePipeline nano_get_compute_pipeline(nano_shader_t *shader) {
    if (shader == NULL) {
        fprintf(stderr, "NANO: nano_get_compute_pipeline() -> Shader is NULL\n");
        return NULL;
    }

    if (shader->compute_pipeline == NULL) {
        fprintf(stderr, "NANO: nano_get_compute_pipeline() -> Compute pipeline not found\n");
        return NULL;
    }

    return shader->compute_pipeline;
}

// Get the render pipeline from the shader info struct if it exists
WGPURenderPipeline nano_get_render_pipeline(nano_shader_t *shader) {
    if (shader == NULL) {
        fprintf(stderr, "NANO: nano_get_render_pipeline() -> Shader is NULL\n");
        return NULL;
    }

    if (shader->render_pipeline == NULL) {
        fprintf(stderr, "NANO: nano_get_render_pipeline() -> Render pipeline not found\n");
        return NULL;
    }

    return shader->render_pipeline;
}

// Core Application Functions (init, event, cleanup)
// -------------------------------------------------

// Function called by sokol_app to initialize the application with
// WebGPU sokol_gfx sglue & cimgui
static void nano_default_init(void) {
    LOG("Initializing NANO WGPU app...\n");

    // Retrieve the WGPU state from the wgpu_entry.h file
    // This state is created and ready to use when running an
    // application with wgpu_start().
    nano_app.wgpu = wgpu_get_state();

    // Once the device is created, we can create the default
    // pipeline for rendering a simple screen quad with a texture

    // TODO: The buffers and shaders for this pipeline should
    // eventually be added to added to our buffer and shader pools
    // accordingly
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

// The event function is called by sokol_app whenever an event
// occurs and it passes the event to simgui_handle_event to handle
// the event
static void nano_default_event(const void *e) { /* simgui_handle_event(e); */ }

// A function that draws a CImGui frame of the current nano_app
// state Include collapsibles for all nested structs
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
    igBegin("Nano Debug", &nano_app.show_debug, ImGuiWindowFlags_None);
    if (igButton("Open ImGui Demo Window", (ImVec2){200, 0})) {
        show_demo = !show_demo;
    }

    // Show the ImGui demo window based on the show_demo flag
    if (show_demo)
        igShowDemoWindow(&show_demo);

    if (igCollapsingHeader_BoolPtr("About Nano", &visible,
                                   ImGuiTreeNodeFlags_CollapsingHeader)) {
        igTextWrapped("Nano is a simple solution for starting a "
                      "new WebGPU based"
                      " application. Nano is designed to use "
                      "C as its primary programming language. The "
                      "only exception is "
                      "CImGui's bindings to the original C++ "
                      "implementation of Dear "
                      "ImGui, but CImGui works fine. Nano is "
                      "currently being rebuilt "
                      "from the ground up so it is not ready for "
                      "anything yet.");
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
                        "JetBrains Mono Nerd Font\0Lilex Nerd "
                        "Font\0Roboto\0\0",
                        3)) {
            nano_set_font(font_info->font_index);
        }
        igSliderFloat("Font Size", (float *)&font_info->font_size, 8.0f, 32.0f,
                      "%.2f", 1.0f);
        // Once the slider is released, set the flag for editing the
        // font size This requires our render pass to basically be
        // completed before we can do this however. This is a
        // workaround for now.
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

                nano_shader_t *shader =
                    &nano_app.shader_pool.shaders[slot].shader_entry;

                char *source = shader->source;
                char *label = (char *)shader->label;

                // Calculate the size of the input text box based on
                // the visual text size of the shader source
                ImVec2 size = {200, 0};
                igCalcTextSize(&size, source, NULL, false, 0.0f);

                // Add some padding to the size
                size.x += 20;
                size.y += 20;

                // Display the shader source in a text box
                igInputTextMultiline(label, source, strlen(source) * 2, size,
                                     ImGuiInputTextFlags_AllowTabInput, NULL,
                                     NULL);

                if (shader->entry_indices.compute != -1 &&
                    shader->entry_indices.vertex != -1 &&
                    shader->entry_indices.fragment != -1) {
                    igText("Shader Types: Compute & Render");
                } else {
                    if (shader->entry_indices.compute != -1) {
                        igText("Shader Type: Compute");
                    } else if (shader->entry_indices.vertex != -1 &&
                               shader->entry_indices.fragment != -1) {
                        igText("Shader Type: Render");
                    }
                }

                if (igButton("Remove Shader", (ImVec2){200, 0})) {
                    nano_release_shader(&nano_app.shader_pool, shader_index);
                }
            }
        }
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
        // We set the depth slice to 0xFFFFFFFF to indicate that the
        // depth slice is not used.
        .depthSlice = ~0u,
        // If our view is a texture view (MSAA Samples > 1), we need
        // to resolve the texture to the swapchain texture
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
    // I am thinking of making all nano + cimgui functionality
    // optional using macros
    ImGui_ImplWGPU_RenderDrawData(igGetDrawData(), render_pass);

    wgpuRenderPassEncoderEnd(render_pass);
}

// -------------------------------------------------------------------------------
// Nano Frame Update Functions
// nano_start_frame() - Called at the beginning of the frame
// callback method nano_end_frame() - Called at the end of the frame
// callback method These functions are used to update the
// application state to ensure that the app is ready to render the
// next frame.
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

    // Get the frame time and calculate the frames per second with
    // delta time
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

    // Create a command buffer so that we can submit the command
    // encoder
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
