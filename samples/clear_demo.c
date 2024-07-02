#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <webgpu/webgpu.h>

#define WGPU_BACKEND_DEBUG
#define CIMGUI_WGPU
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "nano.h"

static WGPURenderPipeline pipeline;
static float clear_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};

// Default wgsl shader
static const char *nano_default_shader =
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
    "    return vec4<f32>(1.0, 0.0, 0.0, 1.0);\n" // Red color
    "}\n";

static void init(void) {

    nano_default_init();

    WGPUDevice device = nano_app.wgpu.device;

    // Create shader modules
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
        .bindGroupLayoutCount = 0,
        .bindGroupLayouts = NULL,
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

    // Cleanup
    wgpuPipelineLayoutRelease(pipeline_layout);
    wgpuShaderModuleRelease(shader);

    // Setup Dear ImGui for WGPU
    igCreateContext(NULL);
    ImGuiIO *io = igGetIO();
    ImGui_ImplWGPU_Init(device, 2, wgpu_get_color_format(),
                        WGPUTextureFormat_Undefined);

    // Set initial display size
    io->DisplaySize =
        (ImVec2){(float)nano_app.dimensions.x, (float)nano_app.dimensions.y};
}

static void frame(void) {

    nano_calc_fps();

    // Update ImGui Display Size
    ImGuiIO *io = igGetIO();
    io->DisplaySize = (ImVec2){nano_app.dimensions.x, nano_app.dimensions.y};
    printf("Display Size: %f, %f\n", io->DisplaySize.x, io->DisplaySize.y);

    // Start the Dear ImGui frame
    // --------------------------

    ImGui_ImplWGPU_NewFrame();
    igNewFrame();

    // Create a simple ImGui window
    igBegin("Hello, ImGui!", NULL, 0);
    igText("This is some useful text.");
    static float f = 0.0f;
    igSliderFloat("float", &f, 0.0f, 1.0f, "%.3f", 0);
    igColorEdit3("clear color", (float *)&clear_color, 0);
    igSameLine(0.0f, -1.0f);
    igText("Application average %.3f ms/frame (%.1f FPS)",
           1000.0f / nano_app.fps, nano_app.fps);
    igEnd();

    // Render ImGui
    igRender();

    // --------------------------
    // End the Dear ImGui frame

    // Create a command encoder for our default Nano render pass action
    WGPUCommandEncoderDescriptor cmd_encoder_desc = {
        .label = "Clear Screen Command Encoder",
    };
    WGPUCommandEncoder cmd_encoder =
        wgpuDeviceCreateCommandEncoder(nano_app.wgpu.device, &cmd_encoder_desc);

    // Get the current swapchain texture view
    WGPUTextureView back_buffer_view = wgpu_get_render_view();
    if (!back_buffer_view) {
        printf("Failed to get current swapchain texture view.\n");
        return;
    }

    WGPURenderPassColorAttachment color_attachment = {
        .view = back_buffer_view,
        // We set the depth slice to 0xFFFFFFFF to indicate that the depth slice
        // is not used.
        .depthSlice = ~0u,
        // If our view is a texture view (MSAA Samples > 1), we need to resolve
        // the texture to the swapchain texture
        .resolveTarget =
            state.desc.sample_count > 1
                ? wgpuSwapChainGetCurrentTextureView(nano_app.wgpu.swapchain)
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
    wgpuRenderPassEncoderSetPipeline(render_pass, pipeline);
    wgpuRenderPassEncoderDraw(render_pass, 3, 1, 0, 0);

    // Render ImGui Draw Data
    ImGui_ImplWGPU_SetEncoder(cmd_encoder);
    ImGui_ImplWGPU_RenderDrawData(igGetDrawData(), render_pass);

    wgpuRenderPassEncoderEnd(render_pass);

    WGPUCommandBufferDescriptor cmd_buffer_desc = {
        .label = "Command Buffer",
    };
    WGPUCommandBuffer cmd_buffer =
        wgpuCommandEncoderFinish(cmd_encoder, &cmd_buffer_desc);
    wgpuQueueSubmit(wgpuDeviceGetQueue(nano_app.wgpu.device), 1, &cmd_buffer);

    wgpuCommandBufferRelease(cmd_buffer);
    wgpuCommandEncoderRelease(cmd_encoder);
}

static void shutdown(void) { nano_default_cleanup(); }

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
