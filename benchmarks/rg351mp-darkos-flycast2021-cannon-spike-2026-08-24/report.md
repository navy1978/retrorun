# Cannon Spike on RG351MP/dArkOS

This session validated the North American Dreamcast release of Cannon Spike,
Product Number `T1215N`, on an RG351MP running dArkOS. Tests used RetroRun
3.1.2, Flycast `0.1catalog-20260745`, the GO2/DRM backend, the RK3326 Mali
build, CPU performance mode and one fixed in-game 3D savestate.

The game image and savestate are intentionally excluded. Every comparable
measurement loaded the same automatic state, warmed up for three seconds and
then ran 600 core frames. All retained runs presented 600 of 600 frames.

## Final results

| Configuration | Runs | Presented frames/s | Active p95 | Active p99 | Audio faults |
| --- | ---: | ---: | ---: | ---: | ---: |
| Conservative baseline | 3 | 33.56 | 39.45 ms | 55.15 ms | 0 |
| Opaque-strip merge | 3 | 38.00 | 34.67 ms | 50.57 ms | 0 |
| Embedded-catalog verification | 3 | 37.97 | 35.11 ms | 48.36 ms | 0 |

Opaque-strip merging improved presented throughput by 13.2%, reduced p95
active frame time by about 4.8 ms and recorded zero buffer underruns, overruns
or dropped audio frames. Manual gameplay review approved the opaque-only
configuration.

The final RK3326 Mali build (`08a1a6c240e37b535e11a2e1553734a811a992f79ffa415b68b41748a2587a2e`)
then repeated the test from deliberately conflicting global options. Built-in
catalog version `20260825` recognized `T1215N`, selected the validated fallback
for `best_performance`, disabled fast depth, enabled opaque merging and restored
accurate alpha/audio settings. Three catalog runs averaged 37.97 frames/s with
600 of 600 frames presented and no audio faults.

## Screening decisions

| Isolated change | Presented frames/s | Decision |
| --- | ---: | --- |
| `fast_depth=enabled` | 38.43 | Rejected: obvious graphical artifacts |
| `fast_depth=vertex_fast_log` | 37.87 | Rejected with the unsafe depth family |
| `opaque_strip_merge=enabled` | 38.07 | Retained and manually approved |
| `mipmapping=disabled` | 33.43 | Rejected: no gain |
| `fog=disabled` | 33.70 | Rejected: no meaningful gain |
| `audio_mixer=fast` | 33.58 | Rejected: no gain |
| `aica_arm_cycles=16` | 33.90 | Rejected: marginal result does not justify lower accuracy |
| `adjacent_state_elision=enabled` | 33.53 | Rejected: no gain |
| `translucent_strip_merge=menu_guarded` | 33.79 | Rejected: no meaningful gain |

Fast depth combined with opaque merging reached 41.45 frames/s, but four
consecutive device-side observations showed clear artifacts. Follow-up manual
A/B tests isolated fast depth as the cause: opaque merge alone rendered
correctly, while fast depth alone reproduced the defect.

The validated profile therefore keeps accurate per-triangle alpha sorting,
mipmapping, fog, accurate audio, 32 AICA cycles and disabled fast depth. Its
only core rendering optimization is opaque-strip merging. Raw JSON results,
including rejected candidates, are retained in `raw/`.
