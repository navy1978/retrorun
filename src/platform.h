#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(RR_PLATFORM_GO2) && defined(RR_PLATFORM_SDL)
#error "Select exactly one RetroRun platform backend"
#endif

#define RR_FOURCC(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
                               ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define RR_PIXEL_FORMAT_RGB565   RR_FOURCC('R', 'G', '1', '6')
#define RR_PIXEL_FORMAT_RGB888   RR_FOURCC('R', 'G', '2', '4')
#define RR_PIXEL_FORMAT_XRGB8888 RR_FOURCC('X', 'R', '2', '4')
#define RR_PIXEL_FORMAT_RGBA8888 RR_FOURCC('R', 'A', '2', '4')
#define RR_PIXEL_FORMAT_RGBA5551 RR_FOURCC('R', 'A', '1', '5')

// Hardware abstraction layer used by the libretro-facing frontend.  Platform
// backends own the native handles; the rest of RetroRun only sees these opaque
// types and portable value objects.

class status;

typedef struct rr_audio rr_audio_t;
typedef struct rr_input rr_input_t;
typedef struct rr_input_state rr_input_state_t;
typedef struct rr_display rr_display_t;
typedef struct rr_surface rr_surface_t;
typedef struct rr_presenter rr_presenter_t;
typedef struct rr_context rr_context_t;
typedef struct rr_frame_buffer rr_frame_buffer_t;

typedef enum {
    RRButtonState_Released = 0,
    RRButtonState_Pressed
} rr_button_state_t;

typedef struct { float x; float y; } rr_thumb_t;

typedef enum {
    RRInputButton_DPadUp = 0, RRInputButton_DPadDown,
    RRInputButton_DPadLeft, RRInputButton_DPadRight,
    RRInputButton_A, RRInputButton_B, RRInputButton_X, RRInputButton_Y,
    RRInputButton_F1, RRInputButton_F2, RRInputButton_F3, RRInputButton_F4,
    RRInputButton_F5, RRInputButton_F6, RRInputButton_F7, RRInputButton_F8,
    RRInputButton_F9, RRInputButton_F10, RRInputButton_F11, RRInputButton_F12,
    RRInputButton_F13, RRInputButton_F14, RRInputButton_F15, RRInputButton_F16,
    RRInputButton_SELECT, RRInputButton_START,
    RRInputButton_THUMBR, RRInputButton_THUMBL,
    RRInputButton_TopLeft, RRInputButton_TopRight,
    RRInputButton_TriggerLeft, RRInputButton_TriggerRight,
    RRInputButton_Quit
} rr_input_button_t;

typedef enum { RRInputThumbstick_Left = 0, RRInputThumbstick_Right } rr_input_thumbstick_t;
typedef enum {
    RRInputFeatureFlags_None = (1 << 0),
    RRInputFeatureFlags_Triggers = (1 << 1),
    RRInputFeatureFlags_RightAnalog = (1 << 2)
} rr_input_feature_flags_t;

typedef enum {
    RRBattery_Status_Unknown = 0, RRBattery_Status_Discharging,
    RRBattery_Status_Charging, RRBattery_Status_Full
} rr_battery_status_t;
typedef struct { uint32_t level; rr_battery_status_t status; } rr_battery_state_t;
typedef struct { uint32_t level; } rr_brightness_state_t;

rr_input_t* rr_input_create(const char* device);
void rr_input_destroy(rr_input_t* input);
rr_input_feature_flags_t rr_input_features_get(rr_input_t* input);
rr_input_state_t* rr_input_state_create();
void rr_input_state_destroy(rr_input_state_t* state);
void rr_input_state_read(rr_input_t* input, rr_input_state_t* state);
rr_button_state_t rr_input_state_button_get(rr_input_state_t* state, rr_input_button_t button);
void rr_input_state_button_set(rr_input_state_t* state, rr_input_button_t button, rr_button_state_t value);
rr_thumb_t rr_input_state_thumbstick_get(rr_input_state_t* state, rr_input_thumbstick_t stick);
void rr_input_state_thumbstick_set_null(rr_input_state_t* state, rr_input_thumbstick_t stick);
void rr_input_battery_read(rr_input_t* input, rr_battery_state_t* state);
void rr_input_brightness_read(rr_input_t* input, rr_brightness_state_t* state);
void rr_input_brightness_write(int value);
bool rr_input_set_rumble(uint16_t low_frequency, uint16_t high_frequency,
                         uint32_t duration_ms);

rr_audio_t* rr_audio_create(int frequency);
void rr_audio_destroy(rr_audio_t* audio);
void rr_audio_submit(rr_audio_t* audio, const short* data, int frames);
uint32_t rr_audio_volume_get(rr_audio_t* audio, const char* control);
void rr_audio_volume_set(rr_audio_t* audio, uint32_t value, const char* control);

typedef enum rr_rotation {
    RR_ROTATION_DEGREES_0 = 0, RR_ROTATION_DEGREES_90,
    RR_ROTATION_DEGREES_180, RR_ROTATION_DEGREES_270,
    RR_ROTATION_HORIZONTAL, RR_ROTATION_VERTICAL
} rr_rotation_t;

typedef struct {
    int major, minor;
    int red_bits, green_bits, blue_bits, alpha_bits;
    int depth_bits, stencil_bits;
} rr_context_attributes_t;

typedef enum {
    RR_VIDEO_FILTER_DEFAULT = 0,
    RR_VIDEO_FILTER_NEAREST,
    RR_VIDEO_FILTER_LINEAR
} rr_video_filter_t;

typedef enum {
    RR_VIDEO_SHADER_OFF = 0,
    RR_VIDEO_SHADER_SCANLINES,
    RR_VIDEO_SHADER_CRT
} rr_video_shader_t;

rr_display_t* rr_display_create();
void rr_display_destroy(rr_display_t* display);
int rr_display_width_get(rr_display_t* display);
int rr_display_height_get(rr_display_t* display);
uint32_t rr_display_backlight_get(rr_display_t* display);
void rr_display_backlight_set(rr_display_t* display, uint32_t value);
int rr_pixel_format_bpp(uint32_t format);

rr_surface_t* rr_surface_create(rr_display_t* display, int width, int height, uint32_t format);
void rr_surface_destroy(rr_surface_t* surface);
int rr_surface_width_get(rr_surface_t* surface);
int rr_surface_height_get(rr_surface_t* surface);
uint32_t rr_surface_format_get(rr_surface_t* surface);
int rr_surface_stride_get(rr_surface_t* surface);
void* rr_surface_map(rr_surface_t* surface);
void rr_surface_unmap(rr_surface_t* surface);
void rr_surface_blit(rr_surface_t* source, int src_x, int src_y, int src_w, int src_h,
                     rr_surface_t* dest, int dst_x, int dst_y, int dst_w, int dst_h,
                     rr_rotation_t rotation);
int rr_surface_save_as_png(rr_surface_t* surface, const char* filename);

rr_presenter_t* rr_presenter_create(rr_display_t* display, uint32_t format, uint32_t background);
void rr_presenter_destroy(rr_presenter_t* presenter);
void rr_presenter_post(rr_presenter_t* presenter, rr_surface_t* surface,
                       int src_x, int src_y, int src_w, int src_h,
                       int dst_x, int dst_y, int dst_w, int dst_h, rr_rotation_t rotation);
void rr_presenter_black(rr_presenter_t* presenter, int x, int y, int width, int height,
                        rr_rotation_t rotation);
void rr_presenter_post_multiple(rr_presenter_t* presenter, rr_surface_t* surface, status* overlays,
                                int src_x, int src_y, int src_w, int src_h,
                                int dst_x, int dst_y, int dst_w, int dst_h,
                                rr_rotation_t rotation, rr_rotation_t blit_rotation,
                                bool widescreen);

rr_context_t* rr_context_create(rr_display_t* display, int width, int height,
                                const rr_context_attributes_t* attributes);
void rr_context_destroy(rr_context_t* context);
void rr_context_make_current(rr_context_t* context);
void rr_context_swap_buffers(rr_context_t* context, int source_width, int source_height,
                             int dest_x, int dest_y, int dest_width, int dest_height,
                             status* overlays);
uintptr_t rr_context_framebuffer_get(rr_context_t* context);
rr_surface_t* rr_context_surface_lock(rr_context_t* context);
void rr_context_surface_unlock(rr_context_t* context, rr_surface_t* surface);
void* rr_context_get_proc_address(const char* symbol);
bool rr_video_vsync_set(bool enabled);
void rr_video_filter_set(rr_video_filter_t filter);
void rr_video_shader_set(rr_video_shader_t shader);

const char* rr_platform_backend_name();
const char* rr_platform_renderer_name();
