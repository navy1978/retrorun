# Native GO2 software scanlines

The software video path is `core_video_refresh` →
`core_video_refresh_NON_OPENGL` → `uiRenderOverlays` → `drawNonOpenGL` →
GO2 presenter → synchronous RGA blit → DRM scanout. OpenGL cores and the SDL2
backend do not use the optimized upload.

## Coordinates and pixels

The existing effect is defined in **core-frame coordinates**, before RGA scaling,
viewport placement and rotation. Even source rows are unchanged; odd source rows
use Q8 gain 174 with `+128` rounding. RGB565 is darkened at its native component
precision. XRGB8888/RGBA8888 preserve the high byte. RGB888 is processed by byte.
The original 0RGB1555-to-RGB565 conversion is retained, including its zero green
low bit; odd-width frames now also convert their last pixel.

`go2_scanlines::copy` combines the upload, optional 1555 conversion and scanlines.
It reads the core framebuffer and writes the GO2 mapping once, without reading
back the destination, allocating a gain map or processing unchanged rows pixel
by pixel. It respects independent source pitch and destination stride, supports
unaligned input, and leaves padding untouched. Surface ownership, map/unmap,
queue synchronization, RGA scaling/rotation, overlays and bezels are unchanged.
Therefore the RGA input pixels are equivalent at every viewport/scaling mode;
scanlines have not been redefined as display-space alternating rows.

Shader OFF and CRT retain their previous paths. CRT still uses its existing
vignette, grille and gain map. No persistent mapping, SIMD intrinsics, GL path,
new dependency or device-specific pixel/stride assumption is introduced.

## Optional diagnostics

Normal builds compile out the stage timers. For an isolated diagnostic build:

```sh
make PLATFORM=linux-go2 config=release GO2_SOFTWARE_PROFILE=1
RETRORUN_GO2_SOFTWARE_PROFILE=/writable/path/stages.json ./retrorun \
  -c /path/to/test.cfg --benchmark 10 --benchmark-warmup 2 \
  --benchmark-json /writable/path/frame-times.json core.so test-content
```

Set `retrorun_video_shader = scanlines` in the isolated test configuration.
Use the normal SDK/compiler options and device environment for the target. The environment variable is
required as well as the build flag. No per-frame output is emitted. The report is
written atomically at shutdown. Sampling uses fixed storage (about 300 KiB), no
per-frame allocation, and excludes benchmark warmup. Averages cover the whole
collection; p95 uses the most recent 4096 frames. All stages have one sample per
software callback, including zero when a stage was not used. Do not mix geometry
or effect modes in one measurement; the report flags such sessions.

Stages include mapping, legacy copying/conversion, legacy effect, fused upload,
unmapping, presenter queue wait, RGA blit, presenter submit and total software
callback. Timings are **nested**: submit includes queue wait and RGA; callback
includes upload and presentation. Do not add these totals together. DRM scanout
runs on the presenter thread; use the existing benchmark presentation counters
and full active-frame timing alongside the stage report. RGA timing is the
synchronous `c_RkRgaBlit` call, including scaling and conversion; it does not
separate those hardware operations.

For the historical control, additionally build with `GO2_LEGACY_SCANLINES=1`.
This retains the exact old two-pass scanline path. Use a clean build or a separate
`OBJDIR` and output for each configuration: Make does not track changed flags.
Compare OFF and ON with identical core, content, viewport, audio, CPU policy and
pacing. Report full active-frame average/p95 and the 16.67 ms budget separately
from upload timings. Profiling has measurement overhead; do not mistake a host
kernel benchmark for RK3326 or end-to-end results.

## Tests

`make -C tests run` includes an independent fixture of the pre-change upload and
effect. Tests compare complete active rows across formats, dimensions, padding
and alignment, preserve core input and destination guards, exhaust RGB565/1555
and RGB888 component combinations, and reject invalid/overflowing layouts.
The odd-width 1555 fixture pads the old path to avoid inheriting its skipped last
pixel. AddressSanitizer/UBSan can be enabled with the repository test target.

Hardware verification still needs representative cores, integer/non-integer
scaling, aspect ratios, rotation, bezel/overlay transitions and CRT/OpenGL smoke
tests. Identical upload bytes establish scanline equivalence, but are not a
substitute for those integration checks or real-game performance measurements.
