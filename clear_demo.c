#define WGPU_BACKEND_DEBUG
#define NANO_DEBUG
#include "nano.h"

static void init(void) { nano_default_init(); }

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
