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
#define WGPU_BACKEND_DEBUG

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

// Initialization callback passed to wgpu_start()
static void init(void) {

    char shader_path[256];

    // Initialize the nano project
    nano_default_init();

    WGPUSupportedLimits limits;
    wgpuDeviceGetLimits(nano_app.wgpu->device, &limits);

    LOG("DEMO: Max Vertex Buffers: %u\n", limits.limits.maxVertexBuffers);
    LOG("DEMO: Max Vertex Attributes: %u\n",
        limits.limits.maxVertexAttributes);

    // Initialize the buffer pool for the compute backend
    nano_init_shader_pool(&nano_app.shader_pool);

    // Fragment and Vertex shader creation
    char triangle_shader_name[] = "rgb-triangle.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH,
             triangle_shader_name);

    // Get the shader code from the file
    shader_code[0] = '\0';
    memcpy(shader_code, nano_read_file(shader_path), 8192);
    LOG("DEMO: Shader code:\n%s\n", shader_code);

    uint32_t triangle_shader_id =
        nano_create_shader_from_file(shader_path, (char
        *)triangle_shader_name);
    if (triangle_shader_id == NANO_FAIL) {
        LOG("DEMO: Failed to create shader\n");
        return;
    }
    
    // Get the shader from the shader pool
    triangle_shader =
        nano_get_shader(&nano_app.shader_pool, triangle_shader_id);
    
    // Create the vertex buffer layout
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
    
    // Assign the vertex buffer to the shader so that it can be compiled into the pipeline.
    int status = nano_shader_create_vertex_buffer(triangle_shader, attributes, 2, 7 * sizeof(float), sizeof(vertex_data), &vertex_data);
    if (status != NANO_OK) {
        LOG("DEMO: Failed to create vertex buffer\n");
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

// Frame callback passed to wgpu_start()
static void frame(void) {

    // Update necessary Nano app state at beginning of frame
    // Get the current command encoder (this is nano_app.wgpu->cmd_encoder)
    // A new command encoder is created with each frame
    WGPUCommandEncoder cmd_encoder = nano_start_frame();

    // Execute the shaders in the order that they were activated.
    nano_execute_shaders();

    igBegin("Nano RGB Triangle Demo", NULL, 0);

    igText("This is a simple triangle demo using Nano.");

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
