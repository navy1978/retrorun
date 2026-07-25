# Porting RetroRun to another device

RetroRun separates the libretro frontend from device I/O through the hardware
abstraction layer in `src/platform/platform.h`. The current implementation in
`src/platform/platform_go2.cpp` adapts that API to the bundled GO2 backend.
The SDL2 implementation used on macOS and Linux/KMSDRM is in
`src/platform/platform_sdl.cpp`. Linux defines `RR_SDL_GLES`, selecting an OpenGL ES 3
context and GLSL ES shaders while macOS keeps its Desktop OpenGL context.
Desktop system discovery for macOS, Linux and Windows is isolated in
`src/services/system-info.cpp`. Menu and OSD composition is isolated in
`src/video/ui-renderer.cpp`, independently from core frame processing in
`src/video/video.cpp`.

## Source layout

- `src/audio`: audio engine and selectable backend adapters.
- `src/config`: configuration parsing, validation and persistence.
- `src/core`: libretro loading, options, save states and disk control.
- `src/input`: logical input, keyboard, rumble and SDL events.
- `src/video`: presentation, overlays, fonts and decorations.
- `src/services`: achievements, networking, file browser and system discovery.
- `src/diagnostics`: logging and benchmark instrumentation.
- `src/platform`: portable hardware contract and backend implementations.
- `src/menu`, `src/go2` and `src/js2xbox`: existing specialised subsystems.

The frontend modules depend only on the portable API:

- `src/input/input.cpp` consumes buttons, sticks, battery and brightness through `rr_input_*`;
- `src/audio/audio.cpp` sends interleaved stereo PCM and controls volume through `rr_audio_*`;
- `src/video/video.cpp`, `src/video/video-helper.cpp` and `src/status.h` use opaque display, surface,
  presenter and graphics-context handles through `rr_*` calls.

## Adding a backend

1. Implement every function declared by `src/platform/platform.h`. Native objects must
   remain private to the backend implementation.
2. Map the logical `RRInputButton_*` values to the device's physical input
   events. Do not put device-specific key codes in `src/input/input.cpp`.
3. Implement audio submission as signed 16-bit, stereo, interleaved PCM. The
   sample rate is passed to `rr_audio_create`.
4. Implement RGB565/RGB888/XRGB8888 surfaces, scaling/blitting, presentation,
   screenshots and the hardware-rendering context used by libretro cores.
5. Select the new backend source in the build and replace backend-specific link
   libraries. Only one implementation of the `rr_*` API must be linked.

For a first bring-up, implement input and software video, use a no-op volume
control if the mixer is unavailable, and test a software-rendered core. Add the
EGL/OpenGL path after frame presentation is stable.

## Backend contract

- An opaque handle returned by `create` stays valid until its matching
  `destroy` call.
- `rr_input_state_read` produces a complete snapshot, not a stream of events.
- Stick axes are normalized to `[-1.0, 1.0]`.
- Rotation values are clockwise and expressed in display coordinates.
- `rr_context_surface_lock` returns a borrowed native frame wrapped in an
  opaque surface; `rr_context_surface_unlock` ends that borrow.
- `rr_video_filter_set` and `rr_video_shader_set` receive portable frontend
  preferences. A backend must preserve its historical presentation path while
  the filter/shader value is `DEFAULT`/`OFF`.
- Hardware post-processing backends return their frontend framebuffer from
  `rr_context_framebuffer_get`; libretro cores then render into that target
  before `rr_context_swap_buffers` applies the selected effect.
- Backends may queue frames internally, but all public calls must preserve the
  ordering used by the frontend.

Device policy such as default button layouts, screen orientation and mixer
control names still lives in the existing configuration/device detection code.
Those are the next candidates for a `DeviceProfile` abstraction once a second
backend is added and its concrete requirements are known.

## Available build backends

- `make` on Linux builds the historical GO2/DRM executable `retrorun`.
- `make PLATFORM=sdl2` builds the optional SDL2/KMSDRM executable
  `retrorun-sdl2` without replacing the native executable.
- `make` on macOS builds the SDL2/Desktop OpenGL executable `retrorun`.

The Linux SDL target asks libretro cores for OpenGL ES 3 and accepts GLES 2/3
hardware callbacks. Vulkan is intentionally rejected because neither the
frontend's libretro Vulkan interface nor AmberELEC's SDL2 Vulkan driver is
currently enabled. Software-rendered cores continue through SDL textures.
