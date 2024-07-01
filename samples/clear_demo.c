#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <webgpu/webgpu.h>

// Include the header file for our entry system
#include "nano.h"

static WGPURenderPipeline pipeline;

// Vertex shader
static const char *vertex_shader_wgsl =
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
    "}\n";

// Fragment shader
// Change fade colour loop
static const char *fragment_shader_wgsl =
    "@fragment\n"
    "fn fs_main() -> @location(0) vec4<f32> {\n"
    "    return vec4<f32>(1.0, 0.0, 0.0, 1.0);\n" // Red color
    "}\n";

static void init(void) {

    nano_default_init();

    WGPUDevice device = nano_app.wgpu.device;

    // Create shader modules
    WGPUShaderModuleDescriptor vertex_shader_desc = {
        .nextInChain = NULL,
        .label = "Vertex Shader",
    };
    WGPUShaderModuleWGSLDescriptor vertex_shader_wgsl_desc = {
        .chain =
            (WGPUChainedStruct){.next = NULL,
                                .sType = WGPUSType_ShaderModuleWGSLDescriptor},
        .code = vertex_shader_wgsl,
    };
    vertex_shader_desc.nextInChain = &vertex_shader_wgsl_desc.chain;
    WGPUShaderModule vertex_shader =
        wgpuDeviceCreateShaderModule(device, &vertex_shader_desc);

    WGPUShaderModuleDescriptor fragment_shader_desc = {
        .nextInChain = NULL,
        .label = "Fragment Shader",
    };
    WGPUShaderModuleWGSLDescriptor fragment_shader_wgsl_desc = {
        .chain =
            (WGPUChainedStruct){.next = NULL,
                                .sType = WGPUSType_ShaderModuleWGSLDescriptor},
        .code = fragment_shader_wgsl,
    };
    fragment_shader_desc.nextInChain = &fragment_shader_wgsl_desc.chain;
    WGPUShaderModule fragment_shader =
        wgpuDeviceCreateShaderModule(device, &fragment_shader_desc);

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
                .module = vertex_shader,
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
                .module = fragment_shader,
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
    wgpuShaderModuleRelease(vertex_shader);
    wgpuShaderModuleRelease(fragment_shader);
}

static void frame(void) {

    nano_calc_fps();

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
        .clearValue = (WGPUColor){1.0f, 0.0f, 0.0f, 1.0f},
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
