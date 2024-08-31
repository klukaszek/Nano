#ifndef NANO_NATIVE_H
#define NANO_NATIVE_H

// TODO: figure out why nano_app.wgpu->device is NULL after copying the state to
// the nano_app struct
//
// TODO: nano_cimgui_scale_to_canvas() needs to be fixed. ImGuiStyleDestroy
// causes a double free even though the style is not null.

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glfw3webgpu/glfw3webgpu.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#ifdef NANO_CIMGUI
    #include "nano_cimgui.h"
#endif

typedef enum {
    WGPU_KEY_INVALID,
    WGPU_KEY_SPACE,
    WGPU_KEY_APOSTROPHE,
    WGPU_KEY_COMMA,
    WGPU_KEY_MINUS,
    WGPU_KEY_PERIOD,
    WGPU_KEY_SLASH,
    WGPU_KEY_0,
    WGPU_KEY_1,
    WGPU_KEY_2,
    WGPU_KEY_3,
    WGPU_KEY_4,
    WGPU_KEY_5,
    WGPU_KEY_6,
    WGPU_KEY_7,
    WGPU_KEY_8,
    WGPU_KEY_9,
    WGPU_KEY_SEMICOLON,
    WGPU_KEY_EQUAL,
    WGPU_KEY_A,
    WGPU_KEY_B,
    WGPU_KEY_C,
    WGPU_KEY_D,
    WGPU_KEY_E,
    WGPU_KEY_F,
    WGPU_KEY_G,
    WGPU_KEY_H,
    WGPU_KEY_I,
    WGPU_KEY_J,
    WGPU_KEY_K,
    WGPU_KEY_L,
    WGPU_KEY_M,
    WGPU_KEY_N,
    WGPU_KEY_O,
    WGPU_KEY_P,
    WGPU_KEY_Q,
    WGPU_KEY_R,
    WGPU_KEY_S,
    WGPU_KEY_T,
    WGPU_KEY_U,
    WGPU_KEY_V,
    WGPU_KEY_W,
    WGPU_KEY_X,
    WGPU_KEY_Y,
    WGPU_KEY_Z,
    WGPU_KEY_LEFT_BRACKET,
    WGPU_KEY_BACKSLASH,
    WGPU_KEY_RIGHT_BRACKET,
    WGPU_KEY_GRAVE_ACCENT,
    WGPU_KEY_WORLD_1,
    WGPU_KEY_WORLD_2,
    WGPU_KEY_ESCAPE,
    WGPU_KEY_ENTER,
    WGPU_KEY_TAB,
    WGPU_KEY_BACKSPACE,
    WGPU_KEY_INSERT,
    WGPU_KEY_DELETE,
    WGPU_KEY_RIGHT,
    WGPU_KEY_LEFT,
    WGPU_KEY_DOWN,
    WGPU_KEY_UP,
    WGPU_KEY_PAGE_UP,
    WGPU_KEY_PAGE_DOWN,
    WGPU_KEY_HOME,
    WGPU_KEY_END,
    WGPU_KEY_CAPS_LOCK,
    WGPU_KEY_SCROLL_LOCK,
    WGPU_KEY_NUM_LOCK,
    WGPU_KEY_PRINT_SCREEN,
    WGPU_KEY_PAUSE,
    WGPU_KEY_F1,
    WGPU_KEY_F2,
    WGPU_KEY_F3,
    WGPU_KEY_F4,
    WGPU_KEY_F5,
    WGPU_KEY_F6,
    WGPU_KEY_F7,
    WGPU_KEY_F8,
    WGPU_KEY_F9,
    WGPU_KEY_F10,
    WGPU_KEY_F11,
    WGPU_KEY_F12,
    WGPU_KEY_KP_0,
    WGPU_KEY_KP_1,
    WGPU_KEY_KP_2,
    WGPU_KEY_KP_3,
    WGPU_KEY_KP_4,
    WGPU_KEY_KP_5,
    WGPU_KEY_KP_6,
    WGPU_KEY_KP_7,
    WGPU_KEY_KP_8,
    WGPU_KEY_KP_9,
    WGPU_KEY_KP_DECIMAL,
    WGPU_KEY_KP_DIVIDE,
    WGPU_KEY_KP_MULTIPLY,
    WGPU_KEY_KP_SUBTRACT,
    WGPU_KEY_KP_ADD,
    WGPU_KEY_KP_ENTER,
    WGPU_KEY_KP_EQUAL,
    WGPU_KEY_LEFT_SHIFT,
    WGPU_KEY_LEFT_CONTROL,
    WGPU_KEY_LEFT_ALT,
    WGPU_KEY_LEFT_SUPER,
    WGPU_KEY_RIGHT_SHIFT,
    WGPU_KEY_RIGHT_CONTROL,
    WGPU_KEY_RIGHT_ALT,
    WGPU_KEY_RIGHT_SUPER,
    WGPU_KEY_MENU,
} wgpu_keycode_t;

typedef void (*wgpu_init_func)(void);
typedef void (*wgpu_frame_func)(void);
typedef void (*wgpu_shutdown_func)(void);
typedef void (*wgpu_key_func)(int key);
typedef void (*wgpu_char_func)(unsigned int c);
typedef void (*wgpu_mouse_btn_func)(int btn);
typedef void (*wgpu_mouse_pos_func)(float x, float y);
typedef void (*wgpu_mouse_wheel_func)(float v);

typedef struct {
    const char *title;
    float res_x;
    float res_y;
    uint32_t sample_count;
    bool no_depth_buffer;
    wgpu_init_func init_cb;
    wgpu_frame_func frame_cb;
    wgpu_shutdown_func shutdown_cb;
} wgpu_desc_t;

typedef struct {
    wgpu_desc_t desc;
    GLFWwindow *window;
    float width;
    float height;
    float clear_color[4];
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUSurface surface;
    WGPUSurfaceTexture surface_texture;
    WGPUTextureView surface_view;
    WGPUSurfaceCapabilities surface_capabilities;
    WGPUCommandEncoder cmd_encoder;
    WGPUTextureFormat render_format;
    WGPUTexture depth_stencil_tex;
    WGPUTextureView depth_stencil_view;
    WGPUTexture msaa_tex;
    WGPUTextureView msaa_view;
    wgpu_key_func key_down_cb;
    wgpu_key_func key_up_cb;
    wgpu_char_func char_cb;
    wgpu_mouse_btn_func mouse_btn_down_cb;
    wgpu_mouse_btn_func mouse_btn_up_cb;
    wgpu_mouse_pos_func mouse_pos_cb;
    wgpu_mouse_wheel_func mouse_wheel_cb;
    bool async_setup_done;
    bool async_setup_failed;
    double last_frame_time;
#ifdef NANO_CIMGUI
    nano_cimgui_data *imgui_data;
#endif
} wgpu_state_t;

static wgpu_state_t state;

// Quick and dirty logging
#ifdef WGPU_BACKEND_DEBUG
    #define WGPU_LOG(...)                                                      \
        printf("\033[0;33m[NANO BACKEND]: ");                                  \
        printf(__VA_ARGS__);                                                   \
        printf("\033[0m")
#else
    #define WGPU_LOG(...)
#endif

#define wgpu_def(val, def) ((val == 0) ? def : val)

// Forward declaration of the platform-specific start function
// (in this case its just emscripten)
void wgpu_platform_start(wgpu_state_t *state);
void wgpu_stop(void);
void wgpu_surface_configure(wgpu_state_t *state);
void wgpu_surface_reinit(wgpu_state_t *state);
static double emsc_get_frametime(void);
static bool emsc_fullscreen(char *id);

void wgpu_start(const wgpu_desc_t *desc) {
    assert(desc);
    assert(desc->title);
    assert((desc->res_x > 0) && (desc->res_y > 0));
    assert(desc->init_cb && desc->frame_cb && desc->shutdown_cb);

    state.desc = *desc;
    state.width = state.desc.res_x;
    state.height = state.desc.res_y;
    state.desc.sample_count = wgpu_def(state.desc.sample_count, 1);
    memcpy(state.clear_color, (float[4]){0.0f, 0.0f, 0.0f, 1.0f},
           sizeof(float[4]));

    wgpu_platform_start(&state);
}

wgpu_state_t *wgpu_get_state(void) { return &state; }

WGPUDevice wgpu_get_device(void) { return state.device; }

int wgpu_width(void) { return state.width; }

int wgpu_height(void) { return state.height; }

void wgpu_key_down(wgpu_key_func fn) { state.key_down_cb = fn; }

void wgpu_key_up(wgpu_key_func fn) { state.key_up_cb = fn; }

void wgpu_char(wgpu_char_func fn) { state.char_cb = fn; }

double last_time;
double wgpu_frametime(void) {
    last_time = last_time == 0 ? glfwGetTime() : last_time;
    double current_time = glfwGetTime();
    double frame_time = current_time - last_time;
    last_time = current_time;
    return frame_time;
}

void wgpu_mouse_btn_down(wgpu_mouse_btn_func fn) {
    state.mouse_btn_down_cb = fn;
}

void wgpu_mouse_btn_up(wgpu_mouse_btn_func fn) { state.mouse_btn_up_cb = fn; }

void wgpu_mouse_pos(wgpu_mouse_pos_func fn) { state.mouse_pos_cb = fn; }

void wgpu_mouse_wheel(wgpu_mouse_wheel_func fn) { state.mouse_wheel_cb = fn; }

static WGPUTextureView wgpu_get_render_view(void) {
    if (state.desc.sample_count > 1) {
        assert(state.msaa_view);
        return state.msaa_view;
    } else {
        /*// TODO: MAKE SURE TO USE SURFACE TEXTURE HERE*/
        /*// WE HAVE TO SET THE SURACE TEXTURE TO A WGPUTEXTURE*/
        /*// AND THEN USE THAT TO CREATE THE VIEW*/
        /*wgpuSurfaceGetCurrentTexture(state.surface, &state.surface_texture);*/
        assert(state.surface_view);
        return state.surface_view;
    }
}

static WGPUTextureView wgpu_get_resolve_view(void) {
    if (state.desc.sample_count > 1) {
        return wgpu_get_render_view();
    } else {
        return 0;
    }
}

static WGPUTextureView wgpu_get_depth_stencil_view(void) {
    return state.depth_stencil_view;
}

static WGPUTextureFormat wgpu_get_color_format(void) {
    return state.render_format;
}

static WGPUTextureFormat wgpu_get_depth_format(void) {
    return WGPUTextureFormat_Depth32FloatStencil8;
}

// GLFW specific code
// ----------------------------------------------------------------------------

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void glfw_key_callback(GLFWwindow *window, int key, int scancode,
                              int action, int mods) {
    wgpu_state_t *state = (wgpu_state_t *)glfwGetWindowUserPointer(window);

    if (action == GLFW_PRESS) {
        if (state->key_down_cb) {
            state->key_down_cb(key);
        }
    } else if (action == GLFW_RELEASE) {
        if (state->key_up_cb) {
            state->key_up_cb(key);
        }
    }

#ifdef NANO_CIMGUI
    nano_cimgui_process_key_event(key, action == GLFW_PRESS);
#endif
}

static void glfw_char_callback(GLFWwindow *window, unsigned int codepoint) {
    wgpu_state_t *state = (wgpu_state_t *)glfwGetWindowUserPointer(window);

    if (state->char_cb) {
        state->char_cb(codepoint);
    }

#ifdef NANO_CIMGUI
    nano_cimgui_process_char_event(codepoint);
#endif
}

static void glfw_mouse_button_callback(GLFWwindow *window, int button,
                                       int action, int mods) {
    wgpu_state_t *state = (wgpu_state_t *)glfwGetWindowUserPointer(window);

    if (action == GLFW_PRESS) {
        if (state->mouse_btn_down_cb) {
            state->mouse_btn_down_cb(button);
        }
    } else if (action == GLFW_RELEASE) {
        if (state->mouse_btn_up_cb) {
            state->mouse_btn_up_cb(button);
        }
    }

#ifdef NANO_CIMGUI
    nano_cimgui_process_mousepress_event(button, action == GLFW_PRESS);
#endif
}

static void glfw_cursor_pos_callback(GLFWwindow *window, double xpos,
                                     double ypos) {
    wgpu_state_t *state = (wgpu_state_t *)glfwGetWindowUserPointer(window);

    if (state->mouse_pos_cb) {
        state->mouse_pos_cb((float)xpos, (float)ypos);
    }

#ifdef NANO_CIMGUI
    nano_cimgui_process_mousepos_event((float)xpos, (float)ypos);
#endif
}

static void glfw_scroll_callback(GLFWwindow *window, double xoffset,
                                 double yoffset) {
    wgpu_state_t *state = (wgpu_state_t *)glfwGetWindowUserPointer(window);

    if (state->mouse_wheel_cb) {
        state->mouse_wheel_cb((float)yoffset);
    }

#ifdef NANO_CIMGUI
    nano_cimgui_process_mousewheel_event((float)yoffset);
#endif
}

static void glfw_framebuffer_size_callback(GLFWwindow *window, int width,
                                           int height) {
    wgpu_state_t *state = (wgpu_state_t *)glfwGetWindowUserPointer(window);
    state->width = width;
    state->height = height;
    wgpu_surface_reinit(state);

#ifdef NANO_CIMGUI
    nano_cimgui_scale_to_canvas(state->desc.res_x, state->desc.res_y,
                                state->width, state->height);
#endif
}

static double glfw_get_frametime(void) {
    static double last_time = 0;
    double current_time = glfwGetTime();
    double delta = current_time - last_time;
    last_time = current_time;
    return delta;
}

static void error_cb(WGPUErrorType type, const char *message, void *userdata) {
    (void)type;
    (void)userdata;
    if (type != WGPUErrorType_NoError) {
        WGPU_LOG("WGPU Backend: ERROR: %s\n", message);
    }
}

static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device,
                              const char *msg, void *userdata) {
    (void)status;
    (void)msg;
    (void)userdata;
    wgpu_state_t *state = (wgpu_state_t *)userdata;
    if (status != WGPURequestDeviceStatus_Success) {
        WGPU_LOG("WGPU Backend: wgpuAdapterRequestDevice failed with %s!\n",
                 msg);
        state->async_setup_failed = true;
        return;
    }
    state->device = device;

    WGPU_LOG("WGPU Backend: Device created successfully.\n");
    WGPU_LOG("WGPU Backend: Device: %p\n", state->device);

    /*wgpuDeviceSetUncapturedErrorCallback(state->device, error_cb, 0);*/
    wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);

    // Set the render format to the first available format
    state->render_format = WGPUTextureFormat_RGBA8Unorm;

    // Create the swapchain (surface) texture
    wgpu_surface_configure(state);
#ifdef NANO_CIMGUI
    // Once the swapchain is created, we can initialize ImGui
    // This is only done if the NANO_CIMGUI macro is defined
    state->imgui_data = nano_cimgui_init(
        state->device, 2, wgpu_get_color_format(), WGPUTextureFormat_Undefined,
        state->desc.res_x, state->desc.res_y, state->width, state->height,
        state->desc.sample_count, NULL, state->window);
    if (!state->imgui_data) {
        WGPU_LOG("WGPU Backend: nano_cimgui_init() failed.\n");
        state->async_setup_failed = true;
        return;
    }
#endif
    state->desc.init_cb();
    wgpuDevicePopErrorScope(state->device, error_cb, 0);
    state->async_setup_done = true;
}

static void request_adapter_cb(WGPURequestAdapterStatus status,
                               WGPUAdapter adapter, const char *msg,
                               void *userdata) {
    (void)msg;
    wgpu_state_t *state = userdata;
    if (status != WGPURequestAdapterStatus_Success) {
        WGPU_LOG("WGPU Backend: wgpuInstanceRequestAdapter failed!\n");
        state->async_setup_failed = true;
    }
    state->adapter = adapter;

    WGPUFeatureName requiredFeatures[1] = {
        WGPUFeatureName_Depth32FloatStencil8};
    WGPUDeviceDescriptor dev_desc = {
        .requiredFeatureCount = 1,
        .requiredFeatures = requiredFeatures,
    };
    wgpuAdapterRequestDevice(adapter, &dev_desc, request_device_cb, userdata);
}

static GLboolean glfw_frame(double time, void *userdata) {
    (void)time;
    wgpu_state_t *state = userdata;
    if (state->async_setup_failed) {
        return GL_FALSE;
    }
    if (!state->async_setup_done) {
        return GL_TRUE;
    }
    wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);
    state->desc.frame_cb();
    wgpuDevicePopErrorScope(state->device, error_cb, 0);
    return GL_TRUE;
}

void wgpu_platform_start(wgpu_state_t *state) {
    glfwSetErrorCallback(glfw_error_callback);

    WGPU_LOG("WGPU Backend: Initializing GLFW\n");
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return;
    }
    WGPU_LOG("WGPU Backend: GLFW initialized successfully\n");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    state->window = glfwCreateWindow(state->width, state->height,
                                     state->desc.title, NULL, NULL);
    GLFWwindow *window = state->window;
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return;
    }

    WGPU_LOG("WGPU Backend: GLFW window created successfully\n");

    WGPU_LOG("GLFW Context: %p\n", window);

    WGPU_LOG("WGPU Backend: Setting up GLFW callbacks\n");
    {
        glfwSetWindowUserPointer(window, state);
        glfwSetKeyCallback(window, glfw_key_callback);
        glfwSetCharCallback(window, glfw_char_callback);
        glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
        glfwSetCursorPosCallback(window, glfw_cursor_pos_callback);
        glfwSetScrollCallback(window, glfw_scroll_callback);
        glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
    }

    WGPU_LOG("WGPU Backend: Creating wgpu instance\n");
    // WebGPU setup
    state->instance = wgpuCreateInstance(NULL);
    assert(state->instance);

    WGPU_LOG("WGPU Backend: Creating wgpu surface\n");

    state->surface = glfwGetWGPUSurface(state->instance, window);
    assert(state->surface);

    // Get the adapter
    WGPURequestAdapterOptions options = {
        .compatibleSurface = state->surface,
        .backendType = WGPUBackendType_Vulkan,
        .powerPreference = WGPUPowerPreference_HighPerformance,
        .forceFallbackAdapter = false,
    };

    wgpuInstanceRequestAdapter(state->instance, &options, request_adapter_cb,
                               state);

    WGPU_LOG("WGPU Backend: Starting main loop\n");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (state->async_setup_done) {
            state->desc.frame_cb();
        }
    }

    wgpu_stop();
    glfwDestroyWindow(window);
    glfwTerminate();
}

// swapchain
// ----------------------------------------------------------------------------
void wgpu_surface_configure(wgpu_state_t *state) {
    assert(state->adapter);
    assert(state->device);
    assert(state->surface);
    assert(state->render_format != WGPUTextureFormat_Undefined);
    assert(0 == state->depth_stencil_tex);
    assert(0 == state->depth_stencil_view);
    assert(0 == state->msaa_tex);
    assert(0 == state->msaa_view);

    WGPU_LOG("WGPU Backend: Creating surface with dimensions: %dx%d\n",
             (int)state->width, (int)state->height);

    WGPUSurfaceCapabilities capabilities;
    wgpuSurfaceGetCapabilities(state->surface, state->adapter, &capabilities);

    /*WGPU_LOG("WGPU Backend: Surface capabilities:\n");*/
    /*WGPU_LOG("WGPU Backend:   - Present modes: %s", capabilities);*/

    WGPUSurfaceConfiguration config = {
        .device = state->device,
        .format = WGPUTextureFormat_BGRA8UnormSrgb,
        .usage = WGPUTextureUsage_RenderAttachment,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .width = (uint32_t)state->width,
        .height = (uint32_t)state->height,
        .presentMode = WGPUPresentMode_Fifo,
    };

    if (state->surface == NULL) {
        WGPU_LOG("WGPU Backend: Surface is NULL\n");
    }

    wgpuSurfaceConfigure(state->surface, &config);
    
    wgpuSurfaceGetCurrentTexture(state->surface, &state->surface_texture);
    if (state->surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        WGPU_LOG("WGPU Backend: Surface texture creation failed\n");
        return;
    }

    state->surface_view = wgpuTextureCreateView(
            state->surface_texture.texture,
            &(WGPUTextureViewDescriptor){
                .format = wgpuTextureGetFormat(state->surface_texture.texture),
                .dimension = WGPUTextureViewDimension_2D,
                .baseMipLevel = 0,
                .mipLevelCount = 1,
                .baseArrayLayer = 0,
                .arrayLayerCount = 1,
            });

    assert(state->surface_view);

    WGPU_LOG("WGPU Backend: Surface created successfully.\n");

    if (!state->desc.no_depth_buffer) {
        WGPU_LOG(
            "WGPU Backend: Creating depth stencil texture with dimensions: "
            "%dx%d\n",
            (int)state->width, (int)state->height);
        state->depth_stencil_tex = wgpuDeviceCreateTexture(
            state->device,
            &(WGPUTextureDescriptor){
                .usage = WGPUTextureUsage_RenderAttachment,
                .dimension = WGPUTextureDimension_2D,
                .size =
                    {
                        .width = (uint32_t)state->width,
                        .height = (uint32_t)state->height,
                        .depthOrArrayLayers = 1,
                    },
                .format = WGPUTextureFormat_Depth32FloatStencil8,
                .mipLevelCount = 1,
                .sampleCount = (uint32_t)state->desc.sample_count});
        assert(state->depth_stencil_tex);
        state->depth_stencil_view =
            wgpuTextureCreateView(state->depth_stencil_tex, NULL);
        assert(state->depth_stencil_view);
        WGPU_LOG("WGPU Backend: Depth stencil texture created successfully.\n");
    }

    if (state->desc.sample_count > 1) {
        WGPU_LOG("WGPU Backend: Creating MSAA texture with dimensions: %dx%d\n",
                 (int)state->width, (int)state->height);
        state->msaa_tex = wgpuDeviceCreateTexture(
            state->device,
            &(WGPUTextureDescriptor){
                .usage = WGPUTextureUsage_RenderAttachment,
                .dimension = WGPUTextureDimension_2D,
                .size =
                    {
                        .width = (uint32_t)state->width,
                        .height = (uint32_t)state->height,
                        .depthOrArrayLayers = 1,
                    },
                .format = state->render_format,
                .mipLevelCount = 1,
                .sampleCount = (uint32_t)state->desc.sample_count,
            });
        
        assert(state->msaa_tex);
        state->msaa_view = wgpuTextureCreateView(state->msaa_tex, NULL);
        
        assert(state->msaa_view);
        WGPU_LOG("WGPU Backend: MSAA texture created successfully.\n");
    }
}

// Release any resources that depend on the surface
void wgpu_surface_discard(wgpu_state_t *state) {
// Release any ImGui resources that depend on the surface
#ifdef NANO_CIMGUI
    nano_cimgui_invalidate_device_objects();
#endif
    if (state->msaa_view) {
        wgpuTextureViewRelease(state->msaa_view);
        state->msaa_view = 0;
    }
    if (state->msaa_tex) {
        wgpuTextureRelease(state->msaa_tex);
        state->msaa_tex = 0;
    }
    if (state->depth_stencil_view) {
        wgpuTextureViewRelease(state->depth_stencil_view);
        state->depth_stencil_view = 0;
    }
    if (state->depth_stencil_tex) {
        wgpuTextureRelease(state->depth_stencil_tex);
        state->depth_stencil_tex = 0;
    }
    if (state->surface_texture.texture) {
        wgpuTextureRelease(state->surface_texture.texture);
        state->surface_texture.texture = 0;
    }
    if (state->surface_view) {
        wgpuTextureViewRelease(state->surface_view);
        state->surface_view = 0;
    }
}

// Handle any surface reinitialization
// Normally this is called when we swap to MSAA or back to non-MSAA
void wgpu_surface_reinit(wgpu_state_t *state) {

#ifdef NANO_CIMGUI
    state->imgui_data->multiSampleCount = state->desc.sample_count;
#endif
    // Release the old surface
    wgpu_surface_discard(state);

    // Reinitialize the surface
    wgpu_surface_configure(state);
}

void wgpu_stop(void) {
    if (state.desc.shutdown_cb) {
        state.desc.shutdown_cb();
    }
    wgpu_surface_discard(&state);
    if (state.device) {
        wgpuDeviceRelease(state.device);
        state.device = 0;
    }
    if (state.adapter) {
        wgpuAdapterRelease(state.adapter);
        state.adapter = 0;
    }
    if (state.instance) {
        wgpuInstanceRelease(state.instance);
        state.instance = 0;
    }

#ifdef NANO_CIMGUI
    nano_cimgui_shutdown();
#endif
}

#endif // NANO_NATIVE_H
