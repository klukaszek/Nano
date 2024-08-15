#ifndef NANO_CIMGUI_IMPL

#define NANO_CIMGUI_IMPL
#include "emscripten/emscripten.h"
#include "emscripten/html5.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include <assert.h>
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <webgpu/webgpu.h>

// Replace ImGui macros with standard C equivalents
#define IM_ASSERT(expr) assert(expr)
#define IM_ALLOC(size) malloc(size)
#define IM_FREE(ptr) free(ptr)

#ifdef NANO_CIMGUI_DEBUG
    #define ILOG(...) printf(__VA_ARGS__);
#else
    #define ILOG(...)
#endif

// ----------------------------------------------------------------------------

// WGPU CImGUI Shaders
const char *__shader_vert_wgsl =
    "struct VertexInput {\n"
    "    @location(0) position: vec2<f32>,\n"
    "    @location(1) uv: vec2<f32>,\n"
    "    @location(2) color: vec4<f32>,\n"
    "};\n"
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "    @location(0) color: vec4<f32>,\n"
    "    @location(1) uv: vec2<f32>,\n"
    "};\n"
    "struct Uniforms {\n"
    "    mvp: mat4x4<f32>,\n"
    "};\n"
    "@group(0) @binding(0) var<uniform> uniforms: Uniforms;\n"
    "@vertex\n"
    "fn main(in: VertexInput) -> VertexOutput {\n"
    "    var out: VertexOutput;\n"
    "    out.position = uniforms.mvp * vec4<f32>(in.position, 0.0, 1.0);\n"
    "    out.color = in.color;\n"
    "    out.uv = in.uv;\n"
    "    return out;\n"
    "}\n";

const char *__shader_frag_wgsl =
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "    @location(0) color: vec4<f32>,\n"
    "    @location(1) uv: vec2<f32>,\n"
    "};\n"
    "@group(0) @binding(1) var s: sampler;\n"
    "@group(0) @binding(2) var t: texture_2d<f32>;\n"
    "@fragment\n"
    "fn main(in: VertexOutput) -> @location(0) vec4<f32> {\n"
    "    return in.color * textureSample(t, s, in.uv);\n"
    "}\n";

// Keys
// ----------------------------------------------------------------------------
typedef enum {
    KEY_INVALID,
    KEY_SPACE,
    KEY_APOSTROPHE,
    KEY_COMMA,
    KEY_MINUS,
    KEY_PERIOD,
    KEY_SLASH,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_SEMICOLON,
    KEY_EQUAL,
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_LEFT_BRACKET,
    KEY_BACKSLASH,
    KEY_RIGHT_BRACKET,
    KEY_GRAVE_ACCENT,
    KEY_WORLD_1,
    KEY_WORLD_2,
    KEY_ESCAPE,
    KEY_ENTER,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_INSERT,
    KEY_DELETE,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_CAPS_LOCK,
    KEY_SCROLL_LOCK,
    KEY_NUM_LOCK,
    KEY_PRINT_SCREEN,
    KEY_PAUSE,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,
    KEY_F25,
    KEY_KP_0,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_KP_DECIMAL,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    KEY_KP_ENTER,
    KEY_KP_EQUAL,
    KEY_LEFT_SHIFT,
    KEY_LEFT_CONTROL,
    KEY_LEFT_ALT,
    KEY_LEFT_SUPER,
    KEY_RIGHT_SHIFT,
    KEY_RIGHT_CONTROL,
    KEY_RIGHT_ALT,
    KEY_RIGHT_SUPER,
    KEY_MENU,
} wgpu_keycode;

// ----------------------------------------------------------------------------

// Structures
typedef struct nano_cimgui_data {

    ImGuiContext *imguiContext;
    WGPUDevice wgpuDevice;
    WGPUQueue defaultQueue;
    WGPUCommandEncoder cmdEncoder;
    WGPUTextureFormat renderTargetFormat;
    WGPUTextureFormat depthStencilFormat;
    uint32_t numFramesInFlight;
    uint32_t frameIndex;
    float deltaTime;
    uint8_t multiSampleCount;

    // Key States
    double LastKeyPressTime[512];
    bool KeyDown[512];
    double KeyRepeatDelay;
    double KeyRepeatRate;

    // WGPU stuff
    WGPURenderPipeline PipelineState;
    WGPUCommandEncoder DefaultCommandEncoder;
    WGPUTexture FontTexture;
    WGPUTextureView FontTextureView;
    WGPUSampler Sampler;
    WGPUBuffer Uniforms;
    WGPUBindGroup CommonBindGroup;
    WGPUBuffer VertexBuffer;
    WGPUBuffer IndexBuffer;
    uint32_t VertexBufferSize;
    uint32_t IndexBufferSize;
} nano_cimgui_data;

// ----------------------------------------------------------------------------

// Forward declarations
nano_cimgui_data *nano_cimgui_init(WGPUDevice device, int num_frames_in_flight,
                                   WGPUTextureFormat render_target_format,
                                   WGPUTextureFormat depth_stencil_format,
                                   float res_X, float res_Y, float width,
                                   float height, uint32_t multiSampleCount,
                                   ImGuiContext *ctx);
void nano_cimgui_shutdown(void);
void nano_cimgui_new_frame(void);
void nano_cimgui_set_encoder(WGPUCommandEncoder encoder);
void nano_cimgui_render_draw_data(ImDrawData *draw_data,
                                  WGPURenderPassEncoder pass_encoder);
void nano_cimgui_end_frame(WGPUCommandEncoder cmd_encoder,
                           WGPUTextureView (*get_render_view)(void),
                           WGPUTextureView (*get_resolve_view)(void));
bool nano_cimgui_create_device_objects();
void nano_cimgui_invalidate_device_objects(void);
WGPUShaderModule nano_cimgui_create_shader_module(WGPUDevice device,
                                                  const char *source);
bool nano_cimgui_create_font_textures(void);
void nano_cimgui_process_key_event(int key, bool down);
// Don't use this if you aren't using my WGPU backend
ImGuiKey nano_cimgui_wgpukey_to_imguikey(int keycode);
void nano_cimgui_process_char_event(unsigned int c);
void nano_cimgui_process_mousewheel_event(float delta);
void nano_cimgui_process_mousepress_event(int button, bool down);
void nano_cimgui_process_mousepos_event(float x, float y);
void nano_cimgui_scale_to_canvas(float res_x, float res_y, float width,
                                 float height);

bool cimgui_ready = false;

// Function Implementations
// ----------------------------------------------------------------------------

// Helper function to retrieve the backend data
nano_cimgui_data *nano_cimgui_get_backend_data(void) {
    if (!cimgui_ready)
        return NULL;
    return (nano_cimgui_data *)igGetIO()->BackendRendererUserData;
}

// Create a shader module from WGSL source code
WGPUShaderModule nano_cimgui_create_shader_module(WGPUDevice device,
                                                  const char *source) {
    // Set up the WGSL shader descriptor
    WGPUShaderModuleWGSLDescriptor wgsl_desc = {
        .chain =
            (WGPUChainedStruct){.next = NULL,
                                .sType = WGPUSType_ShaderModuleWGSLDescriptor},
        .code = source};
    // Create the shader module descriptor
    WGPUShaderModuleDescriptor desc = {.nextInChain = &wgsl_desc.chain};
    // Create and return the shader module
    return wgpuDeviceCreateShaderModule(device, &desc);
}

// Initialize the WGPU backend for ImGui
nano_cimgui_data *nano_cimgui_init(WGPUDevice device, int num_frames_in_flight,
                                   WGPUTextureFormat render_target_format,
                                   WGPUTextureFormat depth_stencil_format,
                                   float res_x, float res_y, float width,
                                   float height, uint32_t multiSampleCount,
                                   ImGuiContext *ctx) {

    if (ctx == NULL) {
        ctx = igCreateContext(NULL);
        if (ctx == NULL)
            return NULL;
        igSetCurrentContext(ctx);
    } else {
        igSetCurrentContext(ctx);
    }

    // Get the ImGui IO
    ImGuiIO *io = igGetIO();

    // Set initial display size
    io->DisplaySize = (ImVec2){width, height};
    IM_ASSERT(io->BackendRendererUserData == NULL &&
              "Already initialized a renderer backend!");

    // Set up key mapping and backend flags
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io->BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io->BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

    io->BackendPlatformName = "cimgui_impl_webgpu";

    // Allocate and initialize backend data
    nano_cimgui_data *bd =
        (nano_cimgui_data *)IM_ALLOC(sizeof(nano_cimgui_data));
    if (bd == NULL)
        return NULL;

    bd->imguiContext = ctx;
    memset(bd, 0, sizeof(nano_cimgui_data));
    memset(bd->LastKeyPressTime, 0, sizeof(bd->LastKeyPressTime));
    memset(bd->KeyDown, 0, sizeof(bd->KeyDown));
    bd->KeyRepeatDelay = 0.5; // Delay before key repeat starts
    bd->KeyRepeatRate = 0.1;  // Rate at which key repeat events are sent

    // Set backend data and flags
    io->BackendRendererUserData = (void *)bd;
    io->BackendRendererName = "imgui_impl_webgpu";
    io->BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    // Initialize WGPU-specific data
    bd->wgpuDevice = device;
    bd->defaultQueue = wgpuDeviceGetQueue(bd->wgpuDevice);
    bd->renderTargetFormat = render_target_format;
    bd->depthStencilFormat = depth_stencil_format;
    bd->numFramesInFlight = (uint32_t)num_frames_in_flight;
    bd->frameIndex = UINT32_MAX;
    bd->deltaTime = 0.0f;
    bd->multiSampleCount = multiSampleCount;

    // Initialize buffer sizes
    bd->VertexBufferSize = 5000;
    bd->IndexBufferSize = 10000;

    // Set up ImGui style scaling
    nano_cimgui_scale_to_canvas(res_x, res_y, width, height);
    
    cimgui_ready = true;

    return bd;
}

// Shutdown the WGPU backend
void nano_cimgui_shutdown(void) {
    nano_cimgui_data *bd = nano_cimgui_get_backend_data();
    IM_ASSERT(bd != NULL &&
              "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO *io = igGetIO();

    // Invalidate device objects
    nano_cimgui_invalidate_device_objects();

    // Release resources
    wgpuQueueRelease(bd->defaultQueue);
    bd->wgpuDevice = NULL;
    bd->numFramesInFlight = 0;
    bd->frameIndex = UINT32_MAX;

    // Clear backend data
    io->BackendRendererName = NULL;
    io->BackendRendererUserData = NULL;
    io->BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    IM_FREE(bd);
}

// Prepare for a new frame
void nano_cimgui_new_frame(void) {
    nano_cimgui_data *bd = nano_cimgui_get_backend_data();
    IM_ASSERT(bd != NULL && "Did you call nano_cimgui_init()?");

    ImGuiIO *io = igGetIO();

    // Update display size for window resizing
    int width, height;
    emscripten_get_canvas_element_size("#canvas", &width, &height);
    io->DisplaySize = (ImVec2){(float)width, (float)height};

    // Update time step (targeting 144 FPS MAX for ImGui)
    double current_time = emscripten_get_now() / 1000.0;
    io->DeltaTime = bd->deltaTime ? (float)(current_time - bd->deltaTime)
                                  : (float)(1.0f / 144.0f);
    bd->deltaTime = current_time;

    // Create device objects if not already created
    nano_cimgui_create_device_objects();

    // Start new frame
    igNewFrame();
}

// Set the default command encoder for the WGPU backend
void nano_cimgui_set_encoder(WGPUCommandEncoder encoder) {
    nano_cimgui_data *bd = nano_cimgui_get_backend_data();
    bd->DefaultCommandEncoder = encoder;
}

// Handle rendering of ImGui's draw data using an existing WGPURenderPassEncoder
void nano_cimgui_render_draw_data(ImDrawData *draw_data,
                                  WGPURenderPassEncoder pass_encoder) {
    nano_cimgui_data *bd = nano_cimgui_get_backend_data();

    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    // Create and grow vertex/index buffers if needed
    size_t vertex_size =
        ((size_t)draw_data->TotalVtxCount * sizeof(ImDrawVert) + 3) & ~3;
    size_t index_size =
        ((size_t)draw_data->TotalIdxCount * sizeof(ImDrawIdx) + 3) & ~3;

    // Resize vertex buffer if necessary
    if (bd->VertexBuffer == NULL ||
        bd->VertexBufferSize < draw_data->TotalVtxCount) {
        if (bd->VertexBuffer) {
            wgpuBufferDestroy(bd->VertexBuffer);
        }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        WGPUBufferDescriptor vb_desc = {
            .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
            .size =
                ((size_t)bd->VertexBufferSize * sizeof(ImDrawVert) + 3) & ~3,
            .label = "Dear ImGui Vertex Buffer"};
        bd->VertexBuffer = wgpuDeviceCreateBuffer(bd->wgpuDevice, &vb_desc);
    }

    // Resize index buffer if necessary
    if (bd->IndexBuffer == NULL ||
        bd->IndexBufferSize < draw_data->TotalIdxCount) {
        if (bd->IndexBuffer) {
            wgpuBufferDestroy(bd->IndexBuffer);
        }
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        WGPUBufferDescriptor ib_desc = {
            .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst,
            .size = ((size_t)bd->IndexBufferSize * sizeof(ImDrawIdx) + 3) & ~3,
            .label = "Dear ImGui Index Buffer"};
        bd->IndexBuffer = wgpuDeviceCreateBuffer(bd->wgpuDevice, &ib_desc);
    }

    // Upload vertex/index data
    ImDrawVert *vtx_dst = (ImDrawVert *)malloc(vertex_size);
    ImDrawIdx *idx_dst = (ImDrawIdx *)malloc(index_size);
    ImDrawVert *vtx_ptr = vtx_dst;
    ImDrawIdx *idx_ptr = idx_dst;

    // Copy vertex and index data from all command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList *cmd_list = draw_data->CmdLists.Data[n];
        memcpy(vtx_ptr, cmd_list->VtxBuffer.Data,
               cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_ptr, cmd_list->IdxBuffer.Data,
               cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_ptr += cmd_list->VtxBuffer.Size;
        idx_ptr += cmd_list->IdxBuffer.Size;
    }

    // Write vertex and index data to GPU buffers
    wgpuQueueWriteBuffer(bd->defaultQueue, bd->VertexBuffer, 0, vtx_dst,
                         vertex_size);
    wgpuQueueWriteBuffer(bd->defaultQueue, bd->IndexBuffer, 0, idx_dst,
                         index_size);

    // Free temporary buffers
    free(vtx_dst);
    free(idx_dst);

    // Setup orthographic projection matrix
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    float mvp[4][4] = {
        {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
        {0.0f, 0.0f, 0.5f, 0.0f},
        {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
    };
    wgpuQueueWriteBuffer(bd->defaultQueue, bd->Uniforms, 0, mvp, sizeof(mvp));

    // Setup render state
    wgpuRenderPassEncoderSetPipeline(pass_encoder, bd->PipelineState);
    wgpuRenderPassEncoderSetBindGroup(pass_encoder, 0, bd->CommonBindGroup, 0,
                                      NULL);
    wgpuRenderPassEncoderSetVertexBuffer(pass_encoder, 0, bd->VertexBuffer, 0,
                                         WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass_encoder, bd->IndexBuffer,
                                        sizeof(ImDrawIdx) == 2
                                            ? WGPUIndexFormat_Uint16
                                            : WGPUIndexFormat_Uint32,
                                        0, WGPU_WHOLE_SIZE);

    // Render command lists
    size_t global_vtx_offset = 0;
    size_t global_idx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList *cmd_list = draw_data->CmdLists.Data[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer.Data[cmd_i];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmd_list, pcmd);
            } else {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min = {pcmd->ClipRect.x - clip_off.x,
                                   pcmd->ClipRect.y - clip_off.y};
                ImVec2 clip_max = {pcmd->ClipRect.z - clip_off.x,
                                   pcmd->ClipRect.w - clip_off.y};
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                wgpuRenderPassEncoderSetScissorRect(
                    pass_encoder, (uint32_t)clip_min.x, (uint32_t)clip_min.y,
                    (uint32_t)(clip_max.x - clip_min.x),
                    (uint32_t)(clip_max.y - clip_min.y));

                // Draw
                wgpuRenderPassEncoderDrawIndexed(
                    pass_encoder, pcmd->ElemCount, 1,
                    pcmd->IdxOffset + global_idx_offset,
                    pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}

typedef struct nano_cimgui_swapchain_info {
    WGPUTextureView render_view;
    WGPUTextureView resolve_view;
} nano_cimgui_swapchain_info;

// End the frame, create the render data and render pass.
// This function requires a command encoder and swapchain info that contains the
// render and resolve views. This function calls nano_cimgui_render_draw_data()
// to render the ImGui draw data on the render pass created in this function.
void nano_cimgui_end_frame(WGPUCommandEncoder cmd_encoder,
                           WGPUTextureView (*get_render_view)(void),
                           WGPUTextureView (*get_resolve_view)(void)) {
    assert(cmd_encoder != NULL && "Command encoder was NULL");

    // Get data from the swapchain info
    nano_cimgui_data *bd = nano_cimgui_get_backend_data();

    igRender();
    if (igGetDrawData() == NULL)
        return;

    // Set the ImGui encoder to our current encoder
    // This is necessary to render the ImGui draw data
    nano_cimgui_set_encoder(cmd_encoder);

    // Create a generic render pass color attachment
    WGPURenderPassColorAttachment color_attachment = {
        .view = get_render_view(),
        // We set the depth slice to 0xFFFFFFFF to indicate that the
        // depth slice is not used.
        .depthSlice = ~0u,
        // If our view is a texture view (MSAA Samples > 1), we need
        // to resolve the texture to the swapchain texture (this is
        // resolved in resolve_view)
        .resolveTarget = get_resolve_view(),
        .loadOp = WGPULoadOp_Load,
        .storeOp = WGPUStoreOp_Store,
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
        ILOG("nano_cimgui_end_frame() -> Failed to begin default render pass "
             "encoder.\n");
        wgpuCommandEncoderRelease(cmd_encoder);
        return;
    }

    // Render ImGui Draw Data
    // This will be refactored into a nano_cimgui_render function
    // I am thinking of making all nano + cimgui functionality
    // optional using macros
    nano_cimgui_render_draw_data(igGetDrawData(), render_pass);

    wgpuRenderPassEncoderEnd(render_pass);
}

// Create WGPU device objects (pipeline, buffers, textures, etc.)
bool nano_cimgui_create_device_objects() {
    nano_cimgui_data *bd = nano_cimgui_get_backend_data();
    if (!bd->wgpuDevice)
        return false;
    if (bd->PipelineState)
        nano_cimgui_invalidate_device_objects();

    // Create vertex and fragment shaders
    WGPUShaderModule vert_module =
        nano_cimgui_create_shader_module(bd->wgpuDevice, __shader_vert_wgsl);
    WGPUShaderModule frag_module =
        nano_cimgui_create_shader_module(bd->wgpuDevice, __shader_frag_wgsl);

    // Create pipeline layout
    WGPUBindGroupLayoutEntry bg_entries[3] = {
        // Vertex shader uniform buffer
        [0] =
            (WGPUBindGroupLayoutEntry){
                .binding = 0,
                .visibility = WGPUShaderStage_Vertex,
                .buffer =
                    (WGPUBufferBindingLayout){
                        .type = WGPUBufferBindingType_Uniform,
                        .hasDynamicOffset = false,
                        .minBindingSize = sizeof(float) * 16 // 4x4 matrix
                    },
                .sampler = {0},
                .texture = {0},
                .storageTexture = {0}},
        // Fragment shader sampler
        [1] =
            (WGPUBindGroupLayoutEntry){
                .binding = 1,
                .visibility = WGPUShaderStage_Fragment,
                .sampler =
                    (WGPUSamplerBindingLayout){
                        .type = WGPUSamplerBindingType_Filtering},
                .texture = {0},
                .storageTexture = {0}},
        // Fragment shader texture
        [2] = (WGPUBindGroupLayoutEntry){
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .sampler = {0},
            .texture =
                (WGPUTextureBindingLayout){
                    .sampleType = WGPUTextureSampleType_Float,
                    .viewDimension = WGPUTextureViewDimension_2D},
            .storageTexture = {0}}};

    // Create bind group layout
    WGPUBindGroupLayoutDescriptor bg_layout_desc = {.entryCount = 3,
                                                    .entries = bg_entries};
    WGPUBindGroupLayout bind_group_layout =
        wgpuDeviceCreateBindGroupLayout(bd->wgpuDevice, &bg_layout_desc);

    // Create pipeline layout
    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .bindGroupLayoutCount = 1, .bindGroupLayouts = &bind_group_layout};
    WGPUPipelineLayout pipeline_layout =
        wgpuDeviceCreatePipelineLayout(bd->wgpuDevice, &pipeline_layout_desc);

    // Define vertex attributes
    WGPUVertexAttribute vertex_attributes[3] = {
        [0] = (WGPUVertexAttribute){.format = WGPUVertexFormat_Float32x2,
                                    .offset = offsetof(ImDrawVert, pos),
                                    .shaderLocation = 0},
        [1] = (WGPUVertexAttribute){.format = WGPUVertexFormat_Float32x2,
                                    .offset = offsetof(ImDrawVert, uv),
                                    .shaderLocation = 1},
        [2] = (WGPUVertexAttribute){.format = WGPUVertexFormat_Unorm8x4,
                                    .offset = offsetof(ImDrawVert, col),
                                    .shaderLocation = 2}};

    // Define vertex buffer layout
    WGPUVertexBufferLayout vertex_buffer_layout = {
        .arrayStride = sizeof(ImDrawVert),
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 3,
        .attributes = vertex_attributes};

    // Define color target state (blending)
    WGPUColorTargetState color_target_state = {
        .format = bd->renderTargetFormat,
        .blend =
            &(WGPUBlendState){
                .color =
                    (WGPUBlendComponent){.operation = WGPUBlendOperation_Add,
                                         .srcFactor = WGPUBlendFactor_SrcAlpha,
                                         .dstFactor =
                                             WGPUBlendFactor_OneMinusSrcAlpha},
                .alpha =
                    (WGPUBlendComponent){
                        .operation = WGPUBlendOperation_Add,
                        .srcFactor = WGPUBlendFactor_One,
                        .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                    }},
        .writeMask = WGPUColorWriteMask_All};

    // Create render pipeline
    WGPURenderPipelineDescriptor pipeline_desc = {
        .layout = pipeline_layout,
        .label = "Dear ImGui Pipeline",
        .vertex = (WGPUVertexState){.module = vert_module,
                                    .entryPoint = "main",
                                    .bufferCount = 1,
                                    .buffers = &vertex_buffer_layout},
        .primitive =
            (WGPUPrimitiveState){.topology = WGPUPrimitiveTopology_TriangleList,
                                 .stripIndexFormat = WGPUIndexFormat_Undefined,
                                 .frontFace = WGPUFrontFace_CW,
                                 .cullMode = WGPUCullMode_None},
        .multisample = (WGPUMultisampleState){.count = bd->multiSampleCount,
                                              .mask = ~0u,
                                              .alphaToCoverageEnabled = false},
        .fragment = &(WGPUFragmentState){.module = frag_module,
                                         .entryPoint = "main",
                                         .targetCount = 1,
                                         .targets = &color_target_state}};

    bd->PipelineState =
        wgpuDeviceCreateRenderPipeline(bd->wgpuDevice, &pipeline_desc);

    // Cleanup
    wgpuBindGroupLayoutRelease(bind_group_layout);
    wgpuPipelineLayoutRelease(pipeline_layout);
    wgpuShaderModuleRelease(vert_module);
    wgpuShaderModuleRelease(frag_module);

    // Create font texture
    if (!nano_cimgui_create_font_textures())
        return false;

    return bd->PipelineState != NULL;
}

// Create font texture for ImGui using WGPU
bool nano_cimgui_create_font_textures() {
    nano_cimgui_data *bd = nano_cimgui_get_backend_data();
    ImGuiIO *io = igGetIO();

    // Get font atlas data
    unsigned char *pixels;
    int width, height;
    ImFontAtlas_GetTexDataAsRGBA32(io->Fonts, &pixels, &width, &height, NULL);

    // Create texture descriptor
    WGPUTextureDescriptor tex_desc = {
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = (WGPUExtent3D){.width = (uint32_t)width,
                               .height = (uint32_t)height,
                               .depthOrArrayLayers = 1},
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    if (bd->FontTexture)
        wgpuTextureDestroy(bd->FontTexture);
    bd->FontTexture = wgpuDeviceCreateTexture(bd->wgpuDevice, &tex_desc);

    // Create texture view descriptor
    WGPUTextureViewDescriptor tex_view_desc = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All};

    if (bd->FontTextureView)
        wgpuTextureViewRelease(bd->FontTextureView);
    bd->FontTextureView =
        wgpuTextureCreateView(bd->FontTexture, &tex_view_desc);

    // Write texture data
    WGPUImageCopyTexture destination = {.texture = bd->FontTexture,
                                        .mipLevel = 0,
                                        .origin = (WGPUOrigin3D){0, 0, 0},
                                        .aspect = WGPUTextureAspect_All};
    WGPUTextureDataLayout source = {.offset = 0,
                                    .bytesPerRow = (uint32_t)width * 4,
                                    .rowsPerImage = (uint32_t)height};
    WGPUExtent3D size = {.width = (uint32_t)width,
                         .height = (uint32_t)height,
                         .depthOrArrayLayers = 1};
    wgpuQueueWriteTexture(bd->defaultQueue, &destination, pixels,
                          width * height * 4, &source, &size);

    // Create sampler
    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
    };
    if (bd->Sampler)
        wgpuSamplerRelease(bd->Sampler);
    bd->Sampler = wgpuDeviceCreateSampler(bd->wgpuDevice, &sampler_desc);

    // Create uniform buffer
    WGPUBufferDescriptor uniform_buffer_desc = {
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = 64, // 4x4 matrix
        .mappedAtCreation = false,
        .label = "Dear ImGui Uniform Buffer"};
    if (bd->Uniforms)
        wgpuBufferDestroy(bd->Uniforms);
    bd->Uniforms = wgpuDeviceCreateBuffer(bd->wgpuDevice, &uniform_buffer_desc);

    if (!bd->PipelineState)
        printf("Pipeline state is null\n");

    // Create bind group
    WGPUBindGroupEntry entries[3] = {
        {.binding = 0, .buffer = bd->Uniforms, .offset = 0, .size = 64},
        {.binding = 1, .sampler = bd->Sampler},
        {.binding = 2, .textureView = bd->FontTextureView}};
    WGPUBindGroupDescriptor bg_desc = {
        .layout = wgpuRenderPipelineGetBindGroupLayout(bd->PipelineState, 0),
        .entryCount = 3,
        .entries = entries};
    if (bd->CommonBindGroup)
        wgpuBindGroupRelease(bd->CommonBindGroup);
    bd->CommonBindGroup = wgpuDeviceCreateBindGroup(bd->wgpuDevice, &bg_desc);

    // Set font texture ID
    ImFontAtlas_SetTexID(io->Fonts, (ImTextureID)(intptr_t)bd->FontTextureView);

    return true;
}

void nano_cimgui_invalidate_device_objects(void) {
    nano_cimgui_data *bd = nano_cimgui_get_backend_data();
    if (!bd->wgpuDevice)
        return;

    if (bd->PipelineState) {
        wgpuRenderPipelineRelease(bd->PipelineState);
        bd->PipelineState = NULL;
    }
    if (bd->FontTexture) {
        wgpuTextureDestroy(bd->FontTexture);
        bd->FontTexture = NULL;
    }
    if (bd->FontTextureView) {
        wgpuTextureViewRelease(bd->FontTextureView);
        bd->FontTextureView = NULL;
    }
    if (bd->Sampler) {
        wgpuSamplerRelease(bd->Sampler);
        bd->Sampler = NULL;
    }
    if (bd->Uniforms) {
        wgpuBufferDestroy(bd->Uniforms);
        bd->Uniforms = NULL;
    }
    if (bd->CommonBindGroup) {
        wgpuBindGroupRelease(bd->CommonBindGroup);
        bd->CommonBindGroup = NULL;
    }
    if (bd->VertexBuffer) {
        wgpuBufferDestroy(bd->VertexBuffer);
        bd->VertexBuffer = NULL;
    }
    if (bd->IndexBuffer) {
        wgpuBufferDestroy(bd->IndexBuffer);
        bd->IndexBuffer = NULL;
    }
    if (bd->VertexBuffer) {
        wgpuBufferDestroy(bd->VertexBuffer);
        bd->VertexBuffer = NULL;
    }
    if (bd->IndexBuffer) {
        wgpuBufferDestroy(bd->IndexBuffer);
        bd->IndexBuffer = NULL;
    }
    bd->VertexBufferSize = 0;
    bd->IndexBufferSize = 0;
}

void nano_cimgui_process_key_event(int key, bool down) {
    ImGuiIO *io = igGetIO();
    nano_cimgui_data *bd = nano_cimgui_get_backend_data();

    // Ensure key is within range
    if (key < 0 || key >= 512)
        return;

    // Add key event to imgui event queue
    ImGuiKey imgui_key = nano_cimgui_wgpukey_to_imguikey(key);
    if (imgui_key == ImGuiKey_None)
        return;

    // Get current time so we can check for key repeat
    double current_time = emscripten_get_now() / 1000.0;

    // Update key state
    if (down) {
        // Set imgui key to active
        if (!bd->KeyDown[key] ||
            current_time - bd->LastKeyPressTime[key] > bd->KeyRepeatRate) {
            ImGuiIO_AddKeyEvent(io, imgui_key, true);
            bd->LastKeyPressTime[key] = current_time;
        }
        // Set imgui key to inactive
    } else {
        ImGuiIO_AddKeyEvent(io, imgui_key, false);
    }

    ILOG("nano_cimgui_process_key_event() -> Key Event: %d\n", key);

    // Set key state to active
    bd->KeyDown[key] = down;
}

void nano_cimgui_process_char_event(unsigned int c) {
    ImGuiIO *io = igGetIO();
    ImGuiIO_AddInputCharacter(io, c);
}

void nano_cimgui_process_mousepress_event(int button, bool down) {
    ImGuiIO *io = igGetIO();
    // We have to swap the button order because emsc uses a different order
    if (button >= 0 && button < 5) {
        if (button == 0)
            io->MouseDown[0] = down; // Left
        if (button == 1)
            io->MouseDown[2] = down; // Right
        if (button == 2)
            io->MouseDown[1] = down; // Middle
        ILOG("nano_cimgui_process_mousepress_event() -> MB Event: %d\n", button);
    }
}

void nano_cimgui_process_mousepos_event(float x, float y) {
    ImGuiIO *io = igGetIO();
    io->MousePos = (ImVec2){x, y};
}

void nano_cimgui_process_mousewheel_event(float delta) {
    ImGuiIO *io = igGetIO();
    ImGuiIO_AddMouseWheelEvent(io, 0.0f, delta);
}

void nano_cimgui_scale_to_canvas(float res_x, float res_y, float width,
                                 float height) {

    // Calculate scale factor based on expected resolution
    float scale_x = res_x / width;
    float scale_y = res_x / height;

    // Set scale factor and ensure it is at least 1.0 (relative to 1080p)
    // This should be changed to the target resolution if we can get it
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    scale = (scale < 1.0f) ? 1.0f : scale;

    // Destroy and recreate the style to preserve the scale
    ImGuiStyle_destroy(igGetStyle());
    ImGuiStyle *new_style = ImGuiStyle_ImGuiStyle();
    ImGuiStyle_ScaleAllSizes(new_style, scale);
}

ImGuiKey nano_cimgui_wgpukey_to_imguikey(int keycode) {
    switch (keycode) {
        case KEY_TAB:
            return ImGuiKey_Tab;
        case KEY_LEFT:
            return ImGuiKey_LeftArrow;
        case KEY_RIGHT:
            return ImGuiKey_RightArrow;
        case KEY_UP:
            return ImGuiKey_UpArrow;
        case KEY_DOWN:
            return ImGuiKey_DownArrow;
        case KEY_PAGE_UP:
            return ImGuiKey_PageUp;
        case KEY_PAGE_DOWN:
            return ImGuiKey_PageDown;
        case KEY_HOME:
            return ImGuiKey_Home;
        case KEY_END:
            return ImGuiKey_End;
        case KEY_INSERT:
            return ImGuiKey_Insert;
        case KEY_DELETE:
            return ImGuiKey_Delete;
        case KEY_BACKSPACE:
            return ImGuiKey_Backspace;
        case KEY_SPACE:
            return ImGuiKey_Space;
        case KEY_ENTER:
            return ImGuiKey_Enter;
        case KEY_ESCAPE:
            return ImGuiKey_Escape;
        case KEY_APOSTROPHE:
            return ImGuiKey_Apostrophe;
        case KEY_COMMA:
            return ImGuiKey_Comma;
        case KEY_MINUS:
            return ImGuiKey_Minus;
        case KEY_PERIOD:
            return ImGuiKey_Period;
        case KEY_SLASH:
            return ImGuiKey_Slash;
        case KEY_SEMICOLON:
            return ImGuiKey_Semicolon;
        case KEY_EQUAL:
            return ImGuiKey_Equal;
        case KEY_LEFT_BRACKET:
            return ImGuiKey_LeftBracket;
        case KEY_BACKSLASH:
            return ImGuiKey_Backslash;
        case KEY_RIGHT_BRACKET:
            return ImGuiKey_RightBracket;
        case KEY_GRAVE_ACCENT:
            return ImGuiKey_GraveAccent;
        case KEY_CAPS_LOCK:
            return ImGuiKey_CapsLock;
        case KEY_SCROLL_LOCK:
            return ImGuiKey_ScrollLock;
        case KEY_NUM_LOCK:
            return ImGuiKey_NumLock;
        case KEY_PRINT_SCREEN:
            return ImGuiKey_PrintScreen;
        case KEY_PAUSE:
            return ImGuiKey_Pause;
        case KEY_KP_0:
            return ImGuiKey_Keypad0;
        case KEY_KP_1:
            return ImGuiKey_Keypad1;
        case KEY_KP_2:
            return ImGuiKey_Keypad2;
        case KEY_KP_3:
            return ImGuiKey_Keypad3;
        case KEY_KP_4:
            return ImGuiKey_Keypad4;
        case KEY_KP_5:
            return ImGuiKey_Keypad5;
        case KEY_KP_6:
            return ImGuiKey_Keypad6;
        case KEY_KP_7:
            return ImGuiKey_Keypad7;
        case KEY_KP_8:
            return ImGuiKey_Keypad8;
        case KEY_KP_9:
            return ImGuiKey_Keypad9;
        case KEY_KP_DECIMAL:
            return ImGuiKey_KeypadDecimal;
        case KEY_KP_DIVIDE:
            return ImGuiKey_KeypadDivide;
        case KEY_KP_MULTIPLY:
            return ImGuiKey_KeypadMultiply;
        case KEY_KP_SUBTRACT:
            return ImGuiKey_KeypadSubtract;
        case KEY_KP_ADD:
            return ImGuiKey_KeypadAdd;
        case KEY_KP_ENTER:
            return ImGuiKey_KeypadEnter;
        case KEY_KP_EQUAL:
            return ImGuiKey_KeypadEqual;
        case KEY_LEFT_SHIFT:
            return ImGuiKey_LeftShift;
        case KEY_LEFT_CONTROL:
            return ImGuiKey_LeftCtrl;
        case KEY_LEFT_ALT:
            return ImGuiKey_LeftAlt;
        case KEY_LEFT_SUPER:
            return ImGuiKey_LeftSuper;
        case KEY_RIGHT_SHIFT:
            return ImGuiKey_RightShift;
        case KEY_RIGHT_CONTROL:
            return ImGuiKey_RightCtrl;
        case KEY_RIGHT_ALT:
            return ImGuiKey_RightAlt;
        case KEY_RIGHT_SUPER:
            return ImGuiKey_RightSuper;
        case KEY_MENU:
            return ImGuiKey_Menu;
        case KEY_0:
            return ImGuiKey_0;
        case KEY_1:
            return ImGuiKey_1;
        case KEY_2:
            return ImGuiKey_2;
        case KEY_3:
            return ImGuiKey_3;
        case KEY_4:
            return ImGuiKey_4;
        case KEY_5:
            return ImGuiKey_5;
        case KEY_6:
            return ImGuiKey_6;
        case KEY_7:
            return ImGuiKey_7;
        case KEY_8:
            return ImGuiKey_8;
        case KEY_9:
            return ImGuiKey_9;
        case KEY_A:
            return ImGuiKey_A;
        case KEY_B:
            return ImGuiKey_B;
        case KEY_C:
            return ImGuiKey_C;
        case KEY_D:
            return ImGuiKey_D;
        case KEY_E:
            return ImGuiKey_E;
        case KEY_F:
            return ImGuiKey_F;
        case KEY_G:
            return ImGuiKey_G;
        case KEY_H:
            return ImGuiKey_H;
        case KEY_I:
            return ImGuiKey_I;
        case KEY_J:
            return ImGuiKey_J;
        case KEY_K:
            return ImGuiKey_K;
        case KEY_L:
            return ImGuiKey_L;
        case KEY_M:
            return ImGuiKey_M;
        case KEY_N:
            return ImGuiKey_N;
        case KEY_O:
            return ImGuiKey_O;
        case KEY_P:
            return ImGuiKey_P;
        case KEY_Q:
            return ImGuiKey_Q;
        case KEY_R:
            return ImGuiKey_R;
        case KEY_S:
            return ImGuiKey_S;
        case KEY_T:
            return ImGuiKey_T;
        case KEY_U:
            return ImGuiKey_U;
        case KEY_V:
            return ImGuiKey_V;
        case KEY_W:
            return ImGuiKey_W;
        case KEY_X:
            return ImGuiKey_X;
        case KEY_Y:
            return ImGuiKey_Y;
        case KEY_Z:
            return ImGuiKey_Z;
        case KEY_F1:
            return ImGuiKey_F1;
        case KEY_F2:
            return ImGuiKey_F2;
        case KEY_F3:
            return ImGuiKey_F3;
        case KEY_F4:
            return ImGuiKey_F4;
        case KEY_F5:
            return ImGuiKey_F5;
        case KEY_F6:
            return ImGuiKey_F6;
        case KEY_F7:
            return ImGuiKey_F7;
        case KEY_F8:
            return ImGuiKey_F8;
        case KEY_F9:
            return ImGuiKey_F9;
        case KEY_F10:
            return ImGuiKey_F10;
        case KEY_F11:
            return ImGuiKey_F11;
        case KEY_F12:
            return ImGuiKey_F12;
        default:
            return ImGuiKey_None;
    }
}

#endif // NANO_CIMGUI_IMPL
