#ifndef WGPU_ENTRY_H
#define WGPU_ENTRY_H
#include <assert.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <webgpu/webgpu.h>

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
    float width;
    float height;
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUSurface surface;
    float clear_color[4];
    WGPUCommandEncoder cmd_encoder;
    WGPUSwapChain swapchain;
    WGPUTextureFormat render_format;
    WGPUTexture depth_stencil_tex;
    WGPUTextureView depth_stencil_view;
    WGPUTexture msaa_tex;
    WGPUTextureView msaa_view;
    WGPUTextureView swapchain_view;
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

typedef struct wgpu_swapchain_info_t {
    WGPUTextureView render_view;
    WGPUTextureView resolve_view;
} wgpu_swapchain_info_t;

static wgpu_state_t state;

// Quick and dirty logging
#ifdef WGPU_BACKEND_DEBUG
    #define WGPU_LOG(...) printf(__VA_ARGS__)
#else
    #define WGPU_LOG(...)
#endif

#define wgpu_def(val, def) ((val == 0) ? def : val)

// Forward declaration of the platform-specific start function
// (in this case its just emscripten)
void wgpu_platform_start(wgpu_state_t *state);
void wgpu_swapchain_init(wgpu_state_t *state);
void wgpu_swapchain_reinit(wgpu_state_t *state);
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

double wgpu_frametime(void) {
    double frame_time = emsc_get_frametime();
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
        assert(state.swapchain);
        return wgpuSwapChainGetCurrentTextureView(state.swapchain);
    }
}

static WGPUTextureView wgpu_get_resolve_view(void) {
    if (state.desc.sample_count > 1) {
        return wgpuSwapChainGetCurrentTextureView(state.swapchain);
    } else {
        return 0;
    }
}

static const void *wgpu_get_depth_stencil_view(void) {
    return (const void *)state.depth_stencil_view;
}

static WGPUTextureFormat wgpu_get_color_format(void) {
    return state.render_format;
}

static WGPUTextureFormat wgpu_get_depth_format(void) {
    if (state.desc.no_depth_buffer) {
        return WGPUTextureFormat_Undefined;
    } else {
        return WGPUTextureFormat_Depth32FloatStencil8;
    }
}

// Expose a function to toggle fullscreen so we can use any key to toggle
static bool wgpu_toggle_fullscreen(char *id) {
    return emsc_fullscreen(id) == EMSCRIPTEN_RESULT_SUCCESS;
}

// Emscripten specific code
// ----------------------------------------------------------------------------

// Emscripten Fullscreen Implementation
static bool emsc_fullscreen(char *id) {
    EMSCRIPTEN_RESULT res = EMSCRIPTEN_RESULT_FAILED;
    if (!id) {
        res = emscripten_request_fullscreen("#canvas", 1);
    } else {
        res = emscripten_request_fullscreen(id, 1);
    }
    return res;
}

static double emsc_get_frametime(void) {
    double now = emscripten_get_now();
    if (state.last_frame_time > 0.0) {
        double frametime = now - state.last_frame_time;
        state.last_frame_time = now;
        return frametime;
    }
    state.last_frame_time = now;
    return 0;
}

// For whatever reason, the state pointer is not being passed to the callback
// functions, so we have to use a global state variable to access the state for
// this method. I really do not know why this is happening.
static void emsc_update_canvas_size() {
    double w, h;
    emscripten_get_element_css_size("#canvas", &w, &h);
    emscripten_set_canvas_element_size("#canvas", w, h);
    state.width = (int)w;
    state.height = (int)h;

    WGPU_LOG("WGPU Backend -> emsc_update_canvas_size(): %.2f %.2f\n",
             state.width, state.height);
}

static EM_BOOL emsc_size_changed(int event_type,
                                 const EmscriptenUiEvent *ui_event,
                                 void *userdata) {
    (void)event_type;
    (void)ui_event;
    // // For some reason, the state pointer is not being passed to the callback
    // // for this function and I get an address of 0. So we can ignore the
    // // userdata.
    // wgpu_state_t *state = (wgpu_state_t *)userdata;
    emsc_update_canvas_size();
    wgpu_swapchain_reinit(&state);
#ifdef NANO_CIMGUI
    nano_cimgui_scale_to_canvas(state.desc.res_x, state.desc.res_y, state.width,
                                state.height);
#endif
    return true;
}

static struct {
    const char *str;
    wgpu_keycode_t code;
} emsc_keymap[] = {
    {"Backspace", WGPU_KEY_BACKSPACE},
    {"Tab", WGPU_KEY_TAB},
    {"Enter", WGPU_KEY_ENTER},
    {"Escape", WGPU_KEY_ESCAPE},
    {"Space", WGPU_KEY_SPACE},
    {"End", WGPU_KEY_END},
    {"Home", WGPU_KEY_HOME},
    {"ArrowLeft", WGPU_KEY_LEFT},
    {"ArrowUp", WGPU_KEY_UP},
    {"ArrowRight", WGPU_KEY_RIGHT},
    {"ArrowDown", WGPU_KEY_DOWN},
    {"Delete", WGPU_KEY_DELETE},
    {"Digit0", WGPU_KEY_0},
    {"Digit1", WGPU_KEY_1},
    {"Digit2", WGPU_KEY_2},
    {"Digit3", WGPU_KEY_3},
    {"Digit4", WGPU_KEY_4},
    {"Digit5", WGPU_KEY_5},
    {"Digit6", WGPU_KEY_6},
    {"Digit7", WGPU_KEY_7},
    {"Digit8", WGPU_KEY_8},
    {"Digit9", WGPU_KEY_9},
    {"KeyA", WGPU_KEY_A},
    {"KeyB", WGPU_KEY_B},
    {"KeyC", WGPU_KEY_C},
    {"KeyD", WGPU_KEY_D},
    {"KeyE", WGPU_KEY_E},
    {"KeyF", WGPU_KEY_F},
    {"KeyG", WGPU_KEY_G},
    {"KeyH", WGPU_KEY_H},
    {"KeyI", WGPU_KEY_I},
    {"KeyJ", WGPU_KEY_J},
    {"KeyK", WGPU_KEY_K},
    {"KeyL", WGPU_KEY_L},
    {"KeyM", WGPU_KEY_M},
    {"KeyN", WGPU_KEY_N},
    {"KeyO", WGPU_KEY_O},
    {"KeyP", WGPU_KEY_P},
    {"KeyQ", WGPU_KEY_Q},
    {"KeyR", WGPU_KEY_R},
    {"KeyS", WGPU_KEY_S},
    {"KeyT", WGPU_KEY_T},
    {"KeyU", WGPU_KEY_U},
    {"KeyV", WGPU_KEY_V},
    {"KeyW", WGPU_KEY_W},
    {"KeyX", WGPU_KEY_X},
    {"KeyY", WGPU_KEY_Y},
    {"KeyZ", WGPU_KEY_Z},
    {0, WGPU_KEY_INVALID},
};

static wgpu_keycode_t emsc_translate_key(const char *str) {
    int i = 0;
    const char *keystr;
    while ((keystr = emsc_keymap[i].str)) {
        if (0 == strcmp(str, keystr)) {
            return emsc_keymap[i].code;
        }
        i += 1;
    }
    return WGPU_KEY_INVALID;
}

static EM_BOOL emsc_keydown_cb(int type, const EmscriptenKeyboardEvent *ev,
                               void *userdata) {
    (void)type;
    wgpu_state_t *state = (wgpu_state_t *)userdata;

    // Handle fullscreen toggle above all other key events
    // to allow the user to toggle fullscreen with F11
    if (strcmp(ev->code, "F11") == 0) {
        return emsc_fullscreen("#canvas") == EMSCRIPTEN_RESULT_SUCCESS;
    }

    // Allow browser shortcuts
    if (ev->ctrlKey || ev->altKey || ev->metaKey) {
        return EM_FALSE;
    }

    // Handle key toggling the key state and calling the callback for the
    // backend
    wgpu_keycode_t wgpu_key = emsc_translate_key(ev->code);
    if (WGPU_KEY_INVALID != wgpu_key) {
        if (state->key_down_cb) {
            state->key_down_cb((int)wgpu_key);
        }

        WGPU_LOG("WGPU Backend -> keydown_cb(): %c\n", ev->keyCode);
        // Send the key event to ImGui if it is enabled
#ifdef NANO_CIMGUI
        nano_cimgui_process_key_event((int)wgpu_key, true);
#endif
    }

    // Since the key pressed callback does not seem to be working, we can just
    // use key down
    if (ev->key[1] == '\0') {
        char c = ev->key[0];
        if (c >= 32 && c <= 126) {
            // I'm probably gonna remove this callback since the emsc keypress
            // callback is not working
            if (state->char_cb) {
                state->char_cb(c);
            }
#ifdef NANO_CIMGUI
            nano_cimgui_process_char_event(c);
#endif
        }
    }

    return EM_TRUE;
}

static EM_BOOL emsc_keyup_cb(int type, const EmscriptenKeyboardEvent *ev,
                             void *userdata) {
    (void)type;
    wgpu_state_t *state = (wgpu_state_t *)userdata;

    wgpu_keycode_t wgpu_key = emsc_translate_key(ev->code);
    if (WGPU_KEY_INVALID != wgpu_key) {
        if (state->key_down_cb) {
            state->key_down_cb((int)wgpu_key);
            WGPU_LOG("WGPU Backend -> keydown_cb(): %c\n", ev->keyCode);
        }

#ifdef NANO_CIMGUI
        nano_cimgui_process_key_event((int)wgpu_key, false);
#endif
    }

    return EM_TRUE;
}

static EM_BOOL emsc_keypress_cb(int type, const EmscriptenKeyboardEvent *ev,
                                void *userdata) {
    (void)type;
    wgpu_state_t *state = (wgpu_state_t *)userdata;
    if (state->char_cb) {
        state->char_cb(ev->charCode);
    }

// ImGui Handling
#ifdef NANO_CIMGUI
    nano_cimgui_process_char_event(ev->charCode);
#endif
    return EM_TRUE;
}

static EM_BOOL emsc_mousedown_cb(int type, const EmscriptenMouseEvent *ev,
                                 void *userdata) {
    (void)type;
    wgpu_state_t *state = (wgpu_state_t *)userdata;
    if (ev->button < 3) {
        if (state->mouse_btn_down_cb) {
            state->mouse_btn_down_cb(ev->button);
        }
#ifdef NANO_CIMGUI
        nano_cimgui_process_mousepress_event(ev->button, true);
#endif

        WGPU_LOG("WGPU Backend -> emsc_mousedown_cb(): %d\n", ev->button);
    }

    return EM_TRUE;
}

static EM_BOOL emsc_mouseup_cb(int type, const EmscriptenMouseEvent *ev,
                               void *userdata) {
    (void)type;
    wgpu_state_t *state = (wgpu_state_t *)userdata;
    if (ev->button < 3) {

        if (state->mouse_btn_up_cb) {
            state->mouse_btn_up_cb(ev->button);
        }
#ifdef NANO_CIMGUI
        nano_cimgui_process_mousepress_event(ev->button, false);
#endif
    }

    return EM_TRUE;
}

static EM_BOOL emsc_mousemove_cb(int type, const EmscriptenMouseEvent *ev,
                                 void *userdata) {
    (void)type;
    wgpu_state_t *state = (wgpu_state_t *)userdata;
    #ifdef NANO_CIMGUI
        if (state->imgui_data) {
            nano_cimgui_process_mousepos_event((float)ev->targetX, (float)ev->targetY);
        }
    #endif
    if (state->mouse_pos_cb) {
        state->mouse_pos_cb((float)ev->targetX, (float)ev->targetY);
    }

    return EM_TRUE;
}

static EM_BOOL emsc_wheel_cb(int type, const EmscriptenWheelEvent *ev,
                             void *userdata) {
    (void)type;
    wgpu_state_t *state = (wgpu_state_t *)userdata;
    if (state->mouse_wheel_cb) {
        state->mouse_wheel_cb(-0.1f * (float)ev->deltaY);
    }

#ifdef NANO_CIMGUI
    nano_cimgui_process_mousewheel_event(-0.1f * (float)ev->deltaY);
#endif

    WGPU_LOG("WGPU Backend -> emsc_wheel_cb(): %f\n", ev->deltaY);
    return EM_TRUE;
}

// Emscripten Mobile Keyboard Implementation
// ----------------------------------------------------------------------------

// Touch tap (single touch) event callbacks)
static EM_BOOL emsc_touchstart_cb(int eventType, const EmscriptenTouchEvent *e,
                                  void *userData) {
    wgpu_state_t *state = (wgpu_state_t *)userData;
    if (e->numTouches > 0) {
        if (state->mouse_btn_down_cb) {
            state->mouse_btn_down_cb(0); // Simulate left mouse button
        }
        if (state->mouse_pos_cb) {
            state->mouse_pos_cb((float)e->touches[0].targetX,
                                (float)e->touches[0].targetY);
        }
#ifdef NANO_CIMGUI
        nano_cimgui_process_mousepress_event(0, true);
        nano_cimgui_process_mousepos_event((float)e->touches[0].targetX,
                                           (float)e->touches[0].targetY);
#endif
    }
    return EM_TRUE;
}

// Touch release (single touch) event callbacks
static EM_BOOL emsc_touchend_cb(int eventType, const EmscriptenTouchEvent *e,
                                void *userData) {
    wgpu_state_t *state = (wgpu_state_t *)userData;
    if (state->mouse_btn_up_cb) {
        state->mouse_btn_up_cb(0); // Simulate left mouse button
    }
#ifdef NANO_CIMGUI
    nano_cimgui_process_mousepress_event(0, false);
#endif
    return EM_TRUE;
}

// Touch move (single touch) event callbacks
static EM_BOOL emsc_touchmove_cb(int eventType, const EmscriptenTouchEvent *e,
                                 void *userData) {
    wgpu_state_t *state = (wgpu_state_t *)userData;
    if (e->numTouches > 0) {
        if (state->mouse_pos_cb) {
            state->mouse_pos_cb((float)e->touches[0].targetX,
                                (float)e->touches[0].targetY);
        }
#ifdef NANO_CIMGUI
        nano_cimgui_process_mousepos_event((float)e->touches[0].targetX,
                                           (float)e->touches[0].targetY);
#endif
    }
    return EM_TRUE;
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
    wgpu_state_t *state = userdata;
    if (status != WGPURequestDeviceStatus_Success) {
        WGPU_LOG("WGPU Backend: wgpuAdapterRequestDevice failed with %s!\n",
                 msg);
        state->async_setup_failed = true;
        return;
    }
    state->device = device;

    wgpuDeviceSetUncapturedErrorCallback(state->device, error_cb, 0);
    wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);

    // setup swapchain
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {
        .chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector,
        .selector = "#canvas",
    };
    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = &canvas_desc.chain,
    };
    state->surface = wgpuInstanceCreateSurface(state->instance, &surf_desc);
    if (!state->surface) {
        WGPU_LOG("WGPU Backend: wgpuInstanceCreateSurface() failed.\n");
        state->async_setup_failed = true;
        return;
    }
    state->render_format =
        wgpuSurfaceGetPreferredFormat(state->surface, state->adapter);
    wgpu_swapchain_init(state);
#ifdef NANO_CIMGUI

    // Once the swapchain is created, we can initialize ImGui
    // This is only done if the NANO_CIMGUI macro is defined
    state->imgui_data = nano_cimgui_init(
        state->device, 2, wgpu_get_color_format(), WGPUTextureFormat_Undefined,
        state->desc.res_x, state->desc.res_y, state->width, state->height,
        state->desc.sample_count, NULL);
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

static EM_BOOL emsc_frame(double time, void *userdata) {
    (void)time;
    wgpu_state_t *state = userdata;
    if (state->async_setup_failed) {
        return EM_FALSE;
    }
    if (!state->async_setup_done) {
        return EM_TRUE;
    }
    wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);
    state->desc.frame_cb();
    wgpuDevicePopErrorScope(state->device, error_cb, 0);
    return EM_TRUE;
}

/// Start platform specific code
void wgpu_platform_start(wgpu_state_t *state) {

    // Set the exit callback for the application
    atexit(state->desc.shutdown_cb);

    // Set canvas size callbacks for emscripten
    emsc_update_canvas_size();
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, true,
                                   emsc_size_changed);
    // Set keyboard callbacks for emscripten
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, true,
                                    emsc_keydown_cb);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, true,
                                  emsc_keyup_cb);
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state,
                                     true, emsc_keypress_cb);

    // Set mouse callbacks for emscripten
    emscripten_set_mousedown_callback("#canvas", state, true,
                                      emsc_mousedown_cb);
    emscripten_set_mouseup_callback("#canvas", state, true, emsc_mouseup_cb);
    emscripten_set_mousemove_callback("#canvas", state, true,
                                      emsc_mousemove_cb);
    emscripten_set_wheel_callback("#canvas", state, true, emsc_wheel_cb);

    // Set touch callbacks for emscripten
    emscripten_set_touchstart_callback("#canvas", state, true,
                                       emsc_touchstart_cb);
    emscripten_set_touchend_callback("#canvas", state, true, emsc_touchend_cb);
    emscripten_set_touchmove_callback("#canvas", state, true,
                                      emsc_touchmove_cb);

    state->instance = wgpuCreateInstance(0);
    assert(state->instance);

    wgpuInstanceRequestAdapter(state->instance, 0, request_adapter_cb, state);

    emscripten_request_animation_frame_loop(emsc_frame, state);
}

// swapchain
// ----------------------------------------------------------------------------
void wgpu_swapchain_init(wgpu_state_t *state) {
    assert(state->adapter);
    assert(state->device);
    assert(state->surface);
    assert(state->render_format != WGPUTextureFormat_Undefined);
    assert(0 == state->swapchain);
    assert(0 == state->depth_stencil_tex);
    assert(0 == state->depth_stencil_view);
    assert(0 == state->msaa_tex);
    assert(0 == state->msaa_view);

    WGPU_LOG("WGPU Backend: Creating swapchain with dimensions: %dx%d\n",
             (int)state->width, (int)state->height);

    state->swapchain = wgpuDeviceCreateSwapChain(
        state->device, state->surface,
        &(WGPUSwapChainDescriptor){
            .usage = WGPUTextureUsage_RenderAttachment,
            .format = state->render_format,
            .width = (uint32_t)state->width,
            .height = (uint32_t)state->height,
            .presentMode = WGPUPresentMode_Fifo,
        });

    assert(state->swapchain);
    WGPU_LOG("WGPU Backend: Swapchain created successfully.\n");

    if (!state->desc.no_depth_buffer) {
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
    }
}

void wgpu_swapchain_discard(wgpu_state_t *state) {

// Release any ImGui resources that depend on the swapchain
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
    if (state->swapchain) {
        wgpuSwapChainRelease(state->swapchain);
        state->swapchain = 0;
    }
}

// Handle any swapchain reinitialization
// Normally this is called when we swap to MSAA or back to non-MSAA
void wgpu_swapchain_reinit(wgpu_state_t *state) {

#ifdef NANO_CIMGUI
    state->imgui_data->multiSampleCount = state->desc.sample_count;
#endif

    // Release the old swapchain
    wgpu_swapchain_discard(state);

    // Reinitialize the swapchain
    wgpu_swapchain_init(state);
}

void wgpu_stop(void) {
    if (state.desc.shutdown_cb) {
        state.desc.shutdown_cb();
    }
    wgpu_swapchain_discard(&state);
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
#endif // WGPU_ENTRY_H
