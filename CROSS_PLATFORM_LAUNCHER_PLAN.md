# Cross-platform Launcher and Libretro Integration Plan

## Purpose

This document describes a future RetroRun milestone focused on two areas:

1. a launcher and content browser usable on both SDL2 desktop systems and
   GO2/Anbernic devices;
2. implementation of missing libretro frontend interfaces, especially disk
   control, keyboard callbacks and core-option visibility.

The implementation must preserve the existing lightweight GO2 backend and keep
platform-specific functionality behind the abstraction in `src/platform.h`.

## Goals

- Allow RetroRun to start without mandatory core and ROM command-line arguments.
- Browse cores and content using the existing OSD and a controller.
- Keep command-line launching fully supported and backward compatible.
- Allow unloading a game and returning to the launcher without exiting RetroRun.
- Share launcher, session and libretro-interface logic between SDL2 and GO2.
- Add desktop conveniences such as drag-and-drop without making them mandatory.
- Avoid loading complete ROM collections or artwork databases into memory on
  handheld devices.

## Non-goals for the first version

- A separate native Cocoa, Win32 or GTK launcher.
- Mandatory cover-art scraping or online metadata services.
- Replacing the current OSD/menu renderer.
- Removing existing shell-script and command-line launch workflows.
- Implementing libretro Vulkan support as part of this milestone.

## Shared launcher design

The launcher should be an OSD screen rendered by RetroRun itself. It should not
depend on a desktop window toolkit, so the same navigation can be used with a
controller on GO2 and SDL2.

Initial launcher features:

- filesystem browser;
- configurable content roots;
- configurable core directories;
- filtering by file extension;
- selection of a core and content file;
- remembered core per extension or system;
- recent games;
- favourites;
- BIOS/system-directory validation;
- launch errors displayed through the OSD;
- return to the launcher after unloading content.

All ways of selecting content should produce the same shared request:

```cpp
struct LaunchRequest {
    std::string core_path;
    std::string content_path;
};
```

The command line, OSD browser, recent-games list and SDL drag-and-drop should
all create a `LaunchRequest` and pass it to the same session-management code.

## Platform differences

| Capability | SDL2 desktop | SDL2 handheld | Native GO2 |
| --- | --- | --- | --- |
| OSD filesystem browser | Yes | Yes | Yes |
| Controller navigation | Yes | Yes | Yes |
| Physical keyboard | Yes | Optional | Optional evdev device |
| Drag-and-drop | Yes | Usually unavailable | No |
| Native file dialog | Optional enhancement | No | No |
| Default content roots | User configuration | Distribution ROM paths | Distribution ROM paths |
| Core discovery | Configured/RetroArch directory | Distribution directory | Distribution directory |

Suggested default content roots for handheld distributions include
`/roms`, `/storage/roms` and distribution-specific paths. They must remain
configurable rather than hard-coded as the only choices.

Directory enumeration should be lazy and paginated. The launcher must not scan
an entire large collection or decode all artwork during startup.

## Application lifecycle refactor

The current lifecycle assumes that a core and ROM are supplied before entering
the main loop. The target lifecycle is:

```text
Initialize platform and frontend
             |
             v
          Launcher
             |
             v
    Load core and content
             |
             v
       Emulation session
             |
             v
  Unload content and core
             |
             v
          Launcher
```

A shared `SessionManager` should own the transitions between these states. It
should make shutdown ordering explicit for audio, video, input, core callbacks,
save data and dynamic libraries.

Suggested states:

```cpp
enum class SessionState {
    Launcher,
    Loading,
    Running,
    Unloading,
    Error,
    Exiting
};
```

Required compatibility behaviour:

- When core and content are supplied on the command line, RetroRun may launch
  them immediately as it does today.
- A new `Return to launcher` menu action should unload the current session.
- The existing `Quit` action should still terminate the application.
- SRAM and automatic state handling must complete before core unload.
- A failed launch must return to the launcher instead of leaving partially
  initialized audio/video state behind.

## Suggested shared components

```text
src/launcher/
  content_browser.*
  core_registry.*
  launch_request.*
  recent_content.*

src/session/
  session_manager.*

src/libretro/
  disk_manager.*
  keyboard_dispatcher.*
  core_option_visibility.*
```

Names and exact locations may be adapted to the existing source layout, but
the responsibilities should remain separated.

## Libretro disk-control interface

RetroRun currently reports the disk-control environment commands as
unimplemented. The frontend should store the callbacks supplied by the core and
provide a shared read-only/action menu such as:

```text
Disc
  Status: Inserted
  Eject / Insert
  Current disc: 1 of 2
  Previous disc
  Next disc
  Disc 1
  Disc 2
```

Required environment support includes the disk-control interface version and
the basic or extended disk-control callback structures exposed by libretro.

This feature belongs in shared frontend code. SDL2 and GO2 require no separate
disk implementation because the core owns the actual virtual media handling.

Acceptance criteria:

- Flycast no longer reports the supported disk-control commands as unhandled.
- Eject, insert and disk-index changes work from the OSD.
- Invalid indexes are rejected safely.
- The menu handles single-disc content without showing irrelevant actions.
- Disk state is cleared when unloading a core.

## Core-option visibility

RetroRun currently receives core-option display updates without applying them.
The shared core-options model should track whether each option is visible and
exclude hidden entries from the OSD.

Acceptance criteria:

- `SET_CORE_OPTIONS_DISPLAY` updates the corresponding menu item.
- Visibility changes do not destroy the selected option value.
- If the selected row becomes hidden, menu selection moves to a valid row.
- Option state is cleared when switching cores.

## Keyboard callback

The libretro-facing keyboard dispatcher should be shared. Platform backends
only translate their native events into a portable RetroRun key event:

```cpp
struct RRKeyEvent {
    bool down;
    unsigned keycode;
    uint32_t character;
    uint16_t modifiers;
};
```

The shared dispatcher forwards this to the keyboard callback registered by the
active core.

### SDL2 implementation

- Consume `SDL_KEYDOWN`, `SDL_KEYUP` and, where appropriate, `SDL_TEXTINPUT`.
- Translate SDL keycodes and modifiers to libretro key values.
- Keep frontend hotkeys separate so opening the menu does not unintentionally
  type into the emulated system.
- Track key-up events even when the menu opens to avoid stuck keys.

### GO2 implementation

- Expose the shared interface even when no keyboard is connected.
- Optionally read USB/Bluetooth keyboards through an evdev keyboard source.
- Do not make a keyboard mandatory for normal handheld operation.
- A future OSD virtual keyboard can emit the same `RRKeyEvent` structure.

Acceptance criteria:

- Cores no longer report the keyboard callback as unimplemented.
- SDL key-down and key-up events reach a test core correctly.
- Frontend hotkeys remain functional and do not become stuck core keys.
- GO2 continues to work normally without a keyboard.

## Platform abstraction additions

The exact API should be designed during implementation, but likely capabilities
include:

- optional file-drop events;
- portable key events;
- platform capability flags;
- configurable filesystem roots;
- optional native file-picker request;
- notification when removable controllers or keyboards appear/disappear.

Prefer capability queries over scattered compile-time checks. For example:

```cpp
enum RRPlatformCapability {
    RRPlatformCapability_FileDrop,
    RRPlatformCapability_PhysicalKeyboard,
    RRPlatformCapability_NativeFilePicker
};
```

The shared launcher must continue to work when every optional capability is
false.

## Configuration ideas

Potential configuration keys:

```ini
retrorun_start_in_launcher = true
retrorun_content_roots = /roms;/storage/roms
retrorun_core_roots = /usr/lib/libretro
retrorun_remember_core_by_extension = true
retrorun_recent_content_limit = 20
```

Defaults must preserve current behaviour when core and content are provided on
the command line. Paths and separators need a portable representation before
the final key format is chosen.

## Desktop-only enhancements built on the shared launcher

After the shared browser works, SDL2 can add:

- ROM drag-and-drop;
- optional native file dialog;
- opening content passed by the operating system;
- keyboard search in long directories;
- multiple content/core roots configured from the menu.

These are input methods for the shared launcher, not a separate launcher.

## Implementation order

Recommended sequence:

1. Implement disk-control interfaces and their shared OSD menu.
2. Implement core-option visibility.
3. Define and test explicit session load/unload ownership.
4. Introduce `SessionManager` while preserving command-line startup.
5. Allow startup without a core and content file.
6. Implement the lazy OSD filesystem browser.
7. Implement core discovery and extension associations.
8. Add `Return to launcher` and robust failed-launch recovery.
9. Add SDL2 drag-and-drop.
10. Implement the shared keyboard dispatcher and SDL2 translation.
11. Add optional GO2 evdev keyboard input.
12. Add recent games, favourites and optional metadata incrementally.

Disk control and core-option visibility should be delivered before the launcher
refactor because they are smaller, independently testable compatibility fixes.

## Risks and safeguards

### Core unload and reload

Some cores retain threads, graphics contexts or callbacks during shutdown.
Centralize teardown and test repeated core switches, not only application exit.

### Audio/video resource ownership

Do not leave SDL, OpenGL, GO2, audio or presenter objects owned implicitly by
global state across sessions. Teardown must be idempotent after partial startup.

### Handheld memory use

Use lazy directory iteration, bounded recent lists and on-demand thumbnails.
Avoid building a full in-memory game database in the first version.

### Input focus

Separate launcher/menu input from core keyboard input. Flush or synthesize key-up
events when changing focus to prevent stuck emulated keys.

### Backward compatibility

Keep current CLI arguments, configuration locations and GO2 launch scripts
working throughout the refactor.

## Overall completion criteria

The milestone is complete when:

- RetroRun can start directly in the launcher on SDL2 and GO2;
- both backends can browse and launch a core/content pair using the OSD;
- command-line launching remains compatible;
- closing a game returns safely to the launcher;
- at least two different cores can be launched sequentially in one process;
- disk switching works in a supported multi-disc core;
- core-option visibility updates the OSD;
- SDL2 keyboard callbacks work with a keyboard-capable core;
- GO2 remains usable without keyboard or desktop-specific facilities;
- no platform-specific launcher logic leaks into the shared session manager.
