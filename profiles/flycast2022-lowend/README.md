# Flycast 2022 Low-End RK3326 profiles

These files preserve the per-game RetroRun configurations used during the
RG351V and RG351MP Flycast 2022 Low-End investigations. They are snapshots of
tested or retained profiles, not global defaults.

See [GAME_PROFILE_MODES.md](GAME_PROFILE_MODES.md) for the implemented
`disabled`, `best_validated` and `best_performance` selection modes and the
current per-game differences.

`flycast-game-catalog.ini` is the editable source form of catalog version
`20260921`. The same data is built into RetroRun, so the feature works when
distributions install only the executable. A copy beside RetroRun is used only
when its `catalog_version` is greater than the built-in version.

When `retrorun_flycast_catalog_update = auto`, RetroRun also checks the
repository copy at most once per day without blocking game startup. A newer
catalog is validated against the supported schema and setting allowlist,
written atomically as `flycast-game-catalog.cache.ini` beside the active
configuration file, and considered from the next launch.

Catalog version `20260905` separates title metadata from profiles that were
actually validated. Title-only baseline entries remain useful for tracking
retail Product numbers, but are neither selectable nor shown in RetroRun's
`Catalog` menu and leave the active `retrorun.cfg` untouched. A validated
device override may still use the correctness-first baseline as its explicit
starting point. Unknown Product numbers behave the same way.

The filename includes the Dreamcast product number printed by Flycast at boot.
Select profiles by product number rather than by ROM filename:

RG353M profiles that passed validation with the experimental upstream
`62085539` dynarec request `retrorun_flycast_core_variant = upstream_620`.
RetroRun then restarts once, before loading content, with
`flycast_upstream_620_libretro.so` beside the originally requested core. An
explicit `retrorun_flycast_upstream_620_core` path may be configured instead.
If that file is absent, RetroRun logs the condition and safely retains the
normal core. RG351-class profiles retain the normal core except for explicitly
device-validated overrides such as Street Fighter III on RG351MP.

The same mechanism supports the separately built `48acb03b` snapshot through
`retrorun_flycast_core_variant = upstream_48ac`. RetroRun looks for
`flycast_upstream_48ac_libretro.so` beside the requested core, or uses the
explicit `retrorun_flycast_upstream_48ac_core` path. This variant is selected
only by device/game profiles that have passed performance, audio and visual
validation; merely installing the alternate core does not change the default.

| Product number | Game | Status |
| --- | --- | --- |
| `MK-51117`, `HDR-0165` | Sonic Adventure 2 | Retail European/North American and Japanese variants. `best_performance` adds opaque-strip merging and the validated `lowend_stable_96` GO2 audio preset. A fixed 300-frame RG351V comparison presented 299 frames in both modes while the preset improved throughput from 33.47 to 36.29 FPS and reduced audio underruns from 19 to 2. Shadows, menus, audio and gameplay were manually approved. |
| `RDC-0140`, `RDC-0149`, `T8116D 50`, `T3602M`, `T3601M`, `T3601N` | Dead or Alive 2 | Observed CDI images plus the Redump retail regional variants. RG353M requires the accurate mixer and 32 ARM7 AICA cycles: 8 cycles dropped impact effects, while 16 cycles restored them but was slower. The current core measured about 48.4 FPS on the fixed state. The experimental `48acb03b` core reached 49.6-54.6 FPS but remained audio-starved in heavy scenes (up to 109 underruns per 12 seconds); it is therefore not selected for this game. |
| `T1401D  50`, `T1401M`, `T1401N` | Soul Calibur | European, Japanese and North American retail variants. Both profiles require `framerate=normal` and disabled frontend pacing: on RG353M the optimized `fullspeed` configuration ran worse than the unoptimized `normal` configuration. |
| `MK-51035`, `HDR-0053` | Crazy Taxi | European/North American and Japanese retail variants. Accurate control profile. No experimental performance candidate produced a repeatable useful gain. |
| `T38706M` | Ikaruga | Japanese retail release. Adaptive v9 profile with accurate per-triangle alpha sorting; manual RG351V review confirmed that it removes the ship rectangles while preserving excellent gameplay. |
| `T1212N`, `T7010D 50`, `T1215M` | Marvel vs. Capcom 2 | North American, European and Japanese retail variants. The approved v28/v9 adaptive profile uses per-triangle alpha sorting to correct the 2D fighter sprites. On the fixed 600-frame USA save state it measured 49.1 FPS, 300 presented frames and zero audio underruns; the faster inaccurate sorter reached 52.1 FPS but visibly corrupted sprites. |
| `MK-51054`, `HDR-0113` | Virtua Tennis / Power Smash | European, North American and Japanese retail variants. The approved RG351V profile combines linear vertex depth with adaptive core skipping and the `hud_last` translucent merge strategy. Manual review confirmed correct court lines, service power gauge, audio and gameplay speed without the broad merge-disable performance penalty. |
| `MK-51058` | Jet Set Radio / Jet Grind Radio | The North American retail image was validated on RG351MP/dArkOS from a fixed gameplay savestate. Accurate per-triangle alpha, disabled translucent merging, fast depth and opaque-strip merging averaged 27.86 presented frames/s versus 25.99 for the conservative baseline (+7.18%). All nine final runs recorded zero audio underruns, overruns or dropped audio frames, and both fast depth alone and the final combination passed manual gameplay review. `best_performance` falls back to this validated profile. |
| `T1215N` | Cannon Spike | The North American retail image was validated on RG351MP/dArkOS from a fixed gameplay savestate. Opaque-strip merging averaged 38.00 presented frames/s versus 33.56 for the conservative baseline (+13.2%), with every frame presented and no audio faults. Fast depth was faster but rejected because it produced obvious graphical artifacts. `best_performance` falls back to the visually approved opaque-only profile. |
| `MK-51037` | Daytona USA 2001 / Daytona USA | The North American retail image was validated on RG351MP/dArkOS from a fixed race savestate. Per-strip alpha sorting plus opaque-strip merging averaged 23.75 presented frames/s versus 16.20 for the conservative baseline (+46.6%), reduced active-frame p95 from 89.27 to 58.52 ms and recorded no skipped frames or audio faults. Both the sorter and final combination passed manual race review. `best_performance` falls back to this validated profile. |
| `MK-5118450` | Shenmue II (Europe) | The European retail image was tested on RG351MP/dArkOS from fixed 3D savestates. Per-strip alpha plus `vertex_fast_log` improved the original 300-frame screen from 14.85 to 19.00 presented frames/s (+27.9%) and reduced active-frame p95 from 91.40 to 54.43 ms; graphics passed manual review. On a later, heavier state with CPU, GPU and DMC governors at `performance`, the `lowend_heavy_100` profile and a 4096-frame buffer reduced the three-run median from 10 to 4 audio underruns and queue-low observations from 98 to 11 versus the 55% WSOLA reference, while retaining 100% playback speed and essentially unchanged throughput (17.38 versus 17.39 frames/s). Adaptive core frameskip reduced underruns to 2 but was rejected because it presented only 160 of 300 frames; direct scanout was unavailable and its fallback was slower. Pinning the OpenAL and frontend audio workers to a reserved fourth CPU raised throughput to 18.48 frames/s but increased median underruns from 4 to 15 and introduced 172 backpressure events, so single-thread audio remains selected. Occasional gaps in the heaviest scene remain a documented RK3326 limitation. Japanese `HDR-0164` and `HDR-0179` releases remain conservative baselines until tested. |
| `T7013D50`, `T1213N`, `T1209M` | Street Fighter III: 3rd Strike | The European, North American and Japanese retail releases use distinct catalog records with the same RG351MP-validated settings. On a fixed USA fight savestate, accurate per-triangle alpha, `vertex_fast_log` depth and opaque-strip merging reached a three-run median of 47.76 FPS versus 44.39 (+7.58%), reduced active-frame p95 from 43.63 to 41.43 ms and recorded no audio underruns or empty queues. The inaccurate per-strip sorter was slower and visually unsafe. A later three-run RG351MP comparison measured 58.05 FPS with the upstream `62085539` core versus 46.73 with the current core (+24.2%); characters, backgrounds, animation, controls and audio passed manual review. The RG351MP `best_performance` override therefore selects `upstream_620`, while other RG351 devices retain the current core until tested. |
| `MK-51019`, `HDR-0010` | Sega Rally 2 | European/North American and Japanese retail variants. The RG353M-specific `best_performance` profile uses the current low-end core with the validated WinCE MMU address LUT, shared block checks, PR=1 FPU-transfer compilation and corrected upstream AICA low-pass filter. It preserves the visually approved 640x480 renderer and accurate mixer, uses 110 MHz legacy SH4 timing, a stable 1470-frame buffer and the 10% `lowend_stable_96` GO2 stretch path. The final fixed eight-second run measured 26.62 presented FPS and two underruns; the same build without the LPF correction measured 26.63 FPS and two underruns. Audio and gameplay were manually approved as almost perfect. The faster `upstream_48ac` snapshot was rejected because it produced 71 underruns and audibly worse audio despite reaching 40.81 FPS. |
| `MK-51000` | Sonic Adventure | The European/North American retail Product number has an RG353M-specific `best_performance` profile. On the fixed gameplay state it reached 27.96 FPS versus 25.59 for the dArkOS stock stack (+9.3%), improved active-frame p95 from 69.22 to 64.11 ms and recorded no skipped frames, audio underruns or empty queues; stock recorded two underruns and two empty queues. Graphics, audio and gameplay were manually approved. Japanese `HDR-0001` and `HDR-0043` remain conservative baselines until tested. |
| `T36812D61`, `T36812D64`, `T1218M`, `T1211N` | Power Stone 2 | The RG353M-specific profile was measured on the North American release from a fixed combat state and associated with all known retail regional Product numbers. Accurate per-triangle alpha reached 58.96 FPS versus 56.17 for the dArkOS stock stack (+5.0%), presented every frame and recorded no audio faults. The per-strip candidate reached only 0.13 FPS more, so it was rejected in favour of the safer renderer. Fast depth, AICA 8 and additional state reuse were all slower. Graphics, HUD, audio and gameplay were manually approved. |

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
variants, unvalidated Power Stone and The House of the Dead 2 combinations,
Skies of Arcadia, Capcom vs. SNK 2, Phantasy Star Online, NFL 2K, NFL 2K1,
Hydro Thunder, F355 Challenge, Virtua Fighter 3tb, Cosmic Smash, Toy Commander,
Rez, Street Fighter Alpha 3 and Sega Bass Fishing. Every known retail Product
number is enumerated in `dreamcast-product-variants.tsv`.
