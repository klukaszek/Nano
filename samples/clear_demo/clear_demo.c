// Toggles stdout logging and enables the nano debug imgui overlay
#include <time.h>
#include <unistd.h>
#define NANO_DEBUG
#define NANO_CIMGUI

#define NANO_NUM_FONTS 3

// Include fonts as header files
#include "JetBrainsMonoNerdFontMono-Bold.h"
#include "LilexNerdFontMono-Medium.h"
#include "Roboto-Regular.h"

#include "nano.h"

// Custom data example for loading into the compute shader
typedef struct {
    float value;
} Data;

#define NUM_DATA 65536
#define MAX_ITERATIONS 100000
char SHADER_PATH[] = "/wgpu-shaders/%s";

// Nano Application
// ------------------------------------------------------

// Size of input and output buffers in this example
size_t buffer_size;

// GPU buffers for the input and output data
WGPUBuffer output_buffer, input_buffer;

// Nano shader structs for the compute and triangle shaders examples
nano_shader_t *compute_shader;
nano_shader_t *triangle_shader;

// GPU data struct that contains the data and status of the copy operation
nano_gpu_data_t gpu_compute;

// Data that will be copied to from the nano_gpu_data_t struct
Data output_data[NUM_DATA];

// Initialization callback passed to wgpu_start()
static void init(void) {

    char shader_path[256];

    // Initialize the nano project
    nano_default_init();

    // Get WGPU Limits
    WGPUSupportedLimits supported_limits;
    wgpuDeviceGetLimits(nano_app.wgpu->device, &supported_limits);
    WGPULimits limits = supported_limits.limits;

    // Initialize the input data
    Data input_data[NUM_DATA];
    for (int i = 0; i < NUM_DATA; ++i) {
        input_data[i].value = i * 0.1f;
    }

    // ------------------------------------------------------
    /* Example: Initialization of WGPU Shaders using Nano */

    // Initialize the buffer pool for the compute backend
    nano_init_shader_pool(&nano_app.shader_pool);

    // Set the buffer size for the compute shader
    buffer_size = NUM_DATA * sizeof(float);

    // COMPUTE SHADER CREATION
    char compute_shader_name[] = "compute-wgpu.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH,
             compute_shader_name);

    // Generate the shader from the shader path
    // TODO: Implement shader hot-reloading via drag drop into cimgui window
    // if possible on the platform.
    uint32_t compute_shader_id =
        nano_create_shader_from_file(shader_path, (char *)compute_shader_name);
    if (compute_shader_id == NANO_FAIL) {
        LOG("DEMO: Failed to create shader\n");
        return;
    }

    // Fragment and Vertex shader creation
    char triangle_shader_name[] = "uv-triangle.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH,
             triangle_shader_name);

    uint32_t triangle_shader_id =
        nano_create_shader_from_file(shader_path, (char *)triangle_shader_name);
    if (triangle_shader_id == NANO_FAIL) {
        LOG("DEMO: Failed to create shader\n");
        return;
    }

    // Get the shader from the shader pool
    compute_shader = nano_get_shader(&nano_app.shader_pool, compute_shader_id);

    triangle_shader =
        nano_get_shader(&nano_app.shader_pool, triangle_shader_id);

    // Assign buffer data to the shader
    nano_shader_assign_buffer_data(compute_shader, 0, 0, buffer_size, NUM_DATA,
                                   0, input_data);

    // Assign buffer data to output buffer, pass NULL for data
    nano_shader_assign_buffer_data(compute_shader, 0, 1, buffer_size, NUM_DATA,
                                   0, NULL);

    // Assign output buffer to the shader so that the num_workgroups
    // can be calculated based on the number of elements in the buffer
    nano_compute_shader_assign_output_buffer(compute_shader, 0, 1);

    // Build and activate the compute shader
    // Once a shader is built, it can be activated and deactivated
    // without incurring the cost of rebuilding the shader.
    // Calling nano_shader_build() or nano_activate_shader(shader, true)
    // will rebuild the shader.
    // This will build the pipeline layouts, bind groups, and necessary
    // pipelines.
    nano_shader_activate(compute_shader, true);
    nano_shader_activate(triangle_shader, true);

    nano_binding_info_t* input_binding = nano_shader_get_binding(compute_shader, 0, 0);

    // Get the input buffer from the shader
    input_buffer = input_binding->data.buffer;

    nano_binding_info_t* output_binding = nano_shader_get_binding(compute_shader, 0, 1);

    // Get the output buffer from the shader
    output_buffer = output_binding->data.buffer;

    // Once our shader is completely built and we have a compute pipeline, we
    // can write the data to the input buffers to be used in the shader pass.
    nano_write_buffer(input_binding, 0, input_data, NUM_DATA * sizeof(Data));

    // Define the gpu data struct that will be used to read the data back from
    // the GPU buffer
    gpu_compute = (nano_gpu_data_t){
        .size = sizeof(input_data),
        .src = output_buffer,
    };
}

// start and end time of performing n passes
// of a compute shader and then copying the data back to the CPU
clock_t start, end;
bool started = false;
// Frame callback passed to wgpu_start()
static void frame(void) {

    // Get start time to measure performance
    // of n passes of the compute shader
    if (!started) {
        started = true;
        start = clock();
    }

    // Update necessary Nano app state at beginning of frame
    // Get the current command encoder (this is nano_app.wgpu->cmd_encoder)
    // A new command encoder is created with each frame
    WGPUCommandEncoder cmd_encoder = nano_start_frame();

    // Execute the shaders in the order that they were activated.
    nano_execute_shaders();

    // Change Nano app state at end of frame
    nano_end_frame();

    // When we want to retrieve the data from the GPU, we can copy the data
    // from the GPU buffer to the CPU buffer. This is done by calling
    // nano_copy_buffer_to_cpu() with the buffer you want to read from as
    // the src. This returns a nano_gpu_data_t struct that contains the data
    // and status of the copy operation. The locked state is a boolean that
    // is set to true when the copy operation is complete. The data is a
    // pointer to the data that was copied from the GPU buffer. The data is
    // only valid if the locked state is true.
    if (compute_shader->in_use && gpu_compute.locked == false) {
        // Deactivate the shader to avoid a
        // wgpuBufferGetMappedRange error when copying the data
        nano_shader_deactivate(compute_shader);

        // Copy the output buffer to the gpu data struct
        int status = nano_copy_buffer_to_cpu(&gpu_compute, NULL);
        if (status == NANO_FAIL) {
            LOG("DEMO: Failed to copy data from GPU to CPU\n");
            return;
        }
    }

    // Check if the copy from the GPU buffer to the CPU is complete
    if (gpu_compute.locked == true) {
        end = clock();
        // The data should either be copied off to a static array
        // or processed in place before the next frame.
        // In this case, we are copying the data to the output_data array.
        memcpy(output_data, gpu_compute.data, buffer_size);

        LOG("GPU TEST: Running shader on GPU\n");

        LOG("\tGPU TEST: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);
        LOG("\tGPU TEST: Iterations %d (double check the shader)\n",
            MAX_ITERATIONS);
        LOG("\tGPU TEST: Last Output data[%d] = %f\n", NUM_DATA - 1,
            output_data[NUM_DATA - 1].value);

        LOG("GPU TEST: Finished running shader on GPU\n");

        // Release the status lock on the GPU copy so that it will not
        // execute again. If you do not release the lock, the copy
        // operation will execute every frame.
        nano_release_gpu_copy(&gpu_compute);
    }
}

// Shutdown callback passed to nano_app_start()
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
        .title = "Nano Basic Demo",
        .res_x = 1920,
        .res_y = 1080,
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .sample_count = 4,
    });
    return 0;
}
