# RG353M dArkOS stack comparison

- Date: 2026-07-27
- Device: Anbernic RG353M / RK3566 / dArkOS
- Method: 30-second warm-up and 90-second fixed measurement
- Runtime: performance CPU/GPU/DMC governors, `nice -n -19`

## Results

| Game / stack | Average FPS | Core avg | Video avg |
| --- | ---: | ---: | ---: |
| Sonic — current RetroRun, generic ARM64 Flycast | 41.886 | 15.177 ms | 7.886 ms |
| Sonic — current RetroRun, RK3566/Cortex-A55 Flycast | 47.287 | 12.505 ms | 7.648 ms |
| Sonic — current RetroRun, original dArkOS Flycast | 47.511 | 13.585 ms | 6.029 ms |
| Sonic — instrumented dArkOS 2.7.7, original dArkOS Flycast | 47.722 | 20.948 ms | 8.695 ms |
| Sonic — instrumented dArkOS 2.7.7, profiled dArkOS Flycast | 47.829 | 20.901 ms | 8.757 ms |
| Soul Calibur — instrumented dArkOS 2.7.7, original dArkOS Flycast | 57.387 | 17.418 ms | 7.137 ms |
| Soul Calibur — current RetroRun, RK3566 Flycast and catalog | 58.860 | 9.387 ms | 7.590 ms |
| Sonic Adventure — dArkOS stock stack, fixed gameplay state | 25.592 | — | — |
| Sonic Adventure — current RetroRun, RK3566 Flycast and catalog | 27.965 | — | — |
| Power Stone 2 — dArkOS stock stack, fixed combat state | 56.166 | — | — |
| Power Stone 2 — current RetroRun, RK3566 Flycast and catalog | 58.961 | — | — |

Core-time fields from the two RetroRun generations are not directly
comparable. The dArkOS 2.7.7 probe measures `retro_run` including its nested
video and audio callbacks. Average FPS uses the same fixed measurement window
and is directly comparable.

## Conclusions

- The generic ARM64 Flycast build caused the RG353M regression.
- The RK3566/Cortex-A55 target improves Sonic by 12.9% over that build.
- Current and dArkOS RetroRun are statistically equivalent when loading the
  same original dArkOS core; replacing the frontend did not cause the slowdown.
- The current Soul Calibur stack reaches 58.860 FPS, 2.6% above the measured
  original dArkOS stack.
- The RG353M Sonic Adventure catalog profile reaches 27.965 FPS, 9.3% above
  stock, improves active-frame p95 from 69.22 to 64.11 ms and eliminates the
  two stock audio underruns/empty-queue events in the fixed gameplay state.
- The RG353M Power Stone 2 profile reaches 58.961 FPS, 5.0% above stock, with
  every frame presented and no audio faults in the fixed combat state.
- The old menu's 51 FPS for Sonic was an upward-biased average of partial
  per-frame estimates. The fixed-window probe measured 47.722 FPS.

The comparison established the build-target regression. A release acceptance
run should repeat each stack with identical overlay and audio-buffer settings;
the JSON output records those settings to prevent accidental comparisons.

## dArkOS diagnostic instrumentation

A detached worktree based on the dArkOS-pinned RetroRun revision adds:

- `--benchmark`, `--benchmark-warmup` and `--benchmark-json`;
- frame, core, video and audio timings;
- raw libretro performance-counter collection;
- no SRAM or save-state writes during a benchmark.

A second detached worktree based on dArkOS Flycast revision `4c293f30` adds
AICA, emulator-thread, PVR process, PVR render and render-wait probes. These
diagnostic changes are kept outside release builds.

The 90-second Sonic internal profile measured:

| Stage | Total | Average |
| --- | ---: | ---: |
| Emulator-thread CPU | 89.250 s | — |
| AICA update | 46.938 s | 0.384 ms/update |
| Render wait | 51.819 s | 12.037 ms/frame |
| PVR process | 5.250 s | 1.224 ms/frame |
| PVR render | 30.978 s | 7.222 ms/frame |

AICA accounts for about 52.6% of emulator-thread CPU time in this workload.
Rendering is the other large optimization target.

## Distribution requirement

RG3566 releases must use a clean Cortex-A55 build of Flycast:

```sh
make clean
make platform=RK3566 FORCE_GLES=1 HAVE_OPENMP=1 HAVE_LTCG=0 -j$(nproc)
```

After applying a rumble patch, clean and run the same target again. Rebuilding
with `platform=goadvance` without cleaning can mix object files compiled with
different platform flags.
