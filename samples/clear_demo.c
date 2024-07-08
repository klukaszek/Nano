#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <webgpu/webgpu.h>

#define WGPU_BACKEND_DEBUG
#define NANO_DEBUG
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "nano.h"

// Global variables for default Nano App
static WGPUBuffer uniform_buffer;
static WGPUBindGroup bind_group;
static WGPURenderPipeline pipeline;
static float clear_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};

// Default wgsl shader
static const char *nano_default_shader =
    "struct Uniforms {\n"
    "    clear_color: vec4<f32>,\n"
    "};\n"
    "@binding(0) @group(0) var<uniform> uniforms: Uniforms;\n"
    "\n"
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "};\n"
    "@vertex\n"
    "fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {\n"
    "    var pos = array<vec2<f32>, 3>(\n"
    "        vec2<f32>(-1.0, -1.0),\n"
    "        vec2<f32>( 3.0, -1.0),\n"
    "        vec2<f32>(-1.0,  3.0)\n"
    "    );\n"
    "    var output: VertexOutput;\n"
    "    output.position = vec4<f32>(pos[vertex_index], 0.0, 1.0);\n"
    "    return output;\n"
    "}\n"
    "@fragment\n"
    "fn fs_main() -> @location(0) vec4<f32> {\n"
    "    return uniforms.clear_color;\n"
    "}\n";

static void init(void) {
    nano_default_init();

    WGPUDevice device = nano_app.wgpu->device;

    // Create uniform buffer
    WGPUBufferDescriptor uniform_buffer_desc = {
        .size = sizeof(clear_color),
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .mappedAtCreation = false,
    };
    uniform_buffer = wgpuDeviceCreateBuffer(device, &uniform_buffer_desc);

    // Create bind group layout
    WGPUBindGroupLayoutEntry bind_group_layout_entry = {
        .binding = 0,
        .visibility = WGPUShaderStage_Fragment,
        .buffer =
            {
                .type = WGPUBufferBindingType_Uniform,
                .hasDynamicOffset = false,
                .minBindingSize = sizeof(clear_color),
            },
        .sampler = {0},
    };
    WGPUBindGroupLayoutDescriptor bind_group_layout_desc = {
        .entryCount = 1,
        .entries = &bind_group_layout_entry,
    };
    WGPUBindGroupLayout bind_group_layout =
        wgpuDeviceCreateBindGroupLayout(device, &bind_group_layout_desc);

    // Create shader module
    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = NULL,
        .label = "Nano Default Draw WGSL Shader",
    };
    WGPUShaderModuleWGSLDescriptor shader_wgsl_desc = {
        .chain =
            (WGPUChainedStruct){.next = NULL,
                                .sType = WGPUSType_ShaderModuleWGSLDescriptor},
        .code = nano_default_shader,
    };
    shader_desc.nextInChain = &shader_wgsl_desc.chain;
    WGPUShaderModule shader =
        wgpuDeviceCreateShaderModule(device, &shader_desc);

    // Create pipeline layout
    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bind_group_layout,
    };
    WGPUPipelineLayout pipeline_layout =
        wgpuDeviceCreatePipelineLayout(device, &pipeline_layout_desc);

    // Create render pipeline
    WGPURenderPipelineDescriptor pipeline_desc = {
        .layout = pipeline_layout,
        .vertex =
            (WGPUVertexState){
                .module = shader,
                .entryPoint = "vs_main",
                .constantCount = 0,
                .constants = NULL,
                .bufferCount = 0,
                .buffers = NULL,
            },
        .primitive =
            (WGPUPrimitiveState){.topology = WGPUPrimitiveTopology_TriangleList,
                                 .stripIndexFormat = WGPUIndexFormat_Undefined,
                                 .frontFace = WGPUFrontFace_CCW,
                                 .cullMode = WGPUCullMode_None},
        .multisample =
            (WGPUMultisampleState){
                .count = 1,
                .mask = ~0u,
                .alphaToCoverageEnabled = false,
            },
        .fragment =
            &(WGPUFragmentState){
                .module = shader,
                .entryPoint = "fs_main",
                .constantCount = 0,
                .constants = NULL,
                .targetCount = 1,
                .targets =
                    &(WGPUColorTargetState){
                        .format = wgpu_get_color_format(),
                        .blend = NULL,
                        .writeMask = WGPUColorWriteMask_All,
                    },
            },
        .depthStencil = NULL,
    };

    pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeline_desc);

    // Create bind group
    WGPUBindGroupEntry bind_group_entry = {
        .binding = 0,
        .buffer = uniform_buffer,
        .offset = 0,
        .size = sizeof(clear_color),
    };
    WGPUBindGroupDescriptor bind_group_desc = {
        .layout = bind_group_layout,
        .entryCount = 1,
        .entries = &bind_group_entry,
    };
    bind_group = wgpuDeviceCreateBindGroup(device, &bind_group_desc);

    // Cleanup
    wgpuPipelineLayoutRelease(pipeline_layout);
    wgpuShaderModuleRelease(shader);
    wgpuBindGroupLayoutRelease(bind_group_layout);
}

static void frame(void) {

    // Update necessary Nano app state at beginning of frame
    nano_start_frame();

    // Create a command encoder for our default Nano render pass action
    WGPUCommandEncoderDescriptor cmd_encoder_desc = {
        .label = "Clear Screen Command Encoder",
    };
    WGPUCommandEncoder cmd_encoder = wgpuDeviceCreateCommandEncoder(
        nano_app.wgpu->device, &cmd_encoder_desc);

    // Start the Dear ImGui frame
    // --------------------------

    // Necessary to call before starting a new frame
    // Will be refactored into a proper nano_cimgui_* function
    ImGui_ImplWGPU_NewFrame();
    igNewFrame();

    igShowDemoWindow(NULL);

    // We need the bool to check if we need to update the font size
    // after the render pass has been completed
    // Render the debug UI. In the future, this will probably return
    // a struct with all critical flags that require updating after
    // the render pass has been completed.
    nano_draw_debug_ui();
    
    bool visible = true;
    igSetNextWindowRefreshPolicy(ImGuiWindowRefreshFlags_RefreshOnFocus);
    igSetNextWindowSize((ImVec2){400, 60}, ImGuiCond_FirstUseEver);
    igSetNextWindowPos((ImVec2){400, 100}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
    igBegin("Clear", &visible, 0);
    igSliderFloat4("RBGA Colour", (float *)&clear_color, 0.0f, 1.0f, "%.2f", 0);
    igEnd();
    igRender();

    wgpuQueueWriteBuffer(wgpuDeviceGetQueue(nano_app.wgpu->device),
                         uniform_buffer, 0, clear_color, sizeof(clear_color));

    // Set the ImGui encoder to our current encoder
    // This is necessary to render the ImGui draw data
    ImGui_ImplWGPU_SetEncoder(cmd_encoder);

    // --------------------------
    // End of the Dear ImGui frame

    // Get the current swapchain texture view
    WGPUTextureView back_buffer_view = wgpu_get_render_view();
    if (!back_buffer_view) {
        printf("Failed to get current swapchain texture view.\n");
        return;
    }

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
        .clearValue = (WGPUColor){clear_color[0], clear_color[1],
                                  clear_color[2], clear_color[3]},
    };

    WGPURenderPassDescriptor render_pass_desc = {
        .label = "Render Pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &color_attachment,
        .depthStencilAttachment = NULL,
    };

    WGPURenderPassEncoder render_pass =
        wgpuCommandEncoderBeginRenderPass(cmd_encoder, &render_pass_desc);
    if (!render_pass) {
        printf("NANO: Failed to begin default render pass encoder.\n");
        wgpuCommandEncoderRelease(cmd_encoder);
        return;
    }

    // Set our render pass encoder to use our pipeline and
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, bind_group, 0, NULL);
    wgpuRenderPassEncoderSetPipeline(render_pass, pipeline);
    wgpuRenderPassEncoderDraw(render_pass, 3, 1, 0, 0);

    // Render ImGui Draw Data
    // This will be refactored into a nano_cimgui_render function
    // I am thinking of making all nano + cimgui functionality optional using
    // macros
    ImGui_ImplWGPU_RenderDrawData(igGetDrawData(), render_pass);

    wgpuRenderPassEncoderEnd(render_pass);

    WGPUCommandBufferDescriptor cmd_buffer_desc = {
        .label = "Command Buffer",
    };
    WGPUCommandBuffer cmd_buffer =
        wgpuCommandEncoderFinish(cmd_encoder, &cmd_buffer_desc);
    wgpuQueueSubmit(wgpuDeviceGetQueue(nano_app.wgpu->device), 1, &cmd_buffer);

    wgpuCommandBufferRelease(cmd_buffer);
    wgpuCommandEncoderRelease(cmd_encoder);

    // Change Nano app state at end of frame
    nano_end_frame();
}

static void shutdown(void) {
    wgpuBindGroupRelease(bind_group);
    wgpuBufferDestroy(uniform_buffer);
    wgpuBufferRelease(uniform_buffer);
    nano_default_cleanup();
}

int main(int argc, char *argv[]) {
    wgpu_start(&(wgpu_desc_t){
        .title = "Solid Color Demo",
        .width = 640,
        .height = 480,
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .sample_count = 1,
    });
    return 0;
}
