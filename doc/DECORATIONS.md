# Screen decorations

[Back to README](../README.md)

Static PNG decorations can fill the unused area around the game without
stretching the emulated image. Enable them under **Settings > Video > Screen
decorations** or with:

```ini
retrorun_decorations = auto
# Optional. The default is a decorations directory next to retrorun.cfg.
retrorun_decorations_path = /storage/.config/retrorun/decorations
```

RetroRun loads the first matching file and keeps the converted RGB565 surface
in memory. Explicit RetroRun files take priority; after them it can reuse a
bezel already installed by the host distribution:

```text
decorations/games/<system>/<rom name>.png
decorations/games/<rom name>.png
decorations/systems/<system>.png
decorations/default.png
```

AmberELEC packs are detected in `/tmp/overlays/bezels` and
`/storage/roms/bezels`. RetroRun reads the active pack and optional system
override from `distribution.conf`, supports its full-name, short-name,
numbered and default per-game `.cfg` files, then falls back to the system PNG.
Compatible packs in `/roms/bezels` or `/roms2/bezels` are also detected for
ArkOS and user installations, either below a named pack such as `default` or
with `systems` directly below `bezels`. If AmberELEC has generated
`/tmp/raappend.cfg`, its custom viewport is reused. Detection and parsing occur
once when the game starts; no distribution-specific work is done per frame.
RetroRun only reads artwork already installed by the distribution and does not
redistribute it.

An optional `.info` file beside a PNG can define the exact game viewport using
Batocera-style `width`, `height`, `top`, `left`, `bottom` and `right` values.
For example, `default.png` can be accompanied by `default.info`:

```json
{
  "width": 1920,
  "height": 1080,
  "top": 0,
  "left": 240,
  "bottom": 0,
  "right": 240
}
```

The values describe the artwork's reference resolution and border sizes;
RetroRun scales the resulting viewport to the current display and fits the
game inside it without changing the game's aspect ratio. The viewport never
affects menus or other frontend pages. Parsing happens only when the decoration
is loaded, not during normal frame rendering.

The **Settings > Video > Decorations** menu can download a small selection of
static borders directly from `libretro/common-overlays`. RetroRun downloads
only the selected PNG, converts its libretro viewport to a local `.info` file
and stores attribution beside it under `downloads/libretro`. It never downloads
or extracts the complete repository. Installed artwork is licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) and can be updated or
removed from the same menu. Downloads run in the background while the menu
remains responsive.

The same menu separates the configured source from the artwork currently in
use. `Source: Automatic` follows an installed distribution pack, while
`Source: RetroRun`, `Source: AmberELEC: <pack>` and `Source: ArkOS: <pack>`
select one explicitly. `Using:` reports the source that actually matched the
current game. Distribution choices appear only when their directories are
present; the scan runs during menu initialization and never per frame.

`<system>` is the first directory after `roms` or `roms2` (falling back to the
ROM's parent directory), and `<rom name>` is the filename without its
extension. This also works when games are grouped in subdirectories. The local
layout is compatible with the core of Batocera's decoration naming scheme.
Images are installed separately and no third-party artwork is bundled with
RetroRun. If no PNG matches, no decoration is composited and the game keeps
its regular video layout. PNG alpha is preserved and matching artwork is
composited over the game, allowing curved frames to mask the rectangular core
image correctly. On GO2 this source-over operation is handled by RGA hardware;
the full artwork is cached in all three presenter framebuffers and normally
only the game-damaged rectangle is blended again.
