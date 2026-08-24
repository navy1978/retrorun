# Flycast 2022 Low-End RK3326 profiles

These files preserve the per-game RetroRun configurations used during the
RG351V and RG351MP Flycast 2022 Low-End investigations. They are snapshots of
tested or retained profiles, not global defaults.

See [GAME_PROFILE_MODES.md](GAME_PROFILE_MODES.md) for the implemented
`disabled`, `best_validated` and `best_performance` selection modes and the
current per-game differences.

`flycast-game-catalog.ini` is the editable source form of catalog version
`20260826`. The same data is built into RetroRun, so the feature works when
distributions install only the executable. A copy beside RetroRun is used only
when its `catalog_version` is greater than the built-in version.

When `retrorun_flycast_catalog_update = auto`, RetroRun also checks the
repository copy at most once per day without blocking game startup. A newer
catalog is validated against the supported schema and setting allowlist,
written atomically as `flycast-game-catalog.cache.ini` beside the active
configuration file, and considered from the next launch.

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

Sonic Adventure, Shenmue, Resident Evil: Code Veronica, Power Stone retail releases, Power Stone 2,
Skies of Arcadia, Capcom vs. SNK 2, Phantasy Star Online,
The House of the Dead 2, NFL 2K, NFL 2K1, Sega Rally 2, Hydro Thunder,
F355 Challenge, Virtua Fighter 3tb, Cosmic Smash,
Toy Commander, Rez, Street Fighter III: 3rd Strike,
Street Fighter Alpha 3 and Sega Bass Fishing are included with every retail
Product Number enumerated in `dreamcast-product-variants.tsv`.
