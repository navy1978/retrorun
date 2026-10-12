# Audio and benchmarking

[Back to README](../README.md)

The experimental hybrid target retains the native GO2/DRM video and evdev
input paths while including both the native OpenAL and SDL2 queued-audio
providers:

```sh
make PLATFORM=linux-go2-hybrid config=release
```

The output is `retrorun-hybrid`. Select the playback provider before launch in
`retrorun.cfg`:

```ini
retrorun_audio_backend = sdl2
```

Accepted values are `auto`, `go2` (or `openal`) and `sdl2` (or `sdl`). `auto`
preserves the GO2 default. SDL2 initializes only its audio subsystem in this
build; it does not acquire KMSDRM or replace the GO2 presenter. The selected
provider is logged at startup and recorded as `audio_backend` in integrated
benchmark JSON.

For a non-persistent benchmark comparison, override it on the command line:

```sh
./retrorun-hybrid --benchmark 60 --benchmark-set audio_backend=sdl2 CORE ROM
```

Benchmark-only overrides also include `audio_buffer` and a deterministic
warm-up confirmation sequence. The latter is useful for save states that stop
at a prompt before reaching the measured scene:

```sh
./retrorun-hybrid --benchmark 60 --benchmark-warmup 10 \
  --benchmark-frames 1800 \
  --benchmark-set confirm_input=true \
  --benchmark-set confirm_input_delay=4 \
  --benchmark-set audio_buffer=512 CORE ROM
```

This injects one A press at the requested delay and one B press 0.75 seconds
later. Both are restricted to warm-up and therefore never enter benchmark
metrics. The warm-up must be longer than the configured delay plus one second,
so both synthetic presses occur before metric collection starts.
`--benchmark-frames` stops measurement after exactly the requested number of
core frames; the duration remains a safety deadline. This makes alternating
comparisons independent of the performance of the candidate being measured.
These overrides are not persisted to `retrorun.cfg`.

The validated low-end compensation can be enabled in `retrorun.cfg`. It is
SDL2-specific, explicitly trades a small amount of audio accuracy for queue
stability, and remains disabled by default:

```ini
retrorun_sdl_audio_stretch_percent = 5
retrorun_sdl_audio_stretch_low_ms = 40
```

The SDL2 provider also has diagnostic environment controls. The `STRETCH`
variables override the corresponding configuration-file values for isolated
tests; all controls preserve existing behavior when unset:

```sh
RETRORUN_SDL_AUDIO_PREFILL_MS=20
RETRORUN_SDL_AUDIO_TARGET_MS=80
RETRORUN_SDL_AUDIO_STRETCH_PERCENT=5
RETRORUN_SDL_AUDIO_STRETCH_LOW_MS=40
```

`PREFILL_MS` delays initial playback until the requested reserve exists;
`TARGET_MS` controls the high-watermark used for submission backpressure. The
two `STRETCH` settings enable an explicitly inaccurate low-end mode:
below the low watermark, SDL2 linearly resamples a batch to add at most the
configured percentage of frames. This can reduce queue starvation when a core
supplies slightly less audio than the output device consumes, but it may alter
pitch or synchronization. RG351V automated tests across Sonic Adventure 2,
Soul Calibur, Crazy Taxi, Dead or Alive 2 and Power Stone found no audio drops
and a neutral aggregate frame count; the first manual Sonic test judged the
result almost perfect. Keep it opt-in until music-loop, transition and A/V-sync
checks cover a wider game set.

The native GO2/OpenAL backend provides the same opt-in low-watermark
compensation. It remains disabled in the recommended configuration:

```ini
retrorun_go2_audio_stretch_percent = 0
retrorun_go2_audio_stretch_low_ms = 20
```

It is disabled by default and is also disabled automatically when
`retrorun_audio_stable_buffer = true`. The percentage is clamped to `0`–`10`
and the low watermark to `0`–`200` ms. Environment variables
`RETRORUN_GO2_AUDIO_STRETCH_PERCENT` and
`RETRORUN_GO2_AUDIO_STRETCH_LOW_MS` override the configuration for isolated
tests. GO2 tracks the number of audio frames remaining in each OpenAL buffer;
when the usable queue falls below the watermark, it linearly expands only the
next submitted batch. Benchmark JSON records the resolved settings, queue
depth observations and `adaptive_stretch_frames`.

This GO2 mode is experimental. Controlled RG351V measurements selected
`3` percent below `20` ms as the least costly candidate, but the subsequent
Soul Calibur listening test found no substantial improvement and still heard
the soundtrack slow when emulation fell behind. It can soften short queue
shortages, but it cannot decouple audio playback from emulation speed. Keep
the percentage at `0` for normal play; larger values can make pitch and A/V
synchronization errors more noticeable.

For repeatable tests the values can also be supplied without editing
`retrorun.cfg`:

```sh
./retrorun-hybrid --benchmark 60 \
  --benchmark-set audio_backend=go2 \
  --benchmark-set go2_audio_stretch_percent=3 \
  --benchmark-set go2_audio_stretch_low_ms=20 CORE ROM
```
