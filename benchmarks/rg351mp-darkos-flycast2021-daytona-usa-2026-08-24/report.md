# Daytona USA on RG351MP/dArkOS

This session validated the North American Dreamcast release of Daytona USA,
Product Number `MK-51037`, on an RG351MP running dArkOS. Tests used RetroRun
3.1.2, Flycast `0.1catalog-20260745`, the GO2/DRM backend, the RK3326 Mali
build, CPU performance mode and one fixed in-race savestate.

The game image and savestate are intentionally excluded. Every comparable
final measurement loaded the same automatic state, warmed up for three
seconds and then ran 600 core frames. Every retained run presented 600 of 600
frames.

## Final results

| Configuration | Runs | Presented frames/s | Active p95 | Active p99 | Audio faults |
| --- | ---: | ---: | ---: | ---: | ---: |
| Conservative baseline | 3 | 16.20 | 89.27 ms | 107.61 ms | 0 |
| Per-strip alpha + opaque-strip merge | 3 | 23.75 | 58.52 ms | 67.97 ms | 0 |

The validated combination improved presented throughput by 46.6%, reduced
active-frame p95 by 30.75 ms and recorded zero buffer underruns, overruns or
dropped audio frames. Manual race review first approved per-strip alpha on its
own and then approved the final combination, including cars, reflections,
trackside geometry and HUD.

The profile keeps mipmapping, fog, accurate audio, 32 AICA cycles and disabled
fast depth. It changes alpha sorting from accurate per-triangle mode to the
faster per-strip path and enables opaque-strip merging. Translucent merging,
texture reuse and adjacent-state elision remain disabled.

The European and North American retail releases share `MK-51037`; the Japanese
release `HDR-0106` remains a conservative baseline because it was not tested
in this session.

## Embedded catalog verification

Catalog version `20260826` was compiled into a fresh RK3326 Mali binary with
SHA-256
`84b1953ad10341dadefbe853b2b1e67c742e8eb88621e31cace1ee9882d87df8`.
The executable depends on the vendor-style unversioned `libEGL.so` and requires
at most GLIBC 2.27.

The device test deliberately supplied conflicting global values: accurate
per-triangle alpha, enabled fast depth, disabled opaque merging, guarded
translucent merging, texture reuse, fast audio, 16 AICA cycles and D24S8. The
built-in catalog recognized `MK-51037`, selected the validated fallback for
`best_performance` and returned every retained value to Flycast. Comparing the
48 core options observed through `GET_VARIABLE` found no difference from the
explicit final profile.

An alternating 300-frame A/B test used the same binary, state and performance
governor:

| Pair | Explicit profile | Built-in catalog | Difference |
| --- | ---: | ---: | ---: |
| 1 | 24.07 frames/s | 24.05 frames/s | -0.08% |
| 2 | 25.73 frames/s | 25.84 frames/s | +0.42% |
| Mean | 24.90 frames/s | 24.94 frames/s | +0.18% |

The first pair recorded one underrun on both paths and the second pair recorded
none on either path. This matched result demonstrates that the catalog path is
equivalent to the explicit configuration. An earlier non-interleaved catalog
group showed lower transient throughput and seven total underruns; it is kept
in `raw/` but excluded from the performance claim because the paired test
showed the variation was not caused by catalog overrides.

## Screening decisions

All screening runs measured the same state for 300 core frames after a
three-second warmup.

| Isolated change | Presented frames/s | Decision |
| --- | ---: | --- |
| Conservative screening baseline | 16.64 | Reference |
| `alpha_sorting=per-strip` | 23.08 | Retained and manually approved |
| `opaque_strip_merge=enabled` | 17.58 | Retained for combination testing |
| `fast_depth=enabled` | 17.60 | Not retained; slower than the approved combination and higher visual risk |
| `fast_depth=vertex_fast_log` | 17.18 | Rejected: smaller gain |
| `mipmapping=disabled` | 16.66 | Rejected: no gain |
| `fog=disabled` | 16.79 | Rejected: no meaningful gain |
| `audio_mixer=fast` | 16.45 | Rejected: slower and less accurate |
| `aica_arm_cycles=16` | 16.92 | Rejected: marginal gain does not justify lower accuracy |
| `adjacent_state_elision=enabled` | 16.80 | Rejected: one audio underrun and no useful gain |
| `translucent_strip_merge=menu_guarded` | 16.76 | Rejected: no meaningful gain |
| `texture_storage_reuse=enabled` | 16.72 | Rejected: no meaningful gain |

Per-strip alpha plus fast depth reached 24.65 frames/s in the 300-frame screen.
Per-strip alpha plus opaque merging was faster at 26.11 frames/s and passed
manual review, so fast depth was unnecessary. The longer three-run final is
the authoritative result. Raw JSON for retained and rejected candidates is in
`raw/`.

## Reproduction

Command shape:

```sh
./retrorun-rk3326-mali \
  -c ./retrorun-daytona-usa-MK-51037.cfg --triggers \
  -s /roms/dreamcast -d /roms/bios \
  --benchmark 60 --benchmark-warmup 3 --benchmark-frames 600 \
  --benchmark-json ./result.json \
  ./flycast_libretro.so \
  "/roms/dreamcast/Daytona USA (USA)/Daytona USA (USA).gdi"
```
