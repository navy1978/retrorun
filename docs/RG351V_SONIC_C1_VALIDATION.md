# RG351V Sonic Adventure 2 C1 validation

## Selected stack

- RetroRun v28
- Flycast adaptive v9
- Catalog mode `best_performance`
- Product numbers `MK-51117` and `HDR-0165`
- GO2 audio buffer: 2048 frames
- GO2 low-water threshold: 150 ms
- GO2 audio preset: `lowend_stable_96`
- Core frame skipping: disabled

The preset resolves to the validated WSOLA 25/33 configuration, a 65 ms
emergency threshold, a 1024-frame WSOLA window and 96% playback pitch.
Environment variables remain available as explicit runtime overrides.

## Numerical validation

The catalog baseline and C1 used the same RG351V, clocks, save-state, RetroRun
frontend, Flycast core, three-second warm-up and 300 core-frame window.

| Metric | Catalog baseline | C1 |
| --- | ---: | ---: |
| Core throughput | 33.47 FPS | 36.29 FPS |
| Presented frames | 299 | 299 |
| Skipped frames | 1 | 1 |
| Core average | 20.329 ms | 18.387 ms |
| Audio underruns | 19 | 2 |

A separate playback comparison kept all other settings fixed:

| Metric | 96% | 100% |
| --- | ---: | ---: |
| Core throughput | 37.62 FPS | 35.95 FPS |
| Presented frames | 299 | 299 |
| Audio underruns | 4 | 4 |
| Active frame p95 | 36.237 ms | 41.404 ms |
| WSOLA-processed frames | 30544 | 34912 |

The 96% setting was retained because it was faster, required less WSOLA work
and sounded better in manual testing.

## Manual validation

The user confirmed:

- no menu corruption;
- no rectangular shadow artifacts;
- smoother gameplay than the previous catalog launch;
- better audio at 96% than at 100%.

The on-device experimental files remain under:

```text
/roms/retrorun-test/sonic-v10-menu-compare-20260728
```
