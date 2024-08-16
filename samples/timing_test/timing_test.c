// Toggles stdout logging and enables the nano debug imgui overlay
#include <pthread.h>
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
#define NUM_CPU_THREADS 4
char SHADER_PATH[] = "/wgpu-shaders/%s";

// CPU Testing
// ------------------------------------------------------

// CPU test thread and completion flag
pthread_t cpu_test_thread;
bool cpu_test_complete = false;

typedef struct {
    Data *in;
    Data *out;
    int start;
    int end;
    int iterations;
} cpu_thread_data_t;

// Perform the equivalent shader operation on the CPU
// This method is passed off to another thread to run on.
void *cpu_test_simple(void *data) {

    Data in[NUM_DATA];
    Data out[NUM_DATA];

    bool *complete = (bool *)data;

    for (int i = 0; i < NUM_DATA; ++i) {
        in[i].value = i * 0.1f;
    }

    // Start the timer
    clock_t test_start = clock();

    // Run the equivalent shader on the CPU
    for (int i = 0; i < MAX_ITERATIONS; ++i) {
        for (int j = 0; j < NUM_DATA; ++j) {
            out[j].value = in[j].value + 1.0f;
        }

        // Copy the output data back to the input data
        memcpy(in, out, sizeof(Data) * NUM_DATA);
    }

    // End the timer
    clock_t test_end = clock();

    // Calculate the time it took to run the shader
    double time = (double)(test_end - test_start) / CLOCKS_PER_SEC;

    LOG("CPU TEST: Running single thread on CPU\n");
    LOG("\tCPU TEST: %f seconds\n", time);
    LOG("\tCPU TEST: Iterations %d\n", MAX_ITERATIONS);
    LOG("\tCPU TEST: CPU Time per iteration %f\n", time / MAX_ITERATIONS);
    LOG("\tCPU TEST: Last Output data[%d] = %f\n", NUM_DATA - 1,
        out[NUM_DATA - 1].value);
    LOG("CPU TEST: Finished running equivalent shader on CPU\n");

    *complete = true;

    return NULL;
}

// Chunk of the CPU test that will be run on a separate thread
void *_cpu_test_threaded(void *data) {

    cpu_thread_data_t *thread_data = (cpu_thread_data_t *)data;
    int start = thread_data->start;
    int end = thread_data->end;

    for (int i = 0; i < thread_data->iterations; ++i) {
        for (int j = start; j < end; ++j) {
            thread_data->out[j].value = thread_data->in[j].value + 1.0f;
        }

        // Copy the output data back to the input data
        memcpy(&thread_data->in[start], &thread_data->out[start],
               sizeof(Data) * (thread_data->end - thread_data->start));
    }

    return NULL;
}

// Multithreaded array processing on the CPU
void *cpu_test_threaded(void *data) {

    Data in[NUM_DATA];
    Data out[NUM_DATA];

    pthread_t threads[NUM_CPU_THREADS];
    cpu_thread_data_t thread_data[NUM_CPU_THREADS];

    bool *complete = (bool *)data;

    for (int i = 0; i < NUM_DATA; ++i) {
        in[i].value = i * 0.1f;
    }

    // Get the chunk size for each thread
    int chunk_size = NUM_DATA / NUM_CPU_THREADS;

    // Start the timer
    clock_t test_start = clock();

    // Dispatch the threads on the CPU as chunks
    // of the input data
    for (int i = 0; i < NUM_CPU_THREADS; ++i) {
        thread_data[i].in = (Data *)&in;
        thread_data[i].out = (Data *)&out;
        thread_data[i].start = i * chunk_size;
        thread_data[i].end = (i + 1) * chunk_size;
        thread_data[i].iterations = MAX_ITERATIONS;

        if (pthread_create(&threads[i], NULL, _cpu_test_threaded,
                           (void *)&thread_data[i]) != 0) {
            LOG("Failed to create thread\n");
        }
    }

    // Join threads
    for (int i = 0; i < NUM_CPU_THREADS; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            LOG("Failed to join thread %d\n", i);
        }
    }

    clock_t test_end = clock();

    double time = (double)(test_end - test_start) / CLOCKS_PER_SEC;

    LOG("CPU TEST: Running multithreaded kernel on CPU\n");
    LOG("\tCPU TEST: %f seconds\n", time);
    LOG("\tCPU TEST: Iterations %d\n", MAX_ITERATIONS);
    LOG("\tCPU TEST: CPU Time per iteration %f\n", time / MAX_ITERATIONS);
    LOG("\tCPU TEST: Last Output data[%d] = %f\n", NUM_DATA - 1,
        out[NUM_DATA - 1].value);
    LOG("CPU TEST: Finished running equivalent shader on CPU\n");

    return NULL;
}

// ------------------------------------------------------

// Nano Application
// ------------------------------------------------------

// Size of input and output buffers in this example
size_t buffer_size;

// Nano shader structs for the compute and triangle shaders examples
nano_shader_t *compute_shader;
nano_shader_t *triangle_shader;

// GPU data struct that contains the data and status of the copy operation
nano_gpu_data_t gpu_compute;

// Initialize the input data
Data input_data[NUM_DATA];

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

    // Set the initial values of the input data
    for (int i = 0; i < NUM_DATA; ++i) {
        input_data[i].value = i * 0.1f;
    }

    // ------------------------------------------------------
    /* Example: Initialization of WGPU Shaders using Nano */

    // Initialize the buffer pool for the compute backend
    nano_init_shader_pool(&nano_app.shader_pool);

    // Set the buffer size for the compute shader
    buffer_size = NUM_DATA * sizeof(Data);

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
    compute_shader = nano_get_shader(compute_shader_id);
    triangle_shader = nano_get_shader(triangle_shader_id);

    // Get the bindings from the compute shader
    nano_binding_info_t *input_binding =
        nano_shader_get_binding(compute_shader, 0, 0);
    nano_binding_info_t *output_binding =
        nano_shader_get_binding(compute_shader, 0, 1);

    // Create buffers for the input and output data
    uint32_t input_id =
        nano_create_buffer(input_binding, buffer_size, NUM_DATA, 0, input_data);
    // We pass NULL since we want an empty buffer of buffer_size
    uint32_t output_id =
        nano_create_buffer(output_binding, buffer_size, NUM_DATA, 0, NULL);

    // Get the buffers from the buffer pool and write to the input one.
    nano_buffer_t *input_buffer = nano_get_buffer(input_id);
    nano_buffer_t *output_buffer = nano_get_buffer(output_id);
    nano_write_buffer(input_buffer);

    // Bind input buffer to the compute shader
    int status = nano_shader_bind_buffer(compute_shader, input_buffer, 0, 0);
    if (status == NANO_FAIL) {
        LOG("DEMO: Failed to bind buffer to shader\n");
        return;
    }

    // Bind output buffer to the compute shader
    status = nano_shader_bind_buffer(compute_shader, output_buffer, 0, 1);
    if (status == NANO_FAIL) {
        LOG("DEMO: Failed to bind buffer to shader\n");
        return;
    }

    // Assign output buffer to the shader so that the num_workgroups
    // can be calculated based on the number of elements expected
    nano_shader_set_num_elems(compute_shader, NUM_DATA);

    // Build and activate the compute shader
    // Once a shader is built, it can be activated and deactivated
    // without incurring the cost of rebuilding the shader.
    // Calling nano_shader_build() or nano_activate_shader(shader, true)
    // will rebuild the shader.
    // This will build the pipeline layouts, bind groups, and necessary
    // pipelines.
    nano_shader_activate(compute_shader, true);
    nano_shader_activate(triangle_shader, true);

    // Define the gpu data struct that will be used to read the data back from
    // the GPU buffer
    gpu_compute = (nano_gpu_data_t){
        .size = sizeof(input_data),
        .src = output_buffer->buffer,
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
        LOG("GPU TEST: Compute GPU Results\n\t");

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

    // Check if the CPU test worker is done
    if (cpu_test_complete) {
        // Join the single threaded CPU test
        if (pthread_join(cpu_test_thread, NULL) != 0) {
            LOG("Failed to join thread\n");
        }
        cpu_test_complete = false;

        // Initialize the CPU test threads
        // Perform multithreaded CPU test once the single threaded test is done
        if (pthread_create(&cpu_test_thread, NULL, cpu_test_threaded,
                           (void *)&cpu_test_complete) != 0) {
            LOG("Failed to create thread\n");
        }
        
        // Join the threaded CPU test
        if (pthread_join(cpu_test_thread, NULL) != 0) {
            LOG("Failed to join thread\n");
        }
        cpu_test_complete = false;
    }
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
