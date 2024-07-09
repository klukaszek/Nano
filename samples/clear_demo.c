#define WGPU_BACKEND_DEBUG
#define NANO_DEBUG
#include "nano.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct {
    float value;
} Data;

char SHADER_PATH[] = "/shaders/wgpu/%s";

size_t buffer_size;
nano_buffer_t *input_buffer;
nano_buffer_t *output_buffer_storage;
WGPUBuffer staging_buffer;
nano_compute_info_t compute_info;
nano_shader_t *shader;
WGPUPipelineLayout pipeline_layout;
WGPUComputePipeline compute_pipeline;
WGPUBindGroup bind_group;

// Callback to handle the mapped data
void mapReadCallback(WGPUBufferMapAsyncStatus status, void *userdata) {
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const Data *output = (const Data *)wgpuBufferGetConstMappedRange(
            staging_buffer, 0, buffer_size);
        // Process the data
        for (int i = 0; i < 4; ++i) {
            printf("Output data[%d] = %f\n", i, output[i].value);
        }
        wgpuBufferUnmap(staging_buffer);
    } else {
        printf("Failed to map buffer for reading.\n");
    }
}

// Execute compute shader
static void nano_compute_pass(void) {
    // Get the queue to submit the command buffer
    WGPUQueue queue = wgpuDeviceGetQueue(nano_app.wgpu->device);
    uint32_t invocation_size = buffer_size / sizeof(Data);

    WGPUCommandEncoder command_encoder =
        wgpuDeviceCreateCommandEncoder(nano_app.wgpu->device, NULL);

    // Begin a compute pass to execute compute shader commands
    WGPUComputePassEncoder compute_pass =
        wgpuCommandEncoderBeginComputePass(command_encoder, NULL);
    wgpuComputePassEncoderSetPipeline(compute_pass, shader->pipeline.compute);
    wgpuComputePassEncoderSetBindGroup(compute_pass, 0, bind_group, 0, NULL);
    // Dispatch the compute shader with the number of workgroups
    uint32_t x = (uint32_t)compute_info.workgroup_size[0];
    uint32_t y = (uint32_t)compute_info.workgroup_size[1];
    uint32_t z = (uint32_t)compute_info.workgroup_size[2];
    wgpuComputePassEncoderDispatchWorkgroups(compute_pass, x, y, z);
    wgpuComputePassEncoderEnd(compute_pass);

    WGPUCommandBuffer command_buffer =
        wgpuCommandEncoderFinish(command_encoder, NULL);
    wgpuQueueSubmit(queue, 1, &command_buffer);

    // Create a new command encoder for the copy operation
    WGPUCommandEncoder copy_encoder =
        wgpuDeviceCreateCommandEncoder(nano_app.wgpu->device, NULL);
    wgpuCommandEncoderCopyBufferToBuffer(copy_encoder,
                                         output_buffer_storage->buffer, 0,
                                         staging_buffer, 0, buffer_size);
    WGPUCommandBuffer copy_command_buffer =
        wgpuCommandEncoderFinish(copy_encoder, NULL);
    wgpuQueueSubmit(queue, 1, &copy_command_buffer);

    // Map the staging buffer for reading asynchronously
    wgpuBufferMapAsync(staging_buffer, WGPUMapMode_Read, 0, buffer_size,
                       mapReadCallback, NULL);
}

void print_directories(const char *path) {
    struct dirent *entry;
    struct stat statbuf;
    DIR *dp = opendir(path);

    if (dp == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dp)) != NULL) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            // Skip the "." and ".." entries
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                printf("%s\n", entry->d_name);
            }
        }
    }

    closedir(dp);
}

static void init(void) {

    // Initialize the nano project
    nano_default_init();

    printf("Nano WGPU Initialized\n");

    // Initialize the input data
    Data input_data[64];
    for (int i = 0; i < 64; ++i) {
        input_data[i].value = i * 0.1f;
    }

    // Initialize the buffer pool for the compute backend
    nano_init_buffer_pool(&nano_app.buffer_pool);
    nano_init_shader_pool(&nano_app.shader_pool);
    
    // ------------------------------------------------------
    /* START OF COMPUTE PIPELINE AND SHADER IMPLEMENTATION */

    size_t num_groups = 0;

    buffer_size = 64 * sizeof(float);

    WGPUQueue queue = wgpuDeviceGetQueue(nano_app.wgpu->device);

    // COMPUTE SHADER CREATION
    char shader_path[256];
    char shader_name[] = "compute-wgpu.wgsl";
    snprintf(shader_path, sizeof(shader_path), SHADER_PATH,
             shader_name);

    printf("Shader Path: %s\n", shader_path);

    // Generate the shader from the shader path
    // Todo: Test shader pool with multiple shaders
    // Todo: Implement shader hot-reloading via drag drop into cimgui window
    // Todo: Implement bind group creation for multiple buffers
    // Todo: Compute pass should run for all active shaders in the shader pool
    uint32_t shader_id =
        nano_create_compute_shader(shader_path, &compute_info, buffer_size, (char *)shader_name);

    shader = nano_get_shader(&nano_app.shader_pool, shader_id);

    printf("Shader ID: %d\n", shader->id);

    // Buffer descriptor for the staging buffer until I come up with a solution
    // for staging buffers
    WGPUBufferDescriptor staging_desc = {
        .size = buffer_size,
        // We must set the usage so that we can read the data
        // from the buffer back to the CPU from the GPU.
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
    };

    // Create the staging buffer to read results back to the CPU
    staging_buffer = wgpuDeviceCreateBuffer(nano_app.wgpu->device, &staging_desc);

    // Get the input buffer from the buffer pool
    input_buffer = nano_get_buffer(&nano_app.buffer_pool, shader->id, 0, 0);

    // Get the output buffer from the buffer pool
    output_buffer_storage =
        nano_get_buffer(&nano_app.buffer_pool, shader->id, 0, 1);

    // BINDGROUP CREATION FOR BUFFERS

    // Create the bind group entry for all 3 buffers
    // This can probably easily be code gen'd in the future
    WGPUBindGroupEntry bg_entries[2] = {
        {
            .binding = 0,
            // implement getting buffer from buffer pool based on binding,
            // group, and shader id
            .buffer = input_buffer->buffer,
            .offset = 0,
            .size = buffer_size,
        },
        {
            .binding = 1,
            .buffer = output_buffer_storage->buffer,
            .offset = 0,
            .size = buffer_size,
        },
    };

    // Create the bind group descriptor
    WGPUBindGroupDescriptor bg_desc = {
        // I am using using 0 here because the test shader only uses group 0
        .layout = shader->pipeline_layout.bg_layouts[0],
        .entryCount = 2,
        .entries = bg_entries,
    };

    // Finally create the bind group
    bind_group = wgpuDeviceCreateBindGroup(nano_app.wgpu->device, &bg_desc);

    // Once we have the bind group and a compute pipeline, we can write the
    // data to the input buffers to be used in the compute shader pass
    wgpuQueueWriteBuffer(queue, input_buffer->buffer, 0, input_data,
                         sizeof(input_data));

    // ------------------------------------------------------
    /* END OF COMPUTE PIPELINE AND SHADER IMPLEMENTATION */

    nano_compute_pass();
}


static void frame(void) {  
    // Update necessary Nano app state at beginning of frame
    // Get the current command encoder (this is nano_app.wgpu->cmd_encoder)
    // A new command encoder is created with each frame
    WGPUCommandEncoder cmd_encoder = nano_start_frame();

    // Change Nano app state at end of frame
    nano_end_frame();
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
        .sample_count = 1,
    });
    return 0;
}
