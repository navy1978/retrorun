# Native GO2 CRT and hardware-core effects

## Software CRT

`go2_crt::Plan` inverse-maps each source pixel using the same radial curvature
coefficient as the native GLES CRT (`center *= 1 + .05 * dot(center, center)`).
Pixels outside the curved screen are black. Vignette and RGB grille use the GLES
style; source-row scanline phase/gain remains the software convention. This is
an intentional visual change from the previous flat software CRT, not a promise
of byte equivalence with it or with a differently scaled GPU viewport.

A cached map contains source offsets, interpolation weights and fixed-point
channel gains. Packed R/B interpolation lanes preserve the scalar Q16 result
while loading each source pixel once; format/filter dispatch is outside the
pixel loop. ARM NEON accelerates the measured 32-bit bilinear hot path with the
same Q16 rounding; other builds use the portable packed implementation. Both
are checked against the scalar reference. Each frame reads core memory and writes the GO2 surface directly;
it does not read back DRM memory. Nearest filtering samples directly; other
filter modes interpolate in the source image. Final viewport/scaling/rotation,
bezels and overlays still belong to RGA. RGB565, 1555-to-565, RGB888 and 32-bit
formats are supported with independent pitches and unaligned input.

Maps rebuild only for source geometry/pitch/pixel size/filter changes. They are
limited to 16 MiB and 4096 pixels per side; allocation/layout failure retains the
old CRT path, without retrying map allocation for the same failed configuration.
No GL context is introduced for software cores. Scanlines OFF/ON retain the
previously optimized paths. `GO2_LEGACY_CRT=1` selects the historical CRT for an
isolated control build; use a separate OBJDIR/output as with scanline profiling.

## Hardware cores

GO2 and the core share one GLES context. The post-processing pass now isolates
blend/depth/stencil/scissor/cull/sample/dither tests, color writes, clear color,
program, texture unit/binding, viewport, buffers and renderbuffer. GLES3 uses a
private VAO and restores read framebuffer, sampler, unpack buffer and rasterizer
discard. GLES2 restores the two attribute descriptions it changes. This prevents
core state from suppressing the fullscreen draw or being damaged by that draw.

The core's native framebuffer target stays **0** throughout OFF/ON changes.
Redirecting an already running core to a newly allocated FBO broke cores that
cache that target. The effect now takes a GPU-only snapshot of the completed
native window buffer. Its texture matches native color components/precision:
RGB565 without alpha must not be copied into an assumed RGBA texture on Mali.
Core-private bindings at callback entry are restored and do not select the input.

Duplicate/paused callbacks reuse the original snapshot without cumulative
filtering. The incoming libretro frame-valid flag is captured before the menu
substitutes lastData. Otherwise a paused menu looked like a newly rendered frame
and could capture stale or already processed window contents. OFF with a new
real core frame bypasses the post pass, preserving the normal fast path. The
common swap API carries this GO2 flag; SDL behavior is unchanged.

The original native GLES CRT shader and curvature parameters are unchanged.
Core-owned private FBOs remain the core's responsibility; the frontend processes
the stable native target advertised through libretro. No CPU GL readback occurs at runtime.
The integration test performs readback to verify output, outside normal builds.

The overlay presenter now receives the actual game source width/height, even
when the full menu surface has different dimensions. Previously a 320×224 Saturn
frame plus a 320×240 menu could request a source rectangle beyond the GBM surface.
Menu surfaces retain their own sizes and existing overlay composition.

## Tests

`make -C tests run` includes CRT format/layout/guard/cache/bounds tests and
byte comparisons against the independent scalar interpolation reference.
`go2_gl_post_test.cpp` separately exercises the actual GO2 platform code on a real
EGL/GLES implementation, with GO2 swap replaced by test-only readback. It covers
GLES2 and GLES3, OFF→ON→CRT→OFF, RGB565 and RGBA8888 native buffers, stable target queries, private
bindings at callback entry, duplicate callbacks and hostile core state (including a separate read
FBO, sampler, unpack buffer, VAO and rasterizer discard). It is not part of the
portable default suite because it requires Linux EGL/GLES/GBM development files.

On Linux with existing development dependencies:

```sh
c++ -std=c++20 -O1 -ffunction-sections -fdata-sections \
  -Isrc -Isrc/go2 -Isrc/config -Isrc/video -Isrc/input -Isrc/core \
  -Isrc/audio -Isrc/menu -Isrc/services -Isrc/diagnostics -Isrc/platform \
  tests/go2_gl_post_test.cpp -Wl,--gc-sections -lEGL -lGLESv2 -lgbm \
  -o /tmp/go2-gl-post-test
/tmp/go2-gl-post-test
```

Mesa uses a surfaceless context. A target build can run against existing Mali
GBM/EGL with `RETRORUN_TEST_DRM=/dev/dri/card0`; coordinate device ownership and
never interrupt an active game. This test creates rendering surfaces, not a new
OS image or production install. Normal core/game visual acceptance remains
necessary in addition to the automated pixel/state checks.

## Native Auto aspect ratio

For landscape games on native landscape MP/V/RK3566 panels, the core display
aspect is fitted inside the physical canvas and centered, independently of
effect mode. The old narrow-aspect branch used height/aspect for width instead
of height*aspect. At 640×480, 8:7 is 549×480 (x=45), not 420×480; a 1.306 core
ratio is 627×480. Auto uses a valid core ratio, falling back to source dimensions
only when the core omits it. Explicit ratios, integer scaling and Tate retain
their respective policies. Tests also cover invalid metadata under release
-ffast-math; finite validation uses IEEE bits to avoid compiler folding.

Core `SET_GEOMETRY` callbacks are coalesced until `retro_run()` returns. Nominal
size and display ratio are then updated without reallocating video buffers or
reinitializing audio/timing. Allocation maxima in this callback are ignored as
required by libretro; full AV updates keep their separate path. This is needed
for cores such as Genesis Plus GX that switch from startup geometry to gameplay.
Explicitly selected display ratios remain fixed across these updates.
