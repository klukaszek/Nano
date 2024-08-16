// ---------------------------------------------------------------------
//  nano.h
//  version 0.4.0 (2024-08-14)
//  author: Kyle Lukaszek
//  --------------------------------------------------------------------
//  C WebGPU Application Framework
//  --------------------------------------------------------------------
//
//  C Application framework for creating 2D/3D applications
//  using WebGPU and WGSL. This header file includes the necessary
//  functions and data structures to create a simple application with
//  rendering and compute capabilities.
//
//  Necessary files for Nano:
//  - nano_web.h: Web entry point for Nano
//      OR
//  - nano_native.h: Native entry point for Nano
//
//  Nano comes with extra headers for additional functionality:
//  - nano_cimgui.h: CImGui integration for Nano
//      - Requires cimgui library to be included and linked in the build
//      - Repo: https://github.com/cimgui/cimgui
//
//  These headers can be included by defining the preprocessor definitions:
//  - #define NANO_CIMGUI
//
//  --------------------------------------------------------------------
//
//  To create a new application, simply include this header file, the entry
//  header file (nano_web.h or nano_native.h) in your project, and include a
//  main.c file with the following code:
//
// ```c
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
//     nano_start_app(&(nano_app_desc_t){
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
// ```
//
// ------------------------------------------------------------------

// TODO: Test render pipeline creation in the shader pool
// TODO: Move default shader for debug UI into the shader pool
// TODO: Add a button to reload the shader in the shader pool once it has
//       been successfully validated. This will allow for quick iteration
//       on the shader code.
// TODO: Implement an event queue for input event handing in the future
// TODO: Implement an actual logging system for Nano so that we can build
//       debug logs and a debug console for the application

// -------------------------------------------------
//  NANO
// -------------------------------------------------

#ifndef NANO_H
#define NANO_H

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <webgpu/webgpu.h>

// Use web based entry point for Nano
// if NANO_NATIVE is not defined
#ifndef NANO_NATIVE
    #include "nano_web.h"
#else
    #include "nano_native.h"
#endif

#define NANO_CIMGUI

// Include CImGui for Nano
// Assumes that nano_cimgui.h & cimgui.h are in the include path
// and cimgui should be available as a static or shared library
#ifdef NANO_CIMGUI
    #define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    #include "cimgui/cimgui.h"
    #include "nano_cimgui.h"
#endif

// Nano Debugging
#define LOG_ERR(...) fprintf(stderr, __VA_ARGS__)
#ifndef NANO_DEBUG
    #define NANO_DEBUG_UI 0
    #define LOG(...)
#else
    #define NANO_DEBUG_UI 1
    #define LOG(...) fprintf(stdout, __VA_ARGS__)
#endif

// Total number of fonts included in nano
#define NANO_MAX_FONTS 16
#ifndef NANO_NUM_FONTS
    #define NANO_NUM_FONTS 0
#endif

// Nano Return Codes
#define NANO_FAIL -1
#define NANO_OK 0

// Parser and Shader Definitions
#define NANO_MAX_IDENT_LENGTH 256     // Maximum length of an identifier parsed
#define NANO_MAX_ENTRIES 3            // Compute, Vertex, Fragment
#define NANO_MAX_GROUPS 4             // Maximum number of bind groups
#define NANO_GROUP_MAX_BINDINGS 8     // Maximum number of bindings per group
#define NANO_MAX_VERTEX_BUFFERS 8     // Maximum number of vertex buffers
#define NANO_MAX_VERTEX_ATTRIBUTES 16 // Maximum cumulative vertex attributes

// Maximum number of buffers that can be stored in the buffer pool
#define NANO_MAX_BUFFERS 16

// Maximum number of compute shaders that can be stored in the shader pool
#define NANO_MAX_SHADERS 16

// Generic Array/Stack Implementation Based on old GGYL code
// Only works with simple data types, not structs or pointers
// Useful for working with handles instead of pointers as they
// are more stable and performant.
// ----------------------------------------

// Call this macro to define an array/stack structure and its functions
// The macro will define the following functions:
// - NAME##_init(NAME *s)
// - NAME##_is_empty(NAME *s)
// - NAME##_is_full(NAME *s)
// - NAME##_push(NAME *s, T value)
// - NAME##_pop(NAME *s)
// - NAME##_peek(NAME *s)
// - NAME##_remove(NAME *s, T value)
// - NAME##_print(NAME *s)
// The macro will also define a struct with the given name, an array of type T,
// and a top index. The array of type T will have a maximum size of MAX_SIZE.

#define DEFINE_ARRAY_STACK(T, NAME, MAX_SIZE)                                  \
    typedef struct NAME {                                                      \
        T data[MAX_SIZE];                                                      \
        int top;                                                               \
    } NAME;                                                                    \
                                                                               \
    static inline void NAME##_init(NAME *s) { s->top = -1; }                   \
                                                                               \
    static inline bool NAME##_is_empty(NAME *s) { return s->top == -1; }       \
                                                                               \
    static inline bool NAME##_is_full(NAME *s) {                               \
        return s->top == MAX_SIZE - 1;                                         \
    }                                                                          \
                                                                               \
    static inline void NAME##_push(NAME *s, T value) {                         \
        if (NAME##_is_full(s)) {                                               \
            LOG("Stack is full! Cannot push element.\n");                      \
            return;                                                            \
        }                                                                      \
        s->data[++s->top] = value;                                             \
    }                                                                          \
                                                                               \
    static inline T NAME##_pop(NAME *s) {                                      \
        if (NAME##_is_empty(s)) {                                              \
            LOG("Stack is empty! Cannot pop element.\n");                      \
            T dummy;                                                           \
            return dummy; /* Return a dummy value, handle this in your code */ \
        }                                                                      \
        return s->data[s->top--];                                              \
    }                                                                          \
                                                                               \
    static inline T NAME##_peek(NAME *s) {                                     \
        if (NAME##_is_empty(s)) {                                              \
            LOG("Stack is empty! Cannot peek element.\n");                     \
            T dummy;                                                           \
            return dummy; /* Return a dummy value, handle this in your code */ \
        }                                                                      \
        return s->data[s->top];                                                \
    }                                                                          \
                                                                               \
    static inline void NAME##_remove(NAME *s, T value) {                       \
        if (NAME##_is_empty(s)) {                                              \
            LOG("NANO: Array is empty! Cannot remove element.\n");             \
            return;                                                            \
        }                                                                      \
        int i, j;                                                              \
        for (i = 0; i <= s->top; i++) {                                        \
            if (s->data[i] == value) {                                         \
                for (j = i; j < s->top; j++) {                                 \
                    s->data[j] = s->data[j + 1];                               \
                }                                                              \
                s->top--;                                                      \
                return;                                                        \
            }                                                                  \
        }                                                                      \
        LOG("NANO: Element not found in the array.\n");                        \
    }                                                                          \
                                                                               \
    static inline void NAME##_print(NAME *s) {                                 \
        LOG("Array contents:\n");                                              \
        for (int i = 0; i <= s->top; i++) {                                    \
            LOG("%d ", s->data[i]);                                            \
        }                                                                      \
        LOG("\n");                                                             \
    }

// -------------------------------------------------------------------------------------------

// NANO STRUCT AND ENUM DEFINITIONS

// ---------------------------------------

// Nano WGSL Parser Definitions
// ---------------------------------------

typedef enum {
    NONE,
    COMPUTE,
    VERTEX,
    FRAGMENT,
} wgsl_shader_type;

typedef enum {
    BUFFER,
    TEXTURE,
    STORAGE_TEXTURE,
} wgsl_binding_type;

// typedefs for the necessary binding information
// this is used to create a union for the different binding types
typedef WGPUBufferUsageFlags wgsl_buffer_usage_t;
typedef WGPUSamplerBindingType wgsl_sampler_info_t;
typedef struct {
    WGPUTextureSampleType sample_type;
    WGPUTextureViewDimension view_dimension;
    WGPUTextureUsageFlags usage;
} wgsl_texture_info_t;

typedef struct {
    WGPUStorageTextureAccess access;
    WGPUTextureFormat format;
    WGPUTextureViewDimension view_dimension;
    WGPUTextureUsageFlags usage;
} wgsl_storage_texture_info_t;

// Keep track order of entry points in the shader
typedef struct {
    int8_t vertex;
    int8_t fragment;
    int8_t compute;
} wgsl_shader_indices_t;

// Keep track of the workgroup size for compute shaders
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t z;
} wgsl_workgroup_size_t;

// Nano WGSL Parser Struct
typedef struct {
    const char *input;
    int position;
} nano_wgsl_parser_t;

// Nano Binding & Buffer Declarations
// ---------------------------------------
typedef WGPUBufferDescriptor nano_buffer_desc_t;

// Contains the data for a buffer binding
// If the shader's binding is a buffer and expects data, we store the data here
// See: nano_shader_t->buffer_data
typedef struct {
    uint32_t id;
    WGPUBuffer buffer;
    size_t size;
    uint32_t count;
    size_t offset;
    void *data;
    const char *label;
} nano_buffer_t;

typedef struct {
    WGPUVertexBufferLayout vertex_buffer_layout;
    WGPUBuffer buffer;
    size_t size;
    void *data;
} nano_vertex_buffer_t;

// Represents data that is copied from the GPU to the CPU
// See: nano_copy_buffer_to_cpu()
typedef struct {
    bool locked;
    size_t size;
    WGPUBuffer src;
    size_t src_offset;
    // The destination pointer for the data
    void *data;
    size_t dst_offset;
    WGPUBuffer _staging;
} nano_gpu_data_t;

// Contains the information that is parsed from the shader source
// for each binding in the shader
typedef struct {
    bool in_use;
    size_t size;

    wgsl_binding_type binding_type;

    // Store the reference to whatever the binding is
    union {
        WGPUBuffer buffer;
        WGPUTexture texture;
        WGPUTextureView texture_view;
    } data;

    // Store the information about the binding
    union {
        wgsl_buffer_usage_t buffer_usage;
        wgsl_texture_info_t texture_info;
    } info;

    int group;
    int binding;

    uint32_t shader_id;
    char data_type[32];
    char name[NANO_MAX_IDENT_LENGTH];
} nano_binding_info_t;

// Nano Pipeline Declarations
// ---------------------------------------
typedef struct nano_pipeline_layout_t {
    WGPUBindGroupLayout bg_layouts[NANO_MAX_GROUPS];
    size_t num_layouts;
} nano_pipeline_layout_t;

// Nano Bind Group Declarations
// ----------------------------------------
typedef WGPUBindGroup nano_bind_group_t;

// Nano Entry Point Declarations
// ----------------------------------------

// Each entry point can have a different pipeline
typedef struct {
    char entry[NANO_MAX_IDENT_LENGTH];
    char label[64];
    bool in_use;
    wgsl_shader_type type;
    wgsl_workgroup_size_t workgroup_size;
} nano_entry_t;

// Nano Shader Declarations
// ----------------------------------------

// Each shader can have multiple entry points
// For example, a compute shader, a vertex shader, and a fragment shader
// The bindings defined in the shader are shared between all entry points
// so we store them in the wgpu_shader_info_t struct
// We can then use the binding information to create the bind group layouts for
// each entry point pipeline
typedef struct {

    uint32_t id;
    uint8_t binding_count;
    uint32_t buffer_size;

    char *source;
    char label[64];
    char path[256];

    // Initialized to -1 to indicate that the group is not set
    // Any binding index that is -1 is not used
    // When creating the bind group layouts, we will skip any unused bindings
    int group_indices[NANO_MAX_GROUPS][NANO_GROUP_MAX_BINDINGS];
    nano_binding_info_t bindings[NANO_MAX_ENTRIES];

    uint8_t entry_point_count;
    nano_entry_t entry_points[NANO_MAX_ENTRIES];

    wgsl_shader_indices_t entry_indices;

} wgpu_shader_info_t;

// This is the struct that will hold all of the information for a shader
// loaded into the shader pool.
typedef struct {
    uint32_t id;
    bool in_use;
    bool built;

    wgsl_shader_type type;
    wgpu_shader_info_t info;

    nano_pipeline_layout_t layout;

    // This is where we store buffer data and size so that Nano can
    // create the buffers for the shader bindings when the shader is loaded
    uint32_t buffers[NANO_MAX_GROUPS][NANO_GROUP_MAX_BINDINGS];
    uint32_t uniform_buffer;

    // Vertex Buffers for the vertex shader
    nano_vertex_buffer_t vertex_buffers[NANO_MAX_VERTEX_BUFFERS];
    uint8_t vertex_buffer_count;
    uint8_t vertex_attribute_count;

    uint64_t vertex_count;

    // Using the buffer size from the buffer data, we can create the bindgroups
    // so that we can bind the buffers to the shader pipeline
    nano_bind_group_t bind_groups[NANO_MAX_GROUPS];

    nano_buffer_t *compute_output_buffer;

    WGPUComputePipeline compute_pipeline;
    WGPURenderPipeline render_pipeline;
} nano_shader_t;

// Nano Buffer Pool Declarations
// ----------------------------------------

// Define an array stack for the active buffers to keep track of the indices
DEFINE_ARRAY_STACK(int, nano_buffer_array, NANO_MAX_BUFFERS)

// Node for the buffer pool hash table
typedef struct {
    nano_buffer_t buffer_entry;
    bool occupied;
} nano_buffer_node_t;

// Buffer pool struct to hold all of the buffers for the Nano instance
typedef struct {
    nano_buffer_node_t buffers[NANO_MAX_BUFFERS];
    size_t buffer_count;
    nano_buffer_array active_buffers;
} nano_buffer_pool_t;

// Nano Shader Pool Declarations
// ----------------------------------------

// Define an array stack for the active shaders to keep track of the indices
DEFINE_ARRAY_STACK(int, nano_shader_array, NANO_MAX_SHADERS)

// Node for the shader pool hash table
typedef struct {
    nano_shader_t shader_entry;
    bool occupied;
} nano_shader_node_t;

typedef struct {
    nano_shader_node_t shaders[NANO_MAX_SHADERS];
    size_t shader_count;
    // One string for all shader labels used for ImGui combo boxes
    // Only updated when a shader is added or removed from the pool
    char shader_labels[NANO_MAX_SHADERS * 64];
    nano_shader_array active_shaders;
} nano_shader_pool_t;

// Nano Font Declarations
// -------------------------------------------

typedef struct {
    const unsigned char *ttf;
    size_t ttf_len;
    const char *name;
#ifdef NANO_CIMGUI
    ImFont *imfont;
#endif
} nano_font_t;

typedef struct {
    nano_font_t fonts[NANO_MAX_FONTS];
    uint32_t font_count;
    uint32_t font_index;
    float font_size;
    bool update_fonts;
} nano_font_info_t;

// Static Nano Font Info Struct to hold GUI fonts
// I will probably implement my own font system in the future but this covers
// imgui for now.
static nano_font_info_t nano_fonts = {
    .fonts = {},
    .font_count = NANO_NUM_FONTS,
    .font_index = 0,
    .font_size = 16.0f,
    .update_fonts = false,
};

// Nano Graphics Settings Declarations
// -------------------------------------------\

// MSAA Settings Information for Nano
typedef struct {
    uint8_t sample_count;
    bool msaa_changed;
    uint8_t msaa_values[2];
    const char *msaa_options[2];
    uint8_t msaa_index;
} nano_msaa_settings_t;

// Graphics Settings Information for Nano
typedef struct nano_gfx_settings_t {
    nano_msaa_settings_t msaa;
} nano_gfx_settings_t;

// Nano Settings Declarations
// -------------------------------------------

// General Settings Information for Nano
typedef struct nano_settings_t {
    nano_gfx_settings_t gfx;
} nano_settings_t;

// Nano State Declarations & Static Definition
// -------------------------------------------

// Desc is part of the State struct
// A Desc is initialized at startup and is used to create the State
typedef wgpu_desc_t nano_app_desc_t;

// State contains necessary WGPU information for drawing and computing
typedef wgpu_state_t nano_wgpu_state_t;

// This is the struct that will hold all of the running application data
// In simple terms, this is the state of the application
typedef struct {
    nano_wgpu_state_t *wgpu;
    bool show_debug;
    float frametime;
    float fps;
    nano_font_info_t font_info;
    nano_buffer_pool_t buffer_pool;
    nano_shader_pool_t shader_pool;
    nano_settings_t settings;
} nano_t;

// Initialize a static nano_t struct to hold the running application data
static nano_t nano_app = {
    .wgpu = 0,
    .show_debug = NANO_DEBUG_UI,
    .frametime = 0.0f,
    .fps = 0.0f,
    .font_info = {0},
    .buffer_pool = {0},
    .shader_pool = {0},
    .settings = {0},
};

// Start the Nano application with the given app description
// THIS IS THE MAIN ENTRY POINT FOR NANO
int nano_start_app(nano_app_desc_t *desc) {

    // Call wgpu_start() with the WGPU description
    // wgpu_start() should be defined in nano_web.h or nano_native.h
    wgpu_start((wgpu_desc_t *)desc);

    return NANO_OK;
}

// -------------------------------------------------------------------------------------------

// Nano WGSL Parser Functions
// ---------------------------------------

// Initialize the parser with the shader source
void init_parser(nano_wgsl_parser_t *parser, const char *input) {
    parser->input = input;
    parser->position = 0;
}

// Peek at the next character in the input
char peek(nano_wgsl_parser_t *parser) {
    return parser->input[parser->position];
}

// Get the next character in the input
char next(nano_wgsl_parser_t *parser) {
    return parser->input[parser->position++];
}

// Check if the parser has reached the end of the input
int is_eof(nano_wgsl_parser_t *parser) { return peek(parser) == '\0'; }

// Skip whitespace in the parsed input
void skip_whitespace(nano_wgsl_parser_t *parser) {
    while (isspace(peek(parser))) {
        next(parser);
    }
}

// Parse a number from the parsed input
int parse_number(nano_wgsl_parser_t *parser) {
    int result = 0;
    while (isdigit(peek(parser))) {
        result = result * 10 + (next(parser) - '0');
    }
    return result;
}

// Parse an identifier from the wgsl input
void parse_identifier(nano_wgsl_parser_t *parser, char *ident, bool is_type) {
    int i = 0;
    if (!is_type) {
        while (isalnum(peek(parser)) || peek(parser) == '_') {
            ident[i++] = next(parser);
            if (i >= NANO_MAX_IDENT_LENGTH - 1)
                break;
        }
    } else {
        while (isalnum(peek(parser)) || peek(parser) == '_' ||
               peek(parser) == '<' || peek(parser) == '>') {
            ident[i++] = next(parser);
            if (i >= NANO_MAX_IDENT_LENGTH - 1)
                break;
        }
    }

    ident[i] = '\0';
}

// Parse the storage class and access flags for a buffer binding
WGPUFlags parse_storage_class_and_access(nano_wgsl_parser_t *parser) {
    WGPUFlags flags = WGPUBufferUsage_None;
    char identifier[NANO_MAX_IDENT_LENGTH];

    parse_identifier(parser, identifier, false);

    if (strcmp(identifier, "uniform") == 0) {
        flags |= WGPUBufferUsage_Uniform;
        // It is important to set the copy dst flag for the uniform buffer
        flags |= WGPUBufferUsage_CopyDst;
    } else if (strcmp(identifier, "storage") == 0) {
        flags |= WGPUBufferUsage_Storage;
    }

    skip_whitespace(parser);
    if (peek(parser) == ',') {
        next(parser); // Skip ','
        skip_whitespace(parser);
        parse_identifier(parser, identifier, false);

        if (strcmp(identifier, "read") == 0) {
            flags |= WGPUBufferUsage_CopySrc;
        } else if (strcmp(identifier, "write") == 0) {
            flags |= WGPUBufferUsage_CopyDst;
        } else if (strcmp(identifier, "read_write") == 0) {
            flags |= WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
        }
    }

    return flags;
}

// Determine the type of the binding being parsed
wgsl_binding_type parse_binding_type(nano_wgsl_parser_t *parser) {
    wgsl_binding_type type = BUFFER;
    char identifier[NANO_MAX_IDENT_LENGTH];
    skip_whitespace(parser);
    parse_identifier(parser, identifier, false);

    if (strcmp(identifier, "texture") == 0) {
        type = TEXTURE;
    } else if (strcmp(identifier, "storage_texture") == 0) {
        type = STORAGE_TEXTURE;
    }

    return type;
}

// Parse the binding information from the shader source
void parse_binding(nano_wgsl_parser_t *parser, wgpu_shader_info_t *info) {
    skip_whitespace(parser);
    if (next(parser) != '@' ||
        strncmp(&parser->input[parser->position], "group", 5) != 0) {
        return;
    }
    parser->position += 5;

    skip_whitespace(parser);
    next(parser); // Skip '('
    int group = parse_number(parser);
    next(parser); // Skip ')'

    skip_whitespace(parser);
    if (strncmp(&parser->input[parser->position], "@binding", 8) != 0) {
        return;
    }
    parser->position += 8;

    skip_whitespace(parser);
    next(parser); // Skip '('
    int binding = parse_number(parser);
    next(parser); // Skip ')'

    skip_whitespace(parser);
    if (strncmp(&parser->input[parser->position], "var", 3) != 0) {
        return;
    }
    parser->position += 3;

    skip_whitespace(parser);
    next(parser); // Skip '<'

    WGPUBufferUsageFlags buffer_usage = parse_storage_class_and_access(parser);

    next(parser); // Skip '>'

    skip_whitespace(parser);
    char name[NANO_MAX_IDENT_LENGTH];
    parse_identifier(parser, name, false);

    skip_whitespace(parser);
    next(parser); // Skip ':'

    // Create a new binding info struct and set the default fields
    nano_binding_info_t *bi = &info->bindings[info->binding_count++];
    bi->group = group;
    bi->binding = binding;
    bi->shader_id = info->id;

    // Get binding type so we can parse the correct information
    wgsl_binding_type binding_type = parse_binding_type(parser);

    // Parse the rest of the binding information based on the type
    switch (binding_type) {
        case TEXTURE:
            // parse_texture(parser, bi);
            break;
        case STORAGE_TEXTURE:
            // parse_storage_texture(parser, bi);
            break;
        case BUFFER:
            bi->binding_type = BUFFER;
            bi->info.buffer_usage = buffer_usage;
            parse_identifier(parser, bi->data_type, true);
            break;
    }

    // Save the name of the binding
    strcpy(bi->name, name);
}

// Parse the entry point information from the shader source
void parse_entry_point(nano_wgsl_parser_t *parser, wgpu_shader_info_t *info) {
    skip_whitespace(parser);
    if (next(parser) != '@') {
        return;
    }

    char attr[NANO_MAX_IDENT_LENGTH];
    parse_identifier(parser, attr, false);

    // Check if the attribute is an entry point
    int index = info->entry_point_count;
    nano_entry_t *ep = &info->entry_points[info->entry_point_count++];
    if (strcmp(attr, "compute") == 0) {
        ep->type = COMPUTE;
    } else if (strcmp(attr, "vertex") == 0) {
        ep->type = VERTEX;
    } else if (strcmp(attr, "fragment") == 0) {
        ep->type = FRAGMENT;
    } else {
        info->entry_point_count--;
        return;
    }

    // Parse workgroup size
    skip_whitespace(parser);
    if (strncmp(&parser->input[parser->position], "@workgroup_size", 15) == 0) {
        parser->position += 15;
        skip_whitespace(parser);
        next(parser); // Skip '('
        ep->workgroup_size.x = parse_number(parser);
        if (next(parser) == ',') {
            ep->workgroup_size.y = parse_number(parser);
            if (next(parser) == ',') {
                ep->workgroup_size.z = parse_number(parser);
            }
        }
        next(parser); // Skip ')'
        skip_whitespace(parser);
    }

    // Ensure workgroup size is at least 1
    if (ep->workgroup_size.x == 0)
        ep->workgroup_size.x = 1;
    if (ep->workgroup_size.y == 0)
        ep->workgroup_size.y = 1;
    if (ep->workgroup_size.z == 0)
        ep->workgroup_size.z = 1;

    // Parse entry point name
    if (strncmp(&parser->input[parser->position], "fn", 2) == 0) {
        parser->position += 2;
        skip_whitespace(parser);
        parse_identifier(parser, ep->entry, false);
    }
}

// Top level function to parse the shader source into a wgpu_shader_info_t
// struct
void parse_shader(nano_wgsl_parser_t *parser, wgpu_shader_info_t *info) {
    while (!is_eof(parser)) {
        skip_whitespace(parser);
        if (peek(parser) == '@') {
            int saved_position = parser->position;
            next(parser); // Skip '@'
            char attr[NANO_MAX_IDENT_LENGTH];
            parse_identifier(parser, attr, false);
            parser->position = saved_position;

            if (strcmp(attr, "group") == 0) {
                parse_binding(parser, info);
            } else if (strcmp(attr, "compute") == 0 ||
                       strcmp(attr, "vertex") == 0 ||
                       strcmp(attr, "fragment") == 0) {
                parse_entry_point(parser, info);
            } else {
                next(parser); // Skip unrecognized attribute
            }
        } else {
            next(parser); // Skip other tokens
        }
    }
}

// -------------------------------------------------------------------------------------------

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

// Settings Functions
// -------------------------------------------------

// Return the default Nano graphics settings
nano_gfx_settings_t nano_default_gfx_settings() {
    return (nano_gfx_settings_t){
        .msaa =
            {
                .sample_count = 1,
                .msaa_changed = false,
                .msaa_values = {1, 4},
                .msaa_options = {"Off", "4x MSAA"},
                .msaa_index = 0,
            },
    };
}

// Return the default Nano settings
nano_settings_t nano_default_settings() {
    return (nano_settings_t){
        .gfx = nano_default_gfx_settings(),
    };
}

// File IO
// -----------------------------------------------

// Function to read a shader into a string
char *nano_read_file(const char *filename) {
    assert(filename != NULL);

    FILE *file = fopen(filename, "rb");
    if (!file) {
        LOG_ERR("NANO: nano_read_file() -> Could not open file %s\n", filename);
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
        LOG_ERR("NANO: nano_read_file() -> Memory allocation failed\n");
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

// Initialize the buffer pool with the correct default values
static void nano_init_buffer_pool(nano_buffer_pool_t *pool) {
    assert(pool != NULL);
    LOG("NANO: Initializing buffer pool\n");
    for (int i = 0; i < NANO_MAX_BUFFERS; i++) {
        pool->buffers[i].occupied = false;
    }
    pool->buffer_count = 0;
    nano_buffer_array_init(&pool->active_buffers);
}

// Find an empty buffer slot in the buffer pool using the buffer id
static int nano_find_empty_buffer_slot(nano_buffer_pool_t *pool,
                                       uint32_t buffer_id) {
    uint32_t hash = buffer_id % NANO_MAX_BUFFERS;
    uint32_t index = hash;

    // Linear probing
    for (int i = 0; i < NANO_MAX_BUFFERS; i++) {
        nano_buffer_node_t *node = &pool->buffers[index];
        if (!node->occupied || node->buffer_entry.id == buffer_id) {
            return (int)index;
        }
        index = (index + 1) % NANO_MAX_BUFFERS;
    }

    // If we get here, the buffer pool is full
    return -1;
}

// Find an occupied buffer slot in the buffer pool using the buffer id
int nano_find_buffer_slot(nano_buffer_pool_t *pool, uint32_t buffer_id) {
    uint32_t index = buffer_id % NANO_MAX_BUFFERS;

    // Linear probing
    for (int i = 0; i < NANO_MAX_BUFFERS; i++) {
        nano_buffer_node_t *node = &pool->buffers[index];
        nano_buffer_t *buffer = &node->buffer_entry;
        if (node->occupied && node->buffer_entry.id == buffer_id) {
            return (int)index;
        }
        index = (index + 1) % NANO_MAX_BUFFERS;
    }

    // If we get here, the buffer was not found
    return -1;
}

// Retrieve a buffer from the buffer pool using the buffer id
nano_buffer_t *nano_get_buffer(nano_buffer_pool_t *pool, uint32_t buffer_id) {
    int index = nano_find_buffer_slot(pool, buffer_id);
    if (index < 0) {
        LOG_ERR(
            "NANO: nano_get_buffer() -> Buffer not found in the buffer pool\n");
        return NULL;
    }

    return pool->buffers[index].occupied ? &pool->buffers[index].buffer_entry
                                         : NULL;
}

// Release a buffer from the buffer pool using the buffer id
// This does not free the data associated with the buffer, it just releases
// the WGPU buffer and marks the buffer slot as unoccupied.
// If you used malloc() to allocate the buffer data, you should free it
int nano_release_buffer(nano_buffer_pool_t *pool, uint32_t buffer_id) {
    int index = nano_find_buffer_slot(pool, buffer_id);
    if (index < 0) {
        return NANO_FAIL;
    }

    nano_buffer_t *buffer = &pool->buffers[index].buffer_entry;

    // Release the buffer
    if (buffer->buffer != NULL) {
        wgpuBufferRelease(buffer->buffer);
    }

    // Reset the buffer entry after releasing the buffer
    pool->buffers[index].occupied = false;
    pool->buffer_count--;
    pool->buffers[index].buffer_entry = (nano_buffer_t){0};
    nano_buffer_array_remove(&pool->active_buffers, buffer_id);

    return NANO_OK;
}

// Buffer Functions
// -------------------------------------------------

// Create a Nano WGPU buffer object using a shader binding description
// This buffer is added to the buffer pool and can be retrieved using the
// buffer id. The buffer pool exists so that shaders can share buffers as long
// as they have the same description (these should be wgpu storage buffers)
uint32_t nano_create_buffer(nano_binding_info_t *binding, size_t size,
                            uint32_t count, size_t offset, void *data) {
    if (binding == NULL) {
        LOG_ERR("NANO: nano_create_buffer() -> Binding info is NULL\n");
        return 0;
    }

    if (binding->binding_type != BUFFER) {
        LOG_ERR("NANO: nano_create_buffer() -> Binding type is not a buffer\n");
        return 0;
    }

    // Align the buffer size to the GPU cache line size of 32 bytes
    size_t gpu_cache_line_size = 32;
    size_t cache_aligned_size =
        (size + (gpu_cache_line_size - 1)) & ~(gpu_cache_line_size - 1);

    // Set the binding size to the cache aligned size
    binding->size = cache_aligned_size;

    // Create the buffer descriptor
    WGPUBufferDescriptor desc = {
        .usage = binding->info.buffer_usage,
        .size = cache_aligned_size,
        .mappedAtCreation = false,
    };

    uint32_t buffer_id = fnv1a_32(binding->name);
    LOG("NANO: Creating buffer %s with id %u\n", binding->name, buffer_id);

    // Get a buffer slot from the buffer pool
    int slot = nano_find_empty_buffer_slot(&nano_app.buffer_pool, buffer_id);
    if (slot < 0) {
        LOG_ERR("NANO: nano_create_buffer() -> Buffer pool is full\n");
        return 0;
    }

    // Construct the buffer entry
    nano_buffer_t buffer = {
        .id = buffer_id,
        .buffer = wgpuDeviceCreateBuffer(nano_app.wgpu->device, &desc),
        .size = cache_aligned_size,
        .count = count,
        .offset = offset,
        .data = data,
        .label = binding->name,
    };

    if (buffer.buffer == NULL) {
        LOG_ERR("NANO: nano_create_buffer() -> Could not create buffer\n");
        return 0;
    }

    // Assign the buffer to the binding data field
    binding->data.buffer = buffer.buffer;

    LOG("NANO: Saving buffer to slot %d\n", slot);

    // Memcopy this nano_buffer_t to the buffer pool
    memcpy(&nano_app.buffer_pool.buffers[slot].buffer_entry, &buffer,
           sizeof(nano_buffer_t));

    // Add the slot index to the active buffers array
    nano_buffer_array_push(&nano_app.buffer_pool.active_buffers, slot);
    nano_app.buffer_pool.buffer_count++;
    nano_app.buffer_pool.buffers[slot].occupied = true;

    return buffer_id;
}

// Assign buffer data to a shader so that the buffer can be properly populated
// Used by nano_shader_build_bindgroup() to create the buffers and bindgroups.
// This function should be called before activating a shader.
// Overwrites existing buffer data if it already exists.
int nano_shader_assign_buffer(nano_shader_t *shader, nano_buffer_t *buffer,
                              uint8_t group, uint8_t binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_assign_buffer() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    if (buffer == NULL) {
        LOG_ERR("NANO: nano_shader_assign_buffer() -> Buffer is NULL\n");
        return NANO_FAIL;
    }

    if (group >= NANO_MAX_GROUPS || binding >= NANO_GROUP_MAX_BINDINGS) {
        LOG_ERR("NANO: nano_shader_assign_buffer() -> Group or binding "
                "index out of bounds\n");
        return NANO_FAIL;
    }

    wgpu_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_shader_assign_buffer() -> Shader info is "
                "NULL\n");
        return NANO_FAIL;
    }

    // Check if the binding exists in the shader info struct
    int index = info->group_indices[group][binding];
    if (index == -1) {
        LOG_ERR("NANO: nano_shader_assign_buffer_data() -> Binding not found "
                "in shader %u\n",
                shader->id);
        return NANO_FAIL;
    }

    // Assign the buffer data to the shader
    shader->buffers[group][binding] = buffer->id;

    return NANO_OK;
}

// Assign a pointer to the data we want to continuously copy to the GPU
// The group and binding is important since we need to know where to copy
// the data to in the shader
int nano_shader_assign_uniform_buffer(nano_shader_t *shader,
                                      nano_buffer_t *buffer, uint8_t group,
                                      uint8_t binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_assign_uniform_data() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    if (shader->in_use) {
        LOG_ERR("NANO: nano_shader_assign_uniform_data() -> Shader is "
                "currently in"
                "use. Make sure to assign buffer data before activating a "
                "shader.\n");
        return NANO_FAIL;
    }

    if (buffer == NULL) {
        LOG_ERR("NANO: nano_shader_assign_uniform_data() -> Data is NULL\n");
        return NANO_FAIL;
    }

    if (buffer->data == NULL) {
        LOG_ERR("NANO: nano_shader_assign_uniform_data() -> Buffer data is "
                "NULL\n");
        LOG_ERR("NANO: nano_shader_assign_uniform_data() -> Make sure to "
                "assign data to your uniform buffer.\n");
        return NANO_FAIL;
    }

    // Assign the buffer data to the shader
    int status = nano_shader_assign_buffer(shader, buffer, group, binding);
    if (status == NANO_FAIL) {
        LOG_ERR("NANO: nano_shader_assign_uniform_data() -> Failed to assign "
                "uniform data\n");
        return NANO_FAIL;
    }

    // Assign the uniform buffer to the shader struct
    shader->uniform_buffer = buffer->id;

    return NANO_OK;
}

// Assign an output buffer to a compute shader so that the shader can
// calculate the workgroup sizes based on the number of elements in the
// output buffer. A compute shader should have a single output buffer so
// this function should be called once.
int nano_compute_shader_assign_output_buffer(nano_shader_t *shader,
                                             uint8_t group, uint8_t binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_assign_output_buffer() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    if (shader->in_use) {
        LOG_ERR("NANO: nano_shader_assign_output_buffer() -> Shader is "
                "currently "
                "use. Make sure to assign buffer data before activating a "
                "shader.\n");
        return NANO_FAIL;
    }

    if (group >= NANO_MAX_GROUPS || binding >= NANO_GROUP_MAX_BINDINGS) {
        LOG_ERR("NANO: nano_shader_assign_output_buffer() -> Group or binding "
                "index out of bounds\n");
        return NANO_FAIL;
    }

    // Set the output buffer pointer for the shader
    // This is mostly used for calculating the workgroup sizes
    // for compute shaders. Compute shaders generally have a single
    // output buffer so it is okay to set this to the buffer data.
    uint32_t buffer_id = shader->buffers[group][binding];
    shader->compute_output_buffer =
        nano_get_buffer(&nano_app.buffer_pool, buffer_id);
    if (shader->compute_output_buffer == NULL) {
        LOG_ERR("NANO: nano_shader_assign_output_buffer() -> Buffer not "
                "found\n");
        return NANO_FAIL;
    }

    return NANO_OK;
}

// Add a vertex buffer to the shader. Requires the WGPUVertexAttribute array
// of all attributes and the number of attributes in the array. The size of
// the buffer and the data to be copied to the buffer should also be
// provided.
int nano_shader_create_vertex_buffer(nano_shader_t *shader,
                                     WGPUVertexAttribute *attribs,
                                     uint8_t count, size_t attribStride,
                                     size_t size, void *data) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_assign_vertex_buffer() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    if (shader->in_use) {
        LOG_ERR("NANO: nano_shader_assign_vertex_buffer() -> Shader is "
                "currently "
                "use. Make sure to assign buffer data before activating a "
                "shader.\n");
        return NANO_FAIL;
    }

    if (count == 0) {
        LOG_ERR("NANO: nano_shader_assign_vertex_buffer() -> Count is 0\n");
        return NANO_FAIL;
    }

    if (shader->vertex_buffer_count >= NANO_MAX_VERTEX_BUFFERS) {
        LOG_ERR("NANO: nano_shader_assign_vertex_buffer() -> Maximum number of "
                "vertex buffers reached\n");
        return NANO_FAIL;
    }

    // Get the vertex buffer layout at the current count
    // Create a buffer descriptor for the vertex buffer
    WGPUBufferDescriptor buffer_desc = {
        .label = "Nano Vertex Buffer",
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = size,
        .mappedAtCreation = false,
    };

    // Create the WGPU buffer for the vertex data
    WGPUBuffer buffer =
        wgpuDeviceCreateBuffer(nano_app.wgpu->device, &buffer_desc);
    if (buffer == NULL) {
        LOG_ERR("NANO: nano_shader_assign_vertex_buffer() -> Could not create "
                "GPU Buffer\n");
        return NANO_FAIL;
    }

    // Create the vertex buffer struct and assign the buffer data
    nano_vertex_buffer_t vertex_buffer = {
        .vertex_buffer_layout =
            (WGPUVertexBufferLayout){
                .arrayStride = attribStride,
                .attributeCount = count,
                .attributes = attribs,
            },
        .buffer = buffer,
        .size = size,
        .data = data,
    };

    LOG("NANO: Vertex Buffer Layout -> Array Stride: %zu, Attribute Count: "
        "%d\n",
        attribStride, count);

    LOG("NANO: Vertex Buffer Data Size: %zu\n", size);

    LOG("NANO: nano_shader_assign_vertex_buffer() -> Vertex buffer %d "
        "assigned\n",
        shader->vertex_buffer_count);

    // Copy the vertex buffer to the shader
    memcpy(&shader->vertex_buffers[shader->vertex_buffer_count], &vertex_buffer,
           sizeof(nano_vertex_buffer_t));

    // Since a shader can have multiple vertex buffers, we need to keep
    // track of the total number of vertex attributes in the shader.
    shader->vertex_attribute_count += count;

    // Increment the vertex buffer count
    shader->vertex_buffer_count++;

    return NANO_OK;
}

// Remove a vertex buffer from the shader
// and shift the rest of the vertex buffers down.
int nano_shader_remove_vertex_buffer(nano_shader_t *shader, uint8_t index) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_remove_vertex_buffer() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    if (shader->in_use) {
        LOG_ERR("NANO: nano_shader_remove_vertex_buffer() -> Shader is "
                "currently "
                "use. Make sure to assign buffer data before activating a "
                "shader.\n");
        return NANO_FAIL;
    }

    if (index >= shader->vertex_buffer_count) {
        LOG_ERR("NANO: nano_shader_remove_vertex_buffer() -> Index out of "
                "bounds\n");
        return NANO_FAIL;
    }

    // Erase the buffer data at the index and shift the rest of the data
    for (int i = index; i < shader->vertex_buffer_count - 1; i++) {
        shader->vertex_buffers[i] = shader->vertex_buffers[i + 1];
    }

    shader->vertex_buffer_count--;

    return NANO_OK;
}

// Get a WGPUBuffer pointer from the shader info struct using the group and
// binding
WGPUBuffer nano_get_gpu_buffer(nano_shader_t *shader, uint8_t group,
                               uint8_t binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_gpu_buffer() -> Shader info is NULL\n");
        return NULL;
    }

    wgpu_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_get_gpu_buffer() -> Shader info is NULL\n");
        return NULL;
    }

    // Retrieve binding index from the group_indices array
    int index = info->group_indices[group][binding];
    if (index == -1) {
        LOG_ERR("NANO: nano_get_gpu_buffer() -> Binding not found\n");
        return NULL;
    }

    // Make sure the binding type is not NULL and is BUFFER
    nano_binding_info_t *binding_info = &info->bindings[index];
    if (binding_info == NULL) {
        LOG_ERR("NANO: nano_get_gpu_buffer() -> Binding info is NULL\n");
        return NULL;
    }

    if (binding_info->binding_type != BUFFER) {
        fprintf(stderr, "NANO: nano_get_gpu_buffer() -> Binding type is "
                        "not a buffer\n");
        return NULL;
    }

    // Return the buffer from the binding info struct as a pointer
    return info->bindings[index].data.buffer;
}

// Get the buffer size from the shader info struct using the group and
// binding
size_t nano_get_buffer_size(nano_shader_t *shader, uint8_t group,
                            uint8_t binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_buffer_size() -> Shader info is NULL\n");
        return 0;
    }

    wgpu_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_get_buffer_size() -> Shader info is NULL\n");
        return 0;
    }

    // Retrieve binding index from the group_indices array
    int index = info->group_indices[group][binding];
    if (index == -1) {
        LOG_ERR("NANO: nano_get_buffer_size() -> Binding not found\n");
        return 0;
    }

    // Return the buffer size from the binding info struct
    return info->bindings[index].size;
}

// Copy the contents of one WGPUBuffer to another WGPUBuffer
int nano_copy_buffer_to_buffer(WGPUBuffer src, size_t src_offset,
                               WGPUBuffer dst, size_t dst_offset, size_t size) {
    if (nano_app.wgpu->device == NULL) {
        LOG_ERR("NANO: nano_copy_buffer_to_buffer() -> Device is NULL\n");
        return NANO_FAIL;
    }

    if (src == NULL) {
        LOG_ERR("NANO: nano_copy_buffer_to_buffer() -> Source buffer is "
                "NULL\n");
        return NANO_FAIL;
    }

    if (dst == NULL) {
        LOG_ERR("NANO: nano_copy_buffer_to_buffer() -> Destination buffer is "
                "NULL\n");
        return NANO_FAIL;
    }

    if (size == 0) {
        LOG_ERR("NANO: nano_copy_buffer_to_buffer() -> Size is 0\n");
        return NANO_FAIL;
    }

    WGPUDevice device = nano_app.wgpu->device;

    // Create a new command encoder for the copy operation
    WGPUCommandEncoder copy_encoder =
        wgpuDeviceCreateCommandEncoder(device, NULL);
    wgpuCommandEncoderCopyBufferToBuffer(copy_encoder, src, src_offset, dst,
                                         dst_offset, size);
    WGPUCommandBuffer copy_command_buffer = wgpuCommandEncoderFinish(
        copy_encoder,
        &(WGPUCommandBufferDescriptor){
            .label = "nano_copy_buffer_to_buffer() Command Buffer",
        });
    wgpuQueueSubmit(wgpuDeviceGetQueue(device), 1, &copy_command_buffer);

    wgpuCommandBufferRelease(copy_command_buffer);
    wgpuCommandEncoderRelease(copy_encoder);

    return NANO_OK;
}

// Write data to a buffer object using the WGPU API
void nano_write_buffer(nano_buffer_t *buffer, size_t offset, void *data,
                       size_t size) {
    if (buffer == NULL) {
        LOG_ERR("NANO: nano_write_buffer() -> Buffer is NULL\n");
        return;
    }
    if (data == NULL) {
        LOG_ERR("NANO: nano_write_buffer() -> Data is NULL\n");
        return;
    }
    if (size == 0) {
        LOG_ERR("NANO: nano_write_buffer() -> Size is 0\n");
        return;
    }

    WGPUQueue queue = wgpuDeviceGetQueue(nano_app.wgpu->device);
    wgpuQueueWriteBuffer(queue, buffer->buffer, offset, data, size);

    LOG("NANO: Wrote To WGPU Buffer %p / %s\n", buffer, buffer->label ? buffer->label : "Unnamed");
}

// Callback to handle the mapped data
void nano_map_read_callback(WGPUBufferMapAsyncStatus status, void *userdata) {
    if (userdata == NULL) {
        LOG_ERR("NANO: nano_map_read_callback() -> Userdata is NULL\n");
        return;
    }

    // If the buffer mapping was successful, copy the data to the CPU
    if (status == WGPUBufferMapAsyncStatus_Success) {
        nano_gpu_data_t *data = (nano_gpu_data_t *)userdata;
        const void *mapped_data = wgpuBufferGetConstMappedRange(
            data->_staging, data->dst_offset, data->size);

        // Copy buffer const mapped range as a void pointer
        memcpy(data->data, mapped_data, data->size);

        // Unmap the buffer after the copy operation is complete
        wgpuBufferUnmap(data->_staging);

        // Copy success!
        LOG("NANO: Copied %zu byte buffer to CPU\n", data->size);

        // Release the staging buffer after the copy operation is complete
        wgpuBufferRelease(data->_staging);

        // Lock the data so that it will not be overwritten
        // until the developer copies the data out from the
        // struct and the dev calls nano_release_gpu_copy()
        data->locked = true;
    } else {
        LOG("NANO: Failed to map buffer for reading.\n");
    }
}

// Copy the contents of a GPU buffer to the CPU
// This is achieved using a staging buffer to read the data back to the CPU
int nano_copy_buffer_to_cpu(nano_gpu_data_t *data,
                            WGPUBufferDescriptor *staging_desc) {
    if (data == NULL) {
        LOG_ERR("NANO: nano_copy_buffer_to_cpu() -> Data is NULL\n");
        return NANO_FAIL;
    }
    if (data->src == NULL) {
        LOG_ERR("NANO: nano_copy_buffer_to_cpu() -> Source buffer is NULL\n");
        return NANO_FAIL;
    }

    WGPUDevice device = nano_app.wgpu->device;
    // If the staging descriptor is NULL, we need to create a new staging
    // buffer with default settings
    if (staging_desc == NULL) {
        // Create the staging buffer to read results back to the CPU
        data->_staging = wgpuDeviceCreateBuffer(
            device,
            &(WGPUBufferDescriptor){
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
                .size = data->size,
                .mappedAtCreation = false,
            });
    } else {
        // Create the staging buffer with the provided descriptor
        data->_staging = wgpuDeviceCreateBuffer(device, staging_desc);
    }

    int status =
        nano_copy_buffer_to_buffer(data->src, data->src_offset, data->_staging,
                                   data->dst_offset, data->size);
    if (status != NANO_OK) {
        LOG_ERR("NANO: nano_copy_buffer_to_cpu() -> Copy buffer to buffer "
                "failed\n");
        return NANO_FAIL;
    }

    // Make sure to set the locked state to false before mapping the buffer
    // This lets the Nano know that the copy operation is not complete
    data->locked = false;
    data->data = malloc(data->size);

    // Map the staging buffer for reading asynchronously
    wgpuBufferMapAsync(data->_staging, WGPUMapMode_Read, 0, data->size,
                       nano_map_read_callback, (void *)data);

    return NANO_OK;
}

// Release unnecessary buffer resources after copying data to the CPU
// Once we copy the data out from this struct, we can free it
int nano_release_gpu_copy(nano_gpu_data_t *data) {
    if (data == NULL) {
        LOG_ERR("NANO: nano_free_gpu_data() -> Data is NULL\n");
        return NANO_FAIL;
    }

    if (data->data)
        free(data->data);

    // Set shader status to false so that we can copy data again
    // if needed
    data->locked = false;

    return NANO_OK;
}

// Shader Pool Functions
// -------------------------------------------------

// Initialize the shader pool with the correct default values
// This function should be called before using the shader pool
static void nano_init_shader_pool(nano_shader_pool_t *table) {
    assert(table != NULL);
    LOG("NANO: Initializing shader pool\n");
    for (int i = 0; i < NANO_MAX_SHADERS; i++) {
        table->shaders[i].occupied = false;
    }
    table->shader_count = 0;
    nano_shader_array_init(&table->active_shaders);
}

// Find a shader slot in the shader pool using the shader id
static int nano_find_shader_slot(nano_shader_pool_t *table,
                                 uint32_t shader_id) {
    uint32_t index = shader_id % NANO_MAX_SHADERS;

    // Linear probing
    for (int i = 0; i < NANO_MAX_SHADERS; i++) {
        nano_shader_node_t *node = &table->shaders[index];
        nano_shader_t *shader = &node->shader_entry;
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
        LOG_ERR("NANO: _nano_update_shader_labels() -> No shaders found\n");
        return NANO_FAIL;
    }

    LOG("NANO: Updating shader labels\n");

    char labels[NANO_MAX_SHADERS * 64] = {0};
    strncpy(labels, "", 1);

    // Concatenate all shader labels into a single string
    for (int i = 0; i < NANO_MAX_SHADERS; i++) {
        if (nano_app.shader_pool.shaders[i].occupied) {
            char *label =
                (char *)nano_app.shader_pool.shaders[i].shader_entry.info.label;
            strncat(labels, label, strlen(label) + 1);
            // Separate the labels with a ? as a placeholder for ImGui \0
            // terminators
            strncat(labels, "?", 1);
        }
    }

    // Keep a copy of the length of the concatenated labels
    // since once the "?" are replaced with null terminators
    // strlen will not work properly
    size_t len = strlen(labels);

    // Replace the "?" with null terminators
    for (int i = 0; i < len; i++) {
        if (labels[i] == '?') {
            labels[i] = '\0';
        }
    }

    // Copy the concatenated labels to the shader pool labels
    // Use memcpy to copy the null terminators as well
    memcpy(nano_app.shader_pool.shader_labels, labels, len);

    return NANO_OK;
}

// Empty a shader slot in the shader pool and properly release the shader
void nano_shader_release(nano_shader_pool_t *table, uint32_t shader_id) {
    int index = nano_find_shader_slot(table, shader_id);
    if (index < 0) {
        return;
    }

    nano_shader_t *shader = &table->shaders[index].shader_entry;

    // Make sure the node in the table knows it is no longer occupied
    table->shaders[index].occupied = false;

    if (shader->info.source)
        free((void *)shader->info.source);

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
    for (int i = 0; i < shader->info.binding_count; i++) {
        nano_binding_info_t binding = shader->info.bindings[i];
        if (binding.data.buffer)
            wgpuBufferRelease(binding.data.buffer);
    }

    // Release the vertex buffers if they exist
    for (int i = 0; i < shader->vertex_buffer_count; i++) {
        nano_vertex_buffer_t *vertex_buffer = &shader->vertex_buffers[i];
        if (vertex_buffer->buffer)
            wgpuBufferRelease(vertex_buffer->buffer);
    }

    // Create a new empty shader entry at the shader slot
    table->shaders[index].shader_entry = (nano_shader_t){0};

    // Then decrement the shader count
    table->shader_count--;

    _nano_update_shader_labels();
}

// Binding functions
// -------------------------------------------------

// Retrieve a binding from the shader info struct regardless of binding type
nano_binding_info_t *nano_shader_get_binding(nano_shader_t *shader, int group,
                                             int binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_get_binding() -> Shader info is NULL\n");
        return NULL;
    }

    wgpu_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_shader_get_binding() -> Shader info is NULL\n");
        return NULL;
    }

    int index = info->group_indices[group][binding];
    if (index == -1) {
        LOG_ERR("NANO: nano_shader_get_binding() -> Binding not found\n");
        return NULL;
    }

    return &info->bindings[index];
}

// Retrieve a binding from the shader info struct by the name
// This is useful when working with a shader that we already know the
// bindings of.
nano_binding_info_t *nano_get_binding_by_name(nano_shader_t *shader,
                                              const char *name) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_binding_by_name() -> Shader info is NULL\n");
        return NULL;
    }

    wgpu_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_get_binding_by_name() -> Shader info is NULL\n");
        return NULL;
    }

    for (int i = 0; i < info->binding_count; i++) {
        nano_binding_info_t binding = info->bindings[i];
        if (strcmp(binding.name, name) == 0) {
            return &info->bindings[i];
        }
    }

    LOG_ERR("NANO: nano_get_binding_by_name() -> Binding \"%s\" not found\n",
            name);
    return NULL;
}

// Font Methods
// -----------------------------------------------

// Load fonts into the Nano application
// This overwrites the existing fonts in the Nano app from 0..font_count
// You should have an array of nano_font_t structs with the font data
// prepared before calling this function.
int nano_load_fonts(nano_font_t *fonts, int font_count, float font_size) {
    if (fonts == NULL) {
        LOG_ERR("NANO: nano_load_fonts() -> Fonts are NULL\n");
        return NANO_FAIL;
    }

    if (font_count == 0) {
        LOG_ERR("NANO: nano_load_fonts() -> No fonts to load\n");
        return NANO_FAIL;
    }

    nano_app.font_info.font_count = font_count;
    nano_app.font_info.font_size = font_size;

    // Copy the fonts to the Nano app font info
    // This will overwrite any existing fonts
    memcpy(nano_fonts.fonts, fonts, sizeof(nano_font_t) * font_count);

    nano_app.font_info.update_fonts = true;

    return NANO_OK;
}

// Function to set the font for the ImGui context
void nano_set_font(int index) {
    if (index < 0 || index >= NANO_MAX_FONTS) {
        LOG_ERR("NANO: nano_set_font() -> Invalid font index\n");
        return;
    }

    LOG("NANO: Setting font to %s\n", nano_app.font_info.fonts[index].name);

// Handle the font setting for cimgui
#ifdef NANO_CIMGUI
    if (&(nano_app.font_info.fonts[index].imfont) == NULL) {
        LOG_ERR("NANO: nano_set_font() -> Font is NULL\n");
        return;
    }

    ImGuiIO *io = igGetIO();

    // Set the font as the default font
    io->FontDefault = nano_app.font_info.fonts[index].imfont;

    LOG("NANO: Set font to %s\n",
        nano_app.font_info.fonts[index].imfont->ConfigData->Name);
#endif
}

// Function to initialize the fonts for Nano and the ImGui context
void nano_init_fonts(nano_font_info_t *font_info, float font_size) {
    if (font_info->font_count == 0) {
        LOG("NANO: nano_init_fonts() -> No Custom Fonts Assigned: Using "
            "Default ImGui Font.\n\t\tDid you forget to define "
            "NANO_NUM_FONTS "
            "> 0?");
        return;
    }

    // Set the app state font info to whatever is passed in
    memcpy(&nano_app.font_info, font_info, sizeof(nano_font_info_t));

// We only care about the ImFont if we are using cimgui
#ifdef NANO_CIMGUI

    ImGuiIO *io = igGetIO();

    // Make sure to clear the font atlas so we do not leak memory each
    // time we have to update the fonts for rendering.
    ImFontAtlas_Clear(io->Fonts);

    // Iterate through the fonts and add them to the font atlas
    for (int i = 0; i < font_info->font_count; i++) {
        nano_font_t *cur_font = &nano_app.font_info.fonts[i];
        cur_font->imfont = ImFontAtlas_AddFontFromMemoryTTF(
            io->Fonts, (void *)cur_font->ttf, cur_font->ttf_len, font_size,
            NULL, NULL);
        strncpy((char *)&cur_font->imfont->ConfigData->Name, cur_font->name,
                strlen(cur_font->name));
        LOG("NANO: Added ImGui Font: %s\n", cur_font->imfont->ConfigData->Name);
    }
#endif

    // Whenever we reach this point, we can assume that the font size has
    // been updated
    nano_app.font_info.update_fonts = false;

    // Set the default font to the first font in the list (JetBrains Mono
    // Nerd)
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

// Shader Building
// -----------------------------------------------

int nano_parse_shader(char *source, nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_parse_shader() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    // Initialize the parser with the shader source code
    nano_wgsl_parser_t parser;
    init_parser(&parser, source);

    shader->info.binding_count = 0;
    shader->info.entry_point_count = 0;

    // Parse the shader to get the entry points and binding information
    // This will populate the shader struct with the necessary information
    // This shader struct will ultimately be used to store the
    // ComputePipeline and PipelineLayout objects.
    parse_shader(&parser, &shader->info);

    // Make sure the shader has at least one entry point
    if (shader->info.entry_point_count == 0) {
        LOG_ERR("NANO: nano_parse_shader() -> Shader parsing failed\n");
        return NANO_FAIL;
    }

    return NANO_OK;
}

// Build the bindings for the shader and populate the group_indices array
// group_indices[group][binding] = index @ binding_info[]
int nano_build_bindings(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_build_bindings() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    wgpu_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_build_bindings() -> Shader info is NULL\n");
        return NANO_FAIL;
    }

    // Iterate through the bindings and assign the index of the binding
    // to the group_indices array
    int binding_count = info->binding_count;

    // If there are no bindings, we can return early
    if (binding_count == 0) {
        return NANO_OK;
    }

    // This allows us to index our list of bindings by group and binding
    for (int i = 0; i < binding_count; i++) {
        nano_binding_info_t binding = info->bindings[i];
        info->group_indices[binding.group][binding.binding] = i;
    }

    return NANO_OK;
}

// Build a pipeline layout for the shader using the bindings in the shader
// a shader must have buffer data assigned to it before calling this
// function. The buffer data can be assigned using
// nano_shader_assign_buffer_data(). If no buffer data is assigned, but
// buffer data is expected, the method will return NANO_FAIL.
int nano_build_pipeline_layout(nano_shader_t *shader) {

    if (shader == NULL) {
        LOG_ERR("Nano: nano_build_pipeline_layout() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    wgpu_shader_info_t *info = &shader->info;

    if (info == NULL) {
        LOG_ERR("Nano: nano_build_pipeline_layout() -> Compute info is NULL\n");
        return NANO_FAIL;
    }

    int num_groups = 0;

    // Create an array of bind group layouts for each group
    WGPUBindGroupLayout bg_layouts[NANO_MAX_GROUPS];
    if (info->binding_count >= NANO_MAX_GROUPS * NANO_GROUP_MAX_BINDINGS) {
        LOG_ERR("NANO: Shader %u: Too many bindings\n", info->id);
        return NANO_FAIL;
    }

    // Initialize the group binding indices array to -1
    for (int i = 0; i < NANO_MAX_GROUPS; i++) {
        for (int j = 0; j < NANO_GROUP_MAX_BINDINGS; j++) {
            info->group_indices[i][j] = -1;
        }
    }

    // If a group has bindings, we must mark the bindings in the group
    // to be non-negative
    int status = nano_build_bindings(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Shader %u: Could not build bindings\n", info->id);
        return NANO_FAIL;
    }

    // Iterate through the groups and create the bind group layout
    for (int i = 0; i < NANO_MAX_GROUPS; i++) {

        // Bind group layout entries for the group
        WGPUBindGroupLayoutEntry bgl_entries[NANO_GROUP_MAX_BINDINGS];
        int num_bindings = 0;

        // Iterate through the bindings in the group and create the bind
        // group layout entries
        for (int j = 0; j < NANO_GROUP_MAX_BINDINGS; j++) {
            int index = info->group_indices[i][j];

            // If the index is -1, we can break out of the loop early since
            // we can assume that there are no more bindings in the group
            if (index == -1) {
                LOG("NANO: Shader %u: No more bindings in group %d\n", info->id,
                    i);
                break;
            }

            // Get the binding information from the shaderinfo struct
            nano_binding_info_t *binding =
                &info->bindings[info->group_indices[i][j]];
            WGPUBufferUsageFlags buffer_usage = binding->info.buffer_usage;

            // Set the according bindgroup layout entry for the
            // binding
            bgl_entries[j] = (WGPUBindGroupLayoutEntry){
                .binding = (uint32_t)binding->binding,
                // Temporarily set the visibility to none
                .visibility = WGPUShaderStage_None,
                // We use a ternary operator to determine the
                // buffer type by inferring the type from
                // binding usage. If it is a uniform, we know
                // the binding type is a uniform buffer.
                // Otherwise, we assume it is a storage buffer.
                .buffer = {.type = (buffer_usage & WGPUBufferUsage_Uniform) != 0
                                       ? WGPUBufferBindingType_Uniform
                                       : WGPUBufferBindingType_Storage},
            };

            // Set the visibility of the binding based on the
            // available entry point types in the shader
            if (info->entry_indices.compute != -1) {
                bgl_entries[j].visibility |= WGPUShaderStage_Compute;
            }
            if (info->entry_indices.fragment != -1) {
                bgl_entries[j].visibility |= WGPUShaderStage_Fragment;
            }
            if (info->entry_indices.vertex != -1) {
                bgl_entries[j].visibility |= WGPUShaderStage_Vertex;
            }

            num_bindings++;
        } // Middle level (j) for-loop

        // BINDGROUP LAYOUT CREATION
        WGPUBindGroupLayoutDescriptor bg_layout_desc = {
            .entryCount = (size_t)num_bindings,
            .entries = bgl_entries,
        };

        if (num_bindings != 0) {
            LOG("NANO: Shader %u: Creating bind group layout for group "
                "%d with %d entries\n",
                info->id, num_groups, num_bindings);

            // Assign the bind group layout to the bind group layout
            // array so that we can create the
            // WGPUPipelineLayoutDescriptor later.
            bg_layouts[i] = wgpuDeviceCreateBindGroupLayout(
                nano_app.wgpu->device, &bg_layout_desc);
            if (bg_layouts[i] == NULL) {
                LOG_ERR("NANO: Shader %u: Could not create bind group "
                        "layout for group %d\n",
                        info->id, num_groups);
                return NANO_FAIL;
            }

            num_groups++;
        } else {
            // If there are no bindings in the group, we can break out of
            // the loop early
            break;
        }

    } // Top level (i) for-loop

    // Assign the number of layouts to the output layout
    shader->layout.num_layouts = num_groups;
    LOG("NANO: Shader %u: Created %d bind group layouts\n", info->id,
        num_groups);

    // Copy the bind group layouts to the output layout
    memcpy(shader->layout.bg_layouts, &bg_layouts, sizeof(bg_layouts));

    return NANO_OK;
}

// Build the shader pipelines for the shader
// If the shader has multiple entry points, we can build multiple pipelines
// The renderpipeline depends on the vertex and fragment entry points
// The computepipeline depends on the compute entry point
int nano_build_shader_pipelines(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_build_shader_pipelines() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    wgpu_shader_info_t *info = &shader->info;

    int retval = NANO_OK;
    int compute_index = info->entry_indices.compute;
    int vertex_index = info->entry_indices.vertex;
    int fragment_index = info->entry_indices.fragment;

    // Create the pipeline layout descriptor
    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .label = (const char *)info->label,
        .bindGroupLayoutCount = shader->layout.num_layouts,
        .bindGroupLayouts = shader->layout.bg_layouts,
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
    WGPUShaderModule shader_module =
        wgpuDeviceCreateShaderModule(nano_app.wgpu->device, &shader_desc);

    // Create the compute pipeline if the compute entry index is valid
    if (compute_index != -1) {

        WGPUComputePipelineDescriptor pipeline_desc = {
            .layout = pipeline_layout_obj,
            .compute =
                {
                    .module = shader_module,
                    .entryPoint = info->entry_points[compute_index].entry,
                },
        };

        if (shader->compute_pipeline) {
            wgpuComputePipelineRelease(shader->compute_pipeline);
        }

        // Set the WGPU pipeline object in our shader info struct
        shader->compute_pipeline = wgpuDeviceCreateComputePipeline(
            nano_app.wgpu->device, &pipeline_desc);
    }

    // If both the vertex and fragment entry indices are valid, we can
    // create a render pipeline for the shader
    if (vertex_index != -1 && fragment_index != -1) {

        WGPURenderPipelineDescriptor renderPipelineDesc = {
            .label = (const char *)info->label,
            .layout = pipeline_layout_obj,
            .vertex =
                {
                    .module = shader_module,
                    .entryPoint = info->entry_points[vertex_index].entry,
                    .bufferCount = shader->vertex_buffer_count,
                    .buffers = &shader->vertex_buffers[0].vertex_buffer_layout,
                },
            .primitive = {.topology = WGPUPrimitiveTopology_TriangleList,
                          .stripIndexFormat = WGPUIndexFormat_Undefined,
                          .frontFace = WGPUFrontFace_CCW,
                          .cullMode = WGPUCullMode_None},
            .multisample =
                (WGPUMultisampleState){
                    .count = nano_app.settings.gfx.msaa.sample_count,
                    .mask = ~0u,
                    .alphaToCoverageEnabled = false,
                },
            .fragment =
                &(WGPUFragmentState){
                    .module = shader_module,
                    .entryPoint = info->entry_points[fragment_index].entry,
                    .targetCount = 1,
                    .targets =
                        &(WGPUColorTargetState){
                            .format = wgpu_get_color_format(),
                            .blend =
                                &(WGPUBlendState){
                                    .color =
                                        (WGPUBlendComponent){
                                            .operation = WGPUBlendOperation_Add,
                                            .srcFactor = WGPUBlendFactor_One,
                                            .dstFactor = WGPUBlendFactor_One,
                                        },
                                    .alpha =
                                        (WGPUBlendComponent){
                                            .operation = WGPUBlendOperation_Add,
                                            .srcFactor = WGPUBlendFactor_One,
                                            .dstFactor = WGPUBlendFactor_One,
                                        },
                                },
                            .writeMask = WGPUColorWriteMask_All,
                        },
                },
            // TODO: Add other render pipeline attributes here
        };

        if (shader->render_pipeline) {
            wgpuRenderPipelineRelease(shader->render_pipeline);
        }
        // Assign the render pipeline to the shader info struct
        shader->render_pipeline = wgpuDeviceCreateRenderPipeline(
            nano_app.wgpu->device, &renderPipelineDesc);

        // Write the vertex buffer data to the GPU so it can be used in the
        // shader pipeline
        WGPUQueue queue = wgpuDeviceGetQueue(nano_app.wgpu->device);

        // Write the vertex buffer data to the GPU
        for (int i = 0; i < shader->vertex_buffer_count; i++) {
            nano_vertex_buffer_t *vertex_buffer = &shader->vertex_buffers[i];
            size_t size = vertex_buffer->size;
            void *data = vertex_buffer->data;
            wgpuQueueWriteBuffer(queue, vertex_buffer->buffer, 0, data, size);
            LOG("NANO: Shader %u: Wrote vertex buffer data to GPU buffer "
                "%p\n",
                info->id, vertex_buffer->buffer);
        }
        LOG("NANO: Shader %u: Created Render Pipeline\n", info->id);

        // If the vertex or fragment entry indices are valid, but the other
        // is not, we can't create a render pipeline
    } else if (vertex_index != -1 || fragment_index != -1) {
        LOG_ERR("NANO: Shader %u: Could not create render pipeline. Missing "
                "paired vertex/fragment shader\n",
                info->id);
        retval = NANO_FAIL;
    }

    // Release the shader modules if they are not null
    if (shader_module)
        wgpuShaderModuleRelease(shader_module);

    return retval;
}

// Called by nano_validate_shader() to build the bind groups for the shader
// Do not call this function directly
int nano_build_bindgroups(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_build_bindgroups() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    // Return early if the shader has no layouts
    int num_layouts = shader->layout.num_layouts;
    if (shader->layout.num_layouts <= 0) {
        return NANO_OK;
    }

    if (shader->in_use) {
        fprintf(stderr, "NANO: nano_build_bindgroups() -> Shader is "
                        "currently in use\n");
        return NANO_FAIL;
    }

    wgpu_shader_info_t *info = &shader->info;

    if (info == NULL) {
        LOG_ERR("NANO: nano_build_bindgroups() -> Shader info is NULL\n");
        return NANO_FAIL;
    }

    WGPUBindGroupEntry *bg_entries[num_layouts];

    // Iterate through the groups and create the bind groups
    for (int i = 0; i < NANO_MAX_GROUPS; i++) {

        bool group_empty = true;
        uint32_t count = 0;

        for (int j = 0; j < NANO_GROUP_MAX_BINDINGS; j++) {

            // If a binding is empty, we can break out of the loop early
            // and assume that there are no more bindings in the group
            if (info->group_indices[i][j] == -1) {
                break;
            } else {
                group_empty = false;
                count++;
            }
        }

        // If the group is empty, we can break out of the loop early
        // and assume that there are no more groups to build
        if (group_empty) {
            break;
        }

        // Otherwise we create a BindGroupEntry list for each binding in the
        // group
        WGPUBindGroupEntry bg_entry[count];

        // Iterate through the bindings in the group and create the bind
        // group. Remember: when defining bindings, make sure to not skip
        // numbers. Always increase the binding number by 1, or increase the
        // group number by 1 and return to 0.
        for (int j = 0; j < count; j++) {
            int index = info->group_indices[i][j];
            nano_binding_info_t binding = info->bindings[index];

            // If the buffer usage is not none, we can create the bindgroup
            // entry
            // TODO: Add support for textures and samplers as bindings
            if (binding.info.buffer_usage != WGPUBufferUsage_None) {
                // Retrieve the buffer id from the shader info struct
                uint32_t buffer_id = shader->buffers[i][j];
                // Retrieve the buffer from the buffer pool using the id
                nano_buffer_t *buffer =
                    nano_get_buffer(&nano_app.buffer_pool, buffer_id);
                if (buffer == NULL) {
                    LOG_ERR("NANO: Shader %u: Could not find buffer %u\n",
                            info->id, buffer_id);
                    return NANO_FAIL;
                }

                // Create the bind group entry for the buffer
                bg_entry[j] = (WGPUBindGroupEntry){
                    .binding = (uint32_t)binding.binding,
                    .buffer = buffer->buffer,
                    .offset = buffer->offset,
                    .size = buffer->size,
                };
            }
        }

        WGPUBindGroupDescriptor bg_desc = {
            .layout = shader->layout.bg_layouts[i],
            .entryCount = count,
            .entries = bg_entry,
        };

        if (shader->bind_groups[i] != NULL) {
            wgpuBindGroupRelease(shader->bind_groups[i]);
        }

        shader->bind_groups[i] =
            wgpuDeviceCreateBindGroup(nano_app.wgpu->device, &bg_desc);
        if (shader->bind_groups[i] == NULL) {
            fprintf(stderr,
                    "NANO: Shader %u: Could not create bind group for "
                    "group %d\n",
                    info->id, i);
            return NANO_FAIL;
        }
    }

    return NANO_OK;
}

WGPUBindGroup nano_get_bindgroup(nano_shader_t *shader, int group) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_bindgroup() -> Shader is NULL\n");
        return NULL;
    }

    if (group < 0 || group >= NANO_MAX_GROUPS) {
        LOG_ERR("NANO: nano_get_bindgroup() -> Invalid group\n");
        return NULL;
    }

    return shader->bind_groups[group];
}

// Find the indices of the entry points in the shader info struct
wgsl_shader_indices_t nano_precompute_entry_indices(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_precompute_entry_indices() -> Shader is NULL\n");
        return (wgsl_shader_indices_t){-1, -1, -1};
    }

    wgpu_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_precompute_entry_indices() -> Shader info is "
                "NULL\n");
        return (wgsl_shader_indices_t){-1, -1, -1};
    }

    // -1 means no entry point found
    wgsl_shader_indices_t indices = {-1, -1, -1};

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

// Shader Functions
// -------------------------------------------------

// validate a shader to ensure that it is ready to be used
int nano_validate_shader(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_validate_shader() -> Shader is NULL\n");
        return NANO_FAIL;
    }
    if (shader->in_use) {
        LOG_ERR("NANO: Shader %u is currently in use\n", shader->id);
        return NANO_FAIL;
    }

    wgpu_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_reload_shader() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    LOG("NANO: Validating shader %u\n", info->id);

    // Parse the compute shader to get the workgroup size as well
    // and group layout requirements. These are stored in the info
    // struct.
    int status = nano_parse_shader(info->source, shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to parse compute shader: %s\n", info->path);
        return NANO_FAIL;
    }

    // Get the entry indices for the shader
    info->entry_indices = nano_precompute_entry_indices(shader);
    int compute_index = info->entry_indices.compute;
    int vertex_index = info->entry_indices.vertex;
    int fragment_index = info->entry_indices.fragment;

    // Log entry point information if it exists
    if (compute_index != -1) {
        nano_entry_t *entry = &info->entry_points[compute_index];
        LOG("NANO: Compute shader %u entry point: %s\n", info->id,
            entry->entry);
        LOG("NANO: Compute shader %u workgroup size: (%d, %d, %d)\n", info->id,
            entry->workgroup_size.x, entry->workgroup_size.y,
            entry->workgroup_size.z);
    }
    if (vertex_index != -1) {
        nano_entry_t *entry = &info->entry_points[vertex_index];
        LOG("NANO: Vertex shader %u entry point: %s\n", info->id, entry->entry);
    }
    if (fragment_index != -1) {
        nano_entry_t *entry = &info->entry_points[fragment_index];
        LOG("NANO: Fragment shader %u entry point: %s\n", info->id,
            entry->entry);
    }

    // Once we reach this point, we can assume that the shader has been
    // successfully validated and reflects the shader source code.
    // If anything goes wrong, the shader will technically not be able to be
    // used until the shader is successfully validated.

    return NANO_OK;
}

// Create our nano_shader_t struct to hold the compute shader and
// pipeline, return shader id on success, 0 on failure Label
// optional. This only supports WGSL shaders for the time being.
// I'll explore SPIRV at a later time
uint32_t nano_create_shader(const char *shader_source, const char *label) {
    if (shader_source == NULL) {
        LOG_ERR("NANO: nano_create_shader() -> "
                "Shader source is NULL\n");
        return 0;
    }

    // Get shader id hash for the compute shader by hashing the
    // shader path
    uint32_t shader_id = nano_hash_shader(shader_source);

    // Find a slot in the shader pool to store the shader
    int slot = nano_find_shader_slot(&nano_app.shader_pool, shader_id);
    if (slot < 0) {
        LOG_ERR("NANO: Shader pool is full. Could not insert "
                "shader %u\n",
                shader_id);
        return NANO_FAIL;
    }

    // If the label is NULL, use the default label
    if (label == NULL) {
        // Shader label
        char default_label[64];
        snprintf(default_label, 64, "Shader %u", shader_id);
        label = default_label;
        LOG("NANO: Using default label for shader %u\n", shader_id);
        LOG("NANO: Default label: %s\n", label);
    }

    // Initialize the shader info struct
    wgpu_shader_info_t info = {
        .id = shader_id,
        .source = strdup(shader_source),
    };

    strncpy((char *)&info.label, label, strlen(label) + 1);

    nano_shader_t shader = (nano_shader_t){
        .id = shader_id,
        .info = info,
        .vertex_count = 3, // Default vertex count is 3 for a triangle
    };

    // Parse the compute shader to get the workgroup size as well
    // and group layout requirements. These are stored in the info
    // struct.
    int status = nano_validate_shader(&shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to validate shader %u\n", shader_id);
        return NANO_FAIL;
    }

    // Add the shader to the shader pool
    memcpy(&nano_app.shader_pool.shaders[slot].shader_entry, &shader,
           sizeof(nano_shader_t));
    // Set the slot as occupied
    nano_app.shader_pool.shaders[slot].occupied = true;

    // Increment the shader count
    nano_app.shader_pool.shader_count++;

    LOG("NANO: Successfully Created Shader -> %u\n", shader_id);

    // Update the shader labels for the ImGui combo boxes
    _nano_update_shader_labels();

    return shader_id;
}

// Create a shader from a file path
uint32_t nano_create_shader_from_file(const char *path, const char *label) {
    if (path == NULL) {
        LOG_ERR("NANO: nano_create_shader_from_file() -> "
                "Shader path is NULL\n");
        return 0;
    }

    // Read the shader source from the file
    char *source = nano_read_file(path);
    if (source == NULL) {
        LOG_ERR("NANO: nano_create_shader_from_file() -> "
                "Could not read shader source\n");
        return 0;
    }

    // Create the shader from the source
    uint32_t shader_id = nano_create_shader(source, label);
    nano_shader_t *shader = nano_get_shader(&nano_app.shader_pool, shader_id);

    // Set the path of the shader
    strncpy((char *)&shader->info.path, path, strlen(path) + 1);

    return shader_id;
}

int nano_shader_set_vertex_count(nano_shader_t *shader, uint32_t count) {
    shader->vertex_count = count;
    return NANO_OK;
}

// Build the bindings, pipeline layout, bindgroups, and pipelines for the
// shader Can be called by the developer to build the shader manually, or
// can be called as part of the activation process as a boolean parameter.
int nano_shader_build(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_build() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    LOG("NANO: Building shader %u...\n", shader->id);

    // Verify that the shader can be parsed properly before building it
    int status = nano_validate_shader(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to validate shader %u\n", shader->id);
        return NANO_FAIL;
    }

    // Use a nano_buffer_data_t struct to store metadata about each buffer
    // Once the developer has added the buffer information to the shader
    // struct, we can call this method to make sure the shader is ready to
    // be executed.
    LOG("NANO: Building bindings and pipeline layouts for shader %u...\n",
        shader->id);

    // Build the pipeline layout
    status = nano_build_pipeline_layout(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to build pipeline layout for shader %u\n",
                shader->info.id);
        return status;
    }

    LOG("NANO: Building bindgroups for shader %u...\n", shader->id);

    // Build bind groups for the shader so we can bind the buffers
    status = nano_build_bindgroups(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to build bindgroup for shader %u\n",
                shader->info.id);
        return status;
    }

    LOG("NANO: Building pipelines for shader %u...\n", shader->id);

    // Build the shader pipelines
    status = nano_build_shader_pipelines(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to build shader pipelines for shader %u\n",
                shader->info.id);
        return 0;
    }

    // Set the built flag to true so we don't have to rebuild the shader
    // unless we explicitly want to by passing the build flag
    shader->built = true;

    return NANO_OK;
}

// Set a shader to the active state.
// Build bindings, pipeline layout, bindgroups, and pipelines
int nano_shader_activate(nano_shader_t *shader, bool build) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_activate() -> Shader is NULL\n");
        return NANO_FAIL;
    }
    if (shader->in_use) {
        LOG_ERR("NANO: Shader %u is already active\n", shader->id);
        return NANO_OK;
    }

    // If the shader has not been built, we build it now
    // If the build flag is set, we rebuild the shader
    if (!shader->built || build) {
        int status = nano_shader_build(shader);
        if (status != NANO_OK) {
            LOG_ERR("NANO: Failed to build shader %u\n", shader->id);
            return NANO_FAIL;
        }
    }

    LOG("NANO: Shader %u: Activating...\n", shader->id);

    // If the shader is not active, we can activate it
    shader->in_use = true;

    // Add the shader to the active shaders list
    nano_shader_array_push(&nano_app.shader_pool.active_shaders, shader->id);

    LOG("NANO: Shader %u: Activated\n", shader->id);

    return NANO_OK;
}

// Deactivate a shader from the active list
int nano_shader_deactivate(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_deactivate() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    // If the shader is not active, return early
    if (!shader->in_use) {
        return NANO_OK;
    }

    // If the shader is active, we can deactivate it
    shader->in_use = false;

    // Remove the shader from the active shaders list
    nano_shader_array_remove(&nano_app.shader_pool.active_shaders, shader->id);

    _nano_update_shader_labels();

    return NANO_OK;
}

// Return shader id from the shader pool using the index
int nano_get_active_shader_id(nano_shader_pool_t *table, int index) {
    if (index < 0 || index >= table->active_shaders.top + 1) {
        LOG_ERR("NANO: nano_get_active_shader_id() -> Invalid index\n");
        return 0;
    }

    return table->active_shaders.data[index];
}

// Check if a shader is active
bool nano_is_shader_active(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_is_shader_active() -> Shader is NULL\n");
        return false;
    }

    return shader->in_use;
}

// Get the number of active shaders
int nano_num_active_shaders(nano_shader_pool_t *table) {
    return table->active_shaders.top + 1;
}

// Return a string of shader pipeline type for ImGui
const char *nano_get_shader_type_str(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_shader_type_str() -> Shader is NULL\n");
        return "Unknown";
    }
    wgpu_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_get_shader_type_str() -> Shader info is NULL\n");
        return "Unknown";
    }
    return (info->entry_indices.compute != -1 &&
            info->entry_indices.vertex != -1 &&
            info->entry_indices.fragment != -1)
               ? "Compute & Render"
           : (info->entry_indices.compute != -1) ? "Compute"
                                                 : "Render";
}

// Get the compute pipeline from the shader info struct if it exists
WGPUComputePipeline nano_get_compute_pipeline(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_compute_pipeline() -> Shader is NULL\n");
        return NULL;
    }

    if (shader->compute_pipeline == NULL) {
        LOG_ERR("NANO: nano_get_compute_pipeline() -> Compute pipeline "
                "not found\n");
        return NULL;
    }

    return shader->compute_pipeline;
}

// Get the render pipeline from the shader info struct if it exists
WGPURenderPipeline nano_get_render_pipeline(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_render_pipeline() -> Shader is NULL\n");
        return NULL;
    }

    if (shader->render_pipeline == NULL) {
        fprintf(stderr, "NANO: nano_get_render_pipeline() -> Render "
                        "pipeline not found\n");
        return NULL;
    }

    return shader->render_pipeline;
}

// Execute a shader pass for a single shader based on the
// pipelines, and the bindgroups.
// To access the data from the shader execution, we can use
// nano_copy_buffer_to_cpu(...) to copy the data from the GPU buffer
// to a struct in memory.
void nano_shader_execute(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_execute() -> Shader is NULL\n");
        return;
    }
    if (!shader->in_use) {
        LOG_ERR("NANO: nano_shader_execute() -> Shader %u is not active\n",
                shader->id);
        return;
    }

    WGPUQueue queue = wgpuDeviceGetQueue(nano_app.wgpu->device);

    // Update the uniform buffer data for the shader if it exists
    if (shader->uniform_buffer != 0) {
        nano_buffer_t *buffer =
            nano_get_buffer(&nano_app.buffer_pool, shader->uniform_buffer);
        if (buffer == NULL) {
            LOG_ERR("NANO: Shader %u: Could not find uniform buffer %u\n",
                    shader->id, shader->uniform_buffer);
            return;
        }
        nano_write_buffer(buffer, buffer->offset, buffer->data, buffer->size);
    }

    // Find the compute shader in the shader info entry points list
    for (int i = 0; i < shader->info.entry_point_count; i++) {
        // We handle compute shaders separately from vertex and fragment
        if (shader->info.entry_points[i].type == COMPUTE) {

            if (shader->compute_pipeline == NULL) {
                LOG_ERR("NANO: Shader %u: Compute pipeline is NULL\n",
                        shader->id);
                return;
            }

            // Create a new command encoder for the compute pass
            // We create a new command encoder for each compute pass
            // to ensure that the compute shader is dispatched separately
            WGPUCommandEncoder command_encoder =
                wgpuDeviceCreateCommandEncoder(nano_app.wgpu->device, NULL);

            // Begin a compute pass to execute compute shader
            WGPUComputePassEncoder compute_pass =
                wgpuCommandEncoderBeginComputePass(command_encoder, NULL);
            wgpuComputePassEncoderSetPipeline(compute_pass,
                                              shader->compute_pipeline);

            // Set the bind groups for the compute pass
            for (int j = 0; j < shader->layout.num_layouts; j++) {
                wgpuComputePassEncoderSetBindGroup(
                    compute_pass, j, nano_get_bindgroup(shader, j), 0, NULL);
            }

            // Get the workgroup size from the shader info
            wgsl_workgroup_size_t workgroup_sizes =
                shader->info.entry_points[i].workgroup_size;

            // Calculate the workgroup size
            size_t workgroup_size =
                workgroup_sizes.x * workgroup_sizes.y * workgroup_sizes.z;

            // Calculate the number of workgroups based on the output buffer
            // size
            size_t num_workgroups =
                (shader->compute_output_buffer->count + workgroup_size - 1) /
                workgroup_size;

            // TODO: ADJUST WORKGROUP SIZE BASED ON SIZE OF OUTPUT BUFFER
            wgpuComputePassEncoderDispatchWorkgroups(compute_pass,
                                                     num_workgroups, 1, 1);

            // Finish the compute pass
            wgpuComputePassEncoderEnd(compute_pass);

            // submit the command buffer that contains the compute
            WGPUCommandBuffer command_buffer =
                wgpuCommandEncoderFinish(command_encoder, NULL);
            wgpuQueueSubmit(queue, 1, &command_buffer);

            // Release the command encoder after we submit to the queue
            wgpuCommandEncoderRelease(command_encoder);

        } else if (shader->info.entry_points[i].type == VERTEX ||
                   shader->info.entry_points[i].type == FRAGMENT) {

            // Get the command encoder for the current nano pass
            WGPUCommandEncoder command_encoder = nano_app.wgpu->cmd_encoder;

            // If a shader has a vertex and fragment entry point, we can
            // queue a render pass here.
            // If the shader has both, this is only called once to make
            // sure the same render pass is not queued multiple times.
            WGPURenderPassDescriptor render_pass_desc = {
                .colorAttachmentCount = 1,
                .colorAttachments =
                    &(WGPURenderPassColorAttachment){
                        .view = wgpu_get_render_view(),
                        .depthSlice = ~0u,
                        .resolveTarget = wgpu_get_resolve_view(),
                        .loadOp = WGPULoadOp_Load,
                        .storeOp = WGPUStoreOp_Store,
                    },
                .depthStencilAttachment = NULL,
            };

            WGPURenderPassEncoder render_pass =
                wgpuCommandEncoderBeginRenderPass(command_encoder,
                                                  &render_pass_desc);
            wgpuRenderPassEncoderSetPipeline(render_pass,
                                             shader->render_pipeline);

            // Assign the bind groups for the render pass
            for (int j = 0; j < shader->layout.num_layouts; j++) {
                wgpuRenderPassEncoderSetBindGroup(
                    render_pass, j, nano_get_bindgroup(shader, j), 0, NULL);
            }

            // Assign the vertex buffers for the render pass
            for (int j = 0; j < shader->vertex_buffer_count; j++) {
                nano_vertex_buffer_t *vtx = &shader->vertex_buffers[j];
                wgpuRenderPassEncoderSetVertexBuffer(render_pass, j,
                                                     vtx->buffer, 0, vtx->size);
            }

            // Draw the vertex buffer
            wgpuRenderPassEncoderDraw(render_pass, shader->vertex_count, 1, 0,
                                      0);
            wgpuRenderPassEncoderEnd(render_pass);

            // Break out of the loop early since the only remaining
            // entry point is the fragment shader which is already compiled
            // We don't want to end this command encoder early since it is
            // shared for drawn shaders. A compute shader gets its own
            // command so that it is dispatched separately.
            break;
        }
    }
}

// Iterate over all active shaders in the shader pool and execute each
// shader with the appropriate bindgroups and pipelines loaded into the GPU.
void nano_execute_shaders(void) {

    WGPUQueue queue = wgpuDeviceGetQueue(nano_app.wgpu->device);

    // Iterate over all active shaders in the shader pool
    int num_active_shaders = nano_num_active_shaders(&nano_app.shader_pool);
    for (int i = 0; i < num_active_shaders; i++) {
        uint32_t shader_id =
            nano_get_active_shader_id(&nano_app.shader_pool, i);
        nano_shader_t *shader =
            nano_get_shader(&nano_app.shader_pool, shader_id);
        if (shader == NULL) {
            LOG_ERR("NANO: Shader %u is NULL\n", shader_id);
            continue;
        }
        nano_shader_execute(shader);
    }
}

// Core Application Functions (init, event, cleanup)
// -------------------------------------------------

// Function called by sokol_app to initialize the application with
// WebGPU sokol_gfx sglue & cimgui
void nano_default_init(void) {
    LOG("NANO: Initializing NANO WGPU app...\n");

    // Retrieve the WGPU state from the wgpu_entry.h file
    // This state is created and ready to use when running an
    // application with wgpu_start().
    nano_app.wgpu = wgpu_get_state();

    // Initialize the buffer pool
    nano_init_buffer_pool(&nano_app.buffer_pool);

    // Initialize the shader pool
    nano_init_shader_pool(&nano_app.shader_pool);

    // Set the fonts
    if (nano_fonts.font_count != 0)
        nano_init_fonts(&nano_fonts, 16.0f);
    else
        nano_init_fonts(NULL, 16.0f);

    nano_app.settings = nano_default_settings();

    nano_app.settings.gfx.msaa.sample_count = nano_app.wgpu->desc.sample_count;

    LOG("NANO: Initialized\n");
}

// Free any resources that were allocated
void nano_default_cleanup(void) {

    if (nano_app.shader_pool.shader_count > 0) {
        for (int i = 0; i < NANO_MAX_SHADERS; i++) {
            if (nano_app.shader_pool.shaders[i].occupied) {
                nano_shader_release(
                    &nano_app.shader_pool,
                    nano_app.shader_pool.shaders[i].shader_entry.id);
            }
        }

        // Free the shader shader
        free(nano_app.shader_pool.shader_labels);
    }

    wgpu_stop();
}

#ifdef NANO_CIMGUI

// This represents the demo window that has all of the Nano application
// information and settings. This is a simple window that can be toggled on
// and off. This is a simple example of how to use the ImGui API to create a
// window for a Nano application.
void nano_draw_debug_ui() {

    assert(nano_app.wgpu != NULL && "Nano WGPU app is NULL\n");
    assert(nano_app.wgpu->device != NULL && "Nano WGPU device is NULL\n");
    assert(nano_app.wgpu->swapchain != NULL && "Nano WGPU swapchain is NULL\n");
    assert(nano_app.wgpu->cmd_encoder != NULL &&
           "Nano WGPU command encoder is NULL\n");

    static bool show_demo = false;
    bool visible = true;
    bool closed = false;

    ImGuiStyle *style = igGetStyle();
    style->ItemSpacing = (ImVec2){5, 10};

    // Set the window size
    igSetNextWindowSize((ImVec2){800, 600}, ImGuiCond_FirstUseEver);

    // Set the window position
    igSetNextWindowPos((ImVec2){20, 20}, ImGuiCond_FirstUseEver,
                       (ImVec2){0, 0});

    // Attempt to create the window with window flags
    // If the window is not created, end the window and return
    // Otherwise, continue to create the window and add the contents
    if (!igBegin("Nano Debug", &nano_app.show_debug,
                 ImGuiWindowFlags_MenuBar)) {
        igEnd();
        return;
    } else {

        // Menu bar implementation
        // Ensure that the ImGuiWindowFlags_MenuBar flag is set in igBegin()
        if (igBeginMenuBar()) {
            if (igBeginMenu("View", true)) {
                if (igMenuItem_BoolPtr("Show ImGui Demo", NULL, NULL, true)) {
                    show_demo = !show_demo;
                }
                igEndMenu();
            }
            igEndMenuBar();
        }

        // About Nano
        // --------------------------
        if (igCollapsingHeader_BoolPtr("About Nano", NULL,
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
        // --------------------------
        if (igCollapsingHeader_BoolPtr("Nano Graphics Information", NULL,
                                       ImGuiTreeNodeFlags_CollapsingHeader |
                                           ImGuiTreeNodeFlags_DefaultOpen)) {
            igText("Nano Runtime Information");
            igSeparator();
            igBulletText("Frame Time: %.2f ms", nano_app.frametime);
            igBulletText("Frames Per Second: %.2f", nano_app.fps);
            igBulletText("Render Resolution: (%d, %d)",
                         (int)nano_app.wgpu->width, (int)nano_app.wgpu->height);

            igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);

            igText("Graphics Settings");

            // MSAA settings
            // --------------------------

            igBullet();

            uint8_t *msaa_values = nano_app.settings.gfx.msaa.msaa_values;
            uint8_t msaa_value = nano_app.settings.gfx.msaa.sample_count;
            uint8_t *msaa_index = &nano_app.settings.gfx.msaa.msaa_index;

            // Find the index of the current MSAA setting
            for (int i = 0; i < 2; i++) {
                if (msaa_values[i] == msaa_value) {
                    *msaa_index = i;
                    break;
                }
            }

            if (igBeginCombo(
                    "MSAA",
                    nano_app.settings.gfx.msaa.msaa_options[*msaa_index],
                    ImGuiComboFlags_None)) {
                for (int i = 0; i < 2; i++) {
                    bool is_selected = (*msaa_index == i);
                    if (igSelectable_Bool(
                            nano_app.settings.gfx.msaa.msaa_options[i],
                            is_selected, ImGuiSelectableFlags_None,
                            (ImVec2){0, 0})) {
                        *msaa_index = i;
                        nano_app.settings.gfx.msaa.msaa_changed = true;
                    }
                    if (is_selected) {
                        igSetItemDefaultFocus();
                    }
                }
                igEndCombo();
            }
            igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);
        }
        // End of Nano Render Information

        // Nano Font Information
        // --------------------------
        if (igCollapsingHeader_BoolPtr("Nano Font Information", NULL,
                                       ImGuiTreeNodeFlags_CollapsingHeader)) {
            nano_font_info_t *font_info = &nano_app.font_info;
            igText("Font Index: %d", font_info->font_index);
            igText("Font Size: %.2f", font_info->font_size);

            // Get the font names for the combo box as a single string
            char font_names[NANO_NUM_FONTS * 41] = {};
            for (int i = 0; i < NANO_NUM_FONTS; i++) {
                strncat(&font_names[i], nano_fonts.fonts[i].name, 40);
                // We have to add a ? to separate the strings
                // until we can replace them with \0
                //
                // strncat will replace the previous \0 with the first
                // character of the copied string so we can use this to
                // separate the strings.
                strncat(&font_names[i], "?", 1);
            }

            // Replace all ? with \0 to separate the strings
            for (int i = 0; i < NANO_NUM_FONTS * 41; i++) {
                if (font_names[i] == '?') {
                    font_names[i] = '\0';
                } else if (font_names[i] == '\0') {
                    break;
                }
            }

            if (igCombo_Str("Select Font", (int *)&font_info->font_index,
                            font_names, 3)) {
                nano_set_font(font_info->font_index);
            }
            igSliderFloat("Font Size", (float *)&font_info->font_size, 8.0f,
                          32.0f, "%.2f", 1.0f);
            // Once the slider is released, set the flag for editing the
            // font size This requires our render pass to basically be
            // completed before we can do this however. This is a
            // workaround for now.
            if (igIsItemDeactivatedAfterEdit()) {
                nano_app.font_info.update_fonts = true;
            }
            igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);
        }

        // Shader Pool Information
        // --------------------------
        if (igCollapsingHeader_BoolPtr("Nano Shader Pool Information", NULL,
                                       ImGuiTreeNodeFlags_CollapsingHeader)) {

            // Create a static label for the active shader combo
            static char active_shader_label[64];
            static uint32_t active_shader_id;
            if (active_shader_label[0] == 0) {
                active_shader_id =
                    nano_get_active_shader_id(&nano_app.shader_pool, 0);
                nano_shader_t *active_shader =
                    nano_get_shader(&nano_app.shader_pool, active_shader_id);
                snprintf(active_shader_label, 64, "%d: %s", 0,
                         active_shader->info.label);
            }

            // Basic shader pool information
            igText("Shader Pool Information");
            igBulletText("Shaders In Memory: %zu",
                         nano_app.shader_pool.shader_count);
            igBulletText("Active Shaders: %d",
                         nano_num_active_shaders(&nano_app.shader_pool));

            igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);

            // Do not display the shader pool if there are no shaders
            if (nano_app.shader_pool.shader_count == 0) {
                igText("No shaders found.\nAdd a shader to inspect it.");
            } else {

                // List all active shaders in the shader pool.
                // --------------------------

                if (igCollapsingHeader_BoolPtr(
                        "Active Shaders", NULL,
                        ImGuiTreeNodeFlags_CollapsingHeader |
                            ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (nano_app.shader_pool.active_shaders.top == -1) {
                        igText("No active shaders found.");
                    } else {
                        igText("Active Shaders In Order Of Execution:");
                        if (igBeginCombo("Select Active Shader",
                                         active_shader_label,
                                         ImGuiComboFlags_None)) {
                            for (int i = 0;
                                 i <
                                 nano_app.shader_pool.active_shaders.top + 1;
                                 i++) {
                                uint32_t shader_id = nano_get_active_shader_id(
                                    &nano_app.shader_pool, i);
                                nano_shader_t *shader = nano_get_shader(
                                    &nano_app.shader_pool, shader_id);
                                char label[64];
                                snprintf(label, 64, "%d: %s", i,
                                         shader->info.label);
                                if (igSelectable_Bool(label, false,
                                                      ImGuiSelectableFlags_None,
                                                      (ImVec2){0, 0})) {
                                    active_shader_id = shader_id;
                                    snprintf(active_shader_label, 64, "%d: %s",
                                             i, shader->info.label);
                                }
                            }
                            igEndCombo();
                        }

                        nano_shader_t *shader = nano_get_shader(
                            &nano_app.shader_pool, active_shader_id);
                        igBulletText("Shader ID: %u", shader->id);
                        igBulletText("Shader Type: %s",
                                     nano_get_shader_type_str(shader));

                        if (igButton("Deactivate Shader", (ImVec2){200, 0})) {
                            nano_shader_deactivate(nano_get_shader(
                                &nano_app.shader_pool, active_shader_id));
                            active_shader_id = nano_get_active_shader_id(
                                &nano_app.shader_pool, 0);
                            nano_shader_t *active_shader = nano_get_shader(
                                &nano_app.shader_pool, active_shader_id);
                            snprintf(active_shader_label, 64, "%d: %s", 0,
                                     active_shader->info.label);
                        }
                    }
                }

                // End of active shaders
                // --------------------------

                igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);

                // List all shaders in the shader pool
                // This will allow us to activate, delete, or edit/inspect
                // the shader
                // --------------------------
                if (igCollapsingHeader_BoolPtr(
                        "Loaded Shaders", NULL,
                        ImGuiTreeNodeFlags_CollapsingHeader)) {

                    igText("Shaders In Memory:");

                    static int shader_index = 0;
                    igCombo_Str("Select Shader", &shader_index,
                                nano_app.shader_pool.shader_labels,
                                nano_app.shader_pool.shader_count);

                    // Find the shader slot with the shader index
                    int slot = _nano_find_shader_slot_with_index(shader_index);
                    if (slot < 0) {
                        igText("Error: Shader not found");
                    } else {
                        // When found, get he shader information and display
                        // it
                        nano_shader_t *shader =
                            &nano_app.shader_pool.shaders[slot].shader_entry;

                        char *source = shader->info.source;
                        char *label = (char *)shader->info.label;

                        // Calculate the size of the input text box based on
                        // the visual text size of the shader source
                        ImVec2 size = {200, 0};
                        igCalcTextSize(&size, source, NULL, false, 0.0f);

                        // Add some padding to the size
                        size.x += 20;
                        size.y += 20;

                        igText("Shader ID: %u", shader->id);
                        igText("Shader Type: %s",
                               nano_get_shader_type_str(shader));

                        // Display the shader source in a text box
                        igInputTextMultiline(label, source, strlen(source) * 2,
                                             size, ImGuiInputTextFlags_None,
                                             NULL, NULL);

                        // Allow shaders to be activated or removed
                        if (!shader->in_use) {
                            if (igButton("Build Shader", (ImVec2){200, 0})) {
                                nano_shader_build(shader);
                            }
                            if (igButton("Activate Shader", (ImVec2){200, 0})) {
                                nano_shader_activate(shader, false);
                            }
                            if (igButton("Remove Shader", (ImVec2){200, 0})) {
                                nano_shader_release(&nano_app.shader_pool,
                                                    shader_index);
                            }
                        } else {
                            igText("Shader is currently in use.");
                        }
                    }
                }
                // End of individual shader information
                // ------------------------------------
            }
        }

        igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);

        // End of shader pool Information
        // ------------------------------

        igText("Misc Settings:");

        igBullet();

        // Set the clear color for the frame
        igSliderFloat4("RGBA Clear", (float *)&nano_app.wgpu->clear_color, 0.0f,
                       1.0f, "%.2f", 0);

        // Button to open the ImGui demo window
        igSeparatorEx(ImGuiSeparatorFlags_Horizontal, 5.0f);

        // Show the ImGui demo window based on the show_demo flag
        if (show_demo)
            igShowDemoWindow(&show_demo);

        // End of the Dear ImGui frame
        igEnd();
    }
}

#endif

// -------------------------------------------------------------------------------
// Nano Frame Update Functions
// nano_start_frame() - Called at the beginning of the frame
// callback method nano_end_frame() - Called at the end of the frame
// callback method These functions are used to update the
// application state to ensure that the app is ready to render the
// next frame.
// -------------------------------------------------------------------------------

// Calculate current frames per second
WGPUCommandEncoder nano_start_frame() {

    // Update the dimensions of the window
    nano_app.wgpu->width = wgpu_width();
    nano_app.wgpu->height = wgpu_height();

#ifdef NANO_CIMGUI
    // Set the display size for ImGui
    ImGuiIO *io = igGetIO();
    io->DisplaySize =
        (ImVec2){(float)nano_app.wgpu->width, (float)nano_app.wgpu->height};
#endif

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

    // Clear the swapchain with a color at the start of the frame
    {
        WGPURenderPassDescriptor render_pass_desc = {
            .colorAttachmentCount = 1,
            .colorAttachments =
                &(WGPURenderPassColorAttachment){
                    .view = wgpu_get_render_view(),
                    .depthSlice = ~0u,
                    .resolveTarget = wgpu_get_resolve_view(),
                    .loadOp = WGPULoadOp_Clear,   // Clear the frame
                    .storeOp = WGPUStoreOp_Store, // Store the clear value
                                                  // for the frame
                    .clearValue =
                        (WGPUColor){
                            .r = nano_app.wgpu->clear_color[0],
                            .g = nano_app.wgpu->clear_color[1],
                            .b = nano_app.wgpu->clear_color[2],
                            .a = nano_app.wgpu->clear_color[3],
                        },
                },
            .depthStencilAttachment = NULL,
        };

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(
            nano_app.wgpu->cmd_encoder, &render_pass_desc);
        wgpuRenderPassEncoderEnd(pass);
    } // End of clear swapchain

// Start the ImGui frame if nano_cimgui is enabled
#ifdef NANO_CIMGUI
    nano_cimgui_new_frame();
#endif

    return nano_app.wgpu->cmd_encoder;
}

// Function called at the end of the frame to present the frame
// and update the nano app state for the next frame.
// This method should be called after nano_start_frame().
void nano_end_frame() {

    assert(nano_app.wgpu != NULL && "Nano WGPU app is NULL\n");
    assert(nano_app.wgpu->device != NULL && "Nano WGPU device is NULL\n");
    assert(nano_app.wgpu->swapchain != NULL && "Nano WGPU swapchain is NULL\n");
    assert(nano_app.wgpu->cmd_encoder != NULL &&
           "Nano WGPU command encoder is NULL\n");

// If nano_cimgui is enabled, draw the debug UI if needed
// and end the ImGui frame so we can render the ImGuiDrawData
#ifdef NANO_CIMGUI
    if (nano_app.show_debug) {
        nano_draw_debug_ui();
    }

    // We pass our command encoder to nano_cimgui to render the
    // ImGuiDrawData onto the frame
    nano_cimgui_end_frame(nano_app.wgpu->cmd_encoder, wgpu_get_render_view,
                          wgpu_get_resolve_view);
#endif

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

    // Settings to update at the end of the frame
    // ------------------------------------------

    // Update the MSAA settings
    if (nano_app.settings.gfx.msaa.msaa_changed) {
        uint8_t current_item_id = nano_app.settings.gfx.msaa.msaa_index;
        uint8_t current_item =
            nano_app.settings.gfx.msaa.msaa_values[current_item_id];
        if (current_item == nano_app.settings.gfx.msaa.sample_count) {
            nano_app.settings.gfx.msaa.msaa_changed = false;
        } else {
            nano_app.settings.gfx.msaa.sample_count = current_item;
            nano_app.wgpu->desc.sample_count = current_item;

            wgpu_swapchain_reinit(nano_app.wgpu);
            // Iterate over all active shaders in the shader pool
            // and rebuild the shaders
            int num_active_shaders =
                nano_num_active_shaders(&nano_app.shader_pool);
            for (int i = 0; i < num_active_shaders; i++) {
                uint32_t shader_id =
                    nano_get_active_shader_id(&nano_app.shader_pool, i);
                nano_shader_t *shader =
                    nano_get_shader(&nano_app.shader_pool, shader_id);
                if (shader == NULL) {
                    LOG_ERR("NANO: Shader %u is NULL\n", shader_id);
                    continue;
                }
                nano_build_shader_pipelines(shader);
            }

            nano_app.settings.gfx.msaa.msaa_changed = false;
            nano_init_fonts(&nano_app.font_info, nano_app.font_info.font_size);
        }
    }

    // Update the font size if the flag is set
    if (nano_app.font_info.update_fonts) {
        nano_init_fonts(&nano_app.font_info, nano_app.font_info.font_size);
    }
}

#endif // NANO_H
