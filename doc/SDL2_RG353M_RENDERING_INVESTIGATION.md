# SDL2 rendering investigation on RG353M

Date: 2026-07-20
Status: unresolved; investigation paused after isolating the failure to the
SDL2/KMSDRM presentation path.

## Scope

This document records the tests performed for the SDL2 rendering defect seen
on an Anbernic RG353M running dArkOS. It is intended to prevent repeating the
same experiments and to provide a starting point for a later comparison with
AmberELEC and other devices.

The main test case was:

- device: RG353M, RK3566, Mali-G52;
- OS: dArkOS, Debian 13 userspace;
- kernel: Linux 5.10.226;
- SDL: 2.32.10, KMSDRM video driver;
- display: internal DSI panel, DRM mode 640x480;
- frontend: `retrorun_sdl2` 3.1.1 development build;
- core: Flycast `0.1 4c293f30`;
- content: Soul Calibur (Dreamcast).

The issue has also been discussed in relation to RG353V/RG353 devices, but the
detailed tests in this document were run on RG353M. It must not yet be assumed
that every RK3566 device has identical behaviour.

## Visible symptoms

- The game image appears vertically incorrect on the physical LCD.
- A band at the bottom contains distorted, stale or repeated image data.
- Bottom overlays are separated from the apparently valid game area by that
  band.
- Full-screen RetroRun pages are affected too: part of the top of the menu can
  be clipped while the lower band remains visible.
- The FPS overlay was initially too close to the edge and too small to read.
  Increasing its size improved readability, but did not fix the underlying
  presentation defect.
- RetroRun frequently terminates with a segmentation fault after the core is
  unloaded. This is a separate shutdown issue and was not established as the
  cause of the on-screen corruption.

## Geometry and display facts collected

RetroRun and SDL consistently reported:

```text
window=640x480
drawable=640x480
source=640x480
viewport=0,0 640x480
rotation=0
video driver=KMSDRM
```

The kernel exposes:

```text
/sys/class/drm/card0-DSI-1/modes: 640x480
/sys/class/graphics/fb0/modes: U:640x480p-0
/sys/class/graphics/fb0/virtual_size: 640,960
```

The 640x960 value is the fbcon virtual/double-buffer height. The active scanout
area is still 640x480.

The active DRM framebuffer differed between the two RetroRun backends:

| Backend | DRM format | Size | Pitch | Object size | Result |
| --- | --- | ---: | ---: | ---: | --- |
| SDL2/KMSDRM | XR24 / XRGB8888 | 640x480 | 2560 | 1,228,800 | Corrupted LCD output |
| Native GO2 | RG24 / RGB888 | 640x480 | 1920 | 921,600 | Correct LCD output |

Connector, CRTC, plane, mode, source rectangle, destination rectangle,
rotation and modifiers did not reveal another meaningful difference. The
dArkOS fbcon also uses XR24 successfully, but with a 640x960 virtual buffer;
that alone did not make the SDL/EGL path work.

## Strongest diagnostic result

A capture of RetroRun's final OpenGL framebuffer, taken immediately before
`SDL_GL_SwapWindow`, was complete and correct through the last row. Screenshots
created by RetroRun were also clean while the physical LCD showed the damaged
band.

This places the defect after RetroRun has composed the game and overlays. The
remaining path is:

```text
final OpenGL framebuffer
    -> EGL/GBM window surface
    -> SDL KMSDRM swap/page flip
    -> Rockchip DRM/VOP scanout
    -> LCD panel
```

It also explains why moving individual overlays did not solve the full-screen
menu clipping: both game and menu are already correct before the final swap.

## Tests performed

### Overlay placement and size

The SDL overlay coordinates were logged and adjusted. Bottom overlays were
placed flush with logical row 480; the FPS rectangle was moved inward and made
larger on handhelds.

Result: FPS readability improved, but the lower band and menu clipping did not.
Overlay coordinates alone are not the root cause.

### Window, drawable and viewport dimensions

SDL window size, drawable size, source size and final viewport were explicitly
queried and logged. They all resolved to 640x480. The final presenter reset the
viewport to the complete drawable.

Result: no evidence of a 480/640 swap or an incorrect logical drawable size.

### OpenGL scissor and core state

Flycast changed its scissor box during rendering, including unusual values such
as `0,-480 640x960`. RetroRun disabled or replaced the core scissor state before
the final composition, and the final framebuffer capture was correct.

Result: retained core scissor state is not responsible for the physical band.

### Frontend FBO versus default framebuffer

The normal frontend-FBO path and the experimental
`RETRORUN_SDL_DEFAULT_FRAMEBUFFER=1` path were both tested.

Result: the defect remained. The default-framebuffer path additionally caused
startup flicker. The frontend FBO itself is not the cause.

### GPU completion before swap

`glFinish()` was forced before `SDL_GL_SwapWindow`.

Result: no visible change. A missing simple GL completion barrier is unlikely.

### VSync/page-flip pacing

The original runs used `retrorun_vsync=false`. A separate configuration was
then launched from `/tmp` so that the repository-local configuration could not
override it. The log explicitly confirmed:

```text
retrorun_vsync: true
```

Result: no visible improvement. Disabled VSync is not the root cause.

### Direct scanout and frameskip settings

The failure was reproduced with `retrorun_drm_direct_scanout=false` and with
adaptive frameskip both enabled and disabled in different runs.

Result: neither setting explains the corruption.

### EGL colour configuration

Both the original RK3566 colour request and an 8/8/8/8 request were tested:

```text
5/6/5/0
8/8/8/8
```

Result: SDL still scanned out XR24 and the visible problem remained. Merely
changing the EGL channel-size request is insufficient.

### Alternative GBM/DRM formats

Temporary `LD_PRELOAD` instrumentation was used only for diagnosis; it was not
installed into the OS or proposed as production code.

- GBM RGB888 was reported as supported, but the Mali EGL window surface could
  not be created with that GBM/EGL combination.
- GBM RGB565 could create an EGL surface when a 5/6/5/0 configuration was
  forced, but SDL could not create a usable DRM framebuffer for scanout.
- This agrees with the native GO2 implementation: it renders using an EGL
  format accepted by the GPU, then converts into a separate RG24/RGB888
  presenter surface rather than directly scanning out the EGL render target.

Result: changing only SDL's GBM format cannot reproduce the working native
RG24 path.

### 640x960 GBM allocation

SDL's GBM allocation was temporarily changed from 640x480 to 640x960 to match
the fbcon virtual height. DRM accepted XR24 buffers with:

```text
size=640x960
pitch=2560
object size=2,457,600
visible CRTC area=640x480
```

The display was black. A second test created a 640x480 DRM framebuffer view at
offset 1,228,800, targeting the other half of the allocation. It was accepted
by DRM but the display remained black.

Result: the fbcon virtual height cannot be copied directly into SDL's EGL/GBM
path. A simple doubled height or half-buffer offset is not a valid fix.

### Audio warnings

PipeWire and RTKit warnings were present in some logs, but audio worked and the
same visual failure occurred independently of those messages.

Result: the PipeWire/RTKit warnings are unrelated to this display defect.

## Causes reasonably excluded

The collected evidence allows us to exclude, for this RG353M test case:

- incorrect RetroRun logical resolution or aspect-ratio calculation;
- an SDL window/drawable size mismatch;
- overlay placement as the primary cause;
- the FPS overlay itself;
- Flycast's scissor state leaking into the final composition;
- corruption inside RetroRun's final OpenGL framebuffer;
- the frontend FBO versus framebuffer-zero choice;
- a missing `glFinish()` before swap;
- disabled VSync;
- adaptive frameskip;
- RetroRun DRM direct scanout, which was disabled;
- PipeWire/RTKit audio warnings;
- a simple 640x960 allocation or second-half offset workaround.

These conclusions apply to the tested software stack. They should be
revalidated if SDL, the kernel, Mali userspace or the core changes.

## Remaining likely area

The unresolved difference is the final KMSDRM scanout format and its interaction
with the dArkOS RK3566 graphics stack:

- SDL 2.32.10 KMSDRM creates an ARGB/XRGB8888 GBM window surface and presents
  XR24 directly;
- the native GO2 backend produces a separate RG24 presenter framebuffer and
  that output is correct;
- the OpenGL image is known to be correct before SDL swaps it;
- RG351V reportedly works with the SDL2 backend on AmberELEC, so a comparison
  of distribution SDL patches, kernel DRM/VOP behaviour, Mali libraries and
  framebuffer formats is important.

This does not yet prove whether the fault is in SDL KMSDRM, the dArkOS SDL
patches, GBM/Mali buffer layout, or the Rockchip DRM/VOP handling of XR24. It
does show that further changes to RetroRun overlay coordinates are unlikely to
help.

## Recommended next steps

1. Build a minimal SDL2/KMSDRM OpenGL ES test program that only clears distinct
   horizontal colour bands and swaps. If it reproduces the corruption,
   RetroRun and Flycast are completely removed from the failing test case.
2. Run that same binary on RG353M/dArkOS and RG351V/AmberELEC.
3. On both devices record SDL version/build patches, kernel version, EGL/GBM
   vendor, DRM mode, active framebuffer format, pitch and object size.
4. Compare dArkOS' SDL KMSDRM patches with AmberELEC's SDL package rather than
   continuing to adjust RetroRun UI coordinates.
5. If XR24 scanout is confirmed broken only on this stack, choose between:
   - fixing the distribution's SDL/KMSDRM or Rockchip DRM integration; or
   - adding an explicitly gated RetroRun presenter which converts the final
     GPU image to an RG24 scanout surface, preferably with RGA, following the
     already working GO2 presenter design.
6. Investigate the shutdown segmentation fault separately; do not mix it with
   the display investigation.

The RG24 conversion/presenter option is larger than a coordinate fix and must
be measured for latency and performance. It should not be implemented until a
minimal SDL program confirms that the failure exists outside RetroRun.

## Device commands useful for the comparison

```sh
cat /var/run/drmMode 2>/dev/null

for f in /sys/class/drm/card*-*/modes; do
    echo "== $f =="
    nl -v0 -ba "$f"
done

cat /sys/class/graphics/fb0/modes
cat /sys/class/graphics/fb0/virtual_size

strings /usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0 | grep -i kmsdrm
uname -a
ldd --version | head -n 1
```

Use the distribution's available DRM inspection tool (`drm_info`, `modetest`
or an equivalent helper) while RetroRun is running to capture the active
framebuffer format and pitch.
