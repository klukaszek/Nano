#include <stdint.h>
#include <time.h>
#include <unistd.h>

// Include fonts as header files
#include "JetBrainsMonoNerdFontMono-Bold.h"
#include "LilexNerdFontMono-Medium.h"
#include "Roboto-Regular.h"
#include <webgpu/webgpu.h>

// Toggles stdout logging and enables the nano debug imgui overlay
#define NANO_DEBUG
#define NANO_CIMGUI
#define WGPU_BACKEND_DEBUG

// Number of custom fonts to load before starting the application
#define NANO_NUM_FONTS 3

// #define NANO_CIMGUI_DEBUG
#include "nano.h"

// Include the cimgui header file so we can use imgui with nano
#include "cimgui/cimgui.h"

char SHADER_PATH[] = "/wgpu-shaders/%s";

// Nano Application
// ------------------------------------------------------

// Nano shader structs for the compute and triangle shaders examples
nano_shader_t *cube_shader;
char shader_path[256];
char shader_code[8192];

// When using the uniform buffer with Nano, if the shader is not working as
// expected, you should use the @align(n) directive in the shader to align the
// buffer in multiples of 16 bytes. See assets/shaders/dot.wgsl for an example.
struct UniformBuffer {
    float time;          // 4 bytes
    float padding;       // 4 bytes
    float resolution[2]; // 8 bytes
} uniform_buffer; // 16 bytes

// Initialization callback passed to nano_start_app()
static void init(void) {

    // Initialize the nano project
    nano_default_init();

    WGPUDevice device = nano_app.wgpu->device;
    WGPUSupportedLimits limits;
    wgpuDeviceGetLimits(nano_app.wgpu->device, &limits);

    LOG("DEMO: Max Vertex Buffers: %u\n", limits.limits.maxVertexBuffers);
    LOG("DEMO: Max Vertex Attributes: %u\n", limits.limits.maxVertexAttributes);

    // Fragment and Vertex shader creation
    char cube_shader_name[] = "texture.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH, cube_shader_name);
    
    // Initialize the uniform buffer
    uniform_buffer.time = 0.0f;
    uniform_buffer.resolution[0] = nano_app.wgpu->width;
    uniform_buffer.resolution[1] = nano_app.wgpu->height;

    uint32_t shader_id = nano_create_shader_from_file(shader_path, cube_shader_name);
    cube_shader = nano_get_shader(shader_id);

    nano_print_shader_info(&cube_shader->info);

    // Build and activate the compute shader
    // Once a shader is built, it can be activated and deactivated
    // without incurring the cost of rebuilding the shader.
    // Calling nano_shader_build() or nano_activate_shader(shader, true)
    // will rebuild the shader.
    // This will build the pipeline layouts, bind groups, and necessary
    // pipelines.
    // nano_shader_activate(cube_shader, true);
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
        (ImVec2){nano_app.wgpu->width - (nano_app.wgpu->width) * 0.5, 25},
        ImGuiCond_FirstUseEver, (ImVec2){0, 0});
    igSetNextWindowSize((ImVec2){0, 75}, ImGuiCond_FirstUseEver);
    igBegin("Nano Wave Demo", NULL, 0);
    igText("This demo shows how to make a simple cube using Nano.");
    igEnd();

    // Change Nano app state at end of frame
    nano_end_frame();

    // Update the uniform buffer
    uniform_buffer.resolution[0] = nano_app.wgpu->width;
    uniform_buffer.resolution[1] = nano_app.wgpu->height;
    uniform_buffer.time += 0.01;
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
        .title = "Nano Wave Demo",
        .res_x = 1920,
        .res_y = 1080,
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .sample_count = 4,
    });

    return 0;
}
