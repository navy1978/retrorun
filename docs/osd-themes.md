# Built-in OSD themes

`retrorun_osd_theme = classic` preserves the traditional RetroRun appearance.
`retrorun_osd_theme = retrorun` selects the RetroRun OS/Launcher palette.
Missing, empty or invalid settings resolve to Classic, including after a previous
selection. Names are lowercase. Selection is applied during configuration loading,
before render workers start; changing the file requires restarting RetroRun.
The option participates in the existing frontend configuration override mechanism.
There is no external theme parser, resource directory or new dependency.

## Existing rendering architecture

- `src/video/video-helper.cpp`: `showInfo`, `drawSelectedMenuRow`,
  `drawMenuRowColumns`, information pages, credits and FPS surfaces.
- `src/video/ui-renderer.cpp`: overlay routing and cached RGB565 popup surfaces;
  slot/save/load/progress, screenshot, pause, fast-forward and informational icons.
- `src/video/fonts.cpp`: existing bitmap glyph rasterizers (8x8, 6x8 and doubled
  8x8); no SDL_ttf/NanoVG font engine is involved in this OSD.
- `src/services/achievements.cpp`: achievement pages, badge cache and notifications.
- `src/input/keyboard.cpp` and `src/services/file_browser.cpp`: independent pages
  using the same bitmap primitives and now the same palette.
- `src/platform/platform_sdl.cpp`: SDL texture composition and a separate GL/GLES
  overlay path for hardware-rendered cores.
- `src/platform/platform_go2.cpp`, `src/go2/display.cpp`: surface mapping and RGA
  composition for the native GO2 backend.

There is no separate runtime volume HUD in this revision. Volume is a menu value
and receives the menu theme; hardware/distribution volume overlays are outside
this renderer. No new volume/input behavior was introduced.

## Theme abstraction

`src/video/osd-theme.h` contains immutable RGB565 `Palette`, `Metrics` and `Theme`
definitions and a startup-selected pointer. Renderers consume semantic properties,
not theme identities. Classic records the existing colors individually, including
its slightly different greens/oranges, notification bevels, shadows and the
backend-specific blue row color. The font assets and fixed bitmap glyph sizes
are preserved. Metrics control row spacing, margins, frame height, popup text
placement and the handheld canvas. Icon design grids remain fixed artwork.

RetroRun reuses the Launcher's exact focus red `#C30019` and Favorites/active gold
`#FFCD40`, quantized once at compile time to RGB565. White is primary content,
gray is secondary/disabled, red is focus/error, gold is success/enabled state.
Boolean getter values (`yes/no`, `On/Off`, etc.) are colored independently from
the focused row. Numeric/Auto values are not falsely advertised as enabled.
No enabled checkmark is added to OFF. Menu value getters and apply/save semantics
are unchanged.

RetroRun uses a 320x240 handheld canvas, 14px logical row spacing, a compact
control footer and up to ten visible rows centered around the existing selection.
Classic retains 240x160, the existing 10/12px rows and Catalog paging treatment.
The bitmap fonts scale through the existing backend projection; no font loading,
resampling or text texture allocation is added by the theme abstraction.

## Composition and exceptions

The ordinary RetroRun game menu has global opacity 220/255. SDL restores texture
alpha/blend mode after each transparent copy. The GL/GLES overlay path uses
constant-alpha blending in the existing pipeline and restores blend color/factors.
GO2 draws the retained game surface before blending the RGB565 menu with RGA's
existing global-alpha source-over mode (`0x0105`, as implemented by the bundled
librga). Classic remains opaque and keeps the GO2 path that skips the hidden game.
No decoration shaders, extra render targets or animation threads were added.

Loading/credits pages, keyboard, disk browser and achievement details remain
opaque so their content stays readable; their text/panels still follow the theme.
Original logos, achievement badge pixels and screenshot/save-state images keep
their own content colors. Credits animation timing is unchanged. The fullscreen
black clearing/loading helpers remain rendering operations rather than theme
colors. No game/core settings, input mapping or emulator rendering are modified.

## Validation

`make -C tests run` includes palette/default/fallback/boolean state tests.
The off-screen fixture in `tests/osd_render_probe.cpp` uses actual menu, keyboard
and slot renderers plus the real SDL texture compositor; its synthetic checker
background requires no ROM, BIOS, display or game. It checks texture state reset.
After a native SDL build, build the fixture from `build/macos` or `build/linux-sdl`:

```
make -f Makefile -f ../../tests/osd_render_probe.mk config=debug osd-probe
/tmp/retrorun-osd-probe/probe /tmp/osd-classic classic
/tmp/retrorun-osd-probe/probe /tmp/osd-retrorun retrorun
```

Classic menu, slot popup and keyboard RGB pixel captures were compared with
renderers compiled directly from the unchanged base commit; all three matched.
The clock footer is excluded from that pixel comparison because it is live data.
The same captures are not a complete hardware visual regression suite.

General RetroRun-theme trials on RG351MP were accepted with GO2 and SDL2.
The broader regression checklist remains: software core and hardware core menus,
GO2/RGA translucency, rotation, long menus, active/OFF values, save/load/progress,
FPS, keyboard, achievements and closing/reopening without stale overlays. Check
both Classic and RetroRun. No installation or hardware result is implied by a
successful cross-build.

On macOS, `RR_OSD_GL_TEST=1` also checks the actual OpenGL overlay blend against
known pixels and verifies that blend color and enable state are restored. This
uses a hidden GL window and requires a graphics session. The software-composition
fixture still runs without that optional check.
