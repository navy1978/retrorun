# Mupen64Plus-Next threaded input

The tested core is Mupen64Plus-Next 3.0-GLES3, source15bada85906593376e8fd697f1b0b3af3a20f512, ELF SHA25685c4d964793d5dd919d92f7ee4e77f58baea90b4b4d338f8d1d8e4ef5bbd4938.
Its `mupen64plus-core/src/main/main.c:main_check_inputs` deliberately skips
`poll_cb()` for threaded GLideN64, expecting frontend early polling.
`retro_run` submits rendering on the frontend thread while its emulation
pthread invokes `input_cb`. Evdev activity alone cannot refresh RetroRun's
frontend copy or run hotkeys. RetroRun3.2.4 relied on the skipped callback;
the later generic post-run fallback restores refresh but does not establish
safe ownership for the asynchronous state reader.

For Mupen64Plus-Next + ThreadedRenderer=True + GLideN64 only, RetroRun now
updates controller/hotkeys on the main thread immediately before `retro_run`.
It publishes mapped joypad and analog values in lock-free 32-bit atomics;
worker callbacks read these values without accessing UI, SDL, settings,
mutable state pointers or analog-detection flags. Each stick pair and joypad
mask is coherent; a running asynchronous reader may naturally cross an update
boundary. No render settings, core code, core binary or framebuffer code change.
False and other cores retain their existing callback timing.

GO2's full-state reader now uses the producer's gamepad mutex, including initial
publication; this mutex is initialized/destroyed explicitly. It covers a small
state copy, never `retro_run` or presentation. No new thread, spin loop, frame
queue, or frame delay is introduced.

The actual True-mode exit also exposed a null call: this core unconditionally
invokes RetroArch-private GET_CLEAR_ALL_THREAD_WAITS_CB on unload. RetroRun
provides a valid callback for commands0/1: there are no RetroArch wait gates to
cancel here. Threaded Mupen is unloaded/joined with its EGL/audio objects still
alive, before context teardown. The native controller discovery installs legacy
signal handlers; the frontend shutdown owner is installed after that discovery.
These details are required for the tested signal/menu exit, rather than masking
an exit crash with SIGKILL.

## Optional diagnostics

Set `RR_INPUT_TRACE=1` for a diagnostic run. It defaults OFF. Counters are
aggregated every approximately2seconds on the main thread: loop, retro_run
entry/exit, evdev events/publications, frontend updates, input_poll, input_state,
nonzero input_state returns and EGL framebuffer requests. Each counter includes
the last caller thread identity (an opaque process-local ID). Callback hot paths
perform only opt-in relaxed atomic increments, with no clock/read/log per frame.
A long blocked iteration delays the report; it is not an independent watchdog.
Disable existing key logging for these measurements to avoid its verbose output.

## Verification performed on RG351MP

Mario Tennis, original test core/configuration, separate config copies and saves:

| Case | Observed schedule |
| --- | --- |
| Diagnostic old-schedule control, True |120iterations/2s, poll0/update0, thousands of state calls on worker |
| Control, False |poll/update on main, approximately115–120iterations/2s |
| Fixed True, active game |120–121updates/2s on main, poll0, state on worker; injected button produces nonzero returns |
| Fixed False |poll/update/state on main; injected button produces nonzero returns |

These are short software counters on the actual handheld, not an FPS benchmark
or a claim about a completed match. The control build uses current source with
the later post-run fallback omitted to reproduce the3.2.4 schedule; it is not a
rebuild of the user's original3.2.4 ELF. Core/render settings were unchanged.
Synthetic evdev presses on the existing GO-Super Gamepad verified button delivery
and opening/closing the menu twice with L3+R3, in both modes. Both modes complete
SIGTERM cleanup and close the core library. The original3.2.4 test binary, core,
configuration and saves remain available. No production runtime aliases change.

Host full `make test`, new snapshot concurrency test, snapshot ASan/UBSan and
TSan, native macOS Release and ARM GO2/SDL2 Release builds pass. The SDK linker
still prints its existing vendor libEGL/libGLES local-symbol warnings. Sanitizers
cover the snapshot test, not the entire Mali/core process. A real button/stick
playthrough and long-session throughput/latency measurements remain manual tests.

`tests/threaded_input_trace_test.py` checks recorded before/after callback,
input, menu and shutdown evidence. The small input snapshot test checks bounds,
signed axes, masks, releases and concurrent publication/reads.

## Manual check

Stop the launcher only after leaving any running game:

```sh
systemctl stop retrorun-launcher.service
cd /storage/test
./retrorun-input-fix -c /storage/test/retrorun.cfg \
  -s /storage/test/input-regression-20261106/saves -d /storage/test \
  /storage/test/mupen64plus_next_libretro.so '/roms/n64/Mario Tennis (USA).z64'
```

Keep ThreadedRenderer=True. Check physical face buttons, Start, D-pad and both
sticks according to the existing mappings, open/close the menu with L3+R3,
then Select+Start to exit. If native left analog is explicitly forced to digital
by the original config, that existing mapping is separate from this regression;
do not silently change it during the A/B test. Repeat using the same configuration
with only ThreadedRenderer=False, then restore True. For diagnostics prefix the
command with `RR_INPUT_TRACE=1` and redirect both output streams to a test log.
Restart the launcher after exiting the emulator with `systemctl start
retrorun-launcher.service`. Do not run both processes against DRM simultaneously.


## Changed files and functions

- `src/main.cpp`: main-loop trace points, early-mode selection, native signal-handler ordering, threaded N64 unload ordering.
- `src/input/input.cpp`, `input.h`: `core_input_configure_early`, `core_input_begin_frame`, `core_input_poll`, frontend update and state snapshot publication/read.
- `src/input/async_input_snapshot.h`: bounded atomic joypad/stick snapshot.
- `src/go2/input.cpp`: evdev trace points, initial publication, full-state copy and gamepad mutex lifecycle.
- `src/core/core_loader.cpp`: private wait-hook negotiation.
- `src/video/video.cpp`: optional framebuffer-callback counter only.
- `src/diagnostics/input_trace.h`: disabled-by-default aggregate counters.
- `tests/Makefile`, `async_input_snapshot_test.cpp`, `threaded_input_trace_test.py`: snapshot and recorded hardware regression checks.

The `fix/n64-threaded-input` branch includes the input and clean-shutdown fixes. Later experimental duplicate-frame/loading changes remain outside this branch commit. RetroRun OS pins a specific commit from this branch instead of applying its older runtime patch stack. Long-session N64 memory growth is a separate unresolved core/driver issue.
