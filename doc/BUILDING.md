# Building RetroRun

[Back to README](../README.md)

## Backends and build outputs

| Host or target | Backend | Build command | Output |
| --- | --- | --- | --- |
| Anbernic Linux, default | Native GO2/DRM | `make config=release` | `retrorun` |
| Linux handheld or desktop | SDL2 + OpenGL ES | `make sdl2 config=release` | `retrorun-sdl2` |
| macOS | SDL2 + Desktop OpenGL | `make` | `retrorun` |

On Linux, the GO2 backend intentionally remains the default for compatibility
with existing AmberELEC packages and supported Anbernic devices. Building the
SDL2 target produces a separate executable and does not replace `retrorun`.

Clone the repository first:

```sh
git clone https://github.com/navy1978/retrorun.git
cd retrorun
```

Initialize the bundled dependencies:

```sh
git submodule update --init --recursive
```

## Native GO2/DRM backend

On a supported Linux toolchain, the default command builds the historical
Anbernic backend:

```sh
make config=release
```

The native build uses DRM/GBM, EGL/OpenGL ES, RGA, evdev, ALSA and OpenAL. It is
normally cross-compiled inside the AmberELEC or device-distribution build
environment, where those libraries and their target headers are available.

The backend can also be selected explicitly:

```sh
make go2 config=release
```

### RK3326 Mali compatibility build

ArkOS/dArkOS and AmberELEC normally expose the unversioned EGL, GLES and GBM
linker names through the same proprietary RK3326 Mali GBM library. Building on
a generic Linux VM can instead record versioned GLVND/Mesa dependencies such as
`libEGL.so.1`, causing EGL initialization to fail on an otherwise supported
handheld.

The helper can download the dArkOS RK3326 blob pinned to a known source commit,
verify its SHA-256 and retain it outside the repository in the user cache:

```sh
tools/build-rk3326-mali.sh --download-mali
```

The download is never implicit. For offline builds, pass an existing AArch64
RK3326 Mali GBM blob instead:

```sh
tools/build-rk3326-mali.sh \
  --mali-lib /path/to/libmali-bifrost-g31-rxp0-gbm.so
```

The output is `dist/retrorun-rk3326-mali`. The helper temporarily presents the
blob as `libEGL.so`, `libGLESv2.so` and `libgbm.so`, performs a clean release
GO2 build, and then removes all temporary links and intermediate build files.
It rejects GLVND/Mesa-style `DT_NEEDED` entries and prints the highest GLIBC
version required by the executable.

To enforce the ABI ceiling of the oldest target distribution, pass its GLIBC
version explicitly:

```sh
tools/build-rk3326-mali.sh \
  --download-mali \
  --max-glibc 2.31
```

The version above is only an example; determine the actual ceiling on the
oldest supported image. Passing the ELF checks prevents the known EGL loader
mismatch, but a release should still be tested on both ArkOS/dArkOS and
AmberELEC before being labelled as a universal binary.

## Linux SDL2/KMSDRM backend

Required target dependencies are:

- a C++20 compiler;
- SDL2 development files;
- libpng and pkg-config;
- EGL, OpenGL ES 3 headers and `libGLESv2`;
- an SDL2 build with KMSDRM support when running without X11 or Wayland.

Build the alternative backend with either command:

```sh
make PLATFORM=sdl2 config=release
```

```sh
make sdl2 config=release
```

Both commands create `retrorun-sdl2`. The build variables `CC`, `CXX`, `SDL_CONFIG`,
`PKG_CONFIG`, `SDL_CFLAGS`, `SDL_LIBS`, `PNG_CFLAGS`, `PNG_LIBS`,
`GLES_CFLAGS` and `GLES_LIBS` can be overridden for a cross-compilation
sysroot. `CPPFLAGS`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS` and `LDLIBS` supplied by a
package builder are preserved; required frontend defines and include paths
remain enabled. Use the target's pkg-config environment, including
`PKG_CONFIG_SYSROOT_DIR` and `PKG_CONFIG_LIBDIR`. For example:

```sh
make sdl2 config=release \
  CXX=aarch64-linux-gnu-g++ \
  CC=aarch64-linux-gnu-gcc \
  SDL_CONFIG=/path/to/sysroot/usr/bin/sdl2-config \
  PKG_CONFIG=/path/to/target-pkg-config
```

Do not copy a macOS or x86-64 Linux executable to an ARM handheld. RetroRun,
the libretro core and all linked libraries must be built for the target CPU and
ABI.


See [UnofficialOS on RG353M](../doc/UNOFFICIALOS_RG353M.md) for the tested SDL2
platform configuration and integration details.

## Hybrid GO2 video/input with selectable audio

Build the hybrid executable with:

```sh
make PLATFORM=linux-go2-hybrid config=release
```

The output is `retrorun-hybrid`. See [Audio and benchmarking](../doc/AUDIO.md)
for provider selection, buffering and benchmark settings.


## AmberELEC packaging

AmberELEC already builds SDL2 with KMSDRM and OpenGL ES. To include both
RetroRun variants in an AmberELEC image, its RetroRun package must:

1. retain the existing default `make config=release` native build;
2. add SDL2 to the package dependencies;
3. run `make PLATFORM=sdl2 config=release` using the AmberELEC target sysroot;
4. install `retrorun-sdl2` alongside `/usr/bin/retrorun`.

Keeping the two executable names separate makes it possible to select the
backend per system or per game without changing the existing launcher.

## ArkOS and dArkOS

ArkOS historically includes `retrorun` and `retrorun32`. The SDL2 variant can
be built using a matching 32-bit or 64-bit distribution toolchain. Check the
architecture of the selected core before choosing the compiler. Since device
images can ship different SDL2 builds, confirm that the target SDL2 provides
the `kmsdrm` video driver.

## macOS SDL2 backend

Install SDL2 and libpng with Homebrew:

```sh
brew install sdl2 libpng
make
```

This creates `retrorun` using SDL2 and Desktop OpenGL. The default window is
960x720 and can be overridden with:

```sh
RETRORUN_WINDOW_WIDTH=1280 RETRORUN_WINDOW_HEIGHT=960 ./retrorun ...
```

## GitHub Actions binary builds

The **Build RetroRun binaries** workflow can be started manually from the
repository's **Actions** tab. It produces three downloadable artifacts:

- `retrorun-linux-go2-aarch64` for the native GO2/DRM backend;
- `retrorun-linux-sdl2-aarch64` for Linux handhelds using SDL2/KMSDRM;
- `retrorun-macos-sdl2-arm64` for Apple Silicon macOS.

The macOS and SDL2 Linux jobs run on GitHub-hosted Apple Silicon and ARM64
runners. The GO2 job runs on a standard GitHub Ubuntu runner but compiles
inside the AmberELEC build container, using the same RG351P ARM64 toolchain,
`librga` and target sysroot as the distribution. The initial GO2 build may take
longer while that environment is prepared; its toolchain, sources and compiler
cache are retained between runs to speed up later builds. No self-hosted runner
is required for this public repository.

When launching the workflow, select only the artifacts needed for the test.
After it completes, open the run summary and download the corresponding
artifact from **Artifacts**. Artifacts are kept for 30 days.
