# Runtime failures and game return

RetroRun's stable process exit codes let the launcher explain failures without
parsing diagnostic log text. Success remains 0; errors remain nonzero for legacy
AmberELEC/ArkOS launch scripts. The numeric contract is mirrored in
`src/launch/RuntimeExitCodes.h` and RetroRun's `src/core/runtime_exit.h`.

| Code | Failure |
| --- | --- |
| 64 | Invalid launch arguments |
| 65 | Core library cannot be loaded |
| 66 | Required libretro entry point is missing |
| 67 | Content missing, empty or unreadable |
| 68 | ZIP preparation/extraction failure |
| 69 | Core rejected the content |
| 70 | Video initialization failure |
| 71 | Audio initialization failure |
| 72 | Requested save-state slot is unavailable |
| 73 | Core initialization exception |
| 74 | Device/platform initialization exception |
| 75 | Memory allocation failure |
| 76 | Unhandled runtime exception |

The session supervisor preserves the actual child status/signal for logs and test
receipts. For normal game sessions it forwards a failure to the restarted GUI;
the GUI restores selection and opens a persistent translated dialog. B dismisses
it. A signal is reported as an unexpected termination, not misidentified as a
missing BIOS or corrupt game. A core returning false does not identify a precise
cause; its message suggests checking format and required BIOS rather than claiming
one is missing. The detailed log remains available in Tools > Logs.

External commands keep a generic failure message unless the supervisor itself
cannot execute them (126/127). Emulator Test Suite owns its existing result page
and does not get an extra game-error modal. Service shutdown does not restart the
GUI or create a spurious failure dialog.

Managed profiles keep game logging beside the original base configuration so
Tools > Logs reads the latest game attempt. In log viewing, only bracketed
`[WARN]`/`[WARNING]` and `[ERROR]` level labels receive yellow/red. Timestamp,
message, stack-trace and continuation text retain the normal color. A leading
`[INFO]`/`[DEBUG]` prevents body words from being mistaken for the level.

Stella compatibility also requires honoring its libretro VFS request and ignoring
unnamed controller descriptors. The frontend provides the standard local POSIX
VFS v3 callbacks. Native tests cover version negotiation, read/write/update,
truncate, 64-bit file sizes and directory iteration. An original synthetic Atari
ROM reproduces the previous controller crash and exercises the corrected load /
run / unload lifecycle without game assets.

The frontend polls input once after retro_run only if the core omitted its input callback, keeping hotkeys available on core error screens.

Managed GO2 sessions animate the shared loading artwork before content loading; SDL keeps graphics work on its owning thread.
