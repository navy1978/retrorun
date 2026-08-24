# Jet Grind Radio on RG351MP/dArkOS

This session promoted Dreamcast Product Number `MK-51058` from conservative
retail coverage to a manually validated low-end profile. Tests used RetroRun
3.1.2, the Flycast `0.1catalog-20260745` core, the GO2/DRM backend, the RK3326
Mali build, CPU performance mode and a fixed in-game 3D savestate.

The savestate and game image are intentionally excluded. Every measurement
loaded the same local automatic state, warmed up for three seconds and then
ran 600 core frames. The core reported a 60 Hz declared rate, while the
baseline delivered about 26 presented callbacks/s; the device's visible game
rate was consistent with the title's observed 30 fps ceiling.

## Final results

| Configuration | Runs | Presented frames/s | Active p95 | Active p99 | Audio faults |
| --- | ---: | ---: | ---: | ---: | ---: |
| Conservative baseline | 3 | 25.99 | 61.72 ms | 79.93 ms | 0 |
| Fast depth | 3 | 27.37 | 62.80 ms | 71.70 ms | 0 |
| Fast depth + opaque merge | 3 | 27.86 | 59.70 ms | 73.61 ms | 0 |
| Embedded-catalog verification | 1 | 27.70 | 60.12 ms | 73.16 ms | 0 |

The final combination improved presented throughput by 7.18% over baseline.
All nine final runs recorded zero buffer underruns, overruns and dropped audio
frames. Manual gameplay review approved fast depth alone and the final
fast-depth/opaque-merge combination, including character outlines, shadows,
graffiti, foreground geometry and HUD.

The final device-side verification used the newly built binary and deliberately
started from conflicting global core options. Catalog version `20260824`
recognized `MK-51058`, resolved the requested `best_performance` mode to the
validated profile, and applied the approved settings as transient overrides.
It presented 599 frames in 21.623 seconds (27.70 frames/s), with no audio
underruns, overruns or dropped frames. Its raw result is
`jet-catalog-final.json`.

## Screening decisions

| Isolated change | Presented frames/s | Change vs baseline | Decision |
| --- | ---: | ---: | --- |
| `fast_depth=menu_guarded_shadow_safe` | 26.29 | +1.18% | Rejected: no useful tail-latency gain |
| `alpha_sorting=per-strip` | 21.62 | -16.79% | Rejected |
| `opaque_strip_merge=enabled` | 26.55 | +2.17% | Retained for the final combination |
| `translucent_strip_merge=menu_guarded` | 25.88 | -0.43% | Rejected |
| `adjacent_state_elision=enabled` | 25.96 | -0.12% | Rejected |
| `fast_depth=enabled` | 27.37 | +5.33% | Retained and manually approved |

The validated profile keeps per-triangle alpha sorting, mipmapping and fog;
disables translucent merging, adjacent-state elision and frame skipping; uses
accurate core audio with 32 AICA cycles and the `lowend_stable_96` frontend
audio preset; and selects a D24S0 EGL configuration.

Raw JSON measurements are split between `screening/` and `finals/`; the final
embedded-catalog confirmation is `jet-catalog-final.json`. The exact
configuration files used for each candidate are in `configs/`.

Command shape:

```sh
./retrorun-darkos-mali \
  -c ./candidate.cfg --triggers \
  -s /roms/dreamcast -d /roms/bios \
  --benchmark 60 --benchmark-warmup 3 --benchmark-frames 600 \
  --benchmark-json ./result.json \
  ./flycast_libretro.so "/roms/dreamcast/Jet Grind Radio (USA)/Jet Grind Radio (USA).cue"
```
