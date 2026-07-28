# Flycast 2022 Low-End RG351V profiles

These files preserve the per-game RetroRun configurations used during the
RG351V Flycast 2022 Low-End investigation. They are snapshots of tested or
retained profiles, not global defaults.

See [GAME_PROFILE_MODES.md](GAME_PROFILE_MODES.md) for the implemented
`disabled`, `best_validated` and `best_performance` selection modes and the
current per-game differences.

`flycast-game-catalog.ini` is the editable source form of catalog version
`20260736`. The same data is built into RetroRun, so the feature works when
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
