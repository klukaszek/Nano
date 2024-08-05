// Toggles stdout logging and enables the nano debug imgui overlay
#include "webgpu.h"
#include <time.h>
#include <unistd.h>

// Include fonts as header files
#include "JetBrainsMonoNerdFontMono-Bold.h"
#include "LilexNerdFontMono-Medium.h"
#include "Roboto-Regular.h"

#define NANO_DEBUG
#define NANO_CIMGUI
// Number of custom fonts to load before starting the application
#define NANO_NUM_FONTS 3

// #define NANO_CIMGUI_DEBUG
#include "nano.h"
#include "nano_web.h"

// Include the cimgui header file so we can use imgui with nano
#include "cimgui/cimgui.h"

// Custom data example for loading into the compute shader
typedef struct {
    float value;
} Data;

char SHADER_PATH[] = "/wgpu-shaders/%s";

// Nano Application
// ------------------------------------------------------

WGPUShaderModule triangle_shader_module;
WGPURenderPipeline triangle_pipeline;
WGPUBuffer triangle_vertex_buffer;

// clang-format off
float vertex_data[] = {
        // positions            // colors
         0.0f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f
    };
// clang-format on

// Nano shader structs for the compute and triangle shaders examples
nano_shader_t *triangle_shader;
char shader_path[256];
char shader_code[8192];

void setup_triangle_pipeline() {

    WGPUShaderModuleWGSLDescriptor wgsl_desc = {
        .chain = {.next = NULL, .sType = WGPUSType_ShaderModuleWGSLDescriptor},
        .code = shader_code,
    };

    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = (WGPUChainedStruct *)&wgsl_desc,
    };

    triangle_shader_module =
        wgpuDeviceCreateShaderModule(nano_app.wgpu->device, &shader_desc);

    WGPUBufferDescriptor buffer_desc = {
        .size = sizeof(vertex_data),
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .mappedAtCreation = false,
        .label = "Triangle Vertex Buffer",
    };

    WGPUQueue queue = wgpuDeviceGetQueue(nano_app.wgpu->device);

    triangle_vertex_buffer =
        wgpuDeviceCreateBuffer(nano_app.wgpu->device, &buffer_desc);

    // Already solved in Nano
    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .bindGroupLayoutCount = 0,
        .bindGroupLayouts = NULL,
    };

    // Already solved in Nano
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(
        nano_app.wgpu->device, &pipeline_layout_desc);

    // TODO: Create nano_shader_assign_vertex_buffer(nano_shader_t *shader,
    //                                              WGPUVertexAttribute
    //                                              *attribs, void *data);
    //
    //       This method saves the information to the shader struct and used to
    //       create the vertex buffer layout for render pipeline creation when
    //       the shader is built.
    WGPUVertexAttribute attributes[2] = {
        {
            .format = WGPUVertexFormat_Float32x4,
            .offset = 0,
            .shaderLocation = 0,
        },
        {
            .format = WGPUVertexFormat_Float32x4,
            .offset = 3 * sizeof(float),
            .shaderLocation = 1,
        },
    };

    WGPUVertexBufferLayout vertex_buffer_layout = {
        .arrayStride = 7 * sizeof(float),
        .attributeCount = 2,
        .attributes = attributes,
    };

    // Create the render pipeline descriptor
    // When automating the creation of the pipeline, it is important to
    // ensure that we can automatically generate the vertex buffer layouts
    // along with our other buffer layouts.
    //
    // TODO: Mostly implemented in Nano, but the vertex buffer layout is not
    // automatically generated yet.
    //
    // 1. Create nano_shader_assign_vertex_buffer()
    // 2. Update nano_build_shader_pipelines() to create the vertex buffer
    // layout
    //    if vertex data is assigned to the shader.
    // 3. Update nano_execute_shader() to properly perform the render pass using
    //    the shader information.
    WGPURenderPipelineDescriptor pipeline_desc = {
        .vertex =
            {
                .module = triangle_shader_module,
                .entryPoint = "vs_main",
                .bufferCount = 1,
                .buffers = &vertex_buffer_layout,
            },
        .primitive =
            {
                .topology = WGPUPrimitiveTopology_TriangleList,
                .stripIndexFormat = WGPUIndexFormat_Undefined,
                .frontFace = WGPUFrontFace_CCW,
                .cullMode = WGPUCullMode_None,
            },
        .multisample =
            {
                .count = nano_app.settings.gfx.msaa.sample_count,
                .mask = ~0u,
                .alphaToCoverageEnabled = false,
            },
        .fragment =
            &(WGPUFragmentState){
                .module = triangle_shader_module,
                .entryPoint = "fs_main",
                .targetCount = 1,
                .targets =
                    &(WGPUColorTargetState){
                        .format = WGPUTextureFormat_BGRA8Unorm,
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
    };

    // Create the render pipeline
    triangle_pipeline =
        wgpuDeviceCreateRenderPipeline(nano_app.wgpu->device, &pipeline_desc);

    // Copy vertex data to the GPU
    wgpuQueueWriteBuffer(queue, triangle_vertex_buffer, 0, vertex_data,
                         sizeof(vertex_data));
}

void release_triangle_pipeline() {
    wgpuBufferRelease(triangle_vertex_buffer);
    wgpuShaderModuleRelease(triangle_shader_module);
    wgpuRenderPipelineRelease(triangle_pipeline);
}

// Render the triangle, takes the frame's command encoder as an argument
void render_triangle(WGPUCommandEncoder encoder) {

    setup_triangle_pipeline();

    // 2nd most important part of getting the shader to draw.
    WGPURenderPassColorAttachment color_attachment = {
        .view = wgpu_get_render_view(),
        .depthSlice = ~0u,
        .resolveTarget =
            nano_app.settings.gfx.msaa.sample_count > 1
                ? wgpuSwapChainGetCurrentTextureView(nano_app.wgpu->swapchain)
                : NULL,
        // Make sure to set the loadOp to Load so that the pass is drawn
        // on top of the previous frame
        .loadOp = WGPULoadOp_Load,
        // Set the storeOp to Store so that the frame is stored for the
        // next swapchain operation
        .storeOp = WGPUStoreOp_Store,
    };

    WGPURenderPassDescriptor render_pass_desc = {
        .colorAttachmentCount = 1,
        .colorAttachments = &color_attachment,
        .depthStencilAttachment = NULL,
    };

    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);

    wgpuRenderPassEncoderSetPipeline(pass, triangle_pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, triangle_vertex_buffer, 0,
                                         sizeof(vertex_data));
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);

    release_triangle_pipeline();
}

// Initialization callback passed to wgpu_start()
static void init(void) {

    char shader_path[256];

    // Initialize the nano project
    nano_default_init();

    // Initialize the buffer pool for the compute backend
    nano_init_shader_pool(&nano_app.shader_pool);

    // Fragment and Vertex shader creation
    char triangle_shader_name[] = "uv-triangle.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH,
             triangle_shader_name);

    // Get the shader code from the file
    shader_code[0] = '\0';
    memcpy(shader_code, nano_read_file(shader_path), 8192);
    LOG("DEMO: Shader code:\n%s\n", shader_code);

    // uint32_t triangle_shader_id =
    //     nano_create_shader_from_file(shader_path, (char
    //     *)triangle_shader_name);
    // if (triangle_shader_id == NANO_FAIL) {
    //     LOG("DEMO: Failed to create shader\n");
    //     return;
    // }
    //
    // triangle_shader =
    //     nano_get_shader(&nano_app.shader_pool, triangle_shader_id);

    // Build and activate the compute shader
    // Once a shader is built, it can be activated and deactivated
    // without incurring the cost of rebuilding the shader.
    // Calling nano_shader_build() or nano_activate_shader(shader, true)
    // will rebuild the shader.
    // This will build the pipeline layouts, bind groups, and necessary
    // pipelines.
    // nano_shader_activate(triangle_shader, true);
}

// Frame callback passed to wgpu_start()
static void frame(void) {

    // Update necessary Nano app state at beginning of frame
    // Get the current command encoder (this is nano_app.wgpu->cmd_encoder)
    // A new command encoder is created with each frame
    WGPUCommandEncoder cmd_encoder = nano_start_frame();

    // Execute the shaders in the order that they were activated.
    // nano_execute_shaders();

    render_triangle(cmd_encoder);

    igBegin("Nano Triangle Demo", NULL, 0);

    igText("This is a simple triangle demo using Nano and WGPU.");

    igEnd();

    // Change Nano app state at end of frame
    nano_end_frame();
}

// Shutdown callback passed to wgpu_start()
static void shutdown(void) { nano_default_cleanup(); }

// Program Entry Point
int main(int argc, char *argv[]) {

    // Add the custom fonts we wish to read from our font header files
    // This is not necessary, it is just an example of how to add custom fonts.
    // Note that fonts cannot be added after wgpu_start() is called. I could
    // probably find a way to add fonts after wgpu_start() is called, but you
    // should probably just load all your fonts once at the beginning of the
    // program unless they are memory intensive.
    if (NANO_NUM_FONTS > 0) {
        LOG("DEMO: Adding custom fonts\n");
        nano_font_t custom_fonts[NANO_NUM_FONTS] = {
            // Font 0
            {
                .ttf = JetBrainsMonoNerdFontMono_Bold_ttf,
                .ttf_len = sizeof(JetBrainsMonoNerdFontMono_Bold_ttf),
                .name = "JetBrains Mono Nerd",
            },
            // Font 1
            {
                .ttf = LilexNerdFontMono_Medium_ttf,
                .ttf_len = sizeof(LilexNerdFontMono_Medium_ttf),
                .name = "Lilex Nerd Font",
            },
            // Font 2
            {
                .ttf = Roboto_Regular_ttf,
                .ttf_len = sizeof(Roboto_Regular_ttf),
                .name = "Roboto",
            },
        };

        // Add the custom fonts to the nano_fonts struct
        memcpy(nano_fonts.fonts, custom_fonts,
               NANO_NUM_FONTS * sizeof(nano_font_t));

        // nano_fonts.font_size = 16.0f; // Default font size
        // nano_fonts.font_index = 0; // Default font
    }

    // Start a new WGPU application
    nano_start_app(&(nano_app_desc_t){
        .title = "Nano Triangle Demo",
        .res_x = 1920,
        .res_y = 1080,
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .sample_count = 4,
    });
    return 0;
}
