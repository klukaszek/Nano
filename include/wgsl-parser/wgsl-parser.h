#include "webgpu.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definitions
#define MAX_IDENT_LENGTH 256
#define MAX_ENTRIES 3 // Compute, Vertex, Fragment
#define MAX_GROUPS 4
#define MAX_BINDINGS 16

// -----------------------------------------------

// Shader Type Information
typedef enum ShaderType {
    NONE,
    COMPUTE,
    VERTEX,
    FRAGMENT,
} ShaderType;

typedef enum BindingType {
    BUFFER,
    TEXTURE,
    STORAGE_TEXTURE,
} BindingType;

// typedefs for the necessary binding information
// this is used to create a union for the different binding types
typedef WGPUBufferUsageFlags BufferInfo;
typedef WGPUSamplerBindingType SamplerInfo;
typedef struct TextureInfo {
    WGPUTextureSampleType sample_type;
    WGPUTextureViewDimension view_dimension;
    WGPUTextureUsageFlags usage;
} TextureInfo;

typedef struct StorageTextureInfo {
    WGPUStorageTextureAccess access;
    WGPUTextureFormat format;
    WGPUTextureViewDimension view_dimension;
    WGPUTextureUsageFlags usage;
} StorageTextureInfo;

// -----------------------------------------------

// Type Information

typedef enum {
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_I32,
    TYPE_U32,
    TYPE_F32,
    TYPE_F16,
    TYPE_VEC2,
    TYPE_VEC3,
    TYPE_VEC4,
    TYPE_MAT2X2,
    TYPE_MAT2X3,
    TYPE_MAT2X4,
    TYPE_MAT3X2,
    TYPE_MAT3X3,
    TYPE_MAT3X4,
    TYPE_MAT4X2,
    TYPE_MAT4X3,
    TYPE_MAT4X4,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_TEXTURE,
    TYPE_SAMPLER,
    TYPE_POINTER,
    TYPE_ATOMIC,
    TYPE_CUSTOM
} WGSLType;

/* ------------------------------------------*/

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t z;
} WorkgroupSize;

typedef struct {
    int8_t vertex;
    int8_t fragment;
    int8_t compute;
} ShaderIndices;

typedef struct {
    bool in_use;
    size_t size;
    
    BindingType binding_type;
    
    // Store the reference to whatever the binding is
    union {
        WGPUBuffer buffer;
        WGPUTexture texture;
        WGPUTextureView texture_view;
    } data;
    
    // Store the information about the binding
    union {
        BufferInfo buffer_usage;
        TextureInfo texture_info;
    } info;

    int group;
    int binding;

    uint32_t shader_id;// Iterate over bindings and print the buffer number for each binding
    
    char data_type[32];
    char name[MAX_IDENT_LENGTH];
} BindingInfo;

typedef struct PipelineLayout {
    WGPUBindGroupLayout bg_layouts[MAX_GROUPS];
    size_t num_layouts;
} PipelineLayout;

// Each entry point can have a different pipeline
typedef struct {
    char entry[MAX_IDENT_LENGTH];
    char label[64];
    bool in_use;
    ShaderType type;
    WorkgroupSize workgroup_size;
} EntryPoint;

// Each shader can have multiple entry points
// For example, a compute shader, a vertex shader, and a fragment shader
// The bindings defined in the shader are shared between all entry points
// so we store them in the ShaderInfo struct
// We can then use the binding information to create the bind group layouts for
// each entry point pipeline
typedef struct {

    uint32_t id;
    bool in_use;
    int binding_count;

    char *source;
    const char *path;
    const char *label;

    // Initialized to -1 to indicate that the group is not set
    // Any binding index that is -1 is not used
    // When creating the bind group layouts, we will skip any unused bindings
    int group_indices[MAX_GROUPS][MAX_BINDINGS];
    BindingInfo bindings[MAX_ENTRIES];

    int entry_point_count;
    EntryPoint entry_points[MAX_ENTRIES];

    ShaderIndices entry_indices;

    PipelineLayout layout;

    WGPUComputePipeline compute_pipeline;
    WGPURenderPipeline render_pipeline;

} ShaderInfo;

typedef struct {
    const char *input;
    int position;
} Parser;

// File IO
// -----------------------------------------------
// Function to read a shader into a string
char *read_file(const char *filename);

// Parser
// -----------------------------------------------
void init_parser(Parser *parser, const char *input);

char peek(Parser *parser);

char next(Parser *parser);

int is_eof(Parser *parser);

void skip_whitespace(Parser *parser);

int parse_number(Parser *parser);

void parse_identifier(Parser *parser, char *ident, bool is_type);

WGPUFlags parse_storage_class_and_access(Parser *parser);

WGSLType parse_type(Parser *parser);

BindingType parse_binding_type(Parser *parser);

void parse_sampler(Parser *parser, BindingInfo *bi);

void parse_texture(Parser *parser, BindingInfo *bi);

void parse_storage_texture(Parser *parser, BindingInfo *bi);

void parse_binding(Parser *parser, ShaderInfo *info);

void parse_entry_point(Parser *parser, ShaderInfo *info);

void parse_shader(Parser *parser, ShaderInfo *info);

void print_shader_info(ShaderInfo *info);
