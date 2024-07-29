// Toggles stdout logging and enables the nano debug imgui overlay
#define NANO_DEBUG

// // Debug WGPU Backend Implementation
// #define WGPU_BACKEND_DEBUG

// // Debug WGPU + CIMGUI Implementation
// #define NANO_CIMGUI_DEBUG

#include "nano.h"

// Custom data example for loading into the compute shader
typedef struct {
    float value;
} Data;

char SHADER_PATH[] = "/wgpu-shaders/%s";

// Size of input and output buffers in this example
size_t buffer_size;

// GPU buffers for the input and output data
WGPUBuffer *output_buffer, *input_buffer;

// Nano shader structs for the compute and triangle shaders examples
nano_shader_t *compute_shader;
nano_shader_t *triangle_shader;

// GPU data struct that contains the data and status of the copy operation
nano_gpu_data_t *gpu_compute = NULL;

// Data that will be copied to from the nano_gpu_data_t struct
Data output_data[64];

// Initialization callback passed to wgpu_start()
static void init(void) {

    char shader_path[256];

    // Initialize the nano project
    nano_default_init();

    // Initialize the input data
    Data input_data[64];
    for (int i = 0; i < 64; ++i) {
        input_data[i].value = i * 0.1f;
    }

    // Initialize the buffer pool for the compute backend
    nano_init_shader_pool(&nano_app.shader_pool);

    // ------------------------------------------------------
    /* START OF COMPUTE PIPELINE AND SHADER IMPLEMENTATION */

    // Set the buffer size for the compute shader
    buffer_size = 64 * sizeof(float);

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
    nano_shader_assign_buffer_data(compute_shader, 0, 0, sizeof(input_data), 64,
                                   0, input_data);

    // Assign buffer data to output buffer, pass NULL for data
    nano_shader_assign_buffer_data(compute_shader, 0, 1, buffer_size, 0, 0,
                                   NULL);

    // Build and activate the compute shader
    // Once a shader is built, it can be activated and deactivated
    // without incurring the cost of rebuilding the shader.
    // Calling nano_shader_build() or nano_activate_shader(shader, true)
    // will rebuild the shader.
    // This will build the pipeline layouts, bind groups, and necessary
    // pipelines.
    nano_shader_activate(compute_shader, true);
    nano_shader_activate(triangle_shader, true);

    // Get the input buffer from the shader
    input_buffer = nano_get_buffer(compute_shader, 0, 0);

    // Get the output buffer from the shader
    output_buffer = nano_get_buffer(compute_shader, 0, 1);

    // Once our shader is completely built and we have a compute pipeline, we
    // can write the data to the input buffers to be used in the shader pass.
    nano_write_buffer(input_buffer, 0, input_data, sizeof(input_data));
}

// Frame callback passed to wgpu_start()
static void frame(void) {
    // Update necessary Nano app state at beginning of frame
    // Get the current command encoder (this is nano_app.wgpu->cmd_encoder)
    // A new command encoder is created with each frame
    WGPUCommandEncoder cmd_encoder = nano_start_frame();

    // Execute the shaders in the order that they were activated.
    nano_execute_shaders();

    // When we want to retrieve the data from the GPU, we can copy the data
    // from the GPU buffer to the CPU buffer. This is done by calling
    // nano_copy_buffer_to_cpu() with the buffer you want to read from as the
    // src. This returns a nano_gpu_data_t struct that contains the data and
    // status of the copy operation. The status is a pointer to a boolean that
    // is set to true when the copy operation is complete. The data is a pointer
    // to the data that was copied from the GPU buffer. The data is only valid
    // if the status is true.
    if (compute_shader->in_use) {
        gpu_compute =
            nano_copy_buffer_to_cpu(output_buffer, 0, 0, buffer_size, NULL);
        LOG("DEMO: Copying data from GPU to CPU\n");
        // Deactivate the shader so that it is only executed once
        nano_deactivate_shader(compute_shader);
        LOG("DEMO: Deactivating compute shader\n");
    };

    // Change Nano app state at end of frame
    nano_end_frame();

    // Check if the copy from the GPU buffer to the CPU is complete
    if (*gpu_compute->status == true) {

        LOG("DEMO: Compute output ready.\n");

        // The data should either be copied off to a static array
        // or processed in place before the next frame.
        // In this case, we are copying the data to the output_data static
        // array.
        memcpy(output_data, gpu_compute->data, buffer_size);

        // For the purposes of this demo, we are printing the output data
        for (int i = 0; i < 64; ++i) {
            LOG("DEMO: Output data[%d] = %f\n", i, output_data[i].value);
        }

        // Free the GPU data after we are done with it
        // We should have either copied the data off, or
        // consumed the data in place before the next frame.
        nano_free_gpu_data(gpu_compute);
    }
}

// Shutdown callback passed to wgpu_start()
static void shutdown(void) { nano_default_cleanup(); }

// Program Entry Point
int main(int argc, char *argv[]) {
    // Start a new WGPU application
    wgpu_start(&(wgpu_desc_t){
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
