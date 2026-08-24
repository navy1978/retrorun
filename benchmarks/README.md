# RetroRun benchmark archive

Each directory represents one reproducible benchmark session and follows this
naming convention:

```text
device-os-core-game-date
```

Names are lowercase ASCII and use hyphens as separators. Every session keeps
its summary in `report.md`; raw measurements are separated into `screening/`
and `finals/`, with `diagnostics/` added when supplementary investigation logs
exist.

| Session | Device / OS | Core and game | Result |
|---|---|---|---|
| [`rg353m-darkos-flycast-soul-calibur-2026-07-20`](rg353m-darkos-flycast-soul-calibur-2026-07-20/report.md) | RG353M / dArkOS | Flycast / Soul Calibur | SDL2 marginally faster; GO2 avoids SDL queue pressure |
| [`rg351v-amberelec-flycast2021-soul-calibur-2026-07-20`](rg351v-amberelec-flycast2021-soul-calibur-2026-07-20/report.md) | RG351V / AmberELEC | Flycast 2021 / Soul Calibur | GO2 best performance and repeatability; persistent audio underruns |
| [`rg351v-amberelec-parallel-n64-mortal-kombat-4-2026-07-20`](rg351v-amberelec-parallel-n64-mortal-kombat-4-2026-07-20/report.md) | RG351V / AmberELEC | ParaLLEl N64 / Mortal Kombat 4 | GO2 lowest frame time; SDL2 passes strict audio criteria |
| [`rg552-amberelec-flycast-soul-calibur-2026-07-21`](rg552-amberelec-flycast-soul-calibur-2026-07-21/report.md) | RG552 / AmberELEC | Flycast / Soul Calibur | Threaded-video GO2 best p95; both backends record audio faults |
| [`rg552-amberelec-parallel-n64-goldeneye-007-2026-07-21`](rg552-amberelec-parallel-n64-goldeneye-007-2026-07-21/report.md) | RG552 / AmberELEC | ParaLLEl N64 / GoldenEye 007 | Cached interpreter required; GO2 fastest, SDL2 audio-clean |
| [`rg351mp-darkos-flycast2021-jet-grind-radio-2026-08-24`](rg351mp-darkos-flycast2021-jet-grind-radio-2026-08-24/report.md) | RG351MP / dArkOS | Flycast 2021 / Jet Grind Radio | Fast depth plus opaque merge: +7.18%, audio clean, visually approved |
| [`rg351mp-darkos-flycast2021-cannon-spike-2026-08-24`](rg351mp-darkos-flycast2021-cannon-spike-2026-08-24/report.md) | RG351MP / dArkOS | Flycast 2021 / Cannon Spike | Opaque merge: +13.2%, audio clean; fast depth rejected for visible artifacts |
| [`rg351mp-darkos-flycast2021-daytona-usa-2026-08-24`](rg351mp-darkos-flycast2021-daytona-usa-2026-08-24/report.md) | RG351MP / dArkOS | Flycast 2021 / Daytona USA | Per-strip alpha plus opaque merge: +46.6%, audio clean and visually approved |

The benchmark implementation runbook remains at the repository root because
it documents the implementation and validation procedure rather than one test
session.
