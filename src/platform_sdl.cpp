#include "platform.h"
#include "keyboard.h"
#include "status.h"

#include <SDL.h>
#include <bitset>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#elif defined(RR_SDL_GLES)
#include <GLES3/gl3.h>
#else
#include <SDL_opengl.h>
#endif
#include <png.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

struct rr_input { SDL_GameController* controller; };
struct rr_input_state {
    rr_button_state_t buttons[RRInputButton_Quit + 1];
    rr_thumb_t sticks[2];
};
struct rr_audio {
    SDL_AudioDeviceID device;
    int volume;
    int frequency;
    std::vector<short> mix_buffer;
};
struct rr_display { SDL_Window* window; int width; int height; int brightness; };
struct rr_surface {
    rr_display_t* display;
    int width, height, stride;
    uint32_t format;
    uint64_t generation;
    std::vector<uint8_t> pixels;
};
struct rr_presenter {
    rr_display_t* display;
    SDL_Renderer* renderer;
    uint32_t background;
    bool loading_wait_completed;
    struct texture_entry {
        SDL_Texture* texture = NULL;
        int width = 0;
        int height = 0;
        Uint32 format = SDL_PIXELFORMAT_UNKNOWN;
        uint64_t generation = 0;
    };
    std::unordered_map<const rr_surface_t*, texture_entry> textures;
};
struct rr_context {
    rr_display_t* display;
    SDL_Window* window;
    SDL_GLContext gl;
    int width;
    int height;
    int framebuffer_width;
    int framebuffer_height;
    bool owns_window;
    GLuint framebuffer;
    GLuint color_texture;
    GLuint depth_stencil;
    GLuint post_program;
    GLuint post_vao;
    GLuint post_vbo;
    GLint uniform_frame_texture;
    GLint uniform_source_size;
    GLint uniform_texture_scale;
    GLint uniform_shader_mode;
    GLint uniform_rotation;
    GLuint overlay_texture;
    int overlay_texture_width;
    int overlay_texture_height;
    GLenum overlay_texture_format;
    const rr_surface_t* overlay_uploaded_surface;
    uint64_t overlay_uploaded_generation;
    std::vector<uint8_t> overlay_upload;
    GLint post_texture_filter;
    bool post_pipeline_failed;
    bool default_framebuffer;
};

static SDL_GameController* active_controller = NULL;
static char renderer_name[128] = "SDL2";
static bool vsync_enabled = false;
static rr_video_filter_t video_filter = RR_VIDEO_FILTER_DEFAULT;
static rr_video_shader_t video_shader = RR_VIDEO_SHADER_OFF;
static rr_presenter_t* active_presenter = NULL;
static rr_context_t* active_context = NULL;
static bool controller_mappings_loaded = false;
static uint64_t next_surface_generation = 1;

static void refresh_display_size(rr_display_t* display) {
    if (!display || !display->window) return;
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(display->window, &width, &height);
    if (width <= 0 || height <= 0 ||
        (width == display->width && height == display->height)) return;

    display->width = width;
    display->height = height;
    if (active_presenter && active_presenter->display == display && active_presenter->renderer)
        SDL_RenderSetLogicalSize(active_presenter->renderer, width, height);
}

static void ensure_sdl(uint32_t flags) {
    if ((SDL_WasInit(flags) & flags) == flags) return;
    const int result = SDL_InitSubSystem(flags);
    if (result != 0)
        std::fprintf(stderr, "RetroRun SDL initialization failed (0x%x): %s\n", flags, SDL_GetError());
}

static void load_controller_mapping_file(const char* path) {
    if (!path || !*path) return;

    std::FILE* file = std::fopen(path, "rb");
    if (!file) return;
    std::fclose(file);

    const int added = SDL_GameControllerAddMappingsFromFile(path);
    if (added >= 0)
        std::fprintf(stderr, "RetroRun SDL controller DB: loaded %d mapping(s) from %s\n",
                     added, path);
    else
        std::fprintf(stderr, "RetroRun SDL controller DB: could not read %s: %s\n",
                     path, SDL_GetError());
}

static void load_controller_mappings() {
    if (controller_mappings_loaded) return;
    controller_mappings_loaded = true;

    // Allow distributions and launch scripts to select their own database.
    load_controller_mapping_file(std::getenv("RETRORUN_SDL_CONTROLLER_DB"));

    // Locations used by AmberELEC/ArkOS and common Linux packages.
    static const char* paths[] = {
        "/storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt",
        "/home/ark/.config/SDL-GameControllerDB/gamecontrollerdb.txt",
        "/usr/share/games/SDL_GameControllerDB/gamecontrollerdb.txt",
        "/usr/share/SDL_GameControllerDB/gamecontrollerdb.txt",
    };
    for (const char* path : paths) load_controller_mapping_file(path);
}

static void add_handheld_controller_fallback(int device_index) {
    const char* name = SDL_JoystickNameForIndex(device_index);
    if (!name || std::strcmp(name, "GO-Super Gamepad") != 0) return;

    const SDL_JoystickGUID device_guid = SDL_JoystickGetDeviceGUID(device_index);
    char guid[33] = {};
    SDL_JoystickGetGUIDString(device_guid, guid, static_cast<int>(sizeof(guid)));

    // Some SDL versions used by ArkOS already recognize this controller, but
    // ship an incomplete mapping without Start/Back/stick clicks. In that
    // case SDL_IsGameController() is true, so checking recognition alone is
    // not sufficient.
    char* existing_mapping = SDL_GameControllerMappingForGUID(device_guid);
    const bool has_start = existing_mapping &&
                           std::strstr(existing_mapping, ",start:") != NULL;
    if (has_start) {
        SDL_free(existing_mapping);
        return;
    }
    if (existing_mapping)
        std::fprintf(stderr,
                     "RetroRun SDL controller: replacing incomplete RG351 mapping: %s\n",
                     existing_mapping);
    SDL_free(existing_mapping);

    // RG351/RK3326 layout. Keep ArkOS' working ABXY ordering and add the
    // missing system buttons using the layout published for this gamepad.
    // The GUID is obtained from the device to support SDL builds whose GUID
    // contains a kernel-dependent CRC component.
    const std::string mapping =
        std::string(guid) +
        ",GO-Super Gamepad,"
        "a:b0,b:b1,x:b2,y:b3,back:b12,start:b13,"
        "dpleft:b10,dpdown:b9,dpright:b11,dpup:b8,"
        "leftshoulder:b4,lefttrigger:b6,rightshoulder:b5,righttrigger:b7,"
        "leftstick:b14,rightstick:b15,leftx:a0,lefty:a1,rightx:a2,righty:a3,"
        "platform:Linux,";
    const int result = SDL_GameControllerAddMapping(mapping.c_str());
    std::fprintf(stderr,
                 "RetroRun SDL controller: complete RG351 mapping %s for GUID %s\n",
                 result >= 0 ? "enabled" : "failed", guid);
    if (result < 0)
        std::fprintf(stderr, "RetroRun SDL controller mapping error: %s\n", SDL_GetError());
}

static SDL_GameController* open_controller(int device_index) {
    const char* name = SDL_JoystickNameForIndex(device_index);
    char guid[33] = {};
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device_index), guid,
                              static_cast<int>(sizeof(guid)));

    add_handheld_controller_fallback(device_index);
    const bool recognized = SDL_IsGameController(device_index) == SDL_TRUE;
    std::fprintf(stderr,
                 "RetroRun SDL joystick[%d]: name='%s', GUID=%s, gamecontroller=%s\n",
                 device_index, name ? name : "unknown", guid,
                 recognized ? "yes" : "no");
    if (!recognized) return NULL;

    SDL_GameController* controller = SDL_GameControllerOpen(device_index);
    if (!controller) {
        std::fprintf(stderr, "RetroRun SDL controller open failed: %s\n", SDL_GetError());
        return NULL;
    }

    char* mapping = SDL_GameControllerMapping(controller);
    const char* controller_name = SDL_GameControllerName(controller);
    std::fprintf(stderr, "RetroRun SDL controller opened: %s%s%s\n",
                 controller_name ? controller_name : "unknown",
                 mapping ? ", mapping=" : "",
                 mapping ? mapping : "");
    SDL_free(mapping);
    return controller;
}

static SDL_GameController* open_first_controller() {
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_GameController* controller = open_controller(i);
        if (controller) return controller;
    }
    return NULL;
}

static int env_dimension(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (!value) return fallback;
    const int parsed = std::atoi(value);
    return parsed > 0 ? parsed : fallback;
}

void rr_platform_preinit() {
    // KMSDRM may load GBM/DRM driver state and create address-space mappings.
    // Do this before a core such as Flycast reserves its 4 GB fast-memory
    // region and installs its fault handler.
    ensure_sdl(SDL_INIT_VIDEO);
    std::fprintf(stderr, "RetroRun SDL video preinitialized: driver=%s\n",
                 SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");
}

static Uint32 sdl_pixel_format(uint32_t format) {
    switch (format) {
    case RR_PIXEL_FORMAT_RGB565: return SDL_PIXELFORMAT_RGB565;
    case RR_PIXEL_FORMAT_RGB888: return SDL_PIXELFORMAT_RGB24;
    case RR_PIXEL_FORMAT_XRGB8888: return SDL_PIXELFORMAT_ARGB8888;
    case RR_PIXEL_FORMAT_RGBA8888: return SDL_PIXELFORMAT_ABGR8888;
    case RR_PIXEL_FORMAT_RGBA5551: return SDL_PIXELFORMAT_ARGB1555;
    default: return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static SDL_Surface* wrap_surface(rr_surface_t* surface) {
    return SDL_CreateRGBSurfaceWithFormatFrom(surface->pixels.data(), surface->width,
                                               surface->height,
                                               rr_pixel_format_bpp(surface->format),
                                               surface->stride,
                                               sdl_pixel_format(surface->format));
}

rr_input_t* rr_input_create(const char*) {
    ensure_sdl(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);
    SDL_StartTextInput();
    load_controller_mappings();
    rr_input_t* input = new rr_input_t();
    input->controller = open_first_controller();
    if (!input->controller)
        std::fprintf(stderr, "RetroRun SDL controller: no compatible gamepad opened\n");
    active_controller = input->controller;
    return input;
}

void rr_input_destroy(rr_input_t* input) {
    if (!input) return;
    if (input->controller) SDL_GameControllerClose(input->controller);
    if (active_controller == input->controller) active_controller = NULL;
    delete input;
}

rr_input_feature_flags_t rr_input_features_get(rr_input_t*) {
    return static_cast<rr_input_feature_flags_t>(RRInputFeatureFlags_Triggers |
                                                  RRInputFeatureFlags_RightAnalog);
}

rr_input_state_t* rr_input_state_create() { return new rr_input_state_t(); }
void rr_input_state_destroy(rr_input_state_t* state) { delete state; }

static void set_key(rr_input_state_t* state, const Uint8* keys, SDL_Scancode key,
                    rr_input_button_t button) {
    if (keys[key]) state->buttons[button] = RRButtonState_Pressed;
}

static float normalized_axis(Sint16 value) {
    return value < 0 ? value / 32768.0f : value / 32767.0f;
}

// Forward declaration — defined at end of file
static void rr_sdl_dispatch_key_event(const SDL_KeyboardEvent* ev);
static void rr_sdl_dispatch_text_event(const SDL_TextInputEvent* ev);

void rr_input_state_read(rr_input_t* input, rr_input_state_t* state) {
    std::memset(state, 0, sizeof(*state));
    bool controller_change = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT)
            state->buttons[RRInputButton_Quit] = RRButtonState_Pressed;
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
            rr_sdl_dispatch_key_event(&event.key);
        else if (event.type == SDL_TEXTINPUT)
            rr_sdl_dispatch_text_event(&event.text);
        else if (event.type == SDL_CONTROLLERDEVICEADDED) {
            std::fprintf(stderr, "RetroRun SDL controller added: device index=%d\n",
                         event.cdevice.which);
            controller_change = true;
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && input->controller &&
                   SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(input->controller)) == event.cdevice.which) {
            std::fprintf(stderr, "RetroRun SDL controller removed: instance=%d\n",
                         event.cdevice.which);
            SDL_GameControllerClose(input->controller);
            input->controller = NULL;
            active_controller = NULL;
            controller_change = true;
        }
    }

    // KMSDRM can report a controller refresh as ADDED followed by REMOVED in
    // the same event batch. Opening immediately on ADDED loses the new device
    // when REMOVED is processed afterwards, so reconnect only after draining
    // the complete queue. Also recover if SDL invalidated the old handle
    // without delivering the expected event order.
    if (input->controller && SDL_GameControllerGetAttached(input->controller) == SDL_FALSE) {
        std::fprintf(stderr, "RetroRun SDL controller detached; reopening input device\n");
        SDL_GameControllerClose(input->controller);
        input->controller = NULL;
        active_controller = NULL;
        controller_change = true;
    }
    if (!input->controller && controller_change) {
        input->controller = open_first_controller();
        active_controller = input->controller;
        if (!input->controller)
            std::fprintf(stderr, "RetroRun SDL controller reconnect failed: no compatible gamepad\n");
    }

    const Uint8* keys = SDL_GetKeyboardState(NULL);
    set_key(state, keys, SDL_SCANCODE_UP, RRInputButton_DPadUp);
    set_key(state, keys, SDL_SCANCODE_DOWN, RRInputButton_DPadDown);
    set_key(state, keys, SDL_SCANCODE_LEFT, RRInputButton_DPadLeft);
    set_key(state, keys, SDL_SCANCODE_RIGHT, RRInputButton_DPadRight);
    set_key(state, keys, SDL_SCANCODE_X, RRInputButton_A);
    set_key(state, keys, SDL_SCANCODE_Z, RRInputButton_B);
    set_key(state, keys, SDL_SCANCODE_S, RRInputButton_X);
    set_key(state, keys, SDL_SCANCODE_A, RRInputButton_Y);
    set_key(state, keys, SDL_SCANCODE_BACKSPACE, RRInputButton_SELECT);
    set_key(state, keys, SDL_SCANCODE_RETURN, RRInputButton_START);
    set_key(state, keys, SDL_SCANCODE_Q, RRInputButton_TopLeft);
    set_key(state, keys, SDL_SCANCODE_W, RRInputButton_TopRight);
    set_key(state, keys, SDL_SCANCODE_1, RRInputButton_TriggerLeft);
    set_key(state, keys, SDL_SCANCODE_2, RRInputButton_TriggerRight);
    set_key(state, keys, SDL_SCANCODE_3, RRInputButton_THUMBL);
    set_key(state, keys, SDL_SCANCODE_4, RRInputButton_THUMBR);
    set_key(state, keys, SDL_SCANCODE_ESCAPE, RRInputButton_Quit);

    SDL_GameController* pad = input->controller;
    if (!pad) return;
#define PAD_BUTTON(sdl, rr) if (SDL_GameControllerGetButton(pad, sdl)) state->buttons[rr] = RRButtonState_Pressed
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_UP, RRInputButton_DPadUp);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_DOWN, RRInputButton_DPadDown);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_LEFT, RRInputButton_DPadLeft);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, RRInputButton_DPadRight);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_A, RRInputButton_B);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_B, RRInputButton_A);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_X, RRInputButton_Y);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_Y, RRInputButton_X);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_BACK, RRInputButton_SELECT);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_START, RRInputButton_START);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, RRInputButton_TopLeft);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, RRInputButton_TopRight);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_LEFTSTICK, RRInputButton_THUMBL);
    PAD_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSTICK, RRInputButton_THUMBR);
#undef PAD_BUTTON
    if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000)
        state->buttons[RRInputButton_TriggerLeft] = RRButtonState_Pressed;
    if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000)
        state->buttons[RRInputButton_TriggerRight] = RRButtonState_Pressed;
    state->sticks[RRInputThumbstick_Left] = {
        normalized_axis(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX)),
        normalized_axis(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY))};
    state->sticks[RRInputThumbstick_Right] = {
        normalized_axis(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX)),
        normalized_axis(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY))};
}

rr_button_state_t rr_input_state_button_get(rr_input_state_t* state, rr_input_button_t button) {
    return state && button >= 0 && button <= RRInputButton_Quit ? state->buttons[button] : RRButtonState_Released;
}
void rr_input_state_button_set(rr_input_state_t* state, rr_input_button_t button, rr_button_state_t value) {
    if (state && button >= 0 && button <= RRInputButton_Quit) state->buttons[button] = value;
}
rr_thumb_t rr_input_state_thumbstick_get(rr_input_state_t* state, rr_input_thumbstick_t stick) {
    return state ? state->sticks[stick] : rr_thumb_t{0.0f, 0.0f};
}
void rr_input_state_thumbstick_set_null(rr_input_state_t* state, rr_input_thumbstick_t stick) {
    if (state) state->sticks[stick] = {0.0f, 0.0f};
}
void rr_input_battery_read(rr_input_t*, rr_battery_state_t* state) { state->level = 100; state->status = RRBattery_Status_Unknown; }
void rr_input_brightness_read(rr_input_t*, rr_brightness_state_t* state) { state->level = 100; }
void rr_input_brightness_write(int) {}
bool rr_input_set_rumble(uint16_t low, uint16_t high, uint32_t duration) {
    return active_controller && SDL_GameControllerRumble(active_controller, low, high, duration) == 0;
}

rr_audio_t* rr_audio_create(int frequency) {
    ensure_sdl(SDL_INIT_AUDIO);
    SDL_AudioSpec wanted = {};
    // 1024 is intentionally retained for ALSA/KMSDRM handhelds: a 512-sample
    // device period is too easy to underrun when a demanding core has a long
    // frame. Queue latency is controlled independently below.
    wanted.freq = frequency; wanted.format = AUDIO_S16SYS; wanted.channels = 2; wanted.samples = 1024;
    rr_audio_t* audio = new rr_audio_t();
    audio->device = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
    audio->volume = 100; audio->frequency = frequency;
    if (audio->device) SDL_PauseAudioDevice(audio->device, 0);
    return audio;
}
void rr_audio_destroy(rr_audio_t* audio) { if (audio) { if (audio->device) SDL_CloseAudioDevice(audio->device); delete audio; } }
void rr_audio_submit(rr_audio_t* audio, const short* data, int frames) {
    if (!audio || !audio->device || frames <= 0) return;
    const size_t samples = static_cast<size_t>(frames) * 2;

    // SDL_QueueAudio has no intrinsic size limit. If emulation gets even
    // slightly ahead of the audio device, an ever-growing queue becomes
    // seconds of audible latency. Use the audio clock as a small secondary
    // pacing source and retain only a short queue.
    const Uint32 bytes_per_ms = static_cast<Uint32>(audio->frequency * 2 * sizeof(short) / 1000);
    // Keep enough queued audio to survive occasional long Flycast frames on
    // RK3326. This is also the effective clock during boot screens where some
    // cores do not yet expose stable video timing.
    const Uint32 target_queue = bytes_per_ms * 80;
    const Uint32 recovery_limit = bytes_per_ms * 250;
    Uint32 queued = SDL_GetQueuedAudioSize(audio->device);
    if (queued > recovery_limit) {
        SDL_ClearQueuedAudio(audio->device);
    } else {
        // Never discard a complete audio block: that creates an audible
        // discontinuity. A short adaptive wait keeps audio/video clocks
        // together and also prevents old Flycast cores from racing through
        // startup screens. Cap it below a typical scheduler time slice.
        for (int attempt = 0; queued > target_queue && attempt < 3; ++attempt) {
            const Uint32 excess_ms =
                (queued - target_queue) / std::max<Uint32>(bytes_per_ms, 1);
            SDL_Delay(std::min<Uint32>(excess_ms + 1, 8));
            queued = SDL_GetQueuedAudioSize(audio->device);
        }
    }

    if (audio->volume >= 100) {
        SDL_QueueAudio(audio->device, data, static_cast<Uint32>(samples * sizeof(short)));
    } else {
        audio->mix_buffer.resize(samples);
        for (size_t i = 0; i < samples; ++i)
            audio->mix_buffer[i] = static_cast<short>((data[i] * audio->volume) / 100);
        SDL_QueueAudio(audio->device, audio->mix_buffer.data(),
                       static_cast<Uint32>(audio->mix_buffer.size() * sizeof(short)));
    }
}
uint32_t rr_audio_volume_get(rr_audio_t* audio, const char*) { return audio ? audio->volume : 0; }
void rr_audio_volume_set(rr_audio_t* audio, uint32_t value, const char*) { if (audio) audio->volume = std::min<uint32_t>(value, 100); }

rr_display_t* rr_display_create() {
    ensure_sdl(SDL_INIT_VIDEO);
    rr_display_t* display = new rr_display_t();
    display->width = env_dimension("RETRORUN_WINDOW_WIDTH", 960);
    display->height = env_dimension("RETRORUN_WINDOW_HEIGHT", 720);
    display->brightness = 100;
#ifdef RR_SDL_GLES
    Uint32 window_flags = SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL;
    const char* windowed = std::getenv("RETRORUN_WINDOWED");
    if (windowed && std::atoi(windowed) != 0)
        window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
    SDL_DisplayMode mode = {};
    if (!(window_flags & SDL_WINDOW_RESIZABLE) && SDL_GetCurrentDisplayMode(0, &mode) == 0) {
        display->width = mode.w;
        display->height = mode.h;
    }
#else
    const Uint32 window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                                SDL_WINDOW_OPENGL;
#endif
    display->window = SDL_CreateWindow("RetroRun SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       display->width, display->height,
                                       window_flags);
    if (!display->window) {
        std::fprintf(stderr, "RetroRun SDL window creation failed: %s\n", SDL_GetError());
    } else {
        // Cocoa may map a newly created window asynchronously. Explicitly
        // expose it before the loading-screen timer starts on first present.
        SDL_ShowWindow(display->window);
        SDL_RaiseWindow(display->window);
        SDL_PumpEvents();
    }
    return display;
}
void rr_display_destroy(rr_display_t* display) { if (display) { if (display->window) SDL_DestroyWindow(display->window); delete display; } }
int rr_display_width_get(rr_display_t* display) { refresh_display_size(display); return display->width; }
int rr_display_height_get(rr_display_t* display) { refresh_display_size(display); return display->height; }
uint32_t rr_display_backlight_get(rr_display_t* display) { return display->brightness; }
void rr_display_backlight_set(rr_display_t* display, uint32_t value) { display->brightness = std::min<uint32_t>(value, 100); }
int rr_pixel_format_bpp(uint32_t format) {
    switch (format) { case RR_PIXEL_FORMAT_RGB565: case RR_PIXEL_FORMAT_RGBA5551: return 16;
                      case RR_PIXEL_FORMAT_RGB888: return 24; default: return 32; }
}

rr_surface_t* rr_surface_create(rr_display_t* display, int width, int height, uint32_t format) {
    if (width <= 0 || height <= 0 || sdl_pixel_format(format) == SDL_PIXELFORMAT_UNKNOWN) return NULL;
    rr_surface_t* surface = new rr_surface_t();
    surface->display = display; surface->width = width; surface->height = height; surface->format = format;
    surface->generation = next_surface_generation++;
    surface->stride = width * (rr_pixel_format_bpp(format) / 8);
    surface->pixels.resize(static_cast<size_t>(surface->stride) * height);
    return surface;
}
void rr_surface_destroy(rr_surface_t* surface) { delete surface; }
int rr_surface_width_get(rr_surface_t* surface) { return surface->width; }
int rr_surface_height_get(rr_surface_t* surface) { return surface->height; }
uint32_t rr_surface_format_get(rr_surface_t* surface) { return surface->format; }
int rr_surface_stride_get(rr_surface_t* surface) { return surface->stride; }
void* rr_surface_map(rr_surface_t* surface) {
    if (!surface) return NULL;
    surface->generation = next_surface_generation++;
    return surface->pixels.data();
}
void rr_surface_unmap(rr_surface_t*) {}
void rr_surface_blit(rr_surface_t* source, int sx, int sy, int sw, int sh,
                     rr_surface_t* dest, int dx, int dy, int dw, int dh, rr_rotation_t) {
    SDL_Surface* src = wrap_surface(source); SDL_Surface* dst = wrap_surface(dest);
    if (src && dst) {
        SDL_Rect sr = {sx,sy,sw,sh}; SDL_Rect dr = {dx,dy,dw,dh};
        SDL_BlitScaled(src,&sr,dst,&dr);
        dest->generation = next_surface_generation++;
    }
    if (src) SDL_FreeSurface(src);
    if (dst) SDL_FreeSurface(dst);
}
int rr_surface_save_as_png(rr_surface_t* surface, const char* filename) {
    SDL_Surface* wrapped = wrap_surface(surface);
    SDL_Surface* rgba = wrapped ? SDL_ConvertSurfaceFormat(wrapped, SDL_PIXELFORMAT_RGBA32, 0) : NULL;
    if (wrapped) SDL_FreeSurface(wrapped);
    if (!rgba) return -1;
    FILE* file = std::fopen(filename, "wb");
    if (!file) { SDL_FreeSurface(rgba); return -1; }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { if (png) png_destroy_write_struct(&png, info ? &info : NULL); std::fclose(file); SDL_FreeSurface(rgba); return -1; }
    png_init_io(png, file);
    png_set_IHDR(png, info, rgba->w, rgba->h, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (int y = 0; y < rgba->h; ++y) png_write_row(png, static_cast<png_bytep>(rgba->pixels) + y * rgba->pitch);
    png_write_end(png, info); png_destroy_write_struct(&png, &info); std::fclose(file); SDL_FreeSurface(rgba); return 0;
}

static void render_surface(rr_presenter_t* presenter, rr_surface_t* surface, const SDL_Rect* src,
                           const SDL_Rect* dst, rr_rotation_t rotation) {
    if (!presenter || !presenter->renderer || !surface) return;
    const Uint32 format = sdl_pixel_format(surface->format);
    auto& entry = presenter->textures[surface];
    if (!entry.texture || entry.width != surface->width ||
        entry.height != surface->height || entry.format != format) {
        if (entry.texture) SDL_DestroyTexture(entry.texture);
        entry.texture = SDL_CreateTexture(presenter->renderer, format,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          surface->width, surface->height);
        entry.width = surface->width;
        entry.height = surface->height;
        entry.format = format;
        entry.generation = 0;
        if (entry.texture)
            SDL_SetTextureBlendMode(entry.texture,
                                    surface->format == RR_PIXEL_FORMAT_RGBA8888
                                        ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    }
    if (!entry.texture) {
        std::fprintf(stderr, "RetroRun SDL streaming texture creation failed: %s\n", SDL_GetError());
        return;
    }
    if (entry.generation != surface->generation &&
        SDL_UpdateTexture(entry.texture, NULL, surface->pixels.data(), surface->stride) != 0) {
        std::fprintf(stderr, "RetroRun SDL streaming texture upload failed: %s\n", SDL_GetError());
        return;
    }
    entry.generation = surface->generation;
    SDL_RenderCopyEx(presenter->renderer, entry.texture, src, dst,
                     90.0 * static_cast<int>(rotation), NULL, SDL_FLIP_NONE);
}

rr_presenter_t* rr_presenter_create(rr_display_t* display, uint32_t, uint32_t background) {
    rr_presenter_t* presenter = new rr_presenter_t(); presenter->display = display; presenter->background = background;
    presenter->loading_wait_completed = false;
    const Uint32 vsync_flag = vsync_enabled ? SDL_RENDERER_PRESENTVSYNC : 0;
    presenter->renderer = SDL_CreateRenderer(display->window, -1,
                                              SDL_RENDERER_ACCELERATED | vsync_flag);
    if (!presenter->renderer)
        presenter->renderer = SDL_CreateRenderer(display->window, -1,
                                                  SDL_RENDERER_SOFTWARE | vsync_flag);
    if (!presenter->renderer && vsync_enabled)
        presenter->renderer = SDL_CreateRenderer(display->window, -1, SDL_RENDERER_SOFTWARE);
    if (presenter->renderer) {
        SDL_RendererInfo info = {};
        if (SDL_GetRendererInfo(presenter->renderer, &info) == 0 && info.name)
            SDL_strlcpy(renderer_name, info.name, sizeof(renderer_name));
        SDL_RenderSetLogicalSize(presenter->renderer, display->width, display->height);
    }
    active_presenter = presenter;
    return presenter;
}
void rr_presenter_destroy(rr_presenter_t* presenter) {
    if (!presenter) return;
    if (active_presenter == presenter) active_presenter = NULL;
    for (auto& item : presenter->textures)
        if (item.second.texture) SDL_DestroyTexture(item.second.texture);
    presenter->textures.clear();
    if (presenter->renderer) SDL_DestroyRenderer(presenter->renderer);
    delete presenter;
}
static void clear_presenter(rr_presenter_t* p) { SDL_SetRenderDrawColor(p->renderer, 8, 8, 8, 255); SDL_RenderClear(p->renderer); }
void rr_presenter_post(rr_presenter_t* p, rr_surface_t* s, int sx, int sy, int sw, int sh,
                       int dx, int dy, int dw, int dh, rr_rotation_t r) {
    clear_presenter(p); SDL_Rect src={sx,sy,sw,sh}, dst={dx,dy,dw,dh}; render_surface(p,s,&src,&dst,r); SDL_RenderPresent(p->renderer);
}
bool rr_presenter_post_direct(rr_presenter_t*, rr_surface_t*, int, int, int, int,
                              int, int, int, int, rr_rotation_t) { return false; }
void rr_presenter_direct_disable(rr_presenter_t*) {}
void rr_presenter_black(rr_presenter_t* p, int, int, int, int, rr_rotation_t) { clear_presenter(p); SDL_RenderPresent(p->renderer); }
void rr_presenter_wait_for_loading_screen(rr_presenter_t* presenter, unsigned milliseconds) {
    if (!presenter || !presenter->display || !presenter->display->window ||
        presenter->loading_wait_completed)
        return;

    presenter->loading_wait_completed = true;
    SDL_ShowWindow(presenter->display->window);
    SDL_RaiseWindow(presenter->display->window);
    // SDL_GetTicks64 was introduced after SDL 2.0.10, which is the version
    // shipped by Ubuntu 20.04. Unsigned subtraction also handles the 32-bit
    // SDL_GetTicks rollover correctly for this short wait.
    const Uint32 started = SDL_GetTicks();
    while (static_cast<Uint32>(SDL_GetTicks() - started) < milliseconds) {
        // Cocoa needs its event queue serviced before a newly created window's
        // first rendered frame is guaranteed to reach the compositor.
        SDL_PumpEvents();
        SDL_Delay(5);
    }
}
void rr_presenter_post_multiple(rr_presenter_t* p, rr_surface_t* base, status* o,
                                int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                                rr_rotation_t r, rr_rotation_t, bool) {
    clear_presenter(p); SDL_Rect src={sx,sy,sw,sh}, dst={dx,dy,dw,dh}; render_surface(p,base,&src,&dst,r);
    const float scale = std::max(1.0f, p->display->width / 640.0f);
    auto overlay = [&](rr_surface_t* s, int x, int y, int w, int h) { SDL_Rect d={x,y,w,h}; render_surface(p,s,NULL,&d,RR_ROTATION_DEGREES_0); };
    if (o->show_full && o->full) overlay(o->full, dx, dy, dw, dh);
    if (o->show_top_left && o->top_left) overlay(o->top_left, 0, 0, o->top_left->width*scale, o->top_left->height*scale);
    if (o->show_top_right && o->top_right) overlay(o->top_right, p->display->width-o->top_right->width*scale, 0, o->top_right->width*scale, o->top_right->height*scale);
    if (o->show_bottom_left && o->bottom_left) overlay(o->bottom_left, 0, p->display->height-o->bottom_left->height*scale, o->bottom_left->width*scale, o->bottom_left->height*scale);
    if (o->show_bottom_right && o->bottom_right) overlay(o->bottom_right, p->display->width-o->bottom_right->width*scale, p->display->height-o->bottom_right->height*scale, o->bottom_right->width*scale, o->bottom_right->height*scale);
    if (o->show_bottom_center && o->bottom_center) overlay(o->bottom_center, (p->display->width-o->bottom_center->width*scale)/2, p->display->height-o->bottom_center->height*scale, o->bottom_center->width*scale, o->bottom_center->height*scale);
    SDL_RenderPresent(p->renderer);
}

static GLuint compile_post_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;

    char log[1024] = {};
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    std::fprintf(stderr, "RetroRun SDL post-processing shader failed: %s\n", log);
    glDeleteShader(shader);
    return 0;
}

static bool ensure_post_pipeline(rr_context_t* context) {
    if (context->post_program) return true;
    if (context->post_pipeline_failed) return false;
#ifdef RR_SDL_GLES
    static const char* vertex_source = R"GLSL(#version 300 es
precision mediump float;
in vec2 position;
in vec2 texcoord;
out vec2 uv;
void main() {
    uv = texcoord;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";
    static const char* fragment_source = R"GLSL(#version 300 es
precision mediump float;
uniform sampler2D frame_texture;
uniform vec2 source_size;
uniform vec2 texture_scale;
uniform int shader_mode;
uniform int rotation_mode;
in vec2 uv;
out vec4 output_color;
void main() {
    vec2 sample_uv = rotation_mode == 1 ? vec2(uv.y, 1.0 - uv.x) :
                     (rotation_mode == 3 ? vec2(1.0 - uv.y, uv.x) :
                     (rotation_mode == 2 ? vec2(1.0 - uv.x, 1.0 - uv.y) : uv));
    vec2 centered = sample_uv * 2.0 - 1.0;
    if (shader_mode == 2) {
        // A single dot product gives both the inexpensive barrel distortion
        // and vignette term. Keep this mediump-friendly for Mali-G31.
        float radius2 = dot(centered, centered);
        centered *= 1.0 + radius2 * 0.05;
        sample_uv = centered * 0.5 + 0.5;
        if (max(abs(centered.x), abs(centered.y)) > 1.0) {
            output_color = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    vec3 color = texture(frame_texture, sample_uv * texture_scale).rgb;
    if (shader_mode > 0) {
        // gl_FragCoord already advances by one for every physical output row.
        // This replaces the previous per-fragment sine with one cheap step.
        color *= mix(0.70, 1.0, step(1.0, mod(gl_FragCoord.y, 2.0)));
    }
    if (shader_mode == 2) {
        float vignette = max(1.0 - 0.22 * dot(centered, centered), 0.58);
        float mask = mod(gl_FragCoord.x, 3.0);
        vec3 grille = vec3(0.93);
        grille.r += 0.07 * (1.0 - step(1.0, mask));
        grille.g += 0.07 * step(1.0, mask) * (1.0 - step(2.0, mask));
        grille.b += 0.07 * step(2.0, mask);
        color *= vignette * grille;
    }
    output_color = vec4(color, 1.0);
}
)GLSL";
#else
    static const char* vertex_source = R"GLSL(#version 150
in vec2 position;
in vec2 texcoord;
out vec2 uv;
void main() {
    uv = texcoord;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";
    static const char* fragment_source = R"GLSL(#version 150
uniform sampler2D frame_texture;
uniform vec2 source_size;
uniform vec2 texture_scale;
uniform int shader_mode;
uniform int rotation_mode;
in vec2 uv;
out vec4 output_color;
void main() {
    vec2 sample_uv = rotation_mode == 1 ? vec2(uv.y, 1.0 - uv.x) :
                     (rotation_mode == 3 ? vec2(1.0 - uv.y, uv.x) :
                     (rotation_mode == 2 ? vec2(1.0 - uv.x, 1.0 - uv.y) : uv));
    vec2 centered = sample_uv * 2.0 - 1.0;
    if (shader_mode == 2) {
        float radius2 = dot(centered, centered);
        centered *= 1.0 + radius2 * 0.055;
        sample_uv = centered * 0.5 + 0.5;
        if (any(lessThan(sample_uv, vec2(0.0))) ||
            any(greaterThan(sample_uv, vec2(1.0)))) {
            output_color = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    vec3 color = texture(frame_texture, sample_uv * texture_scale).rgb;
    if (shader_mode > 0) {
        float scanline = 0.82 + 0.18 * sin(sample_uv.y * source_size.y * 3.14159265);
        color *= scanline;
    }

    if (shader_mode == 2) {
        float vignette = 1.0 - 0.24 * dot(centered, centered);
        float mask = mod(floor(gl_FragCoord.x), 3.0);
        vec3 grille = mask < 1.0 ? vec3(1.00, 0.92, 0.92) :
                      (mask < 2.0 ? vec3(0.92, 1.00, 0.92) : vec3(0.92, 0.92, 1.00));
        color *= max(vignette, 0.55) * grille;
    }
    output_color = vec4(color, 1.0);
}
)GLSL";
#endif

    GLuint vertex = compile_post_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment = compile_post_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        context->post_pipeline_failed = true;
        return false;
    }

    context->post_program = glCreateProgram();
    glAttachShader(context->post_program, vertex);
    glAttachShader(context->post_program, fragment);
    glBindAttribLocation(context->post_program, 0, "position");
    glBindAttribLocation(context->post_program, 1, "texcoord");
#ifndef RR_SDL_GLES
    glBindFragDataLocation(context->post_program, 0, "output_color");
#endif
    glLinkProgram(context->post_program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(context->post_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024] = {};
        glGetProgramInfoLog(context->post_program, sizeof(log), NULL, log);
        std::fprintf(stderr, "RetroRun SDL post-processing program failed: %s\n", log);
        glDeleteProgram(context->post_program);
        context->post_program = 0;
        context->post_pipeline_failed = true;
        return false;
    }

    static const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };
    glGenVertexArrays(1, &context->post_vao);
    glBindVertexArray(context->post_vao);
    glGenBuffers(1, &context->post_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, context->post_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          (void*)(2 * sizeof(GLfloat)));
    glBindVertexArray(0);
    context->uniform_frame_texture = glGetUniformLocation(context->post_program, "frame_texture");
    context->uniform_source_size = glGetUniformLocation(context->post_program, "source_size");
    context->uniform_texture_scale = glGetUniformLocation(context->post_program, "texture_scale");
    context->uniform_shader_mode = glGetUniformLocation(context->post_program, "shader_mode");
    context->uniform_rotation = glGetUniformLocation(context->post_program, "rotation_mode");
    return true;
}

static bool draw_post_processed_frame(rr_context_t* context, int source_width, int source_height,
                                      int left, int bottom, int right, int top,
                                      rr_rotation_t rotation) {
    if ((video_shader == RR_VIDEO_SHADER_OFF && rotation == RR_ROTATION_DEGREES_0) ||
        !ensure_post_pipeline(context)) return false;

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(left, bottom, right - left, top - bottom);
    glUseProgram(context->post_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, context->color_texture);
    const GLint filtering = video_filter == RR_VIDEO_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR;
    if (context->post_texture_filter != filtering) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
        context->post_texture_filter = filtering;
    }
    glUniform1i(context->uniform_frame_texture, 0);
    glUniform2f(context->uniform_source_size,
                static_cast<float>(source_width), static_cast<float>(source_height));
    glUniform2f(context->uniform_texture_scale,
                static_cast<float>(source_width) / context->framebuffer_width,
                static_cast<float>(source_height) / context->framebuffer_height);
    glUniform1i(context->uniform_shader_mode,
                video_shader == RR_VIDEO_SHADER_CRT ? 2 : 1);
    if (video_shader == RR_VIDEO_SHADER_OFF)
        glUniform1i(context->uniform_shader_mode, 0);
    glUniform1i(context->uniform_rotation, static_cast<int>(rotation));
    glBindVertexArray(context->post_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    return true;
}

rr_context_t* rr_context_create(rr_display_t* display, int width, int height,
                                const rr_context_attributes_t* attributes) {
    if (!display || width <= 0 || height <= 0 || !attributes) return NULL;

#ifdef RR_SDL_GLES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, std::max(attributes->major, 3));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,
                        attributes->major >= 3 ? attributes->minor : 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, attributes->major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, attributes->minor);
#endif
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, attributes->red_bits);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, attributes->green_bits);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, attributes->blue_bits);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, attributes->alpha_bits);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, attributes->depth_bits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, attributes->stencil_bits);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    rr_context_t* context = new rr_context_t();
    context->display = display;
    context->width = display->width;
    context->height = display->height;
    context->framebuffer_width = width;
    context->framebuffer_height = height;
    context->window = display->window;
    context->owns_window = false;
    context->framebuffer = 0;
    context->color_texture = 0;
    context->depth_stencil = 0;
    context->post_program = 0;
    context->post_vao = 0;
    context->post_vbo = 0;
    context->uniform_frame_texture = -1;
    context->uniform_source_size = -1;
    context->uniform_texture_scale = -1;
    context->uniform_shader_mode = -1;
    context->uniform_rotation = -1;
    context->overlay_texture = 0;
    context->overlay_texture_width = 0;
    context->overlay_texture_height = 0;
    context->overlay_texture_format = 0;
    context->overlay_uploaded_surface = NULL;
    context->overlay_uploaded_generation = 0;
    context->post_texture_filter = -1;
    context->post_pipeline_failed = false;
    const char* default_framebuffer_env = std::getenv("RETRORUN_SDL_DEFAULT_FRAMEBUFFER");
    context->default_framebuffer = default_framebuffer_env &&
                                   std::atoi(default_framebuffer_env) != 0;
    context->gl = SDL_GL_CreateContext(context->window);
    if (!context->window || !context->gl) {
        std::fprintf(stderr, "RetroRun SDL OpenGL context creation failed: %s\n", SDL_GetError());
        delete context;
        return NULL;
    }
    SDL_GL_MakeCurrent(context->window, context->gl);
#ifdef RR_SDL_GLES
    SDL_strlcpy(renderer_name, "OpenGL ES 3", sizeof(renderer_name));
#else
    SDL_strlcpy(renderer_name, "OpenGL Core", sizeof(renderer_name));
#endif
    if (SDL_GL_SetSwapInterval(vsync_enabled ? 1 : 0) != 0 && vsync_enabled)
        std::fprintf(stderr, "RetroRun SDL could not enable OpenGL VSync: %s\n", SDL_GetError());
    active_context = context;

    const GLubyte* gl_version = glGetString(GL_VERSION);
    const GLubyte* gl_renderer = glGetString(GL_RENDERER);
    std::fprintf(stderr,
                 "RetroRun SDL video: driver=%s, GL=%s, renderer=%s, framebuffer=%s\n",
                 SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown",
                 gl_version ? reinterpret_cast<const char*>(gl_version) : "unknown",
                 gl_renderer ? reinterpret_cast<const char*>(gl_renderer) : "unknown",
                 context->default_framebuffer ? "default (0)" : "frontend FBO");

    // Compatibility path for older hardware-rendered cores which assume the
    // default framebuffer used by the historical GO2 backend. It is opt-in:
    // the frontend FBO remains the normal path. Keep a GPU-side capture
    // texture even in compatibility mode so post-processing can copy the
    // completed default framebuffer without a slow CPU readback.
    if (context->default_framebuffer) {
        glGenTextures(1, &context->color_texture);
        glBindTexture(GL_TEXTURE_2D, context->color_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef RR_SDL_GLES
        // The RG351MP display and Flycast output do not benefit from an
        // 8-bit alpha channel here. RGB565 halves capture-texture bandwidth.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, width, height, 0, GL_RGB,
                     GL_UNSIGNED_SHORT_5_6_5, NULL);
#else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
#endif
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return context;
    }

    glGenFramebuffers(1, &context->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, context->framebuffer);
    glGenTextures(1, &context->color_texture);
    glBindTexture(GL_TEXTURE_2D, context->color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           context->color_texture, 0);

    glGenRenderbuffers(1, &context->depth_stencil);
    glBindRenderbuffer(GL_RENDERBUFFER, context->depth_stencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, context->depth_stencil);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "RetroRun SDL OpenGL framebuffer creation failed\n");
        rr_context_destroy(context);
        return NULL;
    }
    return context;
}
void rr_context_destroy(rr_context_t* context) {
    if (!context) return;
    if (active_context == context) active_context = NULL;
    if (context->gl) SDL_GL_MakeCurrent(context->window, context->gl);
    if (context->post_vbo) glDeleteBuffers(1, &context->post_vbo);
    if (context->post_vao) glDeleteVertexArrays(1, &context->post_vao);
    if (context->post_program) glDeleteProgram(context->post_program);
    if (context->overlay_texture) glDeleteTextures(1, &context->overlay_texture);
    if (context->depth_stencil) glDeleteRenderbuffers(1, &context->depth_stencil);
    if (context->color_texture) glDeleteTextures(1, &context->color_texture);
    if (context->framebuffer) glDeleteFramebuffers(1, &context->framebuffer);
    if (context->gl) SDL_GL_DeleteContext(context->gl);
    if (context->owns_window && context->window) SDL_DestroyWindow(context->window);
    delete context;
}
void rr_context_make_current(rr_context_t* context) {
    if (context && SDL_GL_GetCurrentContext() != context->gl)
        SDL_GL_MakeCurrent(context->window, context->gl);
}

static void blit_overlay(rr_context_t* context, rr_surface_t* surface,
                         int x, int y, int width, int height,
                         int drawable_width, int drawable_height,
                         rr_rotation_t rotation) {
    if (!surface || width <= 0 || height <= 0) return;

    GLint previous_draw = 0;
    GLint previous_program = 0;
    GLint previous_vao = 0;
    GLint previous_array_buffer = 0;
    GLint previous_active_texture = 0;
    GLint previous_texture = 0;
    GLint previous_unpack_alignment = 0;
    GLint previous_viewport[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array_buffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);

    if (!ensure_post_pipeline(context)) {
        glActiveTexture(static_cast<GLenum>(previous_active_texture));
        return;
    }

    const bool rgb565 = surface->format == RR_PIXEL_FORMAT_RGB565;
    const GLenum internal_format = rgb565 ? GL_RGB565 : GL_RGBA8;
    const GLenum source_format = rgb565 ? GL_RGB : GL_RGBA;
    const GLenum source_type = rgb565 ? GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE;

    if (!context->overlay_texture) {
        glGenTextures(1, &context->overlay_texture);
        glBindTexture(GL_TEXTURE_2D, context->overlay_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, context->overlay_texture);
    }

    const bool texture_changed =
        context->overlay_uploaded_surface != surface ||
        context->overlay_uploaded_generation != surface->generation ||
        context->overlay_texture_width != surface->width ||
        context->overlay_texture_height != surface->height ||
        context->overlay_texture_format != internal_format;
    if (texture_changed) {
        // CPU surfaces are top-down while OpenGL textures are sampled bottom-up.
        // Only rebuild this copy when the overlay contents actually change.
        context->overlay_upload.resize(surface->pixels.size());
        for (int row = 0; row < surface->height; ++row) {
            std::memcpy(context->overlay_upload.data() +
                            static_cast<size_t>(row) * surface->stride,
                        surface->pixels.data() +
                            static_cast<size_t>(surface->height - 1 - row) * surface->stride,
                        surface->stride);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (context->overlay_texture_width != surface->width ||
            context->overlay_texture_height != surface->height ||
            context->overlay_texture_format != internal_format) {
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                         surface->width, surface->height, 0,
                         source_format, source_type, context->overlay_upload.data());
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                            surface->width, surface->height,
                            source_format, source_type, context->overlay_upload.data());
        }
        context->overlay_texture_width = surface->width;
        context->overlay_texture_height = surface->height;
        context->overlay_texture_format = internal_format;
        context->overlay_uploaded_surface = surface;
        context->overlay_uploaded_generation = surface->generation;
    }
    const float scale_x = static_cast<float>(drawable_width) / context->display->width;
    const float scale_y = static_cast<float>(drawable_height) / context->display->height;
    const int left = static_cast<int>(x * scale_x);
    const int right = static_cast<int>((x + width) * scale_x);
    const int bottom = drawable_height - static_cast<int>((y + height) * scale_y);
    const int top = drawable_height - static_cast<int>(y * scale_y);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(left, bottom, right - left, top - bottom);
    glUseProgram(context->post_program);
    glUniform1i(context->uniform_frame_texture, 0);
    glUniform2f(context->uniform_source_size,
                static_cast<float>(surface->width), static_cast<float>(surface->height));
    glUniform2f(context->uniform_texture_scale, 1.0f, 1.0f);
    glUniform1i(context->uniform_shader_mode, 0);
    glUniform1i(context->uniform_rotation, static_cast<int>(rotation));
    glBindVertexArray(context->post_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindVertexArray(static_cast<GLuint>(previous_vao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array_buffer));
    glUseProgram(static_cast<GLuint>(previous_program));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
    glActiveTexture(static_cast<GLenum>(previous_active_texture));
    glViewport(previous_viewport[0], previous_viewport[1],
               previous_viewport[2], previous_viewport[3]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previous_draw));
}

static void draw_overlays(rr_context_t* context, status* overlays,
                          int game_x, int game_y, int game_width, int game_height,
                          int drawable_width, int drawable_height,
                          rr_rotation_t rotation) {
    if (!overlays) return;
    const float scale = std::max(1.0f, context->display->width / 640.0f);
    auto draw = [&](rr_surface_t* surface, int x, int y, int width, int height) {
        blit_overlay(context, surface, x, y, width, height,
                     drawable_width, drawable_height, rotation);
    };
    if (overlays->show_full && overlays->full)
        draw(overlays->full, game_x, game_y, game_width, game_height);
    if (overlays->show_top_left && overlays->top_left)
        draw(overlays->top_left, 0, 0, overlays->top_left->width * scale,
             overlays->top_left->height * scale);
    if (overlays->show_top_right && overlays->top_right)
        draw(overlays->top_right, context->display->width - overlays->top_right->width * scale,
             0, overlays->top_right->width * scale, overlays->top_right->height * scale);
    if (overlays->show_bottom_left && overlays->bottom_left)
        draw(overlays->bottom_left, 0,
             context->display->height - overlays->bottom_left->height * scale,
             overlays->bottom_left->width * scale, overlays->bottom_left->height * scale);
    if (overlays->show_bottom_right && overlays->bottom_right)
        draw(overlays->bottom_right,
             context->display->width - overlays->bottom_right->width * scale,
             context->display->height - overlays->bottom_right->height * scale,
             overlays->bottom_right->width * scale, overlays->bottom_right->height * scale);
    if (overlays->show_bottom_center && overlays->bottom_center)
        draw(overlays->bottom_center,
             (context->display->width - overlays->bottom_center->width * scale) / 2,
             context->display->height - overlays->bottom_center->height * scale,
             overlays->bottom_center->width * scale, overlays->bottom_center->height * scale);
}

void rr_context_swap_buffers(rr_context_t* context, int source_width, int source_height,
                             int dest_x, int dest_y, int dest_width, int dest_height,
                             status* overlays, rr_rotation_t rotation) {
    if (!context) return;
    rr_context_make_current(context);

    if (context->default_framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int drawable_width = context->display->width;
        int drawable_height = context->display->height;
        SDL_GL_GetDrawableSize(context->window, &drawable_width, &drawable_height);

        if ((video_shader != RR_VIDEO_SHADER_OFF || rotation != RR_ROTATION_DEGREES_0) &&
            context->color_texture &&
            ensure_post_pipeline(context) &&
            source_width > 0 && source_height > 0 &&
            source_width <= context->framebuffer_width &&
            source_height <= context->framebuffer_height) {
            const float scale_x = static_cast<float>(drawable_width) /
                                  context->display->width;
            const float scale_y = static_cast<float>(drawable_height) /
                                  context->display->height;
            const int left = static_cast<int>(dest_x * scale_x);
            const int right = static_cast<int>((dest_x + dest_width) * scale_x);
            const int bottom = drawable_height -
                               static_cast<int>((dest_y + dest_height) * scale_y);
            const int top = drawable_height - static_cast<int>(dest_y * scale_y);

            // Flycast has completed rendering into framebuffer 0. Snapshot it
            // entirely on the GPU, then render the texture back through the
            // same Scanlines/CRT program used by the frontend-FBO path.
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, context->color_texture);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                                source_width, source_height);
            glViewport(0, 0, drawable_width, drawable_height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            draw_post_processed_frame(context, source_width, source_height,
                                      left, bottom, right, top, rotation);
        }
        draw_overlays(context, overlays, dest_x, dest_y, dest_width, dest_height,
                      drawable_width, drawable_height, rotation);
        SDL_GL_SwapWindow(context->window);
        return;
    }

    int drawable_width = context->display->width;
    int drawable_height = context->display->height;
    SDL_GL_GetDrawableSize(context->window, &drawable_width, &drawable_height);
    const float scale_x = static_cast<float>(drawable_width) / context->display->width;
    const float scale_y = static_cast<float>(drawable_height) / context->display->height;
    const int left = static_cast<int>(dest_x * scale_x);
    const int right = static_cast<int>((dest_x + dest_width) * scale_x);
    const int bottom = drawable_height - static_cast<int>((dest_y + dest_height) * scale_y);
    const int top = drawable_height - static_cast<int>(dest_y * scale_y);

    GLint previous_read = 0;
    GLint previous_draw = 0;
    GLint previous_program = 0;
    GLint previous_vao = 0;
    GLint previous_active_texture = 0;
    GLint previous_texture = 0;
    GLint previous_viewport[4] = {};
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, drawable_width, drawable_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, context->framebuffer);
    const GLenum filtering = video_filter == RR_VIDEO_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR;
    if (!draw_post_processed_frame(context, source_width, source_height,
                                   left, bottom, right, top, rotation)) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, context->framebuffer);
        glBlitFramebuffer(0, 0, source_width, source_height,
                          left, bottom, right, top, GL_COLOR_BUFFER_BIT, filtering);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    draw_overlays(context, overlays, dest_x, dest_y, dest_width, dest_height,
                  drawable_width, drawable_height, rotation);
    SDL_GL_SwapWindow(context->window);
    glUseProgram(static_cast<GLuint>(previous_program));
    glBindVertexArray(static_cast<GLuint>(previous_vao));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    glActiveTexture(static_cast<GLenum>(previous_active_texture));
    glViewport(previous_viewport[0], previous_viewport[1],
               previous_viewport[2], previous_viewport[3]);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous_read));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previous_draw));
}
uintptr_t rr_context_framebuffer_get(rr_context_t* context) {
    return context ? static_cast<uintptr_t>(context->framebuffer) : 0;
}
rr_surface_t* rr_context_surface_lock(rr_context_t* context) {
    if (!context) return NULL;
    rr_context_make_current(context);
    rr_surface_t* surface = new rr_surface_t();
    surface->display = context->display;
    surface->width = context->width;
    surface->height = context->height;
    surface->stride = context->width * 4;
    surface->format = RR_PIXEL_FORMAT_RGBA8888;
    surface->generation = next_surface_generation++;
    surface->pixels.resize(static_cast<size_t>(surface->stride) * surface->height);

    std::vector<uint8_t> bottom_up(surface->pixels.size());
    GLint previous_read_framebuffer = 0;
    GLint previous_read_buffer = 0;
    GLint previous_pack_alignment = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_framebuffer);
    glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack_alignment);
    if (context->default_framebuffer) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, context->framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, context->width, context->height, GL_RGBA, GL_UNSIGNED_BYTE,
                 bottom_up.data());
    glPixelStorei(GL_PACK_ALIGNMENT, previous_pack_alignment);
    glReadBuffer(static_cast<GLenum>(previous_read_buffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      static_cast<GLuint>(previous_read_framebuffer));
    for (int row = 0; row < context->height; ++row) {
        std::memcpy(surface->pixels.data() + static_cast<size_t>(row) * surface->stride,
                    bottom_up.data() + static_cast<size_t>(context->height - 1 - row) * surface->stride,
                    surface->stride);
    }
    return surface;
}
void rr_context_surface_unlock(rr_context_t*, rr_surface_t* surface) { delete surface; }
void* rr_context_get_proc_address(const char* symbol) { return SDL_GL_GetProcAddress(symbol); }
void rr_video_sync() {
    // State loading did not require an explicit GPU synchronization on the
    // SDL backend. Preserve that behaviour while keeping GL calls out of the
    // shared frontend code.
}
bool rr_video_vsync_set(bool enabled) {
    vsync_enabled = enabled;
    bool applied = true;
    if (active_context && active_context->gl) {
        SDL_GL_MakeCurrent(active_context->window, active_context->gl);
        applied = SDL_GL_SetSwapInterval(enabled ? 1 : 0) == 0 && applied;
    }
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (active_presenter && active_presenter->renderer)
        applied = SDL_RenderSetVSync(active_presenter->renderer, enabled ? 1 : 0) == 0 && applied;
#endif
    return applied;
}
bool rr_video_vsync_get() { return vsync_enabled; }
void rr_video_filter_set(rr_video_filter_t filter) { video_filter = filter; }
rr_video_filter_t rr_video_filter_get() { return video_filter; }
void rr_video_shader_set(rr_video_shader_t shader) { video_shader = shader; }
rr_video_shader_t rr_video_shader_get() { return video_shader; }
const char* rr_platform_backend_name() { return "sdl2"; }
const char* rr_platform_renderer_name() { return renderer_name; }

// --- Platform capabilities ---

uint32_t rr_platform_capabilities() {
    return RRPlatformCapability_PhysicalKeyboard;
}

// --- SDL keyboard-to-libretro translation ---

// Scancodes used as frontend gamepad emulation — these are NOT forwarded to
// the core as keyboard events because they represent joypad buttons.
static bool is_frontend_hotkey(SDL_Scancode sc) {
    switch (sc) {
    case SDL_SCANCODE_UP: case SDL_SCANCODE_DOWN:
    case SDL_SCANCODE_LEFT: case SDL_SCANCODE_RIGHT:
    case SDL_SCANCODE_X: case SDL_SCANCODE_Z:
    case SDL_SCANCODE_S: case SDL_SCANCODE_A:
    case SDL_SCANCODE_BACKSPACE: case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_Q: case SDL_SCANCODE_W:
    case SDL_SCANCODE_1: case SDL_SCANCODE_2:
    case SDL_SCANCODE_3: case SDL_SCANCODE_4:
    case SDL_SCANCODE_ESCAPE:
        return true;
    default:
        return false;
    }
}

// Map SDL scancode to libretro RETROK_* keycode.
static unsigned sdl_scancode_to_retrok(SDL_Scancode sc) {
    switch (sc) {
    case SDL_SCANCODE_BACKSPACE:    return 8;   // RETROK_BACKSPACE
    case SDL_SCANCODE_TAB:         return 9;   // RETROK_TAB
    case SDL_SCANCODE_RETURN:      return 13;  // RETROK_RETURN
    case SDL_SCANCODE_ESCAPE:      return 27;  // RETROK_ESCAPE
    case SDL_SCANCODE_SPACE:       return 32;  // RETROK_SPACE
    case SDL_SCANCODE_COMMA:       return 44;  // RETROK_COMMA
    case SDL_SCANCODE_MINUS:       return 45;  // RETROK_MINUS
    case SDL_SCANCODE_PERIOD:      return 46;  // RETROK_PERIOD
    case SDL_SCANCODE_SLASH:       return 47;  // RETROK_SLASH
    case SDL_SCANCODE_0:           return 48;
    case SDL_SCANCODE_1:           return 49;
    case SDL_SCANCODE_2:           return 50;
    case SDL_SCANCODE_3:           return 51;
    case SDL_SCANCODE_4:           return 52;
    case SDL_SCANCODE_5:           return 53;
    case SDL_SCANCODE_6:           return 54;
    case SDL_SCANCODE_7:           return 55;
    case SDL_SCANCODE_8:           return 56;
    case SDL_SCANCODE_9:           return 57;
    case SDL_SCANCODE_SEMICOLON:   return 59;  // RETROK_SEMICOLON
    case SDL_SCANCODE_EQUALS:      return 61;  // RETROK_EQUALS
    case SDL_SCANCODE_LEFTBRACKET: return 91;  // RETROK_LEFTBRACKET
    case SDL_SCANCODE_BACKSLASH:   return 92;  // RETROK_BACKSLASH
    case SDL_SCANCODE_RIGHTBRACKET:return 93;  // RETROK_RIGHTBRACKET
    case SDL_SCANCODE_GRAVE:       return 96;  // RETROK_BACKQUOTE
    case SDL_SCANCODE_A:           return 97;
    case SDL_SCANCODE_B:           return 98;
    case SDL_SCANCODE_C:           return 99;
    case SDL_SCANCODE_D:           return 100;
    case SDL_SCANCODE_E:           return 101;
    case SDL_SCANCODE_F:           return 102;
    case SDL_SCANCODE_G:           return 103;
    case SDL_SCANCODE_H:           return 104;
    case SDL_SCANCODE_I:           return 105;
    case SDL_SCANCODE_J:           return 106;
    case SDL_SCANCODE_K:           return 107;
    case SDL_SCANCODE_L:           return 108;
    case SDL_SCANCODE_M:           return 109;
    case SDL_SCANCODE_N:           return 110;
    case SDL_SCANCODE_O:           return 111;
    case SDL_SCANCODE_P:           return 112;
    case SDL_SCANCODE_Q:           return 113;
    case SDL_SCANCODE_R:           return 114;
    case SDL_SCANCODE_S:           return 115;
    case SDL_SCANCODE_T:           return 116;
    case SDL_SCANCODE_U:           return 117;
    case SDL_SCANCODE_V:           return 118;
    case SDL_SCANCODE_W:           return 119;
    case SDL_SCANCODE_X:           return 120;
    case SDL_SCANCODE_Y:           return 121;
    case SDL_SCANCODE_Z:           return 122;
    case SDL_SCANCODE_DELETE:      return 127; // RETROK_DELETE
    case SDL_SCANCODE_KP_0:        return 256; // RETROK_KP0
    case SDL_SCANCODE_KP_1:        return 257;
    case SDL_SCANCODE_KP_2:        return 258;
    case SDL_SCANCODE_KP_3:        return 259;
    case SDL_SCANCODE_KP_4:        return 260;
    case SDL_SCANCODE_KP_5:        return 261;
    case SDL_SCANCODE_KP_6:        return 262;
    case SDL_SCANCODE_KP_7:        return 263;
    case SDL_SCANCODE_KP_8:        return 264;
    case SDL_SCANCODE_KP_9:        return 265;
    case SDL_SCANCODE_KP_PERIOD:   return 266;
    case SDL_SCANCODE_KP_DIVIDE:   return 267;
    case SDL_SCANCODE_KP_MULTIPLY: return 268;
    case SDL_SCANCODE_KP_MINUS:    return 269;
    case SDL_SCANCODE_KP_PLUS:     return 270;
    case SDL_SCANCODE_KP_ENTER:    return 271;
    case SDL_SCANCODE_KP_EQUALS:   return 272;
    case SDL_SCANCODE_UP:          return 273;
    case SDL_SCANCODE_DOWN:        return 274;
    case SDL_SCANCODE_RIGHT:       return 275;
    case SDL_SCANCODE_LEFT:        return 276;
    case SDL_SCANCODE_INSERT:      return 277;
    case SDL_SCANCODE_HOME:        return 278;
    case SDL_SCANCODE_END:         return 279;
    case SDL_SCANCODE_PAGEUP:      return 280;
    case SDL_SCANCODE_PAGEDOWN:    return 281;
    case SDL_SCANCODE_F1:          return 282;
    case SDL_SCANCODE_F2:          return 283;
    case SDL_SCANCODE_F3:          return 284;
    case SDL_SCANCODE_F4:          return 285;
    case SDL_SCANCODE_F5:          return 286;
    case SDL_SCANCODE_F6:          return 287;
    case SDL_SCANCODE_F7:          return 288;
    case SDL_SCANCODE_F8:          return 289;
    case SDL_SCANCODE_F9:          return 290;
    case SDL_SCANCODE_F10:         return 291;
    case SDL_SCANCODE_F11:         return 292;
    case SDL_SCANCODE_F12:         return 293;
    case SDL_SCANCODE_NUMLOCKCLEAR:return 300;
    case SDL_SCANCODE_CAPSLOCK:    return 301;
    case SDL_SCANCODE_SCROLLLOCK:  return 302;
    case SDL_SCANCODE_RSHIFT:      return 303;
    case SDL_SCANCODE_LSHIFT:      return 304;
    case SDL_SCANCODE_RCTRL:       return 305;
    case SDL_SCANCODE_LCTRL:       return 306;
    case SDL_SCANCODE_RALT:        return 307;
    case SDL_SCANCODE_LALT:        return 308;
    case SDL_SCANCODE_LGUI:        return 311; // RETROK_LSUPER
    case SDL_SCANCODE_RGUI:        return 312; // RETROK_RSUPER
    default:                       return 0;   // RETROK_UNKNOWN
    }
}

// Map SDL key modifier bitmask to libretro RETROKMOD bitmask.
static uint16_t sdl_mod_to_retrokmod(Uint16 sdl_mod) {
    uint16_t mod = 0;
    if (sdl_mod & KMOD_SHIFT) mod |= 0x01; // RETROKMOD_SHIFT
    if (sdl_mod & KMOD_CTRL)  mod |= 0x02; // RETROKMOD_CTRL
    if (sdl_mod & KMOD_ALT)   mod |= 0x04; // RETROKMOD_ALT
    if (sdl_mod & KMOD_GUI)   mod |= 0x08; // RETROKMOD_META
    return mod;
}

// Dispatch an SDL keyboard event to the core. Called from the event loop.
static void rr_sdl_dispatch_key_event(const SDL_KeyboardEvent* ev) {
    static std::bitset<SDL_NUM_SCANCODES> forwarded_keys;

    SDL_Scancode sc = ev->keysym.scancode;
    const bool down = ev->type == SDL_KEYDOWN;

    if (rr_keyboard_text_editing()) {
        if (!down) return;
        const unsigned keycode = sdl_scancode_to_retrok(sc);
        if (!keycode) return;
        RRKeyEvent edit_event = {true, keycode, 0,
                                 sdl_mod_to_retrokmod(ev->keysym.mod)};
        rr_keyboard_event(&edit_event);
        return;
    }

    // A release is forwarded only if its matching press reached the core.
    // This prevents both frontend shortcuts leaking into the core and keys
    // becoming stuck when a frontend screen opens while a key is held.
    if (down && (!rr_keyboard_has_callback() || is_frontend_hotkey(sc))) return;
    if (!down && !forwarded_keys.test(sc)) return;

    unsigned keycode = sdl_scancode_to_retrok(sc);
    if (keycode == 0) return; // Unknown key, don't send garbage

    RRKeyEvent rr_ev;
    rr_ev.down = down;
    rr_ev.keycode = keycode;
    rr_ev.character = 0; // Will be filled by SDL_TEXTINPUT if available
    rr_ev.modifiers = sdl_mod_to_retrokmod(ev->keysym.mod);

    rr_keyboard_event(&rr_ev);
    forwarded_keys.set(sc, down);
}

static uint32_t decode_first_utf8_codepoint(const char* text) {
    const unsigned char* s = reinterpret_cast<const unsigned char*>(text);
    if (!s[0]) return 0;
    if (s[0] < 0x80) return s[0];
    if ((s[0] & 0xe0) == 0xc0 && s[1])
        return ((s[0] & 0x1f) << 6) | (s[1] & 0x3f);
    if ((s[0] & 0xf0) == 0xe0 && s[1] && s[2])
        return ((s[0] & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);
    if ((s[0] & 0xf8) == 0xf0 && s[1] && s[2] && s[3])
        return ((s[0] & 0x07) << 18) | ((s[1] & 0x3f) << 12) |
               ((s[2] & 0x3f) << 6) | (s[3] & 0x3f);
    return 0;
}

static void rr_sdl_dispatch_text_event(const SDL_TextInputEvent* ev) {
    if (!rr_keyboard_has_callback() && !rr_keyboard_text_editing()) return;
    const uint32_t character = decode_first_utf8_codepoint(ev->text);
    if (!character) return;
    RRKeyEvent event = {true, RETROK_UNKNOWN, character, 0};
    rr_keyboard_event(&event);
    event.down = false;
    rr_keyboard_event(&event);
}
