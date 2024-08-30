#include <stdint.h>
#include <time.h>
#include <unistd.h>

// Include fonts as header files
#include "JetBrainsMonoNerdFontMono-Bold.h"
#include "LilexNerdFontMono-Medium.h"
#include "Roboto-Regular.h"
#include "cglm/cglm.h"

#ifdef NANO_NATIVE
    #include <wgpu_native/webgpu.h>
#else
    #include <webgpu/webgpu.h>
#endif

// Toggles stdout logging and enables the nano debug imgui overlay
#define NANO_DEBUG
#define NANO_CIMGUI
#define WGPU_BACKEND_DEBUG

// Number of custom fonts to load before starting the application
#define NANO_NUM_FONTS 3

// #define NANO_CIMGUI_DEBUG
#include "nano.h"

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

typedef struct {
    float position[3];
    float color[3];
} Vertex;

Vertex vertex_data[] = {
    // positions            // colors
    {{0.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
};

// Create the vertex buffer layout
WGPUVertexAttribute attributes[2] = {
    {
        .format = WGPUVertexFormat_Float32x3,
        .offset = offsetof(Vertex, position),
        .shaderLocation = 0,
    },
    {
        .format = WGPUVertexFormat_Float32x3,
        .offset = offsetof(Vertex, color),
        .shaderLocation = 1,
    },
};

// Nano shader structs for the compute and triangle shaders examples
nano_shader_t *triangle_shader;
char shader_path[256];
char shader_code[8192];

// Initialization callback passed to nano_start_app()
static void init(void) {

    char shader_path[256];

    // Initialize the nano project
    nano_default_init();

    WGPUDevice device = nano_app.wgpu->device;
    WGPUSupportedLimits limits;
    wgpuDeviceGetLimits(nano_app.wgpu->device, &limits);

    LOG("DEMO: Max Vertex Buffers: %u\n", limits.limits.maxVertexBuffers);
    LOG("DEMO: Max Vertex Attributes: %u\n", limits.limits.maxVertexAttributes);

    // Fragment and Vertex shader creation
    char triangle_shader_name[] = "rgb-triangle.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH,
             triangle_shader_name);

    uint32_t triangle_shader_id =
        nano_create_shader_from_file(triangle_shader_name, (char *)triangle_shader_name);
    if (triangle_shader_id == NANO_FAIL) {
        LOG("DEMO: Failed to create shader\n");
        return;
    }

    // Get the shader from the shader pool
    triangle_shader = nano_get_shader(triangle_shader_id);

    // Create the vertex buffer for the triangle
    uint32_t vertex_buffer_id =
        nano_create_vertex_buffer(sizeof(vertex_data), 0, &vertex_data, NULL);
    if (vertex_buffer_id == NANO_FAIL) {
        LOG("DEMO: Failed to create vertex buffer\n");
        return;
    }

    // Bind the vertex buffer to the shader
    int status = nano_shader_bind_vertex_buffer(
        triangle_shader, vertex_buffer_id, (WGPUVertexAttribute *)&attributes,
        2, sizeof(Vertex));
    if (status != NANO_OK) {
        LOG("DEMO: Failed to bind vertex buffer\n");
        return;
    }

    // Build and activate the compute shader
    // Once a shader is built, it can be activated and deactivated
    // without incurring the cost of rebuilding the shader.
    // Calling nano_shader_build() or nano_activate_shader(shader, true)
    // will rebuild the shader.
    // This will build the pipeline layouts, bind groups, and necessary
    // pipelines.
    nano_shader_activate(triangle_shader, true);
}

// Frame callback passed to nano_start_app()
static void frame(void) {

    // Update necessary Nano app state at beginning of frame
    // Get the current command encoder (this is nano_app.wgpu->cmd_encoder)
    // A new command encoder is created with each frame
    WGPUCommandEncoder cmd_encoder = nano_start_frame();

    // Execute the shaders in the order that they were activated.
    nano_execute_shaders();
    // OR execute a specific shader using: nano_execute_shader(triangle_shader);

    // Note: If you are using the nano_execute_shaders() function, you do not
    // want to call nano_execute_shader() as it will execute the shader twice
    // on the same command encoder.

    // Set the window position
    igSetNextWindowPos(
        (ImVec2){nano_app.wgpu->width - (nano_app.wgpu->width) * 0.5, 20},
        ImGuiCond_FirstUseEver, (ImVec2){0, 0});
    igBegin("Nano RGB Triangle Demo", NULL, 0);
    igText(
        "This is a simple triangle demo using Nano.\nThis window is being "
        "created outside of nano.h from main.c.\nThis is to demonstrate that "
        "Nano instantiates a complete ImGui instance for the WASM module.");
    igEnd();

    // Change Nano app state at end of frame
    nano_end_frame();
}

// Shutdown callback passed to nano_start_app()
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

        // Load custom fonts before launching the application
        nano_load_fonts(custom_fonts, NANO_NUM_FONTS, 16.0f);
    }

    // Start a new WGPU application
    // This will initialize the WGPU instance with an MSAA TextureView
    // and create a swapchain for rendering.
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
