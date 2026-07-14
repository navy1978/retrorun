#include "platform.h"

#include "go2/audio.h"
#include "go2/display.h"
#include "go2/input.h"
#include "status.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <cstdio>

struct rr_audio { go2_audio_t* native; };
struct rr_input { go2_input_t* native; };
struct rr_input_state { go2_input_state_t* native; };
struct rr_display { go2_display_t* native; };
struct rr_surface { go2_surface_t* native; };
struct rr_presenter { go2_presenter_t* native; };
struct rr_context {
    go2_context_t* native;
    int width;
    int height;
    GLuint framebuffer;
    GLuint color_texture;
    GLuint depth_buffer;
    GLuint stencil_buffer;
    GLuint post_program;
};

static rr_video_filter_t video_filter = RR_VIDEO_FILTER_DEFAULT;
static rr_video_shader_t video_shader = RR_VIDEO_SHADER_OFF;

static_assert(static_cast<int>(RRInputButton_TriggerRight) == static_cast<int>(Go2InputButton_TriggerRight),
              "GO2 and platform button layouts must match");
static_assert(static_cast<int>(RRInputThumbstick_Right) == static_cast<int>(Go2InputThumbstick_Right),
              "GO2 and platform stick layouts must match");
static_assert(static_cast<int>(RR_ROTATION_VERTICAL) == static_cast<int>(GO2_ROTATION_VERTICAL),
              "GO2 and platform rotation layouts must match");

static go2_rotation_t native_rotation(rr_rotation_t value) {
    return static_cast<go2_rotation_t>(value);
}

void rr_platform_preinit() {
    // The native backend links its DRM/GBM/EGL dependencies directly and
    // preserves the historical initialization order.
}

rr_input_t* rr_input_create(const char* device) {
    go2_input_t* native = go2_input_create(device);
    if (!native) return NULL;
    rr_input_t* result = new rr_input_t;
    result->native = native;
    return result;
}
void rr_input_destroy(rr_input_t* input) { if (input) { go2_input_destroy(input->native); delete input; } }
rr_input_feature_flags_t rr_input_features_get(rr_input_t* input) { return static_cast<rr_input_feature_flags_t>(go2_input_features_get(input->native)); }
rr_input_state_t* rr_input_state_create() { rr_input_state_t* s = new rr_input_state_t; s->native = go2_input_state_create(); return s; }
void rr_input_state_destroy(rr_input_state_t* state) { if (state) { go2_input_state_destroy(state->native); delete state; } }
void rr_input_state_read(rr_input_t* input, rr_input_state_t* state) { go2_input_state_read(input->native, state->native); }
rr_button_state_t rr_input_state_button_get(rr_input_state_t* state, rr_input_button_t button) {
    if (button > RRInputButton_TriggerRight) return RRButtonState_Released;
    return static_cast<rr_button_state_t>(go2_input_state_button_get(state->native, static_cast<go2_input_button_t>(button)));
}
void rr_input_state_button_set(rr_input_state_t* state, rr_input_button_t button, rr_button_state_t value) {
    if (button <= RRInputButton_TriggerRight)
        go2_input_state_button_set(state->native, static_cast<go2_input_button_t>(button), static_cast<go2_button_state_t>(value));
}
rr_thumb_t rr_input_state_thumbstick_get(rr_input_state_t* state, rr_input_thumbstick_t stick) { go2_thumb_t v = go2_input_state_thumbstick_get(state->native, static_cast<go2_input_thumbstick_t>(stick)); rr_thumb_t r = {v.x, v.y}; return r; }
void rr_input_state_thumbstick_set_null(rr_input_state_t* state, rr_input_thumbstick_t stick) { go2_input_state_thumbstick_setNul(state->native, static_cast<go2_input_thumbstick_t>(stick)); }
void rr_input_battery_read(rr_input_t* input, rr_battery_state_t* state) { go2_battery_state_t v; go2_input_battery_read(input->native, &v); state->level = v.level; state->status = static_cast<rr_battery_status_t>(v.status); }
void rr_input_brightness_read(rr_input_t* input, rr_brightness_state_t* state) { go2_brightness_state_t v; go2_input_brightness_read(input->native, &v); state->level = v.level; }
void rr_input_brightness_write(int value) { go2_input_brightness_write(value); }
bool rr_input_set_rumble(uint16_t, uint16_t, uint32_t) { return false; }

rr_audio_t* rr_audio_create(int frequency) { go2_audio_t* native = go2_audio_create(frequency); if (!native) return NULL; rr_audio_t* a = new rr_audio_t; a->native = native; return a; }
void rr_audio_destroy(rr_audio_t* audio) { if (audio) { go2_audio_destroy(audio->native); delete audio; } }
void rr_audio_submit(rr_audio_t* audio, const short* data, int frames) { go2_audio_submit(audio->native, data, frames); }
uint32_t rr_audio_volume_get(rr_audio_t* audio, const char* control) { return go2_audio_volume_get(audio->native, control); }
void rr_audio_volume_set(rr_audio_t* audio, uint32_t value, const char* control) { go2_audio_volume_set(audio->native, value, control); }

rr_display_t* rr_display_create() { go2_display_t* native = go2_display_create(); if (!native) return NULL; rr_display_t* d = new rr_display_t; d->native = native; return d; }
void rr_display_destroy(rr_display_t* display) { if (display) { go2_display_destroy(display->native); delete display; } }
int rr_display_width_get(rr_display_t* display) { return go2_display_width_get(display->native); }
int rr_display_height_get(rr_display_t* display) { return go2_display_height_get(display->native); }
uint32_t rr_display_backlight_get(rr_display_t* display) { return go2_display_backlight_get(display->native); }
void rr_display_backlight_set(rr_display_t* display, uint32_t value) { go2_display_backlight_set(display->native, value); }
int rr_pixel_format_bpp(uint32_t format) { return go2_drm_format_get_bpp(format); }

rr_surface_t* rr_surface_create(rr_display_t* display, int width, int height, uint32_t format) { go2_surface_t* native = go2_surface_create(display->native, width, height, format); if (!native) return NULL; rr_surface_t* s = new rr_surface_t; s->native = native; return s; }
void rr_surface_destroy(rr_surface_t* surface) { if (surface) { go2_surface_destroy(surface->native); delete surface; } }
int rr_surface_width_get(rr_surface_t* surface) { return go2_surface_width_get(surface->native); }
int rr_surface_height_get(rr_surface_t* surface) { return go2_surface_height_get(surface->native); }
uint32_t rr_surface_format_get(rr_surface_t* surface) { return go2_surface_format_get(surface->native); }
int rr_surface_stride_get(rr_surface_t* surface) { return go2_surface_stride_get(surface->native); }
void* rr_surface_map(rr_surface_t* surface) { return go2_surface_map(surface->native); }
void rr_surface_unmap(rr_surface_t* surface) { go2_surface_unmap(surface->native); }
void rr_surface_blit(rr_surface_t* source, int sx, int sy, int sw, int sh, rr_surface_t* dest, int dx, int dy, int dw, int dh, rr_rotation_t rotation) { go2_surface_blit(source->native, sx, sy, sw, sh, dest->native, dx, dy, dw, dh, native_rotation(rotation)); }
int rr_surface_save_as_png(rr_surface_t* surface, const char* filename) { return go2_surface_save_as_png(surface->native, filename); }

rr_presenter_t* rr_presenter_create(rr_display_t* display, uint32_t format, uint32_t background) { go2_presenter_t* native = go2_presenter_create(display->native, format, background); if (!native) return NULL; rr_presenter_t* p = new rr_presenter_t; p->native = native; return p; }
void rr_presenter_destroy(rr_presenter_t* presenter) { if (presenter) { go2_presenter_destroy(presenter->native); delete presenter; } }
void rr_presenter_post(rr_presenter_t* p, rr_surface_t* s, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, rr_rotation_t r) { go2_presenter_post(p->native, s->native, sx, sy, sw, sh, dx, dy, dw, dh, native_rotation(r)); }
void rr_presenter_black(rr_presenter_t* p, int x, int y, int w, int h, rr_rotation_t r) { go2_presenter_black(p->native, x, y, w, h, native_rotation(r)); }
void rr_presenter_wait_for_loading_screen(rr_presenter_t*, unsigned) {}
void rr_presenter_post_multiple(rr_presenter_t* p, rr_surface_t* s, status* o, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, rr_rotation_t r, rr_rotation_t br, bool wide) {
    go2_status_t native = {};
    native.top_right = o->top_right ? o->top_right->native : NULL;
    native.bottom_right = o->bottom_right ? o->bottom_right->native : NULL;
    native.bottom_center = o->bottom_center ? o->bottom_center->native : NULL;
    native.top_left = o->top_left ? o->top_left->native : NULL;
    native.bottom_left = o->bottom_left ? o->bottom_left->native : NULL;
    native.full = o->full ? o->full->native : NULL;
    native.show_top_right=o->show_top_right; native.show_bottom_right=o->show_bottom_right;
    native.show_bottom_center=o->show_bottom_center; native.show_top_left=o->show_top_left;
    native.show_bottom_left=o->show_bottom_left; native.show_full=o->show_full;
    native.clean_top_right=o->clean_top_right; native.clean_bottom_right=o->clean_bottom_right;
    native.clean_bottom_center=o->clean_bottom_center; native.clean_top_left=o->clean_top_left;
    native.clean_bottom_left=o->clean_bottom_left; native.clean_full=o->clean_full;
    go2_presenter_post_multiple(p->native, s->native, &native, sx, sy, sw, sh, dx, dy, dw, dh, native_rotation(r), native_rotation(br), wide);
}

static GLuint compile_go2_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;
    char log[1024] = {};
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    std::fprintf(stderr, "RetroRun GO2 post-processing shader failed: %s\n", log);
    glDeleteShader(shader);
    return 0;
}

static GLuint create_go2_post_program() {
    static const char* vertex_source =
        "attribute vec2 position;\n"
        "attribute vec2 texcoord;\n"
        "varying vec2 uv;\n"
        "void main(){ uv=texcoord; gl_Position=vec4(position,0.0,1.0); }\n";
    static const char* fragment_source =
        "precision mediump float;\n"
        "uniform sampler2D frame_texture;\n"
        "uniform vec2 source_size;\n"
        "uniform vec2 texture_scale;\n"
        "uniform int shader_mode;\n"
        "varying vec2 uv;\n"
        "void main(){\n"
        " vec2 sample_uv=uv; vec2 centered=sample_uv*2.0-1.0;\n"
        " if(shader_mode==2){ float r2=dot(centered,centered); centered*=1.0+r2*0.055;\n"
        "  sample_uv=centered*0.5+0.5;\n"
        "  if(sample_uv.x<0.0||sample_uv.y<0.0||sample_uv.x>1.0||sample_uv.y>1.0){ gl_FragColor=vec4(0.0,0.0,0.0,1.0); return; }}\n"
        " vec3 color=texture2D(frame_texture,sample_uv*texture_scale).rgb;\n"
        " if(shader_mode>0){ float scanline=0.82+0.18*sin(sample_uv.y*source_size.y*3.14159265); color*=scanline; }\n"
        " if(shader_mode==2){ float vignette=1.0-0.24*dot(centered,centered);\n"
        "  float mask=mod(floor(gl_FragCoord.x),3.0);\n"
        "  vec3 grille=mask<1.0?vec3(1.0,0.92,0.92):(mask<2.0?vec3(0.92,1.0,0.92):vec3(0.92,0.92,1.0));\n"
        "  color*=max(vignette,0.55)*grille; } gl_FragColor=vec4(color,1.0); }\n";

    GLuint vertex = compile_go2_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment = compile_go2_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "position");
    glBindAttribLocation(program, 1, "texcoord");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) return program;
    char log[1024] = {};
    glGetProgramInfoLog(program, sizeof(log), NULL, log);
    std::fprintf(stderr, "RetroRun GO2 post-processing program failed: %s\n", log);
    glDeleteProgram(program);
    return 0;
}

static bool create_go2_post_framebuffer(rr_context_t* context) {
    glGenTextures(1, &context->color_texture);
    glBindTexture(GL_TEXTURE_2D, context->color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, context->width, context->height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &context->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, context->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           context->color_texture, 0);
    glGenRenderbuffers(1, &context->depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, context->depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, context->width, context->height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              context->depth_buffer);
    glGenRenderbuffers(1, &context->stencil_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, context->stencil_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, context->width, context->height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              context->stencil_buffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "RetroRun GO2 post-processing framebuffer is incomplete; shader disabled\n");
        return false;
    }
    context->post_program = create_go2_post_program();
    return context->post_program != 0;
}

rr_context_t* rr_context_create(rr_display_t* display, int width, int height,
                                const rr_context_attributes_t* attributes) {
    go2_context_attributes_t native = {
        attributes->major, attributes->minor,
        attributes->red_bits, attributes->green_bits, attributes->blue_bits,
        attributes->alpha_bits, attributes->depth_bits, attributes->stencil_bits};
    go2_context_t* native_context = go2_context_create(display->native, width, height, &native);
    if (!native_context) return NULL;

    rr_context_t* context = new rr_context_t{};
    context->native = native_context;
    context->width = width;
    context->height = height;
    if (video_shader != RR_VIDEO_SHADER_OFF && !create_go2_post_framebuffer(context)) {
        if (context->stencil_buffer) glDeleteRenderbuffers(1, &context->stencil_buffer);
        if (context->depth_buffer) glDeleteRenderbuffers(1, &context->depth_buffer);
        if (context->framebuffer) glDeleteFramebuffers(1, &context->framebuffer);
        if (context->color_texture) glDeleteTextures(1, &context->color_texture);
        context->stencil_buffer = 0;
        context->depth_buffer = 0;
        context->framebuffer = 0;
        context->color_texture = 0;
        context->post_program = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    return context;
}

void rr_context_destroy(rr_context_t* context) {
    if (!context) return;
    go2_context_make_current(context->native);
    if (context->post_program) glDeleteProgram(context->post_program);
    if (context->stencil_buffer) glDeleteRenderbuffers(1, &context->stencil_buffer);
    if (context->depth_buffer) glDeleteRenderbuffers(1, &context->depth_buffer);
    if (context->framebuffer) glDeleteFramebuffers(1, &context->framebuffer);
    if (context->color_texture) glDeleteTextures(1, &context->color_texture);
    go2_context_destroy(context->native);
    delete context;
}
void rr_context_make_current(rr_context_t* context) { go2_context_make_current(context->native); }
void rr_context_swap_buffers(rr_context_t* context, int source_width, int source_height, int, int, int, int, status*) {
    if (!context->framebuffer || !context->post_program) {
        go2_context_swap_buffers(context->native);
        return;
    }

    GLint previous_framebuffer = 0;
    GLint previous_program = 0;
    GLint previous_array_buffer = 0;
    GLint previous_active_texture = 0;
    GLint previous_texture = 0;
    GLint previous_viewport[4] = {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array_buffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, source_width, source_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(context->post_program);
    glBindTexture(GL_TEXTURE_2D, context->color_texture);
    const GLint filtering = video_filter == RR_VIDEO_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
    glUniform1i(glGetUniformLocation(context->post_program, "frame_texture"), 0);
    glUniform2f(glGetUniformLocation(context->post_program, "source_size"),
                static_cast<float>(source_width), static_cast<float>(source_height));
    glUniform2f(glGetUniformLocation(context->post_program, "texture_scale"),
                static_cast<float>(source_width) / context->width,
                static_cast<float>(source_height) / context->height);
    glUniform1i(glGetUniformLocation(context->post_program, "shader_mode"),
                video_shader == RR_VIDEO_SHADER_CRT ? 2 :
                (video_shader == RR_VIDEO_SHADER_SCANLINES ? 1 : 0));

    static const GLfloat vertices[] = {
        -1.0f,-1.0f, 0.0f,0.0f,  1.0f,-1.0f, 1.0f,0.0f,
        -1.0f, 1.0f, 0.0f,1.0f,  1.0f, 1.0f, 1.0f,1.0f
    };
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat), vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat), vertices+2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    go2_context_swap_buffers(context->native);

    glUseProgram(static_cast<GLuint>(previous_program));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array_buffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    glActiveTexture(static_cast<GLenum>(previous_active_texture));
    glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
}
uintptr_t rr_context_framebuffer_get(rr_context_t* context) { return context ? context->framebuffer : 0; }
rr_surface_t* rr_context_surface_lock(rr_context_t* context) { go2_surface_t* native = go2_context_surface_lock(context->native); if (!native) return NULL; rr_surface_t* s = new rr_surface_t; s->native=native; return s; }
void rr_context_surface_unlock(rr_context_t* context, rr_surface_t* surface) { go2_context_surface_unlock(context->native, surface->native); delete surface; }
void* rr_context_get_proc_address(const char* symbol) { return reinterpret_cast<void*>(eglGetProcAddress(symbol)); }
void rr_video_sync() {
    glFinish();
    glFlush();
}
bool rr_video_vsync_set(bool) { return false; }
bool rr_video_vsync_get() { return false; }
void rr_video_filter_set(rr_video_filter_t filter) { video_filter = filter; }
rr_video_filter_t rr_video_filter_get() { return video_filter; }
void rr_video_shader_set(rr_video_shader_t shader) { video_shader = shader; }
rr_video_shader_t rr_video_shader_get() { return video_shader; }

const char* rr_platform_backend_name() { return "go2"; }
const char* rr_platform_renderer_name() { return "GO2 / DRM"; }
