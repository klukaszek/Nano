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
nano_shader_t *wave_shader;
char shader_path[256];
char shader_code[8192];

// When using the uniform buffer with Nano, if the shader is not working as
// expected, you should use the @align(n) directive in the shader to align the
// buffer in multiples of 16 bytes. See assets/shaders/dot.wgsl for an example.
struct UniformBuffer {
    float time;          // 4 bytes
    float padding;       // 4 bytes
    float resolution[2]; // 8 bytes
    struct {
        float freq;     // 4 bytes
        float amp;      // 4 bytes
        float speed;    // 4 bytes
        float thickness; // 4 bytes
    } wave;
} uniform_data; // 16 bytes

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

    // Initialize the buffer pool for the compute backend
    nano_init_shader_pool(&nano_app.shader_pool);

    // Fragment and Vertex shader creation
    char wave_shader_name[] = "wave.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH, wave_shader_name);

    uint32_t wave_shader_id =
        nano_create_shader_from_file(shader_path, wave_shader_name);
    if (wave_shader_id == NANO_FAIL) {
        LOG("Failed to create wave shader\n");
        return;
    }

    wave_shader = nano_get_shader(wave_shader_id);
    if (!wave_shader) {
        LOG("Failed to get wave shader\n");
        return;
    }
    
    // Initialize the uniform buffer
    uniform_data.time = 0.0f;
    uniform_data.resolution[0] = nano_app.wgpu->width;
    uniform_data.resolution[1] = nano_app.wgpu->height;

    // Wave parameters
    uniform_data.wave.freq = 5.0f;
    uniform_data.wave.amp = 0.5f;
    uniform_data.wave.speed = 0.2f;
    uniform_data.wave.thickness = 0.005f;

    // Set the vertex count for the shader (if we don't set this, it defaults to
    // 3)
    nano_shader_set_vertex_count(wave_shader, 3); // not needed in this case
    
    nano_binding_info_t *binding = nano_shader_get_binding(wave_shader, 0, 0);
    if (!binding) {
        LOG("Failed to get binding\n");
        return;
    }

    // Create a uniform buffer
    uint32_t buffer_id = nano_create_buffer(binding, sizeof(uniform_data), 1, 0, &uniform_data);
    nano_buffer_t *uniform_buffer = nano_get_buffer(buffer_id);
    if (!uniform_buffer) {
        LOG("Failed to get uniform buffer\n");
        return;
    }
    
    // Assign the uniform buffer to the shader
    int status = nano_shader_bind_uniforms(wave_shader, uniform_buffer, 0, 0);
    if (status == NANO_FAIL) {
        LOG("Failed to assign uniform data\n");
        return;
    }

    // Build and activate the compute shader
    // Once a shader is built, it can be activated and deactivated
    // without incurring the cost of rebuilding the shader.
    // Calling nano_shader_build() or nano_activate_shader(shader, true)
    // will rebuild the shader.
    // This will build the pipeline layouts, bind groups, and necessary
    // pipelines.
    nano_shader_activate(wave_shader, true);
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
    igSetNextWindowSize((ImVec2){0, 225}, ImGuiCond_FirstUseEver);
    igBegin("Nano Wave Demo", NULL, 0);
    igText("This demo shows how to use the fragment shader and a uniform "
           "buffer to create a wave effect using WebGPU, WGSL, and Nano.");
    igSeparatorText("Wave Settings");
    igSetNextItemWidth(150);
    igSliderFloat("Wave Frequency", &uniform_data.wave.freq, 0.0f, 10.0f,
                  "%.2f", 1.0f);
    igSetNextItemWidth(150);
    igSliderFloat("Wave Amplitude", &uniform_data.wave.amp, 0.0f, 5.0f,
                    "%.2f", 1.0f);
    igSetNextItemWidth(150);
    igSliderFloat("Wave Speed", &uniform_data.wave.speed, 0.0f, 1.0f,
                    "%.2f", 1.0f);
    igSetNextItemWidth(150);
    igSliderFloat("Wave Thickness", &uniform_data.wave.thickness, 0.0f, 0.1f,
                    "%.3f", 1.0f);
    igEnd();

    // Change Nano app state at end of frame
    nano_end_frame();

    // Update the uniform buffer
    uniform_data.resolution[0] = nano_app.wgpu->width;
    uniform_data.resolution[1] = nano_app.wgpu->height;
    uniform_data.time += 0.01;
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
