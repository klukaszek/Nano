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

#define LOG_ERR(...) fprintf(stderr, __VA_ARGS__)

#ifndef NANO_DEBUG
    #define NANO_DEBUG_UI 0
    #define LOG(...)
#else
    #define NANO_DEBUG_UI 1
    #define LOG(...) fprintf(stdout, __VA_ARGS__)
#endif

#define NANO_FAIL -1
#define NANO_OK 0

// Generic Array/Stack Implementation Based on old GGYL code
// ----------------------------------------

// Call this macro to define an array/s structure and its functions
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

// Nano Binding & Buffer Declarations
// ---------------------------------------

// Maximum size of the hash table for each of the buffer pools.
#define NANO_MAX_GROUPS 4
#define NANO_GROUP_MAX_BINDINGS 8

typedef BindingInfo nano_binding_info_t;
typedef WGPUBufferDescriptor nano_buffer_desc_t;

// Building the BindGroupDescriptor must be a manual process
// since Nano can't inherently know what  is being passed
// into the BindGroupEntries. When nano_shader_build_bindgroup(shader)
// is called, Nano will check for
typedef struct nano_buffer_data_t {
    uint8_t group;
    uint8_t binding;
    size_t size;
    uint32_t count;
    size_t offset;
    void *data;
} nano_buffer_data_t;

// Nano Entry Point Declarations
// ----------------------------------------
typedef EntryPoint nano_entry_t;

// Nano Pipeline Declarations
// ---------------------------------------
typedef struct nano_pipeline_layout_t {
    WGPUBindGroupLayout bg_layouts[MAX_GROUPS];
    size_t num_layouts;
} nano_pipeline_layout_t;

// Nano Pass Action Declarations
// ----------------------------------------
typedef void (*nano_pass_func)(void);

typedef struct nano_pass_action_t {
    char label[64];
    nano_pass_func func;
} nano_pass_action_t;

// Nano Shader Declarations
// ----------------------------------------

// Maximum number of compute shaders that can be stored in the shader pool
#define NANO_MAX_SHADERS 16

typedef ShaderType nano_shader_type_t;
typedef ShaderInfo nano_shader_info_t;

typedef WGPUBindGroup nano_bind_group_t;

// This is the struct that will hold all of the information for a shader
// loaded into the shader pool.
typedef struct nano_shader_t {
    uint32_t id;
    bool in_use;
    bool built;

    nano_shader_type_t type;
    nano_shader_info_t info;

    nano_pipeline_layout_t layout;

    // A shader can have up to two pass actions
    // This is useful for rendering multiple passes in a single frame
    // For example, a compute shader followed by a render pass
    nano_pass_action_t pass_actions[2];

    // This is where we store buffer data and size so that Nano can
    // create the buffers for the shader bindings when the shader is loaded
    nano_buffer_data_t buffer_data[NANO_MAX_GROUPS][NANO_GROUP_MAX_BINDINGS];

    // Using the buffer size from the buffer data, we can create the bindgroups
    // so that we can bind the buffers to the shader pipeline
    nano_bind_group_t bind_groups[NANO_MAX_GROUPS];

    nano_buffer_data_t *compute_output_buffer;

    WGPUComputePipeline compute_pipeline;
    WGPURenderPipeline render_pipeline;
} nano_shader_t;

// Node for the shader pool hash table
typedef struct ShaderNode {
    nano_shader_t shader_entry;
    bool occupied;
} ShaderNode;

// Nano Shader Pool Declarations
// ----------------------------------------

// Define an array stack for the active shaders to keep track of the indices
DEFINE_ARRAY_STACK(int, nano_index_array, NANO_MAX_SHADERS)

typedef struct nano_shader_pool_t {
    ShaderNode shaders[NANO_MAX_SHADERS];
    int shader_count;
    // One string for all shader labels used for ImGui combo boxes
    // Only updated when a shader is added or removed from the pool
    char shader_labels[NANO_MAX_SHADERS * 64];
    nano_index_array active_shaders;
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

typedef wgpu_state_t nano_wgpu_state_t;

typedef struct nano_gfx_settings_t {

    struct msaa_t {
        uint8_t sample_count;
        bool msaa_changed;
        uint8_t msaa_values[2];
        const char *msaa_options[2];
        uint8_t msaa_index;
    } msaa;

} nano_gfx_settings_t;

typedef struct nano_settings_t {
    nano_gfx_settings_t gfx;
} nano_settings_t;

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

nano_settings_t nano_default_settings() {
    return (nano_settings_t){
        .gfx = nano_default_gfx_settings(),
    };
}

// This is the struct that will hold all of the running application data
// In simple terms, this is the state of the application
typedef struct nano_t {
    nano_wgpu_state_t *wgpu;
    bool show_debug;
    float frametime;
    float fps;
    nano_font_info_t font_info;
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
    .shader_pool = {0},
    .settings = {0},
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

// Buffer Functions
// -------------------------------------------------

// Assign buffer data to a shader so that the buffer can be properly populated
// Used by nano_shader_build_bindgroup() to create the buffers and bindgroups.
// This function should be called before activating a shader.
// Overwrites existing buffer data if it already exists.
int nano_shader_assign_buffer_data(nano_shader_t *shader, uint8_t group,
                                   uint8_t binding, size_t size, uint32_t count,
                                   size_t offset, void *data) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_assign_buffer_data() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    if (shader->in_use) {
        LOG_ERR("NANO: nano_shader_assign_buffer_data() -> Shader is currently "
                "use. Make sure to assign buffer data before activating a "
                "shader.\n");
        return NANO_FAIL;
    }

    if (group >= NANO_MAX_GROUPS || binding >= NANO_GROUP_MAX_BINDINGS) {
        LOG_ERR("NANO: nano_shader_assign_buffer_data() -> Group or binding "
                "index out of bounds\n");
        return NANO_FAIL;
    }

    if (count == 0) {
        LOG_ERR("NANO: nano_shader_assign_buffer_data() -> Count is 0\n");
        return NANO_FAIL;
    }

    nano_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_shader_assign_buffer_data() -> Shader info is "
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

    // Set the buffer data for the shader
    nano_buffer_data_t *buffer_data = &shader->buffer_data[group][binding];
    buffer_data->group = group;
    buffer_data->binding = binding;
    buffer_data->size = size;
    buffer_data->count = count;
    buffer_data->offset = offset;
    buffer_data->data = data;

    return NANO_OK;
}

// Assign an output buffer to a compute shader so that the shader can calculate
// the workgroup sizes based on the number of elements in the output buffer. A
// compute shader should have a single output buffer so this function should be
// called once.
int nano_compute_shader_assign_output_buffer(nano_shader_t *shader,
                                             uint8_t group, uint8_t binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_assign_output_buffer() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    if (shader->in_use) {
        LOG_ERR(
            "NANO: nano_shader_assign_output_buffer() -> Shader is currently "
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
    shader->compute_output_buffer = &shader->buffer_data[group][binding];

    return NANO_OK;
}

// Create a Nano WGPU buffer object within a shader binding
// This method should not be called by the user, it is used internally.
// See nano_shader_assign_buffer_data() to assign buffer data to a shader
// binding and then call nano_shader_build_bindgroup() to create the buffers and
// bindgroups for the shader.
int nano_create_buffer(nano_binding_info_t *binding, size_t size) {
    if (binding == NULL) {
        LOG_ERR("NANO: nano_create_buffer() -> Binding info is NULL\n");
        return NANO_FAIL;
    }

    if (binding->binding_type != BUFFER) {
        LOG_ERR("NANO: nano_create_buffer() -> Binding type is not a buffer\n");
        return NANO_FAIL;
    }

    // Align the buffer size to the GPU cache line size of 32 bytes
    size_t gpu_cache_line_size = 32;
    size_t cache_aligned_size =
        (size + (gpu_cache_line_size - 1)) & ~(gpu_cache_line_size - 1);

    // Set the buffer size to the cache aligned size
    binding->size = cache_aligned_size;

    // Create the buffer descriptor
    WGPUBufferDescriptor desc = {
        .usage = binding->info.buffer_usage,
        .size = cache_aligned_size,
        .mappedAtCreation = false,
    };

    // Release the buffer if it already exists
    if (binding->data.buffer) {
        wgpuBufferRelease(binding->data.buffer);
    }

    // Create the buffer as part of the binding object
    binding->data.buffer = wgpuDeviceCreateBuffer(nano_app.wgpu->device, &desc);
    if (binding->data.buffer == NULL) {
        LOG_ERR("NANO: nano_create_buffer() -> Could not create buffer\n");
        return NANO_FAIL;
    }

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

    nano_shader_info_t *info = &shader->info;
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
        fprintf(
            stderr,
            "NANO: nano_get_gpu_buffer() -> Binding type is not a buffer\n");
        return NULL;
    }

    // Return the buffer from the binding info struct as a pointer
    return info->bindings[index].data.buffer;
}

// Get the buffer size from the shader info struct using the group and binding
size_t nano_get_buffer_size(nano_shader_t *shader, uint8_t group,
                            uint8_t binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_buffer_size() -> Shader info is NULL\n");
        return 0;
    }

    nano_shader_info_t *info = &shader->info;
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

// Represents the action to take when mapping a buffer for reading or writing
typedef struct nano_gpu_data_t {
    bool locked;
    size_t size;
    WGPUBuffer src;
    size_t src_offset;
    // The destination pointer for the data
    void *data;
    size_t dst_offset;
    WGPUBuffer _staging;
} nano_gpu_data_t;

// Copy the contents of one WGPUBuffer to another WGPUBuffer
int nano_copy_buffer_to_buffer(WGPUBuffer src, size_t src_offset,
                               WGPUBuffer dst, size_t dst_offset, size_t size) {
    if (nano_app.wgpu->device == NULL) {
        LOG_ERR("NANO: nano_copy_buffer_to_buffer() -> Device is NULL\n");
        return NANO_FAIL;
    }

    if (src == NULL) {
        LOG_ERR(
            "NANO: nano_copy_buffer_to_buffer() -> Source buffer is NULL\n");
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
void nano_write_buffer(WGPUBuffer buffer, size_t offset, void *data,
                       size_t size) {
    wgpuQueueWriteBuffer(wgpuDeviceGetQueue(nano_app.wgpu->device), buffer,
                         offset, data, size);

    LOG("NANO: Last element of buffer is %.4f\n",
        ((float *)data)[size / sizeof(float) - 1]);
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
    // If the staging descriptor is NULL, we need to create a new staging buffer
    // with default settings
    if (staging_desc == NULL) {
        // Create the staging buffer to read results back to the CPU
        data->_staging = wgpuDeviceCreateBuffer(
            device,
            &(WGPUBufferDescriptor){
                .size = data->size,
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
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

// Binding functions
// -------------------------------------------------

// Retrieve a binding from the shader info struct regardless of binding type
nano_binding_info_t nano_get_binding(nano_shader_t *shader, int group,
                                     int binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_binding() -> Shader info is NULL\n");
        return (nano_binding_info_t){0};
    }

    nano_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_get_binding() -> Shader info is NULL\n");
        return (nano_binding_info_t){0};
    }

    int index = info->group_indices[group][binding];
    if (index == -1) {
        LOG_ERR("NANO: nano_get_binding() -> Binding not found\n");
        return (nano_binding_info_t){0};
    }

    return info->bindings[index];
}

// Retrieve a binding from the shader info struct by the name
// This is useful when working with a shader that we already know the bindings
// of.
nano_binding_info_t nano_get_binding_by_name(nano_shader_t *shader,
                                             const char *name) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_binding_by_name() -> Shader info is NULL\n");
        return (nano_binding_info_t){0};
    }

    nano_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_get_binding_by_name() -> Shader info is NULL\n");
        return (nano_binding_info_t){0};
    }

    for (int i = 0; i < info->binding_count; i++) {
        nano_binding_info_t binding = info->bindings[i];
        if (strcmp(binding.name, name) == 0) {
            return binding;
        }
    }

    LOG_ERR("NANO: nano_get_binding_by_name() -> Binding \"%s\" not found\n",
            name);
    return (nano_binding_info_t){0};
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
    nano_index_array_init(&table->active_shaders);
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
        LOG_ERR("NANO: nano_set_font() -> Invalid font index\n");
        return;
    }

    if (&(nano_app.font_info.fonts[index].imfont) == NULL) {
        LOG_ERR("NANO: nano_set_font() -> Font is NULL\n");
        return;
    }

    ImGuiIO *io = igGetIO();

    // Set the font as the default font
    io->FontDefault = nano_app.font_info.fonts[index].imfont;

    LOG("NANO: Set font to %s\n",
        nano_app.font_info.fonts[index].imfont->ConfigData->Name);

    // TODO: FIGURE OUT DPI SCALING FOR WEBGPU
    // io->FontGlobalScale = sapp_dpi_scale();
}

// Function to initialize the fonts for the ImGui context
void nano_init_fonts(nano_font_info_t *font_info, float font_size) {

    ImGuiIO *io = igGetIO();

    // Make sure to clear the font atlas so we do not leak memory each
    // time we have to update the fonts for rendering.
    ImFontAtlas_Clear(io->Fonts);

    // Set the app state font info to whatever is passed in
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
        LOG_ERR("NANO: nano_parse_shader() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    // Initialize the parser with the shader source code
    Parser parser;
    init_parser(&parser, source);

    shader->info.binding_count = 0;
    shader->info.entry_point_count = 0;

    // Parse the shader to get the entry points and binding information
    // This will populate the shader struct with the necessary information
    // This shader struct will ultimately be used to store the ComputePipeline
    // and PipelineLayout objects.
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

    nano_shader_info_t *info = &shader->info;
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

    LOG("NANO: Shader %u: Building bindings...\n", info->id);

    // Iterate over the groups and bindings to create the buffers to be
    // used for the pipeline layout
    for (int i = 0; i < NANO_MAX_GROUPS; i++) {

        // Keep track of whether the group is empty or not
        bool group_empty = true;

        for (int j = 0; j < NANO_GROUP_MAX_BINDINGS; j++) {

            // If the group index is not -1, we can create the buffer
            // Otherwise, we can break out of the loop early
            if (info->group_indices[i][j] != -1) {
                group_empty = false;
                int index = info->group_indices[i][j];
                nano_binding_info_t *binding = &info->bindings[index];

                // If the buffer usage is not none, create the buffer
                if (binding->info.buffer_usage != WGPUBufferUsage_None) {

                    // Check to make sure that the shader has buffer data
                    // attached to it before creating the buffer.
                    if (shader->buffer_data[i][j].size == 0) {
                        LOG_ERR("NANO: Shader %u: Buffer data not assigned for "
                                "group %d, binding %d\n",
                                info->id, i, j);
                        return NANO_FAIL;
                    }

                    LOG("\tNANO: Shader %u: Group %d -> Creating new buffer "
                        "for "
                        "binding %d \"%s\" "
                        "with type %s\n",
                        info->id, i, binding->binding, binding->name,
                        binding->data_type);

                    size_t buffer_size = shader->buffer_data[i][j].size;

                    // Create the buffer within the shader
                    int status = nano_create_buffer(binding, buffer_size);

                    if (status != NANO_OK) {
                        LOG_ERR("\tNANO: Shader %u: Could not create buffer "
                                "for binding %d\n",
                                info->id, binding->binding);
                        return NANO_FAIL;
                    }
                    // If the buffer usage is none, we can break out of the loop
                } else {
                    LOG_ERR("\tNANO: Shader %u: Binding %d has no usage\n",
                            info->id, binding->binding);
                    return NANO_FAIL;
                }
            }
            // We should technically assume that if we hit -1 then there
            // should be no more bindings left in the group. This means we
            // can break out of the loop early and move on to the next
            // group. This isn't perfect, but I won't have any stupid layouts.
        }

        // If the group is empty, we can log that the group has been built
        if (!group_empty) {
            LOG("NANO: Shader %u: Built bindings for group %d\n", info->id, i);
        }
    }

    return NANO_OK;
}

// Get the binding information from the shader info struct as a
// nano_binding_info_t
nano_binding_info_t nano_get_shader_binding(nano_shader_t *shader, int group,
                                            int binding) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_get_shader_binding() -> Shader info is NULL\n");
        return (nano_binding_info_t){0};
    }

    nano_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_get_shader_binding() -> Shader info is NULL\n");
        return (nano_binding_info_t){0};
    }

    int index = info->group_indices[group][binding];
    if (index == -1) {
        LOG_ERR("NANO: nano_get_shader_binding() -> Binding not found\n");
        return (nano_binding_info_t){0};
    }

    return info->bindings[index];
}

// Build a pipeline layout for the shader using the bindings in the shader
// a shader must have buffer data assigned to it before calling this function.
// The buffer data can be assigned using nano_shader_assign_buffer_data().
// If no buffer data is assigned, but buffer data is expected, the method will
// return NANO_FAIL.
int nano_build_pipeline_layout(nano_shader_t *shader) {

    if (shader == NULL) {
        LOG_ERR("Nano: nano_build_pipeline_layout() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    nano_shader_info_t *info = &shader->info;

    if (info == NULL) {
        LOG_ERR("Nano: nano_build_pipeline_layout() -> Compute info is NULL\n");
        return NANO_FAIL;
    }

    int num_groups = 0;

    // Create an array of bind group layouts for each group
    WGPUBindGroupLayout bg_layouts[MAX_GROUPS];
    if (info->binding_count >= MAX_GROUPS * MAX_BINDINGS) {
        LOG_ERR("NANO: Shader %u: Too many bindings\n", info->id);
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
    int status = nano_build_bindings(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Shader %u: Could not build bindings\n", info->id);
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
            WGPUBufferUsageFlags buffer_usage = binding.info.buffer_usage;

            // Set the according bindgroup layout entry for the
            // binding
            bgl_entries[j] = (WGPUBindGroupLayoutEntry){
                .binding = (uint32_t)binding.binding,
                // Temporarily set the visibility to none
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
            LOG("NANO: Shader %u: Creating bind group layout for group "
                "%d with %d entries\n",
                info->id, num_groups, num_bindings);

            // // If the bind group layout is not NULL, we can release it
            // if (bg_layouts[i] != 0) {
            //     wgpuBindGroupLayoutRelease(bg_layouts[i]);
            // }

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
            // If there are no bindings in the group, we can break out of the
            // loop early
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

    nano_shader_info_t *info = &shader->info;

    int retval = NANO_OK;
    int compute_index = info->entry_indices.compute;
    int vertex_index = info->entry_indices.vertex;
    int fragment_index = info->entry_indices.fragment;

    // Create the pipeline layout descriptor
    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
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

        // Set the WGPU pipeline object in our shader info struct
        shader->compute_pipeline = wgpuDeviceCreateComputePipeline(
            nano_app.wgpu->device, &pipeline_desc);
    }

    // If both the vertex and fragment entry indices are valid, we can create
    // a render pipeline for the shader
    if (vertex_index != -1 && fragment_index != -1) {

        WGPURenderPipelineDescriptor renderPipelineDesc = {
            .label = (const char *)info->label,
            .layout = pipeline_layout_obj,
            .vertex =
                {
                    .module = shader_module,
                    .entryPoint = info->entry_points[vertex_index].entry,
                    .bufferCount = 0,
                    .buffers = NULL,
                    // TODO: Add other vertex attributes here
                },
            .primitive = {.topology = WGPUPrimitiveTopology_TriangleList,
                          .stripIndexFormat = WGPUIndexFormat_Undefined,
                          .frontFace = WGPUFrontFace_CCW,
                          .cullMode = WGPUCullMode_None},
            .multisample =
                (WGPUMultisampleState){
                    .count = (uint32_t)nano_app.settings.gfx.msaa.sample_count,
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

        // Assign the render pipeline to the shader info struct
        shader->render_pipeline = wgpuDeviceCreateRenderPipeline(
            nano_app.wgpu->device, &renderPipelineDesc);

        // If the vertex or fragment entry indices are valid, but the other is
        // not, we can't create a render pipeline
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
        fprintf(
            stderr,
            "NANO: nano_build_bindgroups() -> Shader is currently in use\n");
        return NANO_FAIL;
    }

    nano_shader_info_t *info = &shader->info;

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

        // Iterate through the bindings in the group and create the bind group.
        // Remember: when defining bindings, make sure to not skip numbers.
        // Always increase the binding number by 1, or increase the group number
        // by 1 and return to 0.
        for (int j = 0; j < count; j++) {
            int index = info->group_indices[i][j];
            nano_binding_info_t binding = info->bindings[index];

            // If the buffer usage is not none, we can create the bindgroup
            // entry
            // TODO: Add support for textures and samplers as bindings
            if (binding.info.buffer_usage != WGPUBufferUsage_None) {
                WGPUBuffer buffer = binding.data.buffer;

                bg_entry[j] = (WGPUBindGroupEntry){
                    .binding = (uint32_t)binding.binding,
                    .buffer = nano_get_gpu_buffer(shader, i, j),
                    .offset = shader->buffer_data[i][j].offset,
                    .size = shader->buffer_data[i][j].size,
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
            fprintf(
                stderr,
                "NANO: Shader %u: Could not create bind group for group %d\n",
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
ShaderIndices nano_precompute_entry_indices(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_precompute_entry_indices() -> Shader is NULL\n");
        return (ShaderIndices){-1, -1, -1};
    }

    nano_shader_info_t *info = &shader->info;
    if (info == NULL) {
        LOG_ERR("NANO: nano_precompute_entry_indices() -> Shader info is "
                "NULL\n");
        return (ShaderIndices){-1, -1, -1};
    }

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

    nano_shader_info_t *info = &shader->info;
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
uint32_t nano_create_shader(const char *shader_source, char *label) {
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
                "shader %d\n",
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
    nano_shader_info_t info = {
        .id = shader_id,
        .source = strdup(shader_source),
    };

    strncpy((char *)&info.label, label, strlen(label) + 1);

    nano_shader_t shader = (nano_shader_t){
        .id = shader_id,
        .info = info,
    };

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
uint32_t nano_create_shader_from_file(const char *path, char *label) {
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

// Build the bindings, pipeline layout, bindgroups, and pipelines for the shader
// Can be called by the developer to build the shader manually, or can be called
// as part of the activation process as a boolean parameter.
int nano_shader_build(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_shader_build() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    // Verify that the shader can be parsed properly before building it
    int status = nano_validate_shader(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to validate shader %d\n", shader->id);
        return NANO_FAIL;
    }

    // Use a nano_buffer_data_t struct to store metadata about each buffer
    // Once the developer has added the buffer information to the shader
    // struct, we can call this method to make sure the shader is ready to
    // be executed.

    // Build the pipeline layout
    status = nano_build_pipeline_layout(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to build pipeline layout for shader %d\n",
                shader->info.id);
        return status;
    }

    // Build bind groups for the shader so we can bind the buffers
    status = nano_build_bindgroups(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to build bindgroup for shader %d\n",
                shader->info.id);
        return status;
    }

    // Build the shader pipelines
    status = nano_build_shader_pipelines(shader);
    if (status != NANO_OK) {
        LOG_ERR("NANO: Failed to build shader pipelines for shader %d\n",
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
        LOG_ERR("NANO: Shader %d is already active\n", shader->id);
        return NANO_OK;
    }

    // If the shader has not been built, we build it now
    // If the build flag is set, we rebuild the shader
    if (!shader->built || build) {
        int status = nano_shader_build(shader);
        if (status != NANO_OK) {
            LOG_ERR("NANO: Failed to build shader %d\n", shader->id);
            return NANO_FAIL;
        }
    }

    // If the shader is not active, we can activate it
    shader->in_use = true;

    // Add the shader to the active shaders list
    nano_index_array_push(&nano_app.shader_pool.active_shaders, shader->id);

    return NANO_OK;
}

// Deactivate a shader from the active list
int nano_deactivate_shader(nano_shader_t *shader) {
    if (shader == NULL) {
        LOG_ERR("NANO: nano_deactivate_shader() -> Shader is NULL\n");
        return NANO_FAIL;
    }

    // If the shader is not active, return early
    if (!shader->in_use) {
        return NANO_OK;
    }

    // If the shader is active, we can deactivate it
    shader->in_use = false;

    // Remove the shader from the active shaders list
    nano_index_array_remove(&nano_app.shader_pool.active_shaders, shader->id);

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
    nano_shader_info_t *info = &shader->info;
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
        fprintf(
            stderr,
            "NANO: nano_get_render_pipeline() -> Render pipeline not found\n");
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

    // Create a new command encoder for the compute pass
    WGPUCommandEncoder command_encoder =
        wgpuDeviceCreateCommandEncoder(nano_app.wgpu->device, NULL);

    bool render_pass_queued = false;

    // Find the compute shader in the shader info entry points list
    for (int i = 0; i < shader->info.entry_point_count; i++) {
        if (shader->info.entry_points[i].type == COMPUTE) {

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
            struct WorkgroupSize workgroup_sizes =
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
        } else {
            // If a shader has a vertex and fragment entry point, we can
            // queue a render pass here.
            // If the shader has both, this is only called once to make
            // sure the same render pass is not queued multiple times.
            if (!render_pass_queued) {

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

                wgpuRenderPassEncoderDraw(render_pass, 3, 1, 0, 0);
                wgpuRenderPassEncoderEnd(render_pass);

                // submit the command buffer that contains the render pass
                WGPUCommandBuffer command_buffer =
                    wgpuCommandEncoderFinish(command_encoder, NULL);
                wgpuQueueSubmit(queue, 1, &command_buffer);

                LOG("NANO: Render pass queued for shader %u\n", shader->id);

                render_pass_queued = true;
            }
        }
    }
}

// Iterate over all active shaders in the shader pool and execute each shader
// with the appropriate bindgroups and pipelines loaded into the GPU.
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
static void nano_default_init(void) {
    LOG("NANO: Initializing NANO WGPU app...\n");

    // Retrieve the WGPU state from the wgpu_entry.h file
    // This state is created and ready to use when running an
    // application with wgpu_start().
    nano_app.wgpu = wgpu_get_state();

    // Set the fonts
    nano_init_fonts(&nano_fonts, 16.0f);

    nano_app.settings = nano_default_settings();

    nano_app.settings.gfx.msaa.sample_count = nano_app.wgpu->desc.sample_count;

    LOG("NANO: Initialized\n");
}

// Free any resources that were allocated
static void nano_default_cleanup(void) {

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

// The event function is called by sokol_app whenever an event
// occurs and it passes the event to simgui_handle_event to handle
// the event
static void nano_default_event(const void *e) { /* simgui_handle_event(e); */ }

// This represents the demo window that has all of the Nano application
// information and settings. This is a simple window that can be toggled on and
// off. This is a simple example of how to use the ImGui API to create a window
// for a Nano application.
static void nano_demo_window() {

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
            if (igBeginMenu("File", true)) {
                if (igMenuItem_BoolPtr("New", NULL, false, true)) {
                    // Do something
                }
                if (igMenuItem_BoolPtr("Open", NULL, false, true)) {
                    // Do something
                }
                if (igMenuItem_BoolPtr("Save", NULL, false, true)) {
                    // Do something
                }
                if (igMenuItem_BoolPtr("Save As", NULL, false, true)) {
                    // Do something
                }
                igSeparator();
                if (igMenuItem_BoolPtr("Exit", NULL, false, true)) {
                    // Do something
                }
                igEndMenu();
            }

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
            char font_names[] = "JetBrains Mono Nerd Font\0Lilex Nerd "
                                "Font\0Roboto\0";
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
                nano_app.font_info.update_font = true;
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
                            nano_deactivate_shader(nano_get_shader(
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
                        // When found, get he shader information and display it
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
                                             size, ImGuiInputTextFlags_ReadOnly,
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

// A function that draws a CImGui frame of the current nano_app
// state Include collapsibles for all nested structs
static void nano_draw_debug_ui() {

    assert(nano_app.wgpu != NULL && "Nano WGPU app is NULL\n");
    assert(nano_app.wgpu->device != NULL && "Nano WGPU device is NULL\n");
    assert(nano_app.wgpu->swapchain != NULL && "Nano WGPU swapchain is NULL\n");
    assert(nano_app.wgpu->cmd_encoder != NULL &&
           "Nano WGPU command encoder is NULL\n");

    WGPUCommandEncoder cmd_encoder = nano_app.wgpu->cmd_encoder;

    // Necessary to call before starting a new imgui frame
    // this calls igNewFrame()
    // This handles all the overhead needed to get a new imgui frame
    nano_cimgui_new_frame();
    
    // Draw the demo window as part of this imgui frame
    nano_demo_window();

    // Get the swapchain info for the current frame so we can
    // pass the info on to the nano_cimgui_end_frame() function
    nano_cimgui_end_frame(cmd_encoder, wgpu_get_render_view, wgpu_get_resolve_view);

    // --------------------------
    // End of the Dear ImGui frame
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
                    .storeOp = WGPUStoreOp_Store, // Store the clear value for
                                                  // the frame
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

    return nano_app.wgpu->cmd_encoder;
}

// Function called at the end of the frame to present the frame
// and update the nano app state for the next frame.
// This method should be called after nano_start_frame().
static void nano_end_frame() {

    assert(nano_app.wgpu != NULL && "Nano WGPU app is NULL\n");
    assert(nano_app.wgpu->device != NULL && "Nano WGPU device is NULL\n");
    assert(nano_app.wgpu->swapchain != NULL && "Nano WGPU swapchain is NULL\n");
    assert(nano_app.wgpu->cmd_encoder != NULL &&
           "Nano WGPU command encoder is NULL\n");

    // Show the debug GUI for the Nano application
    if (nano_app.show_debug) {
        nano_draw_debug_ui();
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

    // Settings to update at the end of the frame
    // ------------------------------------------

    // Update the MSAA settings
    if (nano_app.settings.gfx.msaa.msaa_changed) {
        uint8_t current_item_id = nano_app.settings.gfx.msaa.msaa_index;
        uint8_t current_item = nano_app.settings.gfx.msaa.msaa_values[current_item_id];
        if (current_item ==
            nano_app.settings.gfx.msaa.sample_count) {
            nano_app.settings.gfx.msaa.msaa_changed = false;
        } else {
            nano_app.settings.gfx.msaa.sample_count =
                current_item;
            nano_app.wgpu->desc.sample_count =
                current_item;
            wgpu_swapchain_reinit(nano_app.wgpu);
            nano_app.settings.gfx.msaa.msaa_changed = false;
            nano_init_fonts(&nano_app.font_info, nano_app.font_info.font_size);
        }
    }

    // Update the font size if the flag is set
    if (nano_app.font_info.update_font) {
        nano_init_fonts(&nano_app.font_info, nano_app.font_info.font_size);
    }
}
