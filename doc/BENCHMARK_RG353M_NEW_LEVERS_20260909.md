# RG353M Flycast new-lever validation — 2026-09-09

## Scope and immutable test stack

- Device: Anbernic RG353M, RK3566, dArkOS, AArch64.
- RetroRun: `/home/ark/retrorun-rg353m-20260902/retrorun`, SHA-256
  `ec89d09c976b9b3c5f2fd24b837abbd371b2ea4c1c41b98bd5b1d09eebe8e51a`.
- Flycast Low-End B0: version `0.1838b83b-renderq-nodrop-v2`, SHA-256
  `fd63e58b5dc82223f1e1547f01762420dfc29a16443e1bb62dd51f7f81f3ddf7`.
- Catalog foundation: built-in schema 2 revision `20260923`, which contains
  the exact RG353M profiles immediately before their schema-3 chip migration.
- Every run requested and successfully loaded the same game-specific immutable
  save, verified its SHA before and after, used the same ROM, stopped
  EmulationStation, locked CPU/GPU/DMC to their performance settings, and
  restored the frontend and clocks afterwards.
- Short screens used 8 seconds warm-up and 12 seconds measurement. Promotion
  runs used 10 seconds warm-up and a balanced A/B, B/A, A/B sequence of three
  60-second measurements per arm.
- A run was rejected unless exactly one expected process and mapped core were
  proven, state unserialization succeeded, the metrics JSON was structurally
  valid and non-empty, core/video/audio callbacks were present, final core
  options matched the arm, temperature and battery gates passed, and the save
  remained unchanged.

The result bundle contains 330 logs, JSON files and metadata records and is
backed up outside the repository under
`~/.codex-tmp/rg353-catalog-levers-20260909/device-results/results`.

## Complete short no-drop screen

FPS is presented FPS. `U` is audio buffer underruns and `L20` is producer
lateness over 20 ms during the 12-second measurement.

| Game | Baseline FPS | No-drop FPS | Delta | U base/new | L20 base/new | Decision |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Sonic Adventure 2 | 39.820 | 52.614 | +32.13% | 1/36 | 5/1 | Needs long audio validation |
| Dead or Alive 2 | 47.544 | 59.877 | +25.94% | 0/3 | 3/1 | Rejected by manual audio review: impact SFX are missing |
| Ikaruga | 42.979 | 59.960 | +39.51% | 0/10 | 0/0 | Needs long audio validation |
| Marvel vs. Capcom 2 | 41.823 | 59.793 | +42.97% | 0/0 | 1/1 | Promoted to long validation |
| Virtua Tennis | 32.906 | 59.960 | +82.22% | 2/1 | 107/0 | Promoted to long validation |
| Sonic Adventure | 23.655 | 29.904 | +26.42% | 1/1 | 117/42 | Promoted to long validation |
| Shenmue | 28.231 | 29.980 | +6.20% | 0/1 | 33/35 | Defer |
| Power Stone | 46.072 | 59.877 | +29.96% | 0/0 | 1/1 | Promoted to long validation |
| ChuChu Rocket | 49.963 | 59.960 | +20.01% | 0/0 | 1/0 | Promoted to long validation |
| Power Stone 2 | 58.912 | 59.960 | +1.78% | 0/0 | 2/0 | Defer; already near cap |
| The House of the Dead 2 | 39.061 | 54.704 | +40.05% | 0/32 | 0/1 | Needs long audio validation |
| Daytona USA | 36.735 | 46.355 | +26.19% | 0/53 | 1/3 | Defer; audio cost too high |
| Cannon Spike | 47.651 | 59.960 | +25.83% | 0/0 | 0/0 | Promoted to long validation |
| Street Fighter III: 3rd Strike | 51.233 | 59.960 | +17.03% | 0/1 | 1/0 | Needs long validation |
| Crazy Taxi | 40.572 | 58.726 | +44.74% | 1/13 | 1/0 | Needs long audio validation |
| Resident Evil: Code Veronica | 29.978 | 29.917 | -0.20% | 0/1 | 18/22 | Test accurate cycles instead |
| Soul Calibur | 59.793 | 59.877 | +0.14% | 1/1 | 2/0 | Defer; already capped |
| Jet Grind Radio | 28.726 | 29.920 | +4.16% | 0/0 | 10/10 | Defer |
| Shenmue II | 29.983 | 29.978 | -0.02% | 0/0 | 30/33 | Reject no-drop |
| Sega Rally 2 | 43.083 | 43.047 | -0.08% | 1/1 | 0/0 | Reject no-drop |

Single-lever combination screens found no repeatable additive benefit from
shared block checks, PR=1 FMOV compilation, corrected AICA LPF or accurate SH4
cycles on Sonic Adventure 2, House of the Dead 2, Crazy Taxi, Ikaruga,
Daytona USA or Dead or Alive 2. These combinations must not be repeated unless
new evidence changes the test scene or core.

## Replicated 3x60-second results

Values are medians of three runs per arm. `p95` is active-frame time in ms.

| Game / lever | Baseline FPS | Candidate FPS | Delta | U base/new | L20 base/new | p95 base/new | Result |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Sega Rally 2 / accurate cycles | 42.466 | 46.010 | +8.35% | 1/0 | 0/0 | 25.014/24.120 | Strong candidate; no-drop stays off |
| Code Veronica / accurate cycles | 29.857 | 29.878 | +0.07% | 1/1 | 32/5 | 45.748/46.120 | Candidate for substantially cleaner audio scheduling |
| Marvel vs. Capcom 2 / no-drop | 43.304 | 59.756 | +37.99% | 0/5 | 16/3 | 44.221/16.812 | Strong candidate; explicit FPS/audio trade-off |
| Virtua Tennis / no-drop | 36.630 | 59.889 | +63.50% | 2/2 | 510/2 | 46.566/16.828 | Strong clean candidate |
| ChuChu Rocket / no-drop | 52.196 | 59.957 | +14.87% | 0/0 | 0/0 | 30.363/16.740 | Strong clean candidate; target 60 FPS reached |
| Power Stone / no-drop | 44.680 | 59.956 | +34.19% | 0/1 | 2/1 | 43.252/16.779 | Strong candidate |
| Cannon Spike / no-drop | 49.132 | 59.256 | +20.61% | 5/4 | 10/9 | 32.187/16.753 | Strong candidate |
| Sonic Adventure / no-drop | 27.913 | 28.580 | +2.39% | 2/4 | 176/178 | 60.314/47.409 | Rejected for now |

The five pending long validations were completed on 2026-09-11 with the same
immutable stack and balanced A/B, B/A, A/B ordering:

| Game / lever | Baseline FPS | Candidate FPS | Delta | U base/new | L20 base/new | p95 base/new | Result |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Sonic Adventure 2 / no-drop | 45.912 | 54.622 | +18.97% | 2/107 | 18/2 | 40.474/22.872 | Rejected: audible gaps remained with unbounded and bounded-8-ms queues |
| Ikaruga / unbounded no-drop | 44.114 | 55.156 | +25.03% | 0/58 | 3/17 | 45.626/30.442 | Rejected: many audible gaps; bounded follow-up below |
| The House of the Dead 2 / unbounded no-drop | 49.908 | 54.278 | +8.76% | 0/48 | 3/3 | 29.503/19.287 | Rejected: audio slowed and broke up in busy scenes; bounded follow-up below |
| Street Fighter III: 3rd Strike / no-drop | 54.889 | 58.123 | +5.89% | 1/3 | 6/3 | 29.364/18.418 | Promising candidate; manual A/V/input gate remains |
| Crazy Taxi / no-drop | 43.660 | 58.867 | +34.83% | 1/29 | 3/0 | 43.900/17.806 | Large speedup, but requires manual audio review |

All 30 new samples passed the mechanical gates: one expected RetroRun
process, exact frontend/core hashes and core version, correct product and
catalog, successful immutable-state load, unchanged state hash, valid metrics,
performance clocks, battery/temperature safety, and non-empty video/audio
callbacks. The increased underrun counts are therefore real candidate costs,
not invalid-run artifacts. None of these five no-drop changes is promoted
before the required manual audio, video and input review.

Code Veronica reports roughly 1,780 missed deadlines in either arm because
the core declares 60 FPS while this gameplay state runs at its native 30 FPS.
The unchanged count is not an accurate-cycle regression.

Manual DOA2 review rejected no-drop despite its measured 59.877 FPS. The
catalog audio profile (`accurate` mixer, 32 ARM7 AICA cycles) still lost some
impact effects with no-drop. Repeating the review at 16 cycles, after a clean
device reboot, without loading the state, and with the older `-1`/stable
frontend audio buffer did not restore the missing effects; the last legacy
audio configuration also ran at 50 FPS with unacceptable audio. These exact
variants must not be repeated.

The historical session confirms that the `accurate`/32 fix was manually
approved on 2026-08-29 with the USA RDC image, a clean boot and the older
`f254ccf9`-based core. This review instead used the Europe `T8116D50` image and
the current B0/no-drop-capable core. Normalizing the two runtime option logs
shows the same mixer and AICA-cycle settings; the current run additionally has
no-drop enabled and the four new opt-in core paths explicitly disabled. The
root cause is therefore narrowed to ROM/core/no-drop differences, not a
`reicast_*` versus `flycast2022_*` catalog-mapping error. Causality is not yet
proved because the only current-core no-drop-off attempt loaded the suspect
state, while the clean-boot attempt selected the PAL 50 Hz mode. The one useful
follow-up is a clean 60 Hz Europe run with accurate/32 and no-drop disabled;
if that still loses effects, repeat the same clean control with the known-good
USA RDC image. No DOA2 queue change can be promoted before that control.

The 2026-09-11 manual follow-up proved that the device-wide ArkOS audio and
input path is healthy: ChuChu Rocket had correct graphics, controls and full
audio with the native RK3566 build. DOA2 USA likewise had correct graphics,
controls, music and impact effects, but was substantially slower. Replaying
the exact 2026-09-09 fast B0 stack with the Europe image restored the near-60
FPS behavior and reproduced the missing-impact-SFX defect. A native-build
Europe run failed audio/input and is not a performance reference. The next
manual control must therefore use the exact fast B0 stack and change only
no-drop; no compiler, namespace, buffer, ROM or save-state change belongs in
that A/B comparison.

## Manual promotions and bounded queue follow-up

Street Fighter III: 3rd Strike USA (`T1213N`) passed the final RG353M graphics,
audio, input and fluidity review with current Low-End B0 no-drop. Its RK3566
profile therefore no longer requests the older `upstream_620` core. The RK3326
selection is independent and remains unchanged.

Crazy Taxi USA/Europe (`MK-51035`) exposed audible gaps with the unbounded
no-drop wait, so frontend buffer, stable-buffer, WSOLA, threaded-audio,
prebuffer and pacing mitigations were screened and rejected. A core-local
bounded wait preserved the queue speedup without leaving the audio producer
blocked indefinitely. Three paired 60-second runs gave these medians:

| Queue policy | FPS | Underruns | Active p95 |
| --- | ---: | ---: | ---: |
| unbounded no-drop | 56.060 | 97 | 21.241 ms |
| bounded no-drop, 8 ms | 54.714 | 2 | 24.668 ms |

The bounded policy costs 2.4% presented FPS and removes 97.9% of underruns.
The user approved graphics, controls, fluidity, music and effects. A runtime
core option was rejected because even its 8 ms arm fell to 48.935 FPS; the
production solution is a compile-time-specialized `renderq_wait8` core, leaving
the normal B0 binary unchanged. Its final artifact measured 54.436 FPS and one
underrun in the same 12-second scene versus 53.996 FPS and one underrun for the
manually approved hardcoded control. Both runs passed all process, core hash,
state, catalog, clock, thermal, battery and metrics gates.

The same specialized core was then reviewed on Ikaruga and The House of the
Dead 2. Unbounded no-drop was audibly rejected on both games. The bounded arm
passed the manual graphics, controls, fluidity, music and effects gate, then
completed three new 60-second runs from the same immutable saves:

| Game / bounded policy | Baseline FPS | Bounded FPS | Delta | U base/unbounded/bounded | L20 base/bounded | p95 base/bounded |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Ikaruga / no-drop, 8 ms | 44.114 | 57.040 | +29.30% | 0/58/0 | 3/1 | 45.626/28.079 |
| The House of the Dead 2 / no-drop, 8 ms | 49.908 | 53.673 | +7.54% | 0/48/9 | 3/2 | 29.503/22.295 |

All six bounded runs used frontend SHA-256
`1359e14467590ab1fec695df01a478d477d20eceab4576501abb842f536a0f6d`,
core version `0.1838b83b-renderq-wait8-v3` and core SHA-256
`d6c7b030889de59cbc2e2942b753b97309e2bc1e6dbafb8a8979c1476b4da911`.
Each run proved one process, the exact mapped core and catalog hash, the
expected Product number, successful state loading and an unchanged state hash.
The device was charging, temperatures stayed below 64 C, and all metrics and
callback gates passed. Ikaruga eliminated the unbounded arm's underruns while
also running 3.4% faster. House of the Dead 2 traded 1.1% of the unbounded
arm's FPS for an 81.3% underrun reduction; the remaining nine median events
were not audible in the approved busy-scene review.

The clean production frontend was built with GCC/G++ 9.4.0, requires at most
GLIBC 2.27 and has SHA-256
`eb9c81df0dc0e93987fe2957be26cf1dc86138516bb1d971a94413bf001c5a1b`.
With no adjacent external catalog, final built-in-catalog integration runs
started from the normal B0 core and automatically restarted in place into the
exact bounded artifact. Ikaruga measured 58.652 FPS with zero underruns;
House of the Dead 2 measured 52.978 FPS with 11 underruns and no producer
lateness over 20 ms. Both runs proved catalog `20260930`, the correct Product
number, one process, the expected core map and immutable state. This gate also
found and fixed a supervision issue: resolving `/proc/self/exe` before
`execv()` preserves the Linux process name `retrorun` instead of changing it
to `exe` during the core-variant restart.

## Catalog decision and remaining gates

Revision `20260930` retains the exact-product RK3566 candidates from
`20260928` and promotes the releases that passed the manual gate:

- no-drop: `T7010D50`, `MK-51054`, `T1201N`, `MK-51049`, `T1215N`;
- accurate SH4 cycle mode: `MK-51019`, `T1204N`;
- current Low-End B0 replaces the untested `upstream_620` request for the exact
  `T1201N` and `MK-51049` profiles;
- `T1213N` selects current Low-End B0 no-drop, while its RK3326 profile keeps
  `upstream_620`;
- `MK-51035` selects no-drop through the specialized `renderq_wait8` core.
- `MK-51054` keeps current Low-End B0 no-drop after graphics, audio, controls
  and fluidity were manually approved at a replicated 59.889 FPS median;
- `T7010D50` keeps current Low-End B0 no-drop after the same manual gate was
  approved at a replicated 59.756 FPS median;
- `T1201N` keeps current Low-End B0 no-drop and selects per-triangle alpha after
  the manual gate confirmed readable in-game pause-menu text matching stock,
  with a replicated 59.956 FPS median.
- `MK-51049` keeps current Low-End B0 no-drop after graphics, audio, controls
  and fluidity were manually approved at a replicated 59.957 FPS median.
- `T1215N` keeps current Low-End B0 no-drop after graphics, audio, controls and
  fluidity were manually approved at a replicated 59.256 FPS median.
- `MK-51019` keeps accurate SH4 cycle accounting after graphics, audio,
  controls and fluidity were manually approved at a replicated 46.010 FPS
  median with zero median underruns.
- `T1204N` keeps accurate SH4 cycle accounting after graphics, audio, controls
  and fluidity were manually approved at its native 29.878 FPS cap.
- `T38706M` and `MK-5100250` select no-drop through the specialized
  `renderq_wait8` core after their unbounded queue arms were audibly rejected;
  their bounded replicated medians were respectively 57.040 FPS with zero
  underruns and 53.673 FPS with nine underruns.

No regional sibling inherits a new lever without testing. Sonic Adventure is
not changed. The Sonic Adventure 2 no-drop arms remain rejected because both
unbounded and bounded queues produced audible gaps. Every exact-product change
retained by revision `20260930` has now passed its required on-device graphics,
audio, input and fluidity review; untested regional siblings remain unchanged.
