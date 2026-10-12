# UnofficialOS

The companion package provides Retrorun SDL2 for Dreamcast on RK3326,
RK3566, RK3566-BSP and RK3399 images. Cores: Flycast, Flycast2021 and
flycast2021le. Existing emulator defaults are retained.

- Video: SDL selects the backend; no UnofficialOS-specific rendering path.
- Input: dedicated positional SDL mappings, separate from the global database.
- Audio: SDL PulseAudio output through the distribution's PipeWire stack.
- Configuration: `/storage/.config/retrorun/retrorun.cfg`.
- EmulationStation options: frontend settings and supported Flycast core options.
- Cores: `/tmp/cores`; BIOS: `/storage/roms/bios`.
- Saves: `/storage/roms/savestates/retrorun/dreamcast`.
- Build: distribution compiler, preprocessor and linker flags are preserved.

Tested only on RG353M, UnofficialOS 20260624, SDL 2.32.10, KMSDRM and
Mali-G52 GLES 3.2:

- FCEUmm: software video, NES gameplay and audio.
- Flycast2021 and flycast2021le: ChuChu Rocket gameplay, controls, clean audio
  and Select + Start exit through `runemu.sh`.
- Current Flycast: gameplay works; audio also crackles in RetroArch.
- flycast2021le retest passed after rebooting from an initial black screen;
  the cause of that first attempt was not established.

Native Cortex-A35/A55 package builds, macOS/GO2 compilation and virtual SDL
input checks passed. Other hardware and a full firmware build are untested.
