# Credits and third-party libraries

[Back to README](../README.md)

## Artwork attribution

On-screen interface icons are derived from
[Streamline Core Line Free](https://www.streamlinehq.com/icons/core-line-free),
licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
Icons by [Streamline](https://streamlinehq.com).

## History and credits

RetroRun was initially developed by OtherCrashOverride until 2020. Development
has been continued by navy1978 since 2021. This version integrates and extends
[libgo2](https://github.com/OtherCrashOverride/libgo2) and
[rg351p-js2box](https://github.com/lualiliu/RG351P_virtual-gamepad).

### Third-party libraries

- [rcheevos](https://github.com/RetroAchievements/rcheevos), integrated in
  `deps/rcheevos`, provides the RetroAchievements runtime and hashing client
  under the [MIT License](../deps/rcheevos/LICENSE).
- [SDL2](https://www.libsdl.org/), [libpng](http://www.libpng.org/pub/png/libpng.html),
  [libcurl](https://curl.se/libcurl/) and [zlib](https://zlib.net/) provide the
  portable window, input, audio, PNG, network and compression facilities used
  by the SDL2 and macOS builds.
- The native GO2/DRM backend additionally uses Linux system libraries for
  [DRM](https://dri.freedesktop.org/libdrm/)/GBM, EGL/OpenGL ES, RGA, ALSA,
  [OpenAL Soft](https://openal-soft.org/) and
  [libevdev](https://www.freedesktop.org/wiki/Software/libevdev/).

The optional downloadable screen decorations are sourced from
[libretro/common-overlays](https://github.com/libretro/common-overlays) and
are licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
RetroRun records this attribution beside each downloaded artwork and does not
bundle the artwork in the source repository.

Thanks to Cebion, Christian_Haitian, dhwz, madcat1990, pkegg, superdealloc and
Szalik for their contributions and support.
