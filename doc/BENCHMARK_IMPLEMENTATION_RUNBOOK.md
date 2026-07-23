# RetroRun benchmark and remote validation runbook

## Instruction for Codex

Read this file completely, inspect the current repository state, and then carry
out the implementation, validation, remote build, deployment, and benchmarks
described below. Continue until the requested work is complete or a concrete
external blocker is reached.

Preserve unrelated local changes. Never discard, reset, overwrite, or commit
files that are not part of this task. Before editing, record `git status
--short`, the current branch, and the current commit. Use small, reviewable
commits authored as `navy1978 <ivan.morelli@gmail.com>` only after the changes
have passed the relevant checks.

SSH access is authorized only for the hosts, users, directories, compilation,
deployment, and RetroRun tests documented here. Credentials are deliberately
not stored in this repository: use credentials supplied in the invoking
conversation or SSH keys. Do not print passwords in commands, logs, files, or
the final report.

## Objective

Add an integrated benchmark mode:

```text
retrorun --benchmark 60 core_libretro.so game.rom
```

After an optional warm-up, measure exactly 60 seconds using a monotonic clock,
stop the render loop normally, synchronize outstanding work, and print at least:

```text
Core frames:              3597
Video callbacks:          3597
Presented frames:         3592
Skipped frames:              5
Duplicated frames:           0
Average core time:        11.2 ms
Average video time:        1.8 ms
Average audio time:        0.4 ms
Average frontend overhead: 0.3 ms
Missed deadlines:            7
Audio underruns:             0
Direct scanout frames:    3580
Fallback frames:            12
```

Also emit one machine-readable JSON line, prefixed with `BENCHMARK_JSON:`, so a
remote runner can collect results without parsing aligned text. Include the
release, device, backend, renderer, core name/version, declared FPS, sample
rate, measured duration, relevant settings, all counters, and timing
percentiles. Benchmark mode must not save SRAM or savestates and must exit with
a nonzero status if initialization or the measurement fails.

Normal execution must have no clock reads or atomic counter updates on hot paths
when benchmarking is disabled, beyond one cheap disabled-state branch. Keep
threaded audio and direct scanout disabled by default.

## Existing-code audit map

This map records the findings made against commit `37f7722` and the working
tree inspected on 2026-07-20. Line numbers are starting points: after editing,
locate the named function or structure again with `rg -n` and `nl -ba`.
RetroRun mostly uses free functions and C-style structures in these areas, so
the map names the actual owning function/structure rather than inventing class
names.

### CLI, main loop, and lifecycle

| Location | Owner | Finding / required change |
| --- | --- | --- |
| `src/main.cpp:83-94` | `longopts` | Add `--benchmark SECONDS` and JSON-output support. |
| `src/main.cpp:112-156` | `getopt_long` loop | Strictly validate a finite positive duration. `-c` exists in the short option string but has no long option. |
| `src/main.cpp:158-213` | startup | Apply benchmark overrides after config without persisting; keep external network/catalog work outside the measured window. |
| `src/main.cpp:221-252` | state/SRAM startup | Finish optional state loading and legacy sleeps before resetting benchmark counters. |
| `src/main.cpp:549-560` | loop setup | Add warm-up, monotonic measurement window, reset, stop, and final snapshot. |
| `src/main.cpp:605-653` | active loop / `retro_run()` | Main boundary for core frames, callback time, exclusive core time, and frontend overhead. |
| `src/main.cpp:656-691` | fast-forward metrics | Useful prototype, but presentation is counted before backend completion and metrics exist only in fast-forward. |
| `src/main.cpp:698-711` | reset/state actions | Reset does not flush audio; route every reset through one synchronized operation. |
| `src/main.cpp:721-754` | adaptive frameskip | Record adaptive skip reasons separately. |
| `src/main.cpp:757-781` | pacing | Count missed deadlines before resynchronization hides them; make frame duration updateable after runtime FPS changes. |
| `src/main.cpp:847-860` | persistent cleanup | Benchmark must not save SRAM or savestates. |
| `src/main.cpp:870-884` | SDL cleanup | Stop/join workers before core unload while keeping callback-safe backend state until core deinit. |
| `src/main.cpp:885-902` | GO2 cleanup | Audio/video are destroyed before core deinit. `exitFlag` stays `-1`, so the forced `SIGUSR1` branch is always selected. Remove it. |
| `src/core_loader.h:13-32` | `RetroCore` | Track initialized/game-loaded/deinitialized state sufficiently to guarantee exactly-once lifecycle calls. |
| `src/core_loader.cpp:308-366` | HW-render environment | SDL stores `context_destroy`; GO2 stores only `context_reset`. Store and call the GO2 destroy callback too. |
| `src/core_loader.cpp:391-395` | `SET_SYSTEM_AV_INFO` | Currently rejected. Stage new AV info and apply FPS, sample rate, and geometry after `retro_run()`. |
| `src/core_loader.cpp:575-626` | `core_load()` | Gate late callbacks during shutdown before unloading callback code. |
| `src/core_loader.cpp:657-691` | `core_load_game()` | Close the ROM file and free temporary full-content data after `retro_load_game()`; initialize audio with complete FPS/sample-rate timing. |
| `src/core_loader.cpp:700-712` | `core_unload()` | Missing `retro_unload_game()`, state clearing, and handle clearing. Replace with the unified synchronous lifecycle. |

### Shared audio engine and state transitions

| Location | Owner | Finding / required change |
| --- | --- | --- |
| `src/audio.cpp:39-68` | global audio state | Replace fixed/scattered globals with one owned engine: dynamic staging, worker, bounded command queue, state, and diagnostics. |
| `src/audio.cpp:75-99` | `audioThreadLoop()` | Queue mutexing is sound, but depth excludes in-flight data and backend wait is not cancellable. |
| `src/audio.cpp:101-128` | `submitAudio()` | Empty queue permits an oversized chunk; backpressure is not counted. Bound queued plus in-flight frames. |
| `src/audio.cpp:130-171` | `audio_init()` | Reset diagnostics and receive complete timing. Current `originalFps` is assigned later in `main`. |
| `src/audio.cpp:173-193` | `audio_deinit()` | `join()` can hang if backend submission never returns. Request cancellation, acknowledge `Stop`, then join. |
| `src/audio.cpp:195-207` | `audio_discard_pending()` | Clears staging/deque only, not in-flight or backend buffers. Replace with acknowledged end-to-end `Flush`. |
| `src/audio.cpp:209-215` | `SetVolume()` | A late callback can reach a destroyed/null backend. Gate callbacks and make wrappers null-safe. |
| `src/audio.cpp:234-255` | `core_audio_sample()` | Default buffer threshold is uninitialized for single-sample cores, causing tiny submissions. Replace packed signed shift with defined sample writes. |
| `src/audio.cpp:260-312` | `core_audio_sample_batch()` | Threshold initialization exists only here; use bounds-checked dynamic staging and deliberate split/submission. |
| `src/savestate.cpp:108-118` | `LoadState()` | Frontend audio clearing is incomplete; wait for acknowledged audio/video barriers before `retro_unserialize()`. |
| `src/disk_control.cpp:32-52` | disk change | Does not reset queued/backend audio. Flush at the media-transition boundary. |
| `src/achievements.cpp:425-427` | achievement reset | Calls `retro_reset()` directly; route it through the common reset operation. |
| `src/menu_setup.cpp:780-809` | state/reset menu | Centralize pause/resume/load/reset transitions so core, audio, and worker states cannot diverge. |

### GO2/OpenAL and SDL audio backends

| Location | Owner | Finding / required change |
| --- | --- | --- |
| `src/go2/audio.cpp:47-56` | `go2_audio_t` | No diagnostics, cancellation, pause/flush state, or runtime-frequency transition. Add explicit backend lifecycle. |
| `src/go2/audio.cpp:58-115` | `go2_audio_create()` | Context detachment supports worker ownership, but initial/prebuffer state and OpenAL error handling are incomplete. |
| `src/go2/audio.cpp:136-179` | `playAudio()` | Polls forever for `AL_BUFFERS_PROCESSED`. Add cancellation/timeout, underrun detection, queue depth, and recovery accounting. |
| `src/go2/audio.cpp:184-204` | `go2_audio_submit()` | Return/report submission result and duration; remove obsolete timing globals. |
| `src/platform_go2.cpp:107-112` | audio wrappers | Volume get/set dereference null `audio`; make the abstraction shutdown-safe. |
| `src/platform_sdl.cpp:35-43` | `rr_audio` | Add pause, flush, cancel, validity, diagnostics, and submitted-state tracking. |
| `src/platform_sdl.cpp:540-570` | `rr_audio_create()` | Returns an object even if `SDL_OpenAudioDevice()` fails; propagate initialization failure. |
| `src/platform_sdl.cpp:572-623` | `rr_audio_submit()` | Clears queues beyond 250 ms without counting lost frames; count underrun, overrun, drops, max depth, and duration. |

### Video, presenter, and direct scanout

| Location | Owner | Finding / required change |
| --- | --- | --- |
| `src/video.cpp:54-66` | fast-forward counters | `presented` means accepted too early. Benchmark completion must originate in the backend. |
| `src/video.cpp:135-329` | `video_configure()` | Runtime geometry change needs an orderly drain/reconfiguration path. |
| `src/video.cpp:331-340` | `video_prepare_core_unload()` | Calls core `context_destroy` only on SDL; extend to GO2. |
| `src/video.cpp:343-374` | `video_deinit()` | Can destroy resources while detached workers use them. Drain/join first and clear pointers. |
| `src/video.cpp:790-840` | software refresh | Null data is a duplicate/no-new-frame callback, not a valid skipped frame. |
| `src/video.cpp:842-909` | hardware refresh | A no-frame callback may re-present the prior GPU buffer; count duplicate and presentation independently. |
| `src/video.cpp:915-956` | callback/fixed skip | General callback timing is absent and fixed skip returns before accounting. Add benchmark scopes and reason codes. |
| `src/video.cpp:994-1000` | adaptive skip | Early return is uncounted; record it explicitly. |
| `src/video.cpp:1003-1031` | FF throttle | Acceptance is counted before GO2 can reject a full queue. Separate accepted, queued, completed, and dropped. |
| `src/video.cpp:1097-1114` | SDL HW path | Count completion after `rr_context_swap_buffers()` returns. |
| `src/video.cpp:1124-1158` | direct selection | Add candidate/rejection/fallback counters; direct eligibility correctly excludes overlays and shaders. |
| `src/video.cpp:1164-1178` | threaded GO2 video | One detached thread per frame can outlive `context3D`. Replace with one bounded joinable worker. |
| `src/ui-renderer.cpp:313-455` | `uiRenderOverlays()` | Overlay/decoration state changes routing and direct eligibility; include effective state in benchmark metadata. |
| `src/platform.h:70-90` | display diagnostics | Direct/vblank-only structure lacks standard/fallback/completed/drop accounting. Extend it or add a benchmark snapshot API. |
| `src/platform.h:108-113` | audio abstraction | Add validity, pause, flush, cancellation, and diagnostic APIs with matching backend semantics. |
| `src/platform_sdl.cpp:757-790` | SDL presenter | Standard completion point is after `SDL_RenderPresent()`; return/report outcome. |
| `src/platform_sdl.cpp:1116-1133` | GL/VSync setup | Record requested and successfully applied swap interval separately. |
| `src/platform_sdl.cpp:1420-1581` | GL presentation | Hardware completion is after `SDL_GL_SwapWindow()`; instrument completion and blocking duration there. |
| `src/platform_sdl.cpp:1632-1645` | runtime VSync | Use a fresh process per matrix setting and report application failure. |
| `src/go2/struct.h:9-35` | `go2_display_t` | Direct diagnostics are shared across main/render threads without display-level synchronization. |
| `src/go2/struct.h:58-70` | `go2_presenter_t` | Add drain/barrier and completion counters; replace `volatile bool` termination with synchronized state. |
| `src/go2/display.cpp:252-301` | diagnostics | Reset covers direct stats only; include standard/fallback/presenter stats. |
| `src/go2/display.cpp:309-372` | `go2_display_present()` | Actual standard DRM completion occurs here. Return outcome; page-flip waits can otherwise continue indefinitely. |
| `src/go2/display.cpp:493-575` | direct present | Direct count increments before vblank and only 120 vblanks are timed. Define benchmark completion after the transition/wait. |
| `src/go2/display.cpp:578-585` | direct disable | No barrier with queued standard frames; synchronize before changing plane state. |
| `src/go2/display.cpp:1281-1329` | render loop | Correct standard completion location after `go2_display_present()`; associate queued frames with benchmark generation. |
| `src/go2/display.cpp:1370-1401` | presenter destroy | Joins its render thread, but higher-level detached workers may still submit. Stop producers first. |
| `src/go2/display.cpp:1404-1438` | standard post | Fast-forward queue-full return is silent; return outcome and count rejection. |
| `src/go2/display.cpp:1659-1826` | composited post | Same silent drop for overlays/decorations; count RGA/composition and completed fallback. |

### Configuration and reproducibility

| Location | Owner | Finding / required change |
| --- | --- | --- |
| `src/config.cpp:285-310` | `initConfig()` | CWD `retrorun.cfg` overrides `-c`; use a clean temporary CWD or nonpersistent CLI overrides for remote matrices. |
| `src/config.cpp:399-419` | SDL renderer/VSync | Renderer requires restart and VSync is applied during initialization; record requested/applied values. |
| `src/config.cpp:510-514` | declared-FPS pacing | Run separately labelled realtime-paced and throughput benchmarks. |
| `src/config.cpp:534-567` | audio settings | Record buffer/stable/threaded effective values; keep threaded default false. |
| `src/config.cpp:640-692` | video performance settings | Record threaded/direct/frameskip effective values and overridden combinations; keep safe defaults. |
| `src/menu_setup.cpp:521-589` | runtime settings | Some settings persist for restart while others mutate live state; automation must use per-process nonpersistent settings. |

The repository has build workflows and manual game-launch scripts, but no
first-party deterministic frontend tests for worker lifecycle, presentation
accounting, or benchmark semantics. Add fake-backend tests; compilation and one
60-second hardware run are not sufficient concurrency validation.

## Required metric semantics

- `core_frames`: completed calls to `retro_run()` inside the measurement window.
- `video_callbacks`: callbacks made by the core during measured `retro_run()`
  calls; exclude frontend-generated menu/loading refreshes.
- `duplicated_frames`: callbacks that provide no new software frame or hardware
  framebuffer.
- `skipped_frames`: valid new frames rejected before presentation. Track fixed
  frameskip, adaptive frameskip, fast-forward throttle, and presenter queue
  rejection separately as well as in the total.
- `presented_frames`: presentations completed by the backend, not merely calls
  accepted by `core_video_refresh()`.
- `direct_scanout_frames`: completed native DRM direct presentations.
- `fallback_frames`: completed RGA/composited/SDL presentations. On every
  backend, `presented_frames` must equal direct plus fallback frames after the
  final synchronization.
- `missed_deadlines`: scheduled realtime frames whose active work finishes
  after their deadline. Also report maximum lateness and p95/p99 frame time.
- `average_core_time`: exclusive estimate inside `retro_run()`, subtracting
  measured video and audio callback time, clamped at zero.
- `average_video_time` and `average_audio_time`: callback time per core frame.
- `average_frontend_overhead`: active main-loop time outside `retro_run()`,
  excluding pacing sleep.

Record p50/p95/p99 for core, video, audio, active frame time, and deadline
lateness. Record asynchronous worker/backend time separately so threaded modes
do not appear free merely because work moved off the main thread.

## Audio diagnostics and consolidation

Add and reset per-session diagnostics:

```text
audio_buffer_underruns
audio_buffer_overruns
audio_frames_dropped
audio_max_queue_depth
audio_callback_max_duration_us
```

Also distinguish intentional fast-forward muting, flush/reset discards,
backpressure events, queued frames, in-flight frames, and backend submission
time.

Replace the current implicit worker lifecycle with an owned state machine and a
bounded command queue supporting at least `Data`, `Flush`, `Pause`, `Resume`,
and `Stop`. A flush must:

1. clear the frontend staging buffer;
2. invalidate or finish the in-flight chunk;
3. clear pending worker chunks;
4. flush the SDL/OpenAL backend on the thread that owns it;
5. acknowledge completion to the caller.

The queue limit must include queued and in-flight frames. Preserve audio by
backpressure in normal operation; do not silently drop merely because the queue
is full. Count every pressure event. Backend recovery that clears queued audio
must count the discarded frames.

Use the same reset operation on transitions into pause/menu, fast-forward,
state load, core reset, disk change, shutdown, and any runtime sample-rate
change. Resume with backend-appropriate prebuffering. Intentional pause and
fast-forward must not be reported as accidental underruns.

Support `RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO`. Stage the update during the
core callback and apply it safely after `retro_run()` returns. Recreate audio
when the sample rate changes, update frame pacing when FPS changes, and handle
geometry changes without destroying resources from inside a core callback.

Initialize the audio frame threshold for both batch and single-sample cores.
Use dynamically sized, bounds-checked staging storage. Make GO2/OpenAL waits
cancellable so shutdown cannot block forever.

## Core and video lifecycle prerequisites

Unify SDL and GO2 shutdown. Remove the GO2 `exitFlag`/sleep/`SIGUSR1` forced
unload path. The ordered shutdown must be:

1. stop accepting normal input and new benchmark work;
2. disable new audio/video callback submissions;
3. flush and join the audio worker before `retro_unload_game()`;
4. stop and join all video/presentation workers;
5. call the core hardware `context_destroy` while its graphics context exists;
6. call `retro_unload_game()` exactly once;
7. call `retro_deinit()` exactly once;
8. destroy audio/video/input frontend resources;
9. call `dlclose()` and clear every lifecycle flag and handle.

Store the GO2 hardware `context_destroy` callback just as the SDL path does.
Callbacks arriving after submission has been disabled must return safely
without dereferencing a destroyed backend.

Replace the detached per-frame GO2 video threads with one bounded, joinable
worker. Add a presenter drain/barrier before switching between the queued RGA
presenter and direct scanout and before taking the benchmark snapshot. Count a
standard GO2 frame only after `go2_display_present()` completes. Count SDL
software frames after `SDL_RenderPresent()` and hardware frames after
`SDL_GL_SwapWindow()`.

## Local verification

Add focused tests with fake audio and presenter backends where practical. At a
minimum test:

- queue full/backpressure without data loss;
- flush while a chunk is in flight;
- pause/resume repeatedly;
- stop while the backend is waiting;
- reset, state load, and disk change;
- runtime sample-rate and FPS changes;
- a single-sample audio core;
- exact counter reset/snapshot semantics;
- direct/fallback/presented accounting;
- repeated initialization and shutdown.

Run native AddressSanitizer and ThreadSanitizer tests for the isolated workers
when the platform backend cannot be sanitized. Build all available targets and
run `git diff --check`. Do not rewrite or delete existing user files to obtain a
clean tree.

## Remote build host

```text
Host: 192.168.64.6
User: navy78
Repository: /mnt/retrorun-host/retrorun
```

The directory is a shared view of the local repository. Confirm its HEAD and
dirty state before compiling. Never reset or pull over local work. Build the
requested release targets from that directory:

```sh
cd /mnt/retrorun-host/retrorun
make PLATFORM=linux-go2 config=release
make PLATFORM=linux-sdl2 config=release
```

Expected outputs are normally:

```text
/mnt/retrorun-host/retrorun/retrorun
/mnt/retrorun-host/retrorun/retrorun-sdl2
```

Verify with `file`, record SHA-256 checksums, and do not deploy an output if its
architecture or dynamic dependencies are wrong.

## Target test device and deployment

```text
Host: 192.168.0.159
User: ark
Directory: /home/ark/retrorun
GO2 binary: /home/ark/retrorun/retrorun_go
SDL2 binary: /home/ark/retrorun/retrorun_sdl2
Core: /home/ark/.config/retroarch/cores/flycast_rumble_libretro.so
Content: /roms/dreamcast/Soul Calibur (USA)[DCCM].cdi
```

Deploy through a temporary filename. Preserve the previous executable as a
timestamped backup, install atomically, set executable permissions, and verify
the remote checksum. For example, copy GO2 `retrorun` as `retrorun_go.new` and
SDL `retrorun-sdl2` as `retrorun_sdl2.new`, then perform the backup and rename
on the target. If the build host cannot route to the target LAN, copy via a
local temporary directory and then to the target. Do not put passwords on a
command line or use `sshpass` with a literal password.

Before starting, inspect whether another frontend owns DRM, KMS, input, or the
audio device. Do not terminate unrelated processes without explicit authority.
Run from `/home/ark/retrorun`:

```sh
./retrorun_go --benchmark 60 \
  /home/ark/.config/retroarch/cores/flycast_rumble_libretro.so \
  "/roms/dreamcast/Soul Calibur (USA)[DCCM].cdi"

./retrorun_sdl2 --benchmark 60 \
  /home/ark/.config/retroarch/cores/flycast_rumble_libretro.so \
  "/roms/dreamcast/Soul Calibur (USA)[DCCM].cdi"
```

Capture stdout, stderr, exit status, JSON result, kernel messages relevant to
DRM/audio, and the exact binary/config checksum for every run.

## Benchmark matrix

Use separate processes for settings that affect initialization. Prefer
repeatable CLI overrides that do not persist configuration. If temporary config
files are necessary, run from a temporary working directory so the repository's
local `retrorun.cfg` cannot override `-c`, and restore the original target
configuration afterward.

Test applicable combinations in stages:

1. GO2 standard RGA versus direct scanout, without overlays.
2. SDL2/OpenGL ES with VSync off and on.
3. Main-thread versus threaded audio.
4. Decorations and FPS/status overlays off and on.
5. Declared-FPS pacing on and an explicit throughput run without pacing.
6. Stable audio buffering off and on only when the base mode underruns.

Direct scanout with decorations, shaders, menus, screenshots, or visible
overlays is expected to fall back and must be reported as such, not treated as a
direct-scanout failure.

Use a short screening pass, then at least three 60-second runs for promising
configurations. Keep the same core, ROM, savestate/start state, firmware, CPU
governor, display mode, and input sequence. Record temperature and throttling.
Reject any configuration with crashes, shutdown hangs, audio drops/underruns,
invalid presentation accounting, or rendering corruption. Rank the remaining
configurations primarily by missed deadlines and p95/p99 frame time, then by
frontend overhead and latency; raw core-frame count alone is not a quality
score in a paced workload.

## Completion report

Report:

- commits created and files changed;
- local build/test/sanitizer results;
- remote build commands and binary checksums;
- deployment backups and final paths;
- every benchmark JSON record or an attached result file;
- the best safe configuration for this game on GO2 and SDL2;
- regressions, unsupported combinations, and remaining risks;
- confirmation that unrelated local changes and target configuration were
  preserved.
