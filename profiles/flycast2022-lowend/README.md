# Flycast 2022 Low-End game profiles

These files preserve the per-game RetroRun configurations used during the
RG351, RG353 and RG552 Flycast 2022 Low-End investigations. They are snapshots
of tested or retained profiles, not unconditional global defaults.

See [GAME_PROFILE_MODES.md](GAME_PROFILE_MODES.md) for the implemented
`disabled`, `best_validated` and `best_performance` selection modes and the
current per-game differences.

`flycast-game-catalog.ini` is the editable source form of catalog version
`20261001`. The same data is built into RetroRun, so the feature works when
distributions install only the executable. A copy beside RetroRun is used only
when its `catalog_version` is greater than the built-in version.

`catalog_version` is a monotonically increasing revision identifier, not a
literal release date. The previous assigned revision was `20260930`, so this
revision is `20261001` even though it was produced on 2026-09-11; decreasing it
to the calendar date would make the updater reject it as older.

Schema 3 resolves a profile as `global → chip → device`, with the most
specific value winning option by option. Current chip families are:

- RK3326: RG351P, RG351M, RG351V, RG351MP, RGB20S, XU10 and R35S; the shared
  profiles were validated on RG351MP.
- RK3399: RG552; the shared profiles were validated on RG552.
- RK3566: RG503, RG353P, RG353PS, RG353V, RG353VS and RG353M; the shared
  profiles were validated on RG353M.

RetroRun first maps the device name obtained through its existing detection
chain. Only when that name is unknown does it read the NUL-separated Device
Tree `compatible` property from `/proc/device-tree/compatible`, with the sysfs
mirror as a fallback. A future `device.<MODEL>.profile...` record remains a
sparse final override over its chip profile.

When `retrorun_flycast_catalog_update = auto`, RetroRun also checks the
repository copy at most once per day without blocking game startup. A newer
catalog is validated against the supported schema and setting allowlist,
written atomically as `flycast-game-catalog.cache.ini` beside the active
configuration file, and considered from the next launch.

Catalog version `20260905` separates title metadata from profiles that were
actually validated. Title-only baseline entries remain useful for tracking
retail Product numbers, but are neither selectable nor shown in RetroRun's
`Catalog` menu and leave the active `retrorun.cfg` untouched. A validated chip
or device override may still use the correctness-first baseline as its
explicit starting point. Unknown Product numbers behave the same way.

The filename includes the Dreamcast product number printed by Flycast at boot.
Select profiles by product number rather than by ROM filename:

RK3566 profiles that passed validation on RG353M with the experimental
upstream `62085539` dynarec request
`retrorun_flycast_core_variant = upstream_620`.
RetroRun then restarts once, before loading content, with
`flycast_upstream_620_libretro.so` beside the originally requested core. An
explicit `retrorun_flycast_upstream_620_core` path may be configured instead.
If that file is absent, RetroRun logs the condition and safely retains the
normal core. Profiles validated on RG351MP now apply to the complete RK3326
family, including the Street Fighter III `upstream_620` selection.

The same mechanism supports the separately built `48acb03b` snapshot through
`retrorun_flycast_core_variant = upstream_48ac`. RetroRun looks for
`flycast_upstream_48ac_libretro.so` beside the requested core, or uses the
explicit `retrorun_flycast_upstream_48ac_core` path. This variant is selected
only by chip/game or device/game profiles that have passed performance, audio
and visual validation; merely installing the alternate core does not change
the default.

The bounded render-queue build uses the same guarded mechanism through
`retrorun_flycast_core_variant = renderq_wait8`. RetroRun looks for
`flycast_renderq_wait8_libretro.so` beside the requested core, or uses the
explicit `retrorun_flycast_renderq_wait8_core` path. This core changes only the
no-drop queue wait from unbounded to at most 8 ms; it remains unselected for
every profile except an exact product/device combination that passed the full
performance and manual A/V/input gates.

| Product number | Game | Status |
| --- | --- | --- |
| `MK-51117`, `HDR-0165` | Sonic Adventure 2 | Retail European/North American and Japanese variants. `best_performance` adds opaque-strip merging and the validated `lowend_stable_96` GO2 audio preset. A fixed 300-frame RG351V comparison presented 299 frames in both modes while the preset improved throughput from 33.47 to 36.29 FPS and reduced audio underruns from 19 to 2. Shadows, menus, audio and gameplay were manually approved. An RK3566 no-drop experiment reached 54.622 versus 45.912 FPS but was rejected after the RG353M review exposed audible audio gaps; the bounded 8 ms queue used by Crazy Taxi did not remove them. |
| `RDC-0140`, `RDC-0149`, `T8116D 50`, `T3602M`, `T3601M`, `T3601N` | Dead or Alive 2 | Observed CDI images plus the Redump retail regional variants. RG353M requires the accurate mixer and 32 ARM7 AICA cycles: 8 cycles dropped impact effects, while 16 cycles restored them but was slower. The current core measured about 48.4 FPS on the fixed state. The experimental `48acb03b` core reached 49.6-54.6 FPS but remained audio-starved in heavy scenes (up to 109 underruns per 12 seconds); it is therefore not selected for this game. |
| `T1401D  50`, `T1401M`, `T1401N` | Soul Calibur | European, Japanese and North American retail variants. The generic and RK3566 profiles retain `framerate=normal` with disabled frontend pacing. RK3399 has the complete `best_validated` profile validated on RG552: 640x480 per-strip threaded rendering, the frontend video worker, accurate audio with a 735-frame buffer and no WSOLA, and the selectable no-drop render queue. It reached 59.941 presented FPS in the 60-second stability run with complete graphics, correct audio and no underruns; `best_performance` falls back to that same validated profile. Other games and chips do not enable this queue policy unless explicitly catalogued. |
| `MK-51035`, `HDR-0053` | Crazy Taxi | European/North American and Japanese retail variants. The exact `MK-51035` RK3566 profile combines no-drop with the specialized bounded-8-ms core. Against unbounded no-drop, its replicated median retained 54.714 versus 56.060 FPS while reducing underruns from 97 to 2 per minute; graphics, controls, fluidity, music and effects were manually approved. Japanese `HDR-0053` remains unchanged. |
| `T38706M` | Ikaruga | Japanese retail release. The safe profile uses accurate per-triangle alpha sorting, which removes the ship rectangles. On RK3566 the exact release also selects no-drop through the specialized bounded-8-ms core: its replicated median improved from 44.114 to 57.040 FPS with zero underruns, while unbounded no-drop produced 58 underruns and audible gaps. Graphics, audio, controls and fluidity were manually approved. |
| `T1212N`, `T7010D 50`, `T1215M` | Marvel vs. Capcom 2 | North American, European and Japanese retail variants. The approved v28/v9 adaptive profile uses per-triangle alpha sorting to correct the 2D fighter sprites. On the fixed 600-frame USA save state it measured 49.1 FPS, 300 presented frames and zero audio underruns; the faster inaccurate sorter reached 52.1 FPS but visibly corrupted sprites. The exact European `T7010D 50` RK3566 profile adds the manually approved no-drop queue: its replicated RG353M median rose from 43.304 to 59.756 presented FPS, with five rather than zero underruns per minute. Other regions remain unchanged until tested. |
| `MK-51054`, `HDR-0113` | Virtua Tennis / Power Smash | European, North American and Japanese retail variants. The approved RG351V profile combines linear vertex depth with adaptive core skipping and the `hud_last` translucent merge strategy. Manual review confirmed correct court lines, service power gauge, audio and gameplay speed without the broad merge-disable performance penalty. The exact `MK-51054` RK3566 profile adds no-drop: three 60-second RG353M runs improved the median from 36.630 to 59.889 FPS, retained two underruns per minute and reduced producer lateness over 20 ms from 510 to 2. `HDR-0113` remains unchanged. |
| `MK-51058` | Jet Set Radio / Jet Grind Radio | The North American retail image was validated on RG351MP/dArkOS from a fixed gameplay savestate. Accurate per-triangle alpha, disabled translucent merging, fast depth and opaque-strip merging averaged 27.86 presented frames/s versus 25.99 for the conservative baseline (+7.18%). All nine final runs recorded zero audio underruns, overruns or dropped audio frames, and both fast depth alone and the final combination passed manual gameplay review. `best_performance` falls back to this validated profile. |
| `T1215N` | Cannon Spike | The North American retail image was validated on RG351MP/dArkOS from a fixed gameplay savestate. Opaque-strip merging averaged 38.00 presented frames/s versus 33.56 for the conservative baseline (+13.2%), with every frame presented and no audio faults. Fast depth was faster but rejected because it produced obvious graphical artifacts. On RK3566 the manually approved no-drop profile raised the replicated RG353M median from 49.132 to 59.256 FPS, reduced active-frame p95 from 32.187 to 16.753 ms and slightly improved median underruns from five to four per minute; graphics, audio, controls and fluidity passed the device review. |
| `MK-51037` | Daytona USA 2001 / Daytona USA | The North American retail image was validated on RG351MP/dArkOS from a fixed race savestate. Per-strip alpha sorting plus opaque-strip merging averaged 23.75 presented frames/s versus 16.20 for the conservative baseline (+46.6%), reduced active-frame p95 from 89.27 to 58.52 ms and recorded no skipped frames or audio faults. Both the sorter and final combination passed manual race review. `best_performance` falls back to this validated profile. |
| `MK-5118450` | Shenmue II (Europe) | The European retail image was tested on RG351MP/dArkOS from fixed 3D savestates. Per-strip alpha plus `vertex_fast_log` improved the original 300-frame screen from 14.85 to 19.00 presented frames/s (+27.9%) and reduced active-frame p95 from 91.40 to 54.43 ms; graphics passed manual review. On a later, heavier state with CPU, GPU and DMC governors at `performance`, the `lowend_heavy_100` profile and a 4096-frame buffer reduced the three-run median from 10 to 4 audio underruns and queue-low observations from 98 to 11 versus the 55% WSOLA reference, while retaining 100% playback speed and essentially unchanged throughput (17.38 versus 17.39 frames/s). Adaptive core frameskip reduced underruns to 2 but was rejected because it presented only 160 of 300 frames; direct scanout was unavailable and its fallback was slower. Pinning the OpenAL and frontend audio workers to a reserved fourth CPU raised throughput to 18.48 frames/s but increased median underruns from 4 to 15 and introduced 172 backpressure events, so single-thread audio remains selected. Occasional gaps in the heaviest scene remain a documented RK3326 limitation. Japanese `HDR-0164` and `HDR-0179` releases remain conservative baselines until tested. |
| `T7013D50`, `T1213N`, `T1209M` | Street Fighter III: 3rd Strike | The European, North American and Japanese retail releases use distinct catalog records with the same RG351MP-validated settings. On a fixed USA fight savestate, accurate per-triangle alpha, `vertex_fast_log` depth and opaque-strip merging reached a three-run median of 47.76 FPS versus 44.39 (+7.58%), reduced active-frame p95 from 43.63 to 41.43 ms and recorded no audio underruns or empty queues. The inaccurate per-strip sorter was slower and visually unsafe. A later three-run RG351MP comparison measured 58.05 FPS with the upstream `62085539` core versus 46.73 with the current core (+24.2%); characters, backgrounds, animation, controls and audio passed manual review. The shared RK3326 `best_performance` profile therefore selects `upstream_620` on every mapped RK3326 device. |
| `MK-51019`, `HDR-0010` | Sega Rally 2 | European/North American and Japanese retail variants. The RK3566 profile uses the current low-end core with the validated WinCE MMU address LUT, shared block checks, PR=1 FPU-transfer compilation and corrected upstream AICA low-pass filter. It preserves the visually approved 640x480 renderer and accurate mixer, 110 MHz SH4 clock, stable 1470-frame buffer and 10% `lowend_stable_96` GO2 stretch path. On the exact `MK-51019` fixed state, accurate SH4 cycle accounting raised the replicated RG353M median from 42.466 to 46.010 presented FPS (+8.35%) and reduced median underruns from one to zero; no-drop was neutral and remains disabled. Graphics, audio, controls and fluidity passed the final RG353M review. The untested Japanese `HDR-0010` RK3566 profile retains legacy timing. On RK3399, both variants use the manually approved RG552 profile with accurate timing and no alternate core. |
| `MK-51000` | Sonic Adventure | The European/North American retail Product number has an RK3566 `best_performance` profile validated on RG353M. On the fixed gameplay state it reached 27.96 FPS versus 25.59 for the dArkOS stock stack (+9.3%), improved active-frame p95 from 69.22 to 64.11 ms and recorded no skipped frames, audio underruns or empty queues; stock recorded two underruns and two empty queues. Graphics, audio and gameplay were manually approved. Japanese `HDR-0001` and `HDR-0043` remain conservative baselines until tested. |
| `T36801D61`, `T36801D64`, `T1201M`, `T1201N` | Power Stone | Regional releases remain title-only except for chip-specific profiles. The exact `T1201N` RK3566 profile uses the tested current Low-End B0 rather than `upstream_620`, enables the manually approved no-drop queue and uses accurate per-triangle alpha. The accurate sorter is required to preserve the readable in-game pause-menu text seen on the dArkOS stock stack. Its replicated RG353M median rose from 44.680 to 59.956 FPS, with median underruns moving from zero to one and active-frame p95 falling from 43.252 to 16.779 ms. Other RK3566 regions remain unchanged. The RK3399 `T1201N` profile validated on RG552 enables the same approved queue policy and improved the fixed combat state from 45.812 to 59.958 FPS (+30.9%). |
| `MK-51049` | ChuChu Rocket | The exact RK3566 profile uses the tested current Low-End B0 rather than `upstream_620` and adds no-drop. Across three 60-second fixed-state runs it improved the RG353M median from 52.196 to 59.957 presented FPS, with zero underruns and zero producer-late events in both arms. Graphics, audio, controls and fluidity were manually approved on RG353M. |
| `T1204N` | Resident Evil: Code Veronica | The exact USA RK3566 profile enables accurate SH4 cycle accounting only. The game remained at its native cap (29.857 versus 29.878 median presented FPS) with one median underrun in both arms, while audio producer lateness over 20 ms fell from 32 to 5 events per minute. Graphics, audio, controls and fluidity passed the final RG353M review. Other regional Product numbers remain unchanged until tested. |
| `T36812D61`, `T36812D64`, `T1218M`, `T1211N` | Power Stone 2 | The RK3566 profile was measured on RG353M with the North American release and associated with all known retail regional Product numbers. Accurate per-triangle alpha reached 58.96 FPS versus 56.17 for the dArkOS stock stack (+5.0%), presented every frame and recorded no audio faults. The per-strip candidate reached only 0.13 FPS more, so it was rejected in favour of the safer renderer. Fast depth, AICA 8 and additional state reuse were all slower. Graphics, HUD, audio and gameplay were manually approved. On RK3399, the tested North American `T1211N` release enables no-drop over the inherited safe profile and reached 59.942 FPS versus 54.466 on RG552. |
| `MK-51002`, `MK-5100250`, `MK-55045`, `HDR-0007`, `HDR-0011` | The House of the Dead 2 | Untested variants remain title-only. On RK3566 the observed European `MK-5100250` release selects no-drop through the specialized bounded-8-ms core. Its replicated median improved from 49.908 to 53.673 FPS; bounded waiting reduced the unbounded arm's 48 underruns to 9, and the busy-scene graphics, audio and controls review passed. The independent RK3399 profile validated on RG552 combines no-drop with the 2048-frame stable `lowend_stable_96` audio bundle, reached 52.787 FPS with zero underruns, and passed graphics, audio and lightgun review. |

Catalog `20260902` added device-scoped RG353M profiles for `T1215N` (Cannon
Spike), `MK-51037` (Daytona USA 2001), `MK-5100250` (the observed European
House of the Dead 2 CHD), `MK-51058` (Jet Grind Radio) and `MK-5118450`
(Shenmue II). All five were tested from fixed gameplay states at maximum
CPU/GPU/DMC governors and manually approved for graphics and audio. The
profiles use the measured 735-frame/60 ms GO2 path, disabled frontend pacing,
640x480 threaded rendering and per-game validated depth/state-reuse choices.
House of the Dead 2 remains a conservative title-only baseline on RG351-class
devices; its tuned profile is selected only for RG353M.

Catalog `20260903` adds the next RG353M-validated group. Fixed gameplay-state
comparisons against the dArkOS stock RetroRun/core measured 51.35 versus 46.41
FPS for Street Fighter III: 3rd Strike (+10.7%), 43.38 versus 39.13 for Virtua
Tennis (+10.9%), 44.24 versus 40.55 for Ikaruga (+9.1%), and 53.03 versus
48.45 for ChuChu Rocket (+9.5%). Their profiles disable hidden adaptive core
skipping, so those figures are presented frames rather than duplicated output.
Shenmue reached 28.72 versus 28.59 FPS with a better p95 frame time (47.51
versus 51.99 ms), while preserving its correct renderer and audio choices.
The tested regional Product-number variants inherit the corresponding settings.
Resident Evil Code: Veronica remains on its global validated profile: both it
and stock hold the game's approximately 30 FPS limit, so a device override
would add complexity without a real gain. Metropolis Street Racer was excluded
from this test group because the available image did not boot reliably.

Catalog `20260904` adds the RG353M-validated Sonic Adventure profile for the
European/North American `MK-51000` release. It retains the safe global
per-triangle renderer, uses the measured 735-frame/60 ms GO2 audio path with
stable buffering disabled, and disables frontend declared-FPS pacing. On the
fixed gameplay state it measured 27.96 versus 25.59 FPS for the dArkOS stock
stack (+9.3%), with a better p95 and no skipped frames or audio faults. The
Japanese releases remain baseline entries because they were not tested.

Catalog `20260905` adds the RG353M-validated Power Stone 2 profile for all four
known retail regional Product numbers. The fixed USA combat state measured
58.96 versus 56.17 FPS for the dArkOS stock stack (+5.0%), with every frame
presented and no audio faults. The selected profile keeps accurate per-triangle
alpha because the faster sorter added only 0.13 FPS; fast depth, AICA 8 and
additional state reuse were measured and rejected as slower. Graphics, HUD,
audio and gameplay were manually approved on the device.

Catalog `20260916` promotes Sega Rally 2 on RG353M from `upstream_620` to the
existing selectable `upstream_48ac` core at 170 MHz. It also fixes alternate
Flycast detection after `execv()`: a core that exports the private
Product-number ABI remains catalog-capable even when its version string starts
with `v`, so the second process reapplies the complete transient profile rather
than silently falling back to the values in `retrorun.cfg`.

Catalog `20260917` replaces that provisional Sega Rally 2 selection with the
manually approved current-core profile. The RG353M override enables the
selectable WinCE address LUT, shared AArch64 block checks, PR=1 FPU transfers
and corrected AICA LPF, while retaining accurate mixing and the stable GO2
audio path. These experimental core options remain disabled by default and
are enabled only for the two catalogued retail Product-number variants.
An RG353M cross-game screen from fixed save states covered the other 19 local
catalog titles. The CPU options were neutral, slower, or remained below an
already selected alternate core; the corrected LPF was neutral or slower in
those automated measurements. In particular, Shenmue II remained capped at
29.99 FPS with zero underruns, while the repeatable Dead or Alive 2 result was
only 42.44 versus 42.05--42.17 FPS and did not justify changing its
audio-sensitive validated profile.

Catalog `20260921` adds a correctness-first RG351MP fallback for Sega Rally 2.
It preserves the complete environment with the conservative renderer, keeps
the WinCE-required 32 AICA ARM cycles and uses only the validated MMU address
LUT and accurate SH4 scheduler optimizations. The final state-loaded sweep
measured 15.74 core FPS and 11.80 presented FPS, versus 13.43 and 7.13 for the
previous uncatalogued run; audio remains an acknowledged RK3326 limitation.
The same catalog enables the experimental accurate AICA 32-sample fast path
only for the RG351MP Shenmue II profile. In a three-run, 600-frame A/B with the
same state, core and clocks, it raised median core and presented throughput
from 17.889/17.859 to 18.147/18.117 FPS (+1.44%) while leaving the median at
five audio underruns. That option remains disabled by default and for every
other profile. A final fixed-save-state sweep covered all 20 locally available
catalog games on both RG351MP and RG353M, with maximum CPU, GPU and memory
governors and no overlapping RetroRun processes.

Catalog `20260922` adds the RG552 Sega Rally 2 profile for both `MK-51019` and
`HDR-0010`. It reproduces the manually approved Low-End B0 (`838b83b64`)
no-WSOLA configuration: a stable 2048-frame audio buffer, explicitly enabled
frontend video worker, 640x480 threaded rendering and the validated accurate
WinCE LUT, shared-block, FPU-transfer, AICA LPF and SH4-cycle options. It
deliberately does not select another core; `best_performance` falls back to the
same device-scoped `best_validated` profile.

Catalog `20260923` adds complete RG552 Soul Calibur profiles for `T1401D  50`,
`T1401N` and `T1401M`. The clean same-binary test changed only the selectable
render-queue policy: three 600-frame runs improved from a 47.431 FPS median to
59.940 FPS (+26.37%), and the approved 60-second run held 59.941 presented FPS
with no underruns or audio lateness above 20 ms. The device profile pins the
validated stock-equivalent graphics, audio and frontend-thread settings and
enables `reicast_render_queue_no_drop`; `best_performance` falls back to this
device-scoped `best_validated` profile.

Catalog `20260924` extends that RG552-only queue policy to the twelve
fixed-gameplay-state profiles validated with the same frontend and Low-End
core: ChuChu Rocket, Dead or Alive 2, Ikaruga, Crazy Taxi, Marvel vs. Capcom 2,
Street Fighter III: 3rd Strike, Virtua Tennis, Cannon Spike, Daytona USA, Jet
Grind Radio, Sonic Adventure 2 and Shenmue II. Ikaruga, MVC2 and Virtua Tennis
also disable the core's adaptive frame skipping, which had halved presented
frames. Sonic adds the 2048-frame stable/WSOLA audio bundle that reduced the
long-run median to two underruns per minute while preserving its 32.4% FPS
gain. Sega Rally deliberately keeps no-drop disabled because its replicated
median was neutral. The option remains absent from global defaults and from
untested device/game combinations.

The queue policy is the narrow Low-End backport of official Flycast commit
`a00aad5fa73b9f31125c49a3814a40e15aa76b98` (`pvr: auto frame skip to replace
current and previous synchronous rendering`). That upstream change waits for
an occupied render queue when frame skipping is disabled; the older Low-End
path instead recycled and silently dropped the incoming render context.

Catalog `20260925` completes the 20-game RG552 performance screen. Sonic
Adventure adds the no-drop queue policy after its replicated 60-second median
rose from 27.292 to 29.271 presented FPS (+7.25%); the median audio cost was
one additional underrun per minute. Resident Evil: Code Veronica already held
its native 30 FPS cap, so no-drop and the MMU/FMOV/shared-block/AICA levers
remain disabled. Its RG552 profile enables only accurate SH4 cycle accounting:
three 60-second runs held 29.98 presented FPS with zero underruns and reduced
audio producer lateness above 20 ms from 32--35 to 5--9 events per minute. The
final fixed-state graphics, gameplay-speed, audio and input check was manually
approved on the device.

Catalog `20260926` promotes the final three manually approved RG552 profiles.
Power Stone (`T1201N`) rose from 45.812 to 59.958 presented FPS (+30.9%), and
Power Stone 2 (`T1211N`) rose from 54.466 to 59.942 FPS (+10.1%); both enable
only the no-drop render queue over their inherited safe graphics profiles.
The House of the Dead 2 (`MK-5100250`) combines no-drop with the validated
2048-frame stable `lowend_stable_96` audio bundle; its captured candidate
reached 52.787 FPS with zero underruns. Graphics, HUD, audio and controls (or
lightgun input for House of the Dead 2) were approved from immutable fixed
states. The overrides remain limited to the exact retail Product numbers and
to RG552; untested regional variants stay title-only.

Catalog `20260927` introduces schema 3 and migrates the three validated device
catalogs to SoC scopes. RG351MP results now form the RK3326 catalog, RG353M
results the RK3566 catalog, and RG552 results the RK3399 catalog. Every mapped
device on the same chip reuses those settings; a sparse device-specific record
can still override individual values last. Model mapping takes precedence, and
only an unknown model falls back to the Device Tree `compatible` property.
This revision number is the monotonic successor of `20260926`, not the date on
which the change was made.

Catalog `20260928` stages the exact-product RK3566 candidates found by the
RG353M new-lever screen. Balanced A/B/A three-by-60-second fixed-state runs
select no-drop for `T7010D50`, `MK-51054`, `T1201N`, `MK-51049` and `T1215N`,
and accurate SH4 cycle accounting for `MK-51019` and `T1204N`. The two former
`upstream_620` requests for `T1201N` and `MK-51049` are removed because these
results were measured with the current Low-End B0 core. Sonic Adventure's
2.39% FPS gain is not selected: underruns doubled, producer lateness did not
improve and one queue run was an outlier. Untested regional Product numbers
are deliberately unchanged. The staged profiles remain labelled candidates
until the final on-device graphics, audio and input review is complete; full
measurements and remaining work are recorded in
`doc/BENCHMARK_RG353M_NEW_LEVERS_20260909.md`.

Catalog `20260929` promotes the two exact RK3566 releases that completed the
manual gate. Street Fighter III: 3rd Strike USA (`T1213N`) uses the current
Low-End core with no-drop and no longer requests `upstream_620`; the RK3326
profile remains unchanged. Crazy Taxi USA/Europe (`MK-51035`) combines no-drop
with the specialized `renderq_wait8` core. Against the unbounded no-drop build,
three 60-second fixed-state runs reduced median underruns from 97 to 2 while
retaining 54.714 versus 56.060 FPS (-2.4%). A same-scene production-artifact
check measured 54.436 FPS and one underrun, matching the manually approved
hardcoded control at 53.996 FPS and one underrun. Graphics, controls, fluidity,
music and effects were approved on RG353M. The Japanese Crazy Taxi release and
the other staged candidates remain unchanged pending their own manual gates.

Catalog `20260930` completes the RG353M review. Current Low-End B0 no-drop is
promoted for Virtua Tennis USA (`MK-51054`), Marvel vs. Capcom 2 Europe
(`T7010D50`), Power Stone USA (`T1201N`), ChuChu Rocket USA (`MK-51049`) and
Cannon Spike USA (`T1215N`). Accurate SH4 cycle accounting is promoted for
Sega Rally 2 USA (`MK-51019`) and Code Veronica USA (`T1204N`). Ikaruga
(`T38706M`) and the observed European House of the Dead 2 (`MK-5100250`)
select no-drop through the specialized `renderq_wait8` core after their
unbounded arms were audibly rejected. Every retained exact-product change
passed the final graphics, audio, controls and fluidity review. Power Stone
also selects per-triangle alpha so its small in-game pause menu retains the
readable text shown by the stock dArkOS stack. Virtua Tennis increased
from 36.630 to 59.889 presented FPS (+63.5%) while holding the median at two
underruns and reducing producer lateness above 20 ms from 510 to 2 events per
minute. MVC2 increased from 43.304 to 59.756 FPS (+38.0%); its five median
underruns per minute were not audibly objectionable in the approved gameplay
review. Power Stone increased from 44.680 to 59.956 FPS (+34.2%), with one
median underrun and a much lower 16.779 ms active-frame p95. Ikaruga reached
57.040 FPS with zero underruns, while House of the Dead 2 reached 53.673 FPS
with nine median underruns and clean approved busy-scene audio. Untested
regional siblings remain unchanged.

Catalog `20261001` completes the RG351V/RK3326 review. Exact releases promoted
after powered fixed-state benchmarks and the final graphics, audio, controls
and fluidity gate are Marvel vs. Capcom 2 Europe (`T7010D50`), Jet Grind Radio
USA (`MK-51058`), Crazy Taxi USA/Europe (`MK-51035`), Virtua Tennis USA
(`MK-51054`), Street Fighter III: 3rd Strike USA (`T1213N`), Code Veronica USA
(`T1204N`), Ikaruga Japan (`T38706M`) and Sega Rally 2 USA (`MK-51019`). The
no-drop profiles also disable core-side frame skipping where required. The
Street Fighter profile now uses current Low-End B0 no-drop instead of
`upstream_620`; Crazy Taxi, Code Veronica and Ikaruga select accurate SH4 cycle
accounting, while Sega Rally enables shared dynarec block checks in its safe
validated fallback. A final Sega Rally no-drop experiment with the bounded
`renderq_wait8` core was rejected: its 300-frame screen was effectively flat
at 17.21 versus 17.19 FPS with 22 underruns in both arms, and the manual review
reported worse audio and fluidity. Schema 3 makes the retained profiles
available to every recognized RK3326 device, with any device-specific setting
still taking precedence.

`dreamcast-product-variants.tsv` is the machine-checked map between the Redump
retail releases and the Product numbers returned by Flycast. When adding a
game, first enumerate its retail regional/revision entries from the upstream
Dreamcast DAT, then add every corresponding IP.BIN Product number to both this
table and the runtime catalog. Demo, beta and trial discs require independent
validation and are not inherited automatically.

See `REDUMP_SOURCE.md` for the exact upstream URL, attribution, license and the
important distinction between a Redump serial and Flycast's IP.BIN Product
number.

`experimental/` contains alternatives deliberately excluded from the normal
profiles. Soul Calibur's `performance` profile retains the faster
`top_hud_last` path, whose intermittent health-bar/scenery ordering defect
still requires broader visual review. Its audio-thread profile is retained
only for manual comparison; audio multithreading remains disabled in the
normal profile and in RetroRun defaults.

These profiles use the source/dArkOS core-option prefix `reicast_`. An
AmberELEC build configured with the `flycast2022` option prefix must replace
only `reicast_` with `flycast2022_`.

Example:

```sh
./retrorun -f \
  -c ./profiles/flycast2022-lowend/retrorun-sonic-MK-51117.cfg \
  -s /storage/roms/dreamcast \
  -d /storage/roms/bios \
  ./flycast_libretro.so \
  "/storage/roms/dreamcast/Sonic Adventure 2.cdi"
```

The Sonic profile has automatic loading enabled and automatic saving disabled
so the validated gameplay state is not overwritten during renderer tests. Its
Fast Depth guard keeps the fast logarithmic vertex path during moving gameplay,
uses accurate depth for low-complexity/font-like menu scenes, and detects a
paused 3D scene after three identical geometry signatures. The shadow-safe
variant additionally uses accurate fragment depth only for opaque PowerVR
shadow receivers with more than a 4x vertex-depth range. This removed Sonic's
black rectangular projected-shadow artifacts. The cost depends on the scene:
an early stable aggregate measured 0.35%, while the final clean 600-frame
comparison measured a 3.84% advantage for the aggressive mode.

Both catalog modes keep the shadow-safe depth value. `best_performance` also
enables opaque-strip merging, which produced the validated 30.6% gain. The
older depth-only experiment remains in
`experimental/retrorun-sonic-aggressive-MK-51117.cfg` for reproducibility; it
retains menu/pause protection but accepts the documented rectangular-shadow
artifact and is no longer the fastest measured profile.

The Sonic `best_performance` profile also selects a 2048-frame frontend
buffer, a 150 ms GO2 low-water threshold and the `lowend_stable_96` audio
preset. The preset keeps environment variables as explicit emergency
overrides, but makes the validated WSOLA 25/33 configuration, 1024-frame
window and 96% playback pitch reproducible from the versioned catalog.

## Baseline retail coverage added in catalog 20260746

The metadata table also records retail games and regional Product numbers for
which at least one release or device remains title-only. Without an explicit
validated device override, these coverage baselines are not selectable:
RetroRun hides them from `Catalog` and leaves the user's configuration
untouched. Promote them only after repeatable benchmarking and manual
audio/video review.

The remaining coverage includes the Japanese Sonic Adventure and Shenmue II
variants, untested regional Power Stone and The House of the Dead 2 variants,
Skies of Arcadia, Capcom vs. SNK 2, Phantasy Star Online, NFL 2K, NFL 2K1,
Hydro Thunder, F355 Challenge, Virtua Fighter 3tb, Cosmic Smash, Toy Commander,
Rez, Street Fighter Alpha 3 and Sega Bass Fishing. Every known retail Product
number is enumerated in `dreamcast-product-variants.tsv`.
