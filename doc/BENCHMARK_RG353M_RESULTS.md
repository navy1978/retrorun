# RetroRun integrated benchmark — RG353M

- Date: 2026-07-20
- Device: Anbernic RG353M / RK3566 / dArkOS
- Core: Flycast `0.1 4c293f30`
- Content: `Soul Calibur (USA)[DCCM].cdi`
- Display: DSI `640x480`, CPU governor `ondemand`

## Outcome

SDL2 is marginally faster than GO2 for this game, but neither backend meets
all of the strict acceptance criteria in the runbook:

- GO2 has correct presentation accounting and no dropped audio frames, but
  every final run records 6 real OpenAL underruns.
- SDL2 has no underruns or dropped audio frames and is about 2% faster, but
  its producer repeatedly waits because the SDL audio queue exceeds the 80 ms
  target. The maximum observed queue is 4255 frames, about 96.5 ms at 44.1 kHz.
- SDL2 also retains the previously documented RG353M bottom-band rendering
  defect, so it cannot yet be called the safest visual backend.

For normal RG353M use today, the least risky configuration is GO2 standard
presentation with stable audio buffering, main-thread audio/video, direct
scanout off, declared-FPS pacing on, and overlays off. For raw performance,
SDL2 with VSync on is slightly ahead, provided the known rendering issue and
extra audio-queue latency are acceptable.

## Final 60-second repetitions

Each run used a 5-second warm-up followed by 60 seconds of measurement. No
SRAM or savestate was read or written by benchmark mode.

| Backend/run | Frames | Missed deadlines | Active p95 | Active p99 | Audio underrun | Queue pressure/overrun | Drops |
|---|---:|---:|---:|---:|---:|---:|---:|
| GO2 #1 | 2990 | 2283 | 32.614 ms | 49.008 ms | 6 | 0 | 0 |
| GO2 #2 | 3030 | 2274 | 32.001 ms | 48.181 ms | 6 | 0 | 0 |
| GO2 #3 | 2967 | 2223 | 34.580 ms | 49.189 ms | 6 | 0 | 0 |
| SDL2 #1 | 3046 | 2160 | 33.302 ms | 48.821 ms | 0 | 2705 | 0 |
| SDL2 #2 | 3060 | 2510 | 32.254 ms | 48.716 ms | 0 | 2719 | 0 |
| SDL2 #3 | 3061 | 2374 | 32.350 ms | 48.888 ms | 0 | 2702 | 0 |

Three-run averages:

| Backend | Frames | Missed deadlines | Active p95 | Active p99 | Audio result |
|---|---:|---:|---:|---:|---|
| GO2 | 2995.7 | 2260.0 | 33.065 ms | 48.793 ms | 6 underruns/run, no drops |
| SDL2 | 3055.7 | 2348.0 | 32.635 ms | 48.808 ms | no underruns/drops; 2708.7 queue-pressure events/run |

All six final processes exited with status 0 and had exact presentation
accounting (`presented_frames == video_callbacks == direct + fallback`).

## 30-second screening matrix

| Configuration | Frames | p95 | p99 | Audio/result |
|---|---:|---:|---:|---|
| GO2 standard | 1305 | 37.288 | 81.758 | 10 underruns |
| GO2 direct scanout | 557 | 66.748 | 103.203 | 211 underruns; rejected |
| GO2 threaded audio | 1294 | 37.577 | 60.239 | 735 drops, 805 backpressure; rejected |
| GO2 stable audio | 1555 | 33.080 | 49.830 | 6 underruns; best GO2 base |
| GO2 stable + threaded video | 1578 | 33.191 | 51.171 | one extra presented frame; accounting rejected |
| GO2 throughput | 1559 | 35.448 | 53.135 | no benefit over paced mode |
| GO2 overlays | 1540 | 31.360 | 48.186 | more missed deadlines; not selected |
| SDL2 requested VSync off | 1599 | 30.754 | 47.726 | VSync was still applied by KMSDRM |
| SDL2 VSync on | 1615 | 28.504 | 50.384 | best SDL2 base |
| SDL2 threaded audio | 1630 | 26.654 | 47.227 | 735 drops, 1447 backpressure; rejected |
| SDL2 throughput | 1598 | 31.280 | 49.244 | no benefit over paced mode |
| SDL2 overlays | 1595 | 29.770 | 49.889 | no material benefit |

The GO2 direct path did activate for 550 of 557 presented frames; its poor
result is therefore a real incompatibility/performance result for this Flycast
build, not a failure to enter direct scanout. SDL/KMSDRM reported
`vsync_applied=true` for both requested VSync states.

## Environment and artifacts

- Source baseline: `37f7722c5211f1eeaa26d7ef09880bffb402f1e0`
- Implementation commits:
  - `3456ffb` — integrated benchmark mode
  - `69481ef` — deterministic worker/benchmark tests
  - `66fbf1f` — concurrent Flycast audio and legacy shutdown hardening
- GO2 binary SHA-256:
  `cb26eb6ab2b6532bc533e92c714e07d14c4d1c30c3d3014be31aaae882616e65`
- SDL2 binary SHA-256:
  `7d9531acadf82a7a2ac31d904aa775625ba23ed6fd7005cc8be6da4bdee288ed`
- Target configuration SHA-256:
  `b3489b9affc820354a694a49fa128830d4d8c60e5296ba9f83f376c8f5ae1f39`
- Final deployment backup suffix: `20260720-150309`
- Final paths: `/home/ark/retrorun/retrorun_go` and
  `/home/ark/retrorun/retrorun_sdl2`
- Test temperature range: 43.125–50.625 °C; no thermal throttling was logged.
- Battery was monitored throughout and finished at 18%, discharging.
- An unrelated `fluidsynth` process already owned an audio device before the
  tests and was deliberately left untouched.
- Kernel logs contain the existing `rga2: unknown ioctl cmd!` warning and no
  new DRM, audio, or thermal failures.

The exact JSON, stdout/stderr, metadata, and kernel tails for all 18 benchmark
runs are retained outside Git. This document keeps the compact, reviewable
results required to reproduce and compare the selected configurations.

## Validation

Completed successfully:

- `make test`
- `make test-asan`
- `make test-tsan`
- `make config=debug macos-sdl2`
- VM release builds for `PLATFORM=linux-go2` and `PLATFORM=linux-sdl2`
- `git diff --check`

Device ASan found and led to a fix for concurrent Flycast audio callbacks.
This legacy Flycast then proved faulty in `retro_unload_game()` and its shared
object finalizers. Benchmark mode performs the complete frontend drain and
deinitialization, writes the result, and lets process exit unload this specific
legacy core without calling its crashing/hanging finalizer path. Other cores
retain the normal synchronous lifecycle.

Unrelated working-tree files, screenshots, decorations, the target
`retrorun.cfg`, and the existing `fluidsynth` process were preserved.
