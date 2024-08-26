#include <math.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

// Include fonts as header files
#include "JetBrainsMonoNerdFontMono-Bold.h"
#include "LilexNerdFontMono-Medium.h"
#include "Roboto-Regular.h"
#include "cglm/cglm.h"
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

typedef struct {
    float position[3];
    float color[4];
} VertexData;

VertexData cube_vertex_data[] = {
    // Front face
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f, 1.0}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f, 1.0}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f, 1.0}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f, 1.0}},
    
    // Back face
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f, 1.0}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f, 1.0}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0}},
    {{-0.5f,  0.5f, -0.5f}, {0.5f, 0.5f, 0.5f, 1.0}}
};

uint16_t cube_indices[] = {
    // Front face
    0, 1, 2,  2, 3, 0,
    // Right face
    1, 5, 6,  6, 2, 1,
    // Back face
    5, 4, 7,  7, 6, 5,
    // Left face
    4, 0, 3,  3, 7, 4,
    // Top face
    3, 2, 6,  6, 7, 3,
    // Bottom face
    4, 5, 1,  1, 0, 4
};

WGPUVertexAttribute attributes[2] = {
    {
        .format = WGPUVertexFormat_Float32x3,
        .offset = offsetof(VertexData, position),
        .shaderLocation = 0
    },
    {
        .format = WGPUVertexFormat_Float32x3,
        .offset = offsetof(VertexData, color),
        .shaderLocation = 1
    }
};

// Define the matrices needed for 3D projection
mat4 model, view, projection, mvp;

// When using the uniform buffer with Nano, if the shader is not working as
// expected, you should use the @align(n) directive in the shader to align the
// buffer in multiples of 16 bytes. See assets/shaders/dot.wgsl for an example.
struct UniformBuffer {
    float mvp[16]; // 4x4 matrix (16 floats x 4 bytes = 64 bytes)
    float time; // 1 float (4 bytes)
    float padding[3]; // Padding to ensure 16-byte alignment
} __attribute__((aligned((16)))) uniform_data; // 16 bytes

void update_mvp() {

    // Update the model matrix and have it rotate around the y axis
    glm_mat4_identity(model);
    glm_rotate(model, uniform_data.time, (vec3){0.0f, 1.0f, 0.0f});
    glm_translate(model, (vec3){0.0f, 0.0f, -1.0f});
    
    // Update the view matrix
    glm_mat4_identity(view);
    glm_lookat((vec3){0.0f, 0.0f, 3.0f}, (vec3){0.0f, 0.0f, 0.0f},
               (vec3){0.0f, 1.0f, 0.0f}, view);
    
    // Update the projection matrix with a 90 degree field of view
    glm_mat4_identity(projection);
    glm_perspective(glm_rad(90.0f),
                    (float)nano_app.wgpu->width / (float)nano_app.wgpu->height,
                    0.1f, 100.0f, projection);

    glm_mat4_mul(projection, view, mvp);
    glm_mat4_mul(mvp, model, mvp);

    // Assign the mvp matrix to the uniform buffer
    memcpy(uniform_data.mvp, mvp, sizeof(float) * 16);
}

// Initialization callback passed to nano_start_app()
static void init(void) {

    // Initialize the nano project
    nano_default_init();

    WGPUDevice device = nano_app.wgpu->device;
    WGPUSupportedLimits limits;
    wgpuDeviceGetLimits(nano_app.wgpu->device, &limits);

    LOG("DEMO: Max VertexData Buffers: %u\n", limits.limits.maxVertexBuffers);
    LOG("DEMO: Max VertexData Attributes: %u\n", limits.limits.maxVertexAttributes);

    // Fragment and VertexData shader creation
    char cube_shader_name[] = "cube.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH, cube_shader_name);

    // Assign values to the uniform buffer
    uniform_data.time = 0.0f;

    // Initialize the model, view, and projection matrices
    update_mvp();

    uint32_t shader_id =
        nano_create_shader_from_file(shader_path, cube_shader_name);
    cube_shader = nano_get_shader(shader_id);
    
    // Create a vertex buffer for the cube
    uint32_t vertex_buffer_id = nano_create_vertex_buffer(
        sizeof(cube_vertex_data), 0, &cube_vertex_data, NULL);

    // Assign the vertex buffer to the shader
    nano_shader_bind_vertex_buffer(cube_shader, vertex_buffer_id,
                                   (WGPUVertexAttribute *)&attributes, 2,
                                   sizeof(VertexData));
    
    // Create an index buffer for the cube
    uint32_t index_buffer_id =
        nano_create_index_buffer(sizeof(cube_indices), 0, &cube_indices, NULL);

    // Assign the index buffer to the shader
    nano_shader_bind_index_buffer(cube_shader, index_buffer_id,
                                  WGPUIndexFormat_Uint16);
    
    // Get the binding info for the uniforms at (0, 0)
    nano_binding_info_t *binding = nano_shader_get_binding(cube_shader, 0, 0);
    if (!binding) {
        LOG("Failed to get binding\n");
        return;
    }

    // Create a uniform buffer
    uint32_t uniform_buffer_id =
        nano_create_buffer(binding, sizeof(uniform_data), 1, 0, &uniform_data);

    // Bind the uniform buffer to the shader
    nano_shader_bind_uniforms(cube_shader, uniform_buffer_id, 0, 0);

    nano_shader_set_vertex_count(cube_shader, 36);

    nano_print_shader_info(&cube_shader->info);

    // Build and activate the compute shader
    // Once a shader is built, it can be activated and deactivated
    // without incurring the cost of rebuilding the shader.
    // Calling nano_shader_build() or nano_activate_shader(shader, true)
    // will rebuild the shader.
    // This will build the pipeline layouts, bind groups, and necessary
    // pipelines.
    nano_shader_activate(cube_shader, true);
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

    igText("Time: %.2fs", uniform_data.time);
    igEnd();

    // Change Nano app state at end of frame
    nano_end_frame();

    // Update the uniform time field
    uniform_data.time += nano_app.frametime / 1000.0f;

    // Update the mvp for the uniform buffer
    update_mvp();
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
