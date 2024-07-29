#include <threads.h>
#include <unistd.h>
#define WGPU_BACKEND_DEBUG
#define NANO_DEBUG
// #define NANO_CIMGUI_DEBUG
#include "nano.h"
#include <stdarg.h>

typedef struct {
    float value;
} Data;

char SHADER_PATH[] = "/wgpu-shaders/%s";

size_t buffer_size;
WGPUBuffer *output_buffer, *input_buffer;
nano_shader_t *shader;
nano_shader_t *triangle_shader;
WGPUComputePipeline compute_pipeline;
nano_gpu_data_t gpu_compute;
Data output_data[64];

static void init(void) {

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

    size_t num_groups = 0;

    buffer_size = 64 * sizeof(float);

    WGPUQueue queue = wgpuDeviceGetQueue(nano_app.wgpu->device);

    char shader_path[256];

    // COMPUTE SHADER CREATION
    char compute_shader_name[] = "compute-wgpu.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH,
             compute_shader_name);

    // Generate the shader from the shader path
    // Todo: Implement bind group creation for multiple buffers - see
    // nano_activate_shader() Todo: Implement shader hot-reloading via drag drop
    // into cimgui window Todo: Compute pass should run for all active shaders
    // in the shader pool
    uint32_t compute_shader_id =
        nano_create_shader_from_file(shader_path, (char *)compute_shader_name);
    if (compute_shader_id == NANO_FAIL) {
        printf("Failed to create shader\n");
        return;
    }

    // Fragment and Vertex shader creation
    char triangle_shader_name[] = "uv-triangle.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH,
             triangle_shader_name);

    uint32_t triangle_shader_id =
        nano_create_shader_from_file(shader_path, (char *)triangle_shader_name);
    if (triangle_shader_id == NANO_FAIL) {
        printf("Failed to create shader\n");
        return;
    }

    // Get the shader from the shader pool
    shader = nano_get_shader(&nano_app.shader_pool, compute_shader_id);

    triangle_shader =
        nano_get_shader(&nano_app.shader_pool, triangle_shader_id);

    // Assign buffer data to the shader
    nano_shader_assign_buffer_data(shader, 0, 0, sizeof(input_data), 64, 0,
                                   input_data);

    // Assign buffer data to output buffer, pass NULL for data
    nano_shader_assign_buffer_data(shader, 0, 1, buffer_size, 0, 0, NULL);

    // Build and activate the compute shader
    // Once a shader is built, it can be activated and deactivated
    // without incurring the cost of rebuilding the shader.
    // Calling nano_shader_build() or nano_activate_shader(shader, true)
    // will rebuild the shader.
    // This will build the pipeline layouts, bind groups, and necessary
    // pipelines.
    nano_shader_activate(shader, true);
    nano_shader_activate(triangle_shader, true);

    // Get the input buffer from the shader
    input_buffer = nano_get_buffer(shader, 0, 0);

    // Get the output buffer from the shader
    output_buffer = nano_get_buffer(shader, 0, 1);
    compute_pipeline = nano_get_compute_pipeline(shader);

    // Once our shader is completely built and we have a compute pipeline, we
    // can write the data to the input buffers to be used in the shader pass.
    nano_write_buffer(input_buffer, 0, input_data, sizeof(input_data));
}

static void frame(void) {
    // Update necessary Nano app state at beginning of frame
    // Get the current command encoder (this is nano_app.wgpu->cmd_encoder)
    // A new command encoder is created with each frame
    WGPUCommandEncoder cmd_encoder = nano_start_frame();
    
    // Execute the shaders in order that they were activated.
    nano_execute_shaders();

    // Copy the data from the GPU buffer to the CPU
    // The data is copied asynchronously, so we need to check if the copy is
    // complete. The gpu_compute.status will be set to true when the copy is
    // complete. The data can be accessed from gpu_compute.data.
    // When status is NULL, the copy has not been started.
    if (gpu_compute.status == NULL) {
        gpu_compute =
            nano_copy_buffer_to_cpu(output_buffer, 0, 0, buffer_size, NULL);
    };

    // Change Nano app state at end of frame
    nano_end_frame();

    // Make sure the gpu_compute struct has data to share
    if (gpu_compute.status != NULL) {
        // Check if the copy from the GPU buffer to the CPU is complete
        if (*gpu_compute.status == true) {

            // The data should either be copied off to another array
            // or processed in place before the next frame.
            // In this case, we are copying the data to the output_data static
            // array.
            memcpy(output_data, gpu_compute.data, buffer_size);

            // Free the GPU data after copying the data
            nano_free_gpu_data(&gpu_compute);
            for (int i = 0; i < 64; ++i) {
                printf("Output data[%d] = %f\n", i, output_data[i].value);
            }

            // Deactivate the shader so that it is only executed once
            nano_deactivate_shader(shader);
        }
    }
}

static void shutdown(void) { nano_default_cleanup(); }

int main(int argc, char *argv[]) {
    wgpu_start(&(wgpu_desc_t){
        .title = "Solid Color Demo",
        .res_x = 1920,
        .res_y = 1080,
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .sample_count = 4,
    });
    return 0;
}
