# Flycast 2022 Low-End RK3326 profiles

These files preserve the per-game RetroRun configurations used during the
RG351V and RG351MP Flycast 2022 Low-End investigations. They are snapshots of
tested or retained profiles, not global defaults.

See [GAME_PROFILE_MODES.md](GAME_PROFILE_MODES.md) for the implemented
`disabled`, `best_validated` and `best_performance` selection modes and the
current per-game differences.

`flycast-game-catalog.ini` is the editable source form of catalog version
`20260901`. The same data is built into RetroRun, so the feature works when
distributions install only the executable. A copy beside RetroRun is used only
when its `catalog_version` is greater than the built-in version.

When `retrorun_flycast_catalog_update = auto`, RetroRun also checks the
repository copy at most once per day without blocking game startup. A newer
catalog is validated against the supported schema and setting allowlist,
written atomically as `flycast-game-catalog.cache.ini` beside the active
configuration file, and considered from the next launch.

Catalog version `20260901` separates correctness-first settings for cataloged,
title-only baselines from the performance defaults inherited by explicitly
validated profiles. Those baseline entries now use per-triangle alpha and
disable translucent/opaque merging, fast depth and texture-storage reuse,
while leaving the previous low-end audio choices unchanged. Unknown Product
numbers remain entirely controlled by the active `retrorun.cfg`.

The filename includes the Dreamcast product number printed by Flycast at boot.
Select profiles by product number rather than by ROM filename:

| Product number | Game | Status |
| --- | --- | --- |
| `MK-51117`, `HDR-0165` | Sonic Adventure 2 | Retail European/North American and Japanese variants. `best_performance` adds opaque-strip merging and the validated `lowend_stable_96` GO2 audio preset. A fixed 300-frame RG351V comparison presented 299 frames in both modes while the preset improved throughput from 33.47 to 36.29 FPS and reduced audio underruns from 19 to 2. Shadows, menus, audio and gameplay were manually approved. |
| `RDC-0140`, `RDC-0149`, `T8116D 50`, `T3602M`, `T3601M`, `T3601N` | Dead or Alive 2 | Observed CDI images plus the Redump retail regional variants. Mapping the observed `RDC-0140` image to the approved profile measured about 41.2 FPS versus 31.6 FPS without it. |
| `T1401D  50`, `T1401M`, `T1401N` | Soul Calibur | European, Japanese and North American retail variants. Both profiles require `framerate=normal` and disabled frontend pacing: on RG353M the optimized `fullspeed` configuration ran worse than the unoptimized `normal` configuration. |
| `MK-51035`, `HDR-0053` | Crazy Taxi | European/North American and Japanese retail variants. Accurate control profile. No experimental performance candidate produced a repeatable useful gain. |
| `T38706M` | Ikaruga | Japanese retail release. Adaptive v9 profile with accurate per-triangle alpha sorting; manual RG351V review confirmed that it removes the ship rectangles while preserving excellent gameplay. |
| `T1212N`, `T7010D 50`, `T1215M` | Marvel vs. Capcom 2 | North American, European and Japanese retail variants. The approved v28/v9 adaptive profile uses per-triangle alpha sorting to correct the 2D fighter sprites. On the fixed 600-frame USA save state it measured 49.1 FPS, 300 presented frames and zero audio underruns; the faster inaccurate sorter reached 52.1 FPS but visibly corrupted sprites. |
| `MK-51054`, `HDR-0113` | Virtua Tennis / Power Smash | European, North American and Japanese retail variants. The approved RG351V profile combines linear vertex depth with adaptive core skipping and the `hud_last` translucent merge strategy. Manual review confirmed correct court lines, service power gauge, audio and gameplay speed without the broad merge-disable performance penalty. |
| `MK-51058` | Jet Set Radio / Jet Grind Radio | The North American retail image was validated on RG351MP/dArkOS from a fixed gameplay savestate. Accurate per-triangle alpha, disabled translucent merging, fast depth and opaque-strip merging averaged 27.86 presented frames/s versus 25.99 for the conservative baseline (+7.18%). All nine final runs recorded zero audio underruns, overruns or dropped audio frames, and both fast depth alone and the final combination passed manual gameplay review. `best_performance` falls back to this validated profile. |
| `T1215N` | Cannon Spike | The North American retail image was validated on RG351MP/dArkOS from a fixed gameplay savestate. Opaque-strip merging averaged 38.00 presented frames/s versus 33.56 for the conservative baseline (+13.2%), with every frame presented and no audio faults. Fast depth was faster but rejected because it produced obvious graphical artifacts. `best_performance` falls back to the visually approved opaque-only profile. |
| `MK-51037` | Daytona USA 2001 / Daytona USA | The North American retail image was validated on RG351MP/dArkOS from a fixed race savestate. Per-strip alpha sorting plus opaque-strip merging averaged 23.75 presented frames/s versus 16.20 for the conservative baseline (+46.6%), reduced active-frame p95 from 89.27 to 58.52 ms and recorded no skipped frames or audio faults. Both the sorter and final combination passed manual race review. `best_performance` falls back to this validated profile. |
| `MK-5118450` | Shenmue II (Europe) | The European retail image was tested on RG351MP/dArkOS from fixed 3D savestates. Per-strip alpha plus `vertex_fast_log` improved the original 300-frame screen from 14.85 to 19.00 presented frames/s (+27.9%) and reduced active-frame p95 from 91.40 to 54.43 ms; graphics passed manual review. On a later, heavier state with CPU, GPU and DMC governors at `performance`, the `lowend_heavy_100` profile and a 4096-frame buffer reduced the three-run median from 10 to 4 audio underruns and queue-low observations from 98 to 11 versus the 55% WSOLA reference, while retaining 100% playback speed and essentially unchanged throughput (17.38 versus 17.39 frames/s). Adaptive core frameskip reduced underruns to 2 but was rejected because it presented only 160 of 300 frames; direct scanout was unavailable and its fallback was slower. Pinning the OpenAL and frontend audio workers to a reserved fourth CPU raised throughput to 18.48 frames/s but increased median underruns from 4 to 15 and introduced 172 backpressure events, so single-thread audio remains selected. Occasional gaps in the heaviest scene remain a documented RK3326 limitation. Japanese `HDR-0164` and `HDR-0179` releases remain conservative baselines until tested. |
| `T7013D50`, `T1213N`, `T1209M` | Street Fighter III: 3rd Strike | The European, North American and Japanese retail releases use distinct catalog records with the same RG351MP-validated settings. On a fixed USA fight savestate, accurate per-triangle alpha, `vertex_fast_log` depth and opaque-strip merging reached a three-run median of 47.76 FPS versus 44.39 (+7.58%), reduced active-frame p95 from 43.63 to 41.43 ms and recorded no audio underruns or empty queues. The inaccurate per-strip sorter was slower and visually unsafe. Each regional `best_performance` profile explicitly inherits its corresponding `best_validated` profile. |
| `MK-51019`, `HDR-0010` | Sega Rally 2 | European/North American and Japanese retail variants. The RG353M-specific `best_performance` profile preserves the visually approved 640x480 renderer and accurate audio path, disables frontend pacing and selects the validated 735-frame/60 ms prebuffer configuration. With the Cortex-A55 `-O3`/LTO Flycast build at `62085539`, the fixed race savestate produced 27,017.7 audio samples/s versus 21,412.9 for the stock core (+26.2%) and about 20.26 versus 14.52 presented frames/s (+39.5%). Graphics and audio were manually approved on the device. |

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

The catalog also recognizes the following retail games and regional product
numbers with the conservative built-in defaults. These entries are coverage
baselines, not RG351V performance approvals; promote them only after repeatable
benchmarking and manual audio/video review.

Sonic Adventure, Shenmue, Shenmue II, Resident Evil: Code Veronica, Power Stone retail releases, Power Stone 2,
Skies of Arcadia, Capcom vs. SNK 2, Phantasy Star Online,
The House of the Dead 2, NFL 2K, NFL 2K1, Hydro Thunder,
F355 Challenge, Virtua Fighter 3tb, Cosmic Smash,
Toy Commander, Rez, Street Fighter III: 3rd Strike,
Street Fighter Alpha 3 and Sega Bass Fishing are included with every retail
Product Number enumerated in `dreamcast-product-variants.tsv`.
