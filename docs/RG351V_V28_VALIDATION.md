# RG351V RetroRun v28 / Flycast adaptive v9 validation

## Final candidate

The accepted RG351V candidate combines:

- RetroRun `v28`, with the OpenAL queue fix and pitch-preserving WSOLA;
- Flycast adaptive frame skip `v9`;
- the `doa-adaptive-v23-fast-aica.cfg` Dead or Alive 2 profile.

The protected device bundle is stored outside the repositories at:

```text
/roms/retrorun-test/doa-adaptive-v2/v28-vault-20260728
```

Its `SHA256SUMS` file is the binary integrity reference. The accepted RetroRun
audio settings are 96 percent playback, 25 percent WSOLA, 33 percent emergency
WSOLA at 65 ms, 60/120 ms watermarks, a 1024-frame WSOLA window and dynamic
pitch disabled.

## Dead or Alive 2 result

The representative 30.014-second gameplay measurement produced 1,248 core
frames, or 41.58 FPS. Manual testing found gameplay speed and video excellent
and audio almost perfect. This is the release candidate to preserve.

Native-30-FPS intermissions remain the known limitation:

| Scene | Adaptive v9 | Frame skip disabled |
| --- | ---: | ---: |
| Slot 2 | 111 core / 71 presented | 111 core / 104 presented |
| Slot 3 | 109 core / 70 presented | 102 core / 94 presented |

Frame skipping does not increase core throughput in these scenes. It only
removes presented frames, because rendering is not their main bottleneck.
Flycast experiments v29 through v36 were rejected: they improved some
intermission samples but reduced normal gameplay to roughly 33-35 FPS.

## Manual cross-game observations

These are qualitative compatibility observations, not controlled benchmarks:

| Game | v28 / adaptive v9 observation |
| --- | --- |
| Sonic Adventure 2 | Very fast, with fast audio and correct gameplay speed; menus render incorrectly. |
| Soul Calibur | Excellent speed; menus have the same rendering defect. |
| Crazy Taxi | Excellent speed; menus have the same rendering defect. |
| Marvel vs. Capcom 2 | The menu-style rendering defect also affects 2D character sprites; not a release candidate yet. |
| Power Stone | Good result in the tested section. |
| Ikaruga | Excellent speed, but some ships have rectangular artifacts around them. |
| Virtua Tennis | Good speed, but some court lines are missing. |

The graphics defects must be treated separately from adaptive frame skipping.
Likely investigation areas are strip merging, translucent/opaque merge policy
and multi-draw batching. Do not weaken the validated Dead or Alive 2 profile
without per-game configuration.

The RetroRun menu must be tested without `--benchmark`. Benchmark mode clears
menu requests every frame by design and can make the menu appear for only a
fraction of a second.

## Safe next step: native cadence guard

Diagnostic tests confirmed that the two saved intermissions are native 30 FPS
content:

- slot 2: 93.1 percent of PVR frames were separated by two Dreamcast VBlanks;
- slot 3: 86.7 percent of PVR frames were separated by two Dreamcast VBlanks.

A future adaptive controller may suspend draw skipping after a sustained
two-VBlank PVR cadence. This approach generalizes to native-30-FPS content and
does not depend on scene geometry or a list of known cutscenes.

Before integration it must be tested against a gameplay save state. The guard
needs entry/exit hysteresis, must restore the previous adaptive level after a
60-FPS transition, and must retain at least 40 FPS in the validated gameplay
window. Until that comparison passes, adaptive v9 remains the release version.
