# Save-state previews

After a successful state save, RetroRun attempts to write a PNG beside the actual
state filename: `<state filename>.png`. Numbered slots and core-specific naming
therefore remain distinct without another index. Existing states are unchanged;
a preview appears the next time the slot is saved successfully.

The capture uses the current game frame before UI overlays. It preserves aspect
ratio, fits within 320x240, never upscales, and applies logical game rotation.
Software and OpenGL paths share the screenshot conversion/PNG writer. Capturing a
preview does not add a screenshot gallery item, flash, sound or notification.
There is no periodic capture, new dependency or per-frame image processing.

State serialization/write failures leave the previous state intact and report a
save failure. States and previews use temporary sibling files and atomic rename.
An obsolete preview is invalidated before replacing its state. PNG failure does
not invalidate a successful state save; the slot simply has no preview. Very long
legal filenames can save even if appending `.png` exceeds the filesystem limit.
A crash between the two renames may leave a valid state without a preview.

## Current limitations

The existing three-slot filenames and automatic-load policy are preserved.
In particular, manual Slot 1 currently shares the automatic-state filename.
The preview feature does not separate those states or change autosave defaults. Compatibility still
depends on the core/version and state format. Do not treat a PNG as proof that a
state is compatible or enable autosave blindly over an existing Slot 1.

## Verification

`make -C tests run` covers atomic write/capture failures, old-thumbnail removal,
slot isolation, temporary-file cleanup, long filenames, bounded sizing, portrait,
cropping and rotation. The transition test exercises actual SaveState failure
and successful-state/failed-image behavior.

`tests/state_preview_core.cpp` is an original diagnostic core, built explicitly
for development, never bundled as a system emulator. With the RG351MP toolchain:

```sh
$CXX -std=c++17 -shared -fPIC -I src/core -DPREVIEW_TEST_HW=0   tests/state_preview_core.cpp -o state_preview_software.so -lGLESv2
$CXX -std=c++17 -shared -fPIC -I src/core -DPREVIEW_TEST_HW=1   tests/state_preview_core.cpp -o state_preview_opengl.so -lGLESv2
```

Actual RG351MP checks used isolated saves/configuration, the diagnostic core and
120 generated frames; both software and GLES produced an 8-byte state plus a
320x240 PNG. A separate GLES direct-scanout run recorded 91 direct frames and
saved its preview successfully. A requested threaded mode resolved to the normal
fallback because this device reports threaded video unsupported; the worker
path was not hardware-validated. Launcher tests use 640x480 offscreen SDL.
Commercial games, existing user states and physical controller acceptance were
not exercised by these diagnostic checks.


## Explicit launcher resume

`--resume-slot 1`, `2` or `3` restores exactly that manual slot, even when normal
auto-load is disabled. The path uses the runtime's existing core-specific naming;
a missing selected state fails rather than quietly starting a new session.
`--start-new` bypasses state loading and disables automatic saving for this
session only, preserving existing states unless the user manually saves.
It cannot be combined with `--resume-slot`. Ordinary launches and `--restart`
retain their existing behavior.

The launcher obtains available slots, timestamps and preview paths from the
independent core service, and presents them under Activity -> Continue Playing.
It selects the newest available state for the resolved core, without pretending
to know binary state compatibility across core versions. Runtime unserialization
remains authoritative. The selection screen does not scan files during rendering.
Actual RG351MP diagnostic check: Slot2 counter7000 restored and advanced to7119;
fresh start preserved both existing slots and the PNG, verified by hashes.
