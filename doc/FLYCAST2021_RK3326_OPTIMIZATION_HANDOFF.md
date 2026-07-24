# Flycast 2021 RK3326 optimization handoff

Last update: 2026-07-24

This document is the self-contained continuation point for the Flycast 2021
performance work. It incorporates
`flycast_rk3326_opengl_optimisation_plan.md` without discarding the work and
measurements already completed.

## Scope and invariant

- Primary target: RG351V / RK3326 / Cortex-A35 / Mali-G31, AmberELEC.
- Core under optimization: `libretro/flycast` revision
  `4c293f306bc16a265c2d768af5d0cea138426054` (Flycast 2021).
- Reference game/scene: Sonic Adventure 2, the reproducible slow scene loaded
  from the existing save state.
- Frontend: RetroRun hybrid, GO2 video and SDL2 audio.
- Goal: improve full emulation speed and frame pacing. Never alter SH4, AICA,
  input or VBlank timing merely to make the FPS counter rise.
- Keep emulated VBlanks, rendered frames and presented frames as separate
  metrics.
- Do not commit, ship or combine an optimization until its isolated build has
  passed performance and visual-correctness tests.
- Automated device campaigns have now been completed for renderer state
  elision and audio queue transients. The first subjective Sonic Adventure 2
  validation of the experimental audio rate compensation passed on
  2026-07-23: the developer judged the audio almost perfect and acceptable.

## Executive conclusion

The supplied plan is technically sound and fits the measured bottleneck. Its
most important premise is proven: this core is dominated by the old GLES draw
path and driver calls, not by compiler flags or texture conversion. The plan
also correctly identifies two concrete patterns present at `4c293f3`:

1. every rendered frame loops over every cached pipeline shader, switches to
   each program and rewrites all global uniforms, even when that shader is not
   used in the frame;
2. both 32-to-16-bit index conversion paths destroy and recreate their
   temporary `List<u16>` storage on each upload.

Both changes were implemented and device-tested. Persistent index storage and
lazy uniforms did not deliver a material combined gain and are rejected for
this workload. Multiple translucent strip-merging approaches were faster but
all violated menu/transparency correctness. Adjacent exact-state elision passed
stability and a five-game visual matrix, but its original **+13.84%** claim was
caused by comparing a 4.0-second baseline input delay with a 4.5-second
candidate delay. The final matched three-run A/B measured only **+0.51% FPS**,
-0.72% core average time and +0.39% worse video average time. It is therefore
not justified as a compatible default performance optimization. Per the
developer's decision, preserve it as an explicit experimental option,
disabled by default, rather than discarding it. No compatible renderer source
change has yet passed both the performance and visual gates.

The audio campaign also found a distinct frontend problem: during the hard
Sonic scene the core supplies audio slightly below the 44.1 kHz output demand,
so the SDL queue repeatedly empties despite the old aggregate underrun counter
remaining zero. Opt-in 5% low-watermark rate compensation reduced empty-queue
observations by 84.2% in a five-minute A/B with only -0.39% core frames and
0.71% average added audio. The first manual Sonic Adventure 2 listening test
judged it almost perfect and acceptable. Keep it opt-in until the smaller
multi-game pitch, loop, transition and A/V-sync matrix is complete.

The follow-up automated matrix extended the result to Crazy Taxi, Dead or
Alive 2 and Power Stone. Aggregate frames were neutral (+0.03%), empty-queue
observations fell 53.7%, low-queue observations fell 30.4%, and all six runs
had zero drops and zero backend underruns. The feature is now available as
bounded, disabled-by-default `retrorun.cfg` settings as well as environment
overrides.

The final manual same-scene backend A/B on 2026-07-23 changed only
`retrorun_audio_backend`, while keeping the configurable Flycast2021 core,
adjacent state elision, save state and video/core settings identical. The
developer judged GO2 audio **clearly cleaner** than SDL2 with 5% low-watermark
compensation: SDL2 still crackled, whereas GO2 only occasionally slowed and
then accelerated while recovering. For manual Sonic Adventure 2 play on the
RG351V, GO2 is therefore the current subjective preference. This is not yet a
cross-device default: GO2's speed variation is a timing-quality trade-off and
the result still needs a small multi-game listening check.

RetroRun's new adaptive GO2/OpenAL path was subsequently measured on the same
device. The initial 5%/40 ms profile reduced underruns by 16.5% but lost 3.15%
presented FPS because the normal queue depth was itself only about 23-25 ms,
causing compensation to run almost continuously. Reducing the profile to
3%/20 ms retained about 15.9% underrun reduction, cut generated compensation
from roughly 127,650 to 17,115 frames/run, and reduced the throughput cost to
about 1.6%. This is the current conservative GO2 compatibility candidate;
zero remains the accurate/performance default.

The 36 newer upstream Flycast2021 commits were also reviewed and tested as a
compatible 20-change bundle and corrected low-end ablations. No group improved
the Sonic workload: safe audio scheduling was performance-neutral and made
producer lateness worse; isolated dynarec and TexCache changes regressed;
atomic and audio/vblank groups reduced OpenAL overruns but also reduced
emulation throughput and moved the pressure into producer-late events. Do not
update the low-end core wholesale for performance. Full evidence is in
`benchmarks/rg351v-amberelec-flycast2021-upstream-2026-sonic-adventure-2-2026-07-23/REPORT.md`.

The configurable SH4-clock option from the fork audit was tested on
2026-07-24 using the same saved Sonic scene. Two matched 90-second runs averaged
37.013 emulated frames/s at the accurate 200 MHz default and 36.818 at
180 MHz, making 180 MHz 0.53% slower. Mean core time and audio underruns were
also slightly worse at 180 MHz. Single 160 and 140 MHz runs did not recover a
gain; 140 MHz fell to 36.372 frames/s. Keep 200 MHz and reject SH4 downclocking
as a performance option for this workload. Results are in
`benchmarks/rg351v-amberelec-flycast2021-sonic-adventure-2-sh4-clock-2026-07-24/`.

The same final configurable build also passed its manual Sonic visual gate
with adjacent state elision enabled and strip merge disabled: gameplay,
transparency and menus were all reported graphically perfect. This confirms
the prior five-game state-elision matrix using the exact release-style core,
not only the earlier one-off experimental binaries.

Roadmap estimate after the corrected release gate: about **93% of the current
investigation and benchmark plan is complete**. Renderer-source integration
readiness is low because no compatible change has a material verified gain;
frontend audio-compensation readiness is around **85%**. The
representative RTT/fog/palette visual matrix is complete, as are the
VMU/crosshair and specialized cache-lifetime audits. Remaining work is
dominated by finding a new renderer target with a larger measured cost than
buffer streaming, integrating the retained optimizations as disabled-by-default
options, and completing the longer subjective pitch/music-loop/A/V-sync audio
checks.

The first additional representative check was completed on 2026-07-23:
Crazy Taxi passed on the RG351V with the retained state-elision build. Gameplay,
shadows, sky/fog, signs and overlays appeared correct. Audio was generally
good, with brief crackle during visible frame loss; treat that as a transient
audio-under-load observation rather than a renderer regression. Palette-heavy
ChuChu Rocket also passed: textures, sprites, text and transparency appeared
correct. It normally ran at 60 FPS but could fall to about 35 FPS with many
sprites visible, without a corresponding visual artifact. Power Stone then
passed the additional 3D/RTT-sensitive check: menus, HUD, effects, shadows,
transparency and gameplay were correct, with no texture corruption or
stale-frame artifact. The retained state-elision candidate has therefore
completed its representative visual matrix, but the subsequent matched
performance gate rejected it as an optimization.

## What has already been measured

Controlled campaign conditions:

- 15 seconds warm-up plus 60 seconds measurement;
- fresh process for each run;
- three repetitions with rotated order;
- CPU `performance` at 1296 MHz;
- DMC `performance` at 786 MHz;
- GPU `performance` at 520 MHz;
- identical save state and configuration;
- original governors restored afterward.

Key renderer profile:

- approximately 759,943 `glDrawElements` calls over 3,019 rendered frames;
- approximately 251.7 draw calls per rendered frame;
- `glDrawElements`: 15.988 CPU seconds, about 69.4% of measured
  `DrawStrips` CPU time;
- GL/render-state setup: 3.668 CPU seconds, about 15.9%;
- VBO upload: 0.645 CPU seconds;
- modifier volumes: 0.386 CPU seconds;
- texture update: 0.836 CPU seconds over a 60-second profile, therefore not
  the first bottleneck for this scene;
- complete renderer work is the largest measured subsystem, AICA is second and
  TA parsing third.

Derived over the 3,019 rendered frames, the measured draw path costs were
approximately 5.30 ms/frame in `glDrawElements()`, 1.22 ms/frame in
`SetGPState()` and 0.21 ms/frame in the aggregate VBO upload scope. The implied
measured `DrawStrips()` total was about 23.04 CPU seconds, or 7.63 ms/rendered
frame. Consequently, draw submission is dominant but state setup is material;
the existing evidence does not support a 40% buffer-upload bottleneck in this
specific Sonic scene.

## Direct answer to the renderer-cost questions

The additional profiling questions raised on 2026-07-22 are valuable, but some
already have partial or complete answers. Keep the distinction between a
renderer scope and the time spent strictly inside a GL driver entry point:

| Question | What is already known | What is still required |
|---|---|---|
| Time in `SetGPState()` | Directly scoped in the prior internal profiler around the normal `DrawList()` call. It measured 3.668 CPU seconds, about 15.9% of `DrawStrips()` and 1.22 ms/rendered frame. Sonic uses the per-strip path, so this measurement is relevant to the reference configuration. | Split its contents into real state-cache changes, `UseProgram`, texture work and `glUniform*` only if the lazy-uniform experiment or Phase 0 counts make that worthwhile. |
| Time in `glBufferData()` | The earlier aggregate VBO scope, which contains the main vertex upload, index conversion/upload and modifier upload, measured only 0.645 CPU seconds: about 2.8% of `DrawStrips()` and 0.21 ms/rendered frame. | The Phase 0 core separates vertex upload, index upload and conversion. A deeper diagnostic can time the raw wrapper entry separately from surrounding CPU work. Do not infer raw driver time from the old aggregate alone. |
| Time converting indices | It was included in the old VBO scope, not isolated, and the repeated `Free()`/`Init()` plus 32-to-16-bit loop has been confirmed statically. | The new Phase 0 build already has a dedicated conversion timer. Its first real-device run will provide the missing value. |
| Time in `glTexImage2D()` | The outer texture-update path measured 0.836 CPU seconds over 242 updates, about 3.53 ms per update. Updates were too infrequent to be the primary average bottleneck in this scene. Phase 0 counts calls and uploaded bytes. | Separate Dreamcast texture conversion/cache handling from the raw `glTexImage2D()`/`glTexSubImage2D()` call. Fog and palette uploads must also be identified separately because they use fixed storage. |
| Time in `glUniform*()` | Uniform calls inside per-strip state setup are included in the measured 3.668-second `SetGPState()` scope. The unconditional all-cached-shader update loop has also been confirmed in source. Phase 0 counts all uniform calls. | Time the all-shader uniform batch and, in a separate high-overhead diagnostic build, the GL uniform wrappers. Timing every uniform call must not be enabled in performance A/B builds because clock reads can materially perturb this very hot path. |

This changes the interpretation, not the overall order of work. The current
data say that reducing compatible draw submissions remains the strongest
measured opportunity, while avoiding redundant state/uniform work is the next
credible target. Buffer streaming and texture-storage experiments remain
valid, but the Sonic profile currently ranks them below draw/state work. A
different game or scene can produce a different ranking, so Phase 0 must still
report these counters per workload.

## Assessment of the state-key and deferred-state proposal

The proposal received on 2026-07-22 is incorporated with the following
corrections:

- Optimize CPU time per rendered frame, not a draw-call number in isolation.
- The measured reference is about 252 submissions/rendered frame, not the
  illustrative 600. Compatible-run statistics must use the actual workload.
- A 25% reduction of the measured 5.30 ms draw-entry scope gives a theoretical
  upper bound of about 1.33 ms/frame only if cost scales linearly. It is not a
  forecast. The estimated 0.5 ms lazy-uniform gain and 5%, 10% or 15% overall
  gains remain hypotheses until device A/B runs establish them.
- The conservative per-strip batching experiment has already been implemented
  and measured at +1.58%. The next work is to explain that result with exact
  call reduction and state-run statistics, rather than implementing another
  batching scheme immediately.
- Do not reorder PowerVR primitives merely to sort state. Here, "deferred state
  sorting" means deriving state once and eliding redundant transitions while
  preserving the exact draw order. Any real reordering needs separate PowerVR
  correctness proof and is out of the low-risk phase.

Source inspection also shows that `GLCache` already suppresses unchanged
`glUseProgram`, texture binds, blend function, culling, depth function/mask,
scissor, enable/disable and most integer texture parameters. Therefore a new
cache must not simply reproduce those comparisons. The measured 1.22 ms in
`SetGPState()` also includes shader-key construction and map lookup, tile-clip
derivation, palette/fog/trilinear decisions, texture-related logic and
unconditional per-draw uniforms. These are the most plausible sources of the
remaining CPU cost.

The safe experiment sequence is:

1. Test the existing lazy-uniform release candidate against the matching A35
   control, using the saved Sonic scene and the established long protocol.
2. Run a diagnostic lazy-uniform-plus-Phase-0 build to compare uniform and
   program calls/frame, without using its instrumented throughput as the
   release result.
3. Add hierarchical, non-overlapping `SetGPState()` scopes for shader lookup,
   uniforms, texture lookup/binding, sampler state, clipping/scissor,
   blend/depth/cull and remaining CPU logic. Report both requested operations
   and wrapper calls that actually reach GL.
4. Add a statistics-only `DrawStateKey`. Per list/pass, record original draws,
   adjacent equal-state pairs, compatible run count, mean/max run length,
   program transitions, texture transitions and blend/depth transitions. Do
   not alter order or rendering in this build.
5. If repeated adjacent keys are common, implement a top-level same-key fast
   path that skips redundant shader lookup and uniform/state preparation while
   still issuing the original draw. This tests state elision independently of
   batching and has lower correctness risk.
6. Only then compare state elision, conservative strip merge and their
   combination. A draw reduction without proportional CPU/frame improvement
   is not a success.

The `DrawStateKey` must contain derived renderer state, not just raw pointer
identity: list type, sorting mode, shader features/program, texture ID,
palette/trilinear values, effective clipping, blend factors, cull (including
`cflip`/`gcflip`), depth function/write mask, stencil-relevant state and sampler
parameters. Frame-global state needs a generation or invalidation rule. A
conservative raw `PolyParam` equality counter can be recorded alongside it,
but is expected to underestimate safe state reuse.

Core frameskip is still a separate final experiment. Source inspection confirms
that the existing `ta_skip` path avoids TA parsing/rendering for eligible
non-RTT frames and protects render-to-texture work. It must be validated with
emulated VBlanks, rendered/presented frames, audio and RTT correctness; the FPS
counter alone cannot validate it.

## Audio-emulation opportunity

Do not conflate RetroRun audio output with Dreamcast audio emulation. The
frontend side is already well characterized: SDL2, 512 frames, stable buffering
off and audio threading off produced zero underruns/drops, and changing among
the stable SDL2 buffer sizes did not improve throughput. More backend tuning is
therefore a low-priority performance target for this Sonic workload.

The emulated AICA path cannot be dismissed. The clean internal profile measured
95,374 AICA updates and 5.337 CPU seconds over 60 seconds, making it the second
largest measured subsystem after rendering. The scope contains both
`aicaarm::run(32)` and the batched `AICA_Sample32()` path, so it does not yet
distinguish ARM7 execution, channel stepping/ADPCM-PCM generation, CDDA mixing
and final sample output. Its totals overlap other threads and must not be added
to renderer totals.

The current low-end configuration already selects the important inexpensive
path: DSP disabled keeps `NoBatch` false, uses the cache-friendlier 32-sample
mixer, and the AICA ARM7 uses the AArch64 dynarec. Enabling DSP was measured at
about -9.6% in the Sonic option screening and must remain disabled.

Sonic's streamed soundtrack should not be treated as a host MP3 decode. A
compressed game stream such as CRI ADX is decoded by software running on the
emulated Dreamcast CPU/audio environment and then consumed by AICA; subjective
music complexity alone does not predict emulator cost. Any software decode on
the emulated SH4 would also not be fully attributed to the existing AICA scope.

After the first renderer candidates, add a low-overhead audio diagnostic with
one timer around each aggregate block, never around every sample:

1. `aicaarm::run(32)`;
2. `AICA_Sample32()` total;
3. all channel `Step()` work;
4. CDDA/external-input and final mixing;
5. `WriteSample()`/libretro delivery;
6. active channel count and PCM8/PCM16/ADPCM sample counts.

### Diagnostic builds prepared on 2026-07-22

The aggregate AICA diagnostic above is now implemented by
`flycast2021-aica-aggregate-stats-experimental.patch`. It reports approximately
once per second and times whole 32-sample blocks. It does not put a clock around
each sample. The output separates ARM7 execution, total `AICA_Sample32()`, all
channel generation, final mix plus delivery, generated/delivered samples, and
active/generated work split between PCM16, PCM8, ADPCM and streaming ADPCM.

Four new Cortex-A35/AArch64 builds are available in the VM under
`/home/navy78/flycast-candidates-20260722/`:

| Build | Purpose | SHA-256 |
|---|---|---|
| `flycast2021_4c293f3_lazy_phase0_a35_libretro.so` | lazy-uniform experiment with the same Phase 0 GL counters as the control; use it to validate the uniform-call and CPU/frame change | `a5296d1d33f36e41aff9229c9262afdcee677e8122d7dcafd0034c54b28e305f` |
| `flycast2021_4c293f3_aica_stats_a35_libretro.so` | isolated low-overhead AICA aggregate diagnostic, without Phase 0 GL instrumentation | `d2306a9d9173ed39912bbaa377bb82656c5ead556607dee953fb5e97c5f2f1c0` |
| `flycast2021_4c293f3_phase0_aica_stats_a35_libretro.so` | combined GL plus AICA diagnostic for correlation in one run; expect more instrumentation overhead | `f7e3f6cc47fdc3f69b6f3bf0d73e2daeb382057d72c59e9e4f2e49d495a8005c` |
| `flycast2021_4c293f3_setgp_stats_a35_libretro.so` | Phase 0 plus detailed `SetGPState()` scopes and derived adjacent-state/run statistics | `7e3d260d8022f8ae128e6a2341b8fd6ceefea212b1db0ccabb77b0068f0399d1` |

All four completed with exit status zero and were verified as 64-bit ARM
AArch64 shared objects. The build flags include
`-mcpu=cortex-a35+crc+fp+simd+crypto`, `-mno-outline-atomics`, `-Ofast` and the
appropriate diagnostic defines. These binaries are measurement tools, not
release candidates. In particular, do not compare their raw throughput against
the clean baseline without accounting for timer/counter overhead.

Recommended device order is: clean baseline, clean lazy-uniform candidate,
Phase 0 control, lazy+Phase0, isolated AICA stats, then the combined GL+AICA
build only if cross-correlation is needed. The isolated AICA build should be
the primary audio measurement because it does not carry the GL counter cost.

The SetGP diagnostic is implemented by
`flycast2021-setgp-derived-state-stats-experimental.patch`. Its scopes are
shader lookup/preparation, program plus uniforms, clipping plus stencil,
texture plus sampler state, and blend/cull/depth fixed state. It also records
adjacent compatibility and run lengths using effective program, texture,
clipping, stencil, sampler, blend, cull and depth state. The state resets at
each rendered frame, so reported runs never merge across frame boundaries.
This build performs several clocks per `SetGPState()` call and must only be
used for diagnosis, not for throughput ranking.

Compare the same saved scene with music active and with a game-side condition
that genuinely stops stream decoding, if such a reproducible condition can be
created. Merely muting frontend output is not a valid test because emulation and
mixing can continue unchanged. Retain audio work only if it lowers full
emulation CPU time without pitch, timing, channel, looping or crackling
regressions.

### Audio performance and crackle workstream

Audio is now a parallel optimization track, not a replacement for the renderer
and compatibility work. The renderer remains the largest measured cost and the
state-elision validation continues unchanged. The audio track has two goals:

1. eliminate intermittent crackling, especially during startup and heavy frame
   spikes;
2. reduce Dreamcast audio-emulation and frontend-delivery CPU cost without
   increasing latency noticeably or weakening emulation correctness.

The existing whole-run result of zero RetroRun underruns does **not** disprove
the reported crackling. A short producer stall, an ALSA/SDL xrun, irregular
delivery, queue oscillation, clipping or a core-side discontinuity can be
audible while a coarse aggregate counter remains zero. The next diagnostic
must therefore retain a small histogram/ring of transient events rather than
only totals.

#### Measurement order

1. Reproduce the difficult Sonic startup and saved slow scene with the clean
   baseline and state-elision core. Record wall-clock timestamps for each core
   audio batch and frontend submission, queue depth before/after submission,
   callback gaps, maximum callback duration, ALSA/SDL recovery/xrun reports and
   emulation-speed/VBlank data.
2. Add low-overhead 1 ms, 5 ms, 10 ms and 20 ms tail buckets for producer gaps
   and queue starvation. Preserve the worst events with timestamp, queue depth
   and concurrent core/video frame time so a crackle can be correlated with a
   rendering or SH4 spike.
3. Run the isolated AICA aggregate build on both the stable music scene and the
   crackling startup. Compare ARM7, channel generation, CDDA/external input,
   final mixing and delivery; do not time individual samples.
4. Confirm whether the failure is frontend starvation, host backend/xrun,
   emulation falling below real time, or AICA/core discontinuity before changing
   buffering or mixer code.

#### Optimization candidates, in priority order

- Remove allocation, locking, logging and avoidable copies from the real-time
  delivery path; reuse aligned interleaved sample buffers and submit whole
  batches.
- Add a small startup prefill and low/high watermarks so short frame-time spikes
  consume reserve instead of producing a crackle. Keep steady-state latency
  bounded and do not continuously grow the queue.
- Measure 256, 512, 768 and 1024-frame queues only around the reproducible
  transient. The existing 512-frame SDL2, non-threaded, unstable-buffer setting
  remains the control and should not be replaced from subjective evidence alone.
- Compare direct and worker-thread delivery with scheduling/callback telemetry.
  A worker is useful only if it isolates slow backend calls without adding
  contention or stale audio.
- In AICA, optimize only the measured block: fast inactive-channel rejection,
  hoisting invariant channel state, reducing repeated address/format branches,
  avoiding redundant conversion/mixing and evaluating a carefully validated
  AArch64/NEON block mixer. PCM16, PCM8, ADPCM and streaming ADPCM require
  separate correctness coverage.
- Inspect ARM7 dynarec dispatch and block lookup only if `aicaarm::run(32)` is
  dominant in the crackling scene. Do not assume the mixer is the bottleneck.
- Treat frameskip or graphics gains as an audio improvement only when they
  restore real-time production and measurably remove starvation; never skip
  SH4, AICA, input or Dreamcast timing to hide an underrun.

Do not add time stretching, sample dropping/duplication, pitch changes or
lower-quality resampling as silent defaults. Such modes may be evaluated only
as explicit low-end options after the accurate path is characterized.

#### Audio campaign result (2026-07-22/23)

RetroRun now exports low-overhead queue depth, empty/low observations,
producer-gap and backend-submit tail buckets, callback maxima, and rate-
compensation totals in benchmark JSON. The complete campaign is archived in
`benchmarks/rg351v-amberelec-flycast2021-audio-transients-sonic-adventure-2-2026-07-22/REPORT.md`;
the follow-up matrix is in
`benchmarks/rg351v-amberelec-flycast2021-audio-multigame-2026-07-23/REPORT.md`.

The measurements establish these points:

- the valid 90-second state-elision control produced 42,547 audio frames/s for
  a 44,100 Hz SDL device and recorded 1,262 empty-queue observations;
- two repeated opt-in 5%/40 ms runs reduced empties to 315 and 288 (-75.0% and
  -77.2%) with -0.74% and -0.48% core frames;
- a paired five-minute run reduced empties from 2,610 to 413 (-84.2%) and low
  observations from 6,635 to 2,651 (-60.0%) with -0.39% core frames; only
  0.71% audio was added over the full evolving scene;
- the clean baseline core independently improved from 1,539 to 383 empties
  (-75.1%), proving that the result is a frontend behavior rather than a
  renderer side effect;
- in a lighter Soul Calibur startup cross-check, compensation added only 0.18%
  audio, changed frames by +0.18% and reduced empties from 69 to 45;
- across Crazy Taxi, Dead or Alive 2 and Power Stone, aggregate frames changed
  from 9,101 to 9,104 (+0.03%), empty observations fell from 1,341 to 621
  (-53.7%), low observations fell from 2,933 to 2,041 (-30.4%), and no run
  dropped audio or reported a backend underrun;
- 20 ms startup prefill, 256/1024-frame core batches, proportional correction
  and a block-invariant AICA experiment were all inferior and are rejected.

The later manual Sonic Adventure 2 backend A/B adds an important subjective
qualification to these automated results. With the same state-elision build,
save state and game options, GO2 sounded markedly cleaner than SDL2 with
5%/40 ms compensation. GO2 occasionally slowed and then sped up, but the
developer preferred this to SDL2 crackling. Consequently:

- keep SDL2 as the instrumented benchmark backend and as an explicit
  low-watermark-compensation experiment;
- prefer GO2 for current manual Sonic Adventure 2 play on RG351V;
- do not promote either backend as a universal default until the same A/B is
  repeated with at least two additional demanding games.

The implementation is deliberately opt-in through
`retrorun_sdl_audio_stretch_percent = 5` and
`retrorun_sdl_audio_stretch_low_ms = 40`, with equivalent uppercase
environment overrides for isolated tests; default behavior is unchanged. It
is a low-end compatibility/performance mode, not accurate audio. Automated
results are strong and the first manual Sonic listening gate passed, but
multi-game pitch, loop, channel, transition and synchronization validation
remains.

#### Audio acceptance criteria

An audio change is retained only when repeated tests show all of the following:

- no audible crackle in the startup transient and a five-minute mixed-scene
  session;
- zero backend xruns, zero dropped audio frames and no queue starvation;
- improved worst-case producer/callback gaps or at least 5% lower measured AICA
  or delivery CPU time;
- no pitch, channel, looping, CDDA, ADPCM, synchronization or rumble regression;
- no more than 1% loss in full emulation throughput and no material increase in
  steady-state input-to-audio latency.

Every audio A/B must be run once with the clean baseline core and once with the
state-elision core. This keeps audio conclusions separate from renderer gains
while also testing whether faster rendering removes the starvation transient.

The prior internal profiler is diagnostic and costs about 2% throughput. It
already
provides subsystem and draw-path timing plus RetroRun frame/audio/tail-latency
metrics, but it does not yet satisfy the new plan's complete Phase 0 counter
set.

## Renderer files and call paths confirmed

The static inspection requested by the supplied plan found these principal
paths at exact revision `4c293f3`:

- `core/rend/gles/gles.cpp`: `OpenGLRenderer::RenderFrame()` owns dynamic
  vertex/index uploads, the GLES2 32-to-16-bit index conversion, global shader
  setup and the high-level render pass;
- `core/rend/gles/gldraw.cpp`: opaque, punch-through, modifier-volume and
  translucent list dispatch reaches `DrawList()` / `DrawSorted()` and finally
  `DrawStrips()`;
- `core/rend/gles/gles.cpp`: `DrawStrips()` is the dominant submission path and
  calls `glDrawElements`; the corrected saved-scene Phase 0 run measured about
  558 draw calls per rendered frame. The older startup-scene profile measured
  roughly 252 and is not representative of this slow saved scene;
- `core/rend/gles/glcache.h`: caches several states but tracks texture binding
  too broadly for a renderer that uses multiple texture units; it is also the
  correct place for later state-cache experiments;
- `core/rend/gles/gltex.cpp`: texture conversion/cache/update path; earlier
  scoped timing showed it is not the first bottleneck in this specific scene;
- `core/libretro-common/glsm/glsm.c`: central libretro GL wrappers used for the
  first diagnostic counters (`Draw*`, program/uniform, texture, buffer and
  upload operations);
- `core/hw/pvr/spg.cpp`, `core/hw/pvr/ta_vtx.cpp` and
  `core/libretro/libretro.cpp`: respectively supply emulated VBlank,
  TA/render-skip and presentation counters without changing their decisions.

No rendering decisions were changed in the Phase 0 patch. Its atomic counters
and stage clocks do add diagnostic overhead, which is why it must never be
used as the throughput candidate itself.

## Results already obtained

### Corrected saved-scene campaign on 2026-07-22

The earlier benchmark configuration did not set `retrorun_auto_load = true`.
Consequently, the 2026-07-21 strip-merge result below and the first diagnostic
runs on 2026-07-22 measured game startup rather than the intended saved slow
scene. They remain useful historical evidence but are superseded for this
workload by the verified saved-scene campaign.

After explicitly enabling auto-load, RetroRun logged a successful
`retro_unserialize` of the 28,151,408-byte state and sent the required confirm
input after four seconds. Later candidates changed that delay to 4.5 seconds
after the developer observed that 4.0 could be too early. The original report
incorrectly compared those different workloads.

| Build | Input delay | FPS mean | Core avg | p95 | p99 | Decision |
|---|---:|---:|---:|---:|---:|---|
| clean A35 baseline, historical | 4.0 s | 33.592 | 22.802 ms | 36.739 ms | 45.309 ms | baseline for 4.0-second candidates only |
| conservative strip merge A35 | 4.0 s | 39.539 | 18.726 ms | 32.964 ms | 40.645 ms | **+17.70%; visual regression, inaccurate mode only** |
| GLES3 primitive restart A35 | 4.5 s | 37.731 | 19.865 ms | 31.527 ms | 38.742 ms | visual regression; percentage against 4.0 baseline withdrawn |
| adjacent exact-state elision, historical | 4.5 s | 38.240 | 19.763 ms | 33.704 ms | 41.800 ms | visually correct; percentage against 4.0 baseline withdrawn |
| clean A35 baseline, final matched A/B | 4.5 s | 37.906 | 19.956 ms | 33.823 ms | 41.608 ms | corrected baseline |
| adjacent exact-state elision, final matched A/B | 4.5 s | 38.100 | 19.813 ms | 33.696 ms | 41.507 ms | **+0.51%; reject as performance change** |

The individual FPS ranges do not overlap: baseline 33.282–33.825 versus strip
merge 39.041–39.888. Strip merge reduces average core time by 17.88%, p95 by
10.27%, p99 by 10.30% and video average time by 5.90%. This exceeds the
acceptance thresholds by a wide margin. It still requires visual checks for
strip connectivity, transparency, fog/palette, clipping and render-to-texture
correctness before integration. Those checks subsequently failed, so the gain
is retained only as evidence and as a possible explicit inaccurate mode.

Manual validation then found small menu transparency regressions and slightly
worse crackling during the difficult initial section. Source review identified
one concrete bug in the experiment: its translucent pre-index sort used
`left.zvZ > right.zvZ`, whereas original `SortPParams()` uses
`left.zvZ < right.zvZ`. A new build restored the original comparator and added
the missing `texid1` compatibility check. These corrections were subsequently
integrated into the configurable core:

```text
flycast2021_4c293f3_configurable_orderfix_a35_libretro.so
SHA-256 df8bf0aca1e4491bfd7ee16c31f0187c1b19fd8b8314501789990021839d463d
```

The final manual Sonic Adventure 2 check reported good performance and fully
working controls, but the menu was still rendered incorrectly. The remaining
leading cause is therefore the actual joining of translucent strips through
degenerate indices, or another unmodelled per-strip state distinction. Neither
fast build is suitable as a compatible default.

A balanced GO2/480x360 A/B then measured the original configurable core at
33.172 FPS and the corrected configurable core at 32.833 FPS (-1.02%). The
run-level ranges overlap, so the corrections have a small cost/no meaningful
performance effect. They are retained because they restored Soul Calibur menu
correctness, not because they improve speed. Detailed evidence is in
`benchmarks/rg351v-amberelec-flycast2021-configurable-orderfix-sonic-adventure-2-2026-07-23/REPORT.md`.

A later manual cross-game check on 2026-07-23 used the verified-booting
`Dead or Alive 2 (USA)[RDC].cdi` image. In the scenes inspected by the user,
the original strip-merge build showed no evident transparency, menu or
gameplay defect and remained visually indistinguishable from the accurate
candidate. Its audio crackled heavily, however. The exact adjacent-state
elision candidate was also visually correct and crackled less, although the
audio was still not clean. Both runs used the same RetroRun SDL2 audio
configuration (512-frame configured buffer, stable/threaded audio disabled,
5% low-queue compensation). Neither log reported an ALSA underrun or device
failure, so the audible failure is currently attributed to irregular/late
audio production while emulation falls behind rather than backend
initialization. This one compatible-looking game does not overturn the known
transparency failures in Sonic Adventure 2; strip merge remains suitable only
as an explicit inaccurate mode.

The later GO2/480x360 manual cross-check made the per-game split even clearer.
Using the original fastest strip-merge A35 binary and changing only the game:

- `Soul Calibur.cdi` ran, but its menus were not visible with the original
  reverse-order/incomplete-key build;
- `Dead or Alive 2 (USA)[RDC].cdi` showed no visual or menu problem in the
  inspected scenes;
- GO2 audio was judged good in both games.

Soul Calibur was then repeated with `stripmerge-orderfix`, which restores the
original sort direction and includes the second texture identity in merge
compatibility. At the same GO2/480x360 settings, gameplay was fast, audio was
clean and all menus were visible. This isolates the Soul Calibur failure to
the older merge implementation rather than proving that the title is
inherently incompatible with strip merging.

This remains direct evidence for a per-game override or explicit whitelist.
Never enable strip merge globally: the corrected variant still previously
failed Sonic menu validation. A final GO2/480x360 retest showed readable menu
text but malformed/incompletely drawn menu graphics; it was usable, not
correct. The later configurable-orderfix build reproduced the same Sonic
result while retaining good performance. Soul Calibur and Dead or Alive 2 now
have positive title-specific evidence, while Sonic remains outside the
whitelist.

The same Sonic retest exposed an independent frontend configuration issue.
The benchmark-derived cfg omitted `retrorun_analog_to_digital`, so RetroRun's
historical `left_forced` default mapped the left stick to D-pad and returned
zero for the native analog axes. Start/A/B worked, but Sonic could not move.
Adding `retrorun_analog_to_digital = none` restored native analog movement.
This setting is required in the Flycast/Flycast2021 RG351V profile.

Both retained experiments were subsequently integrated into one configurable
core without changing their release policy:

```ini
flycast2021_adjacent_state_elision = disabled
flycast2021_translucent_strip_merge = disabled
```

The second option accepts `inaccurate` as its opt-in value. A balanced
eight-run Sonic campaign compared the original A35 core, the configurable
binary with both options disabled, and each option independently. The disabled
binary measured 35.860 versus 35.995 FPS for the original (-0.38%, within
normal variance), with essentially identical core p95 and video cost. This
validates the unchanged default path.

State elision measured 36.318 FPS (+0.90% versus original). Inaccurate strip
merge measured 40.384 FPS (+12.20%), reduced average core time 13.19% and
reduced active-frame p95 8.96%. All runs loaded the verified save state,
completed without crashes or GL errors and reported zero backend underruns or
dropped audio frames. The strip-mode gain is real, but the known visual
compatibility failures still require it to remain explicitly inaccurate and
disabled by default. Full evidence is in
`benchmarks/rg351v-amberelec-flycast2021-configurable-render-options-sonic-adventure-2-2026-07-23/REPORT.md`.

Two additional runs enabled both options. They were stable but measured
40.086 FPS, 0.74% below strip merge alone, with worse core p95/p99. Do not
combine them: state elision is the compatible experimental choice; strip merge
is the separate inaccurate performance choice.

The earlier `Dead or Alive 2 (Europe)` CUE/BIN dump is not valid evidence for
renderer or audio comparisons. Both Flycast2021 and modern Flycast produced
long invalid-frame sequences with it, and Flycast2021 reported TA/list
overruns. The replacement USA CDI boots and plays, ruling out RetroRun's audio
backend as the cause of the original complete post-logo audio loss.

Further isolation is now complete. Raw pre-index sorting without merging also
broke the menu; adjacent-only merging preserved it but regressed to 26.344 FPS.
Exact original sorting plus original `make_index()` still broke the menu. A
GLES3 primitive-restart build then preserved strip boundaries with fixed-index
sentinels rather than degenerate connector triangles and produced the same
defect. Its clean result was 37.731 FPS (+12.32% over baseline), but 4.57% below
the original strip merge. This rules out sort direction, approximate depth,
the custom index generator and degenerate connectors as necessary causes. The
shared unsafe operation is collapsing multiple translucent `PolyParam` entries
into one draw call; compatible work must retain those draw boundaries.

The next order-preserving experiment retained every draw call and skipped only
redundant `SetGPState()` calls for adjacent `PolyParam` entries with an exact
state match. The previously failing menu rendered correctly. Its three
historical runs were 38.319, 37.998 and 38.403 FPS, but they used 4.5-second
input timing and were incorrectly compared to the 4.0-second baseline. The
final matched A/B produced 37.906 versus 38.100 FPS (+0.51%), -0.72% average
core time, -0.38% core p95, -0.24% core p99 and +0.39% worse average video
time. This is measurement noise and fails the acceptance threshold.

Cross-game validation with Soul Calibur also passed: the user found no menu,
transparency or gameplay defects. A same-revision A35 baseline/candidate boot
sequence measured 57.241 versus 57.345 FPS. The sequence is already near the
60 Hz ceiling, so FPS is not a useful discriminator, but average video time
fell from 5.912 to 5.200 ms (-12.04%) and missed deadlines from 2,672 to 1,656,
with zero underruns. Raw data and methodology are in
`benchmarks/rg351v-amberelec-flycast2021-state-elision-soul-calibur-2026-07-22/REPORT.md`.

A source-level correctness audit then covered the specialized paths. The
cached state is local to each `DrawList()` and cannot cross frames, render
passes, list types or RTT targets. Complete raw `pcw/tcw/tsp/isp`, clipping and
resolved texture IDs cover every per-polygon input read by `SetGPState()`.
Fog/palette globals are updated before the lists, while VMU and light-gun
overlays are drawn afterward and outside the fast path. No missing dependency
or invalidation leak was found. Details are in
`benchmarks/rg351v-amberelec-flycast2021-state-elision-source-audit-2026-07-23/REPORT.md`.
Representative visual checks remain prudent, but the main architectural risk
is now source-audited.

Combining state elision with the previously tested lazy-uniform change did not
stack: under the same 4.5-second historical timing, a Sonic saved-scene screen
measured 38.04 FPS versus the 38.240 FPS state-elision mean. The approximately
-0.5% difference is noise. The combined change is rejected as unnecessary
complexity; neither value is evidence of a gain over the final matched
baseline.

Single corrected-scene screens found lazy uniforms at 34.072 FPS (+1.43%, not
yet beyond noise), persistent index buffers at 33.464 FPS (reject), the
three-patch combined build at 39.299 FPS (inferior to strip merge alone), and
core frameskip 1 at 25.609 FPS (strongly negative; reject). All clean runs had
zero audio underruns and zero dropped audio frames.

An isolated texture-unit-aware cache subsequently measured 33.696 FPS,
22.730 ms average core time and 6.933 ms average video time: only +0.31% over
the baseline mean. It is rejected as a performance patch for this scene.

The remaining uncached float texture parameters were then isolated on top of
state elision. Three 90-second saved-scene runs per build measured 38.222 FPS
for state elision and 38.243 FPS with cached LOD-bias/anisotropy requests
(+0.06%). Average core time was 19.734 versus 19.740 ms, while video time
changed only from 6.412 to 6.390 ms (-0.34%). This is below noise and the
acceptance threshold, so the patch is rejected without spending a visual
validation gate. Raw results are in
`benchmarks/rg351v-amberelec-flycast2021-state-elision-floatcache-sonic-adventure-2-2026-07-23/REPORT.md`.

The next order-preserving buffer-streaming screening compared the original
exact-size `glBufferData(data)` path with two alternatives: orphaning followed
by `glBufferSubData()`, and retained power-of-two capacity followed by
`glBufferSubData()`. Two thermally balanced 90-second saved-scene runs per
build measured 38.728 FPS for baseline, 38.844 for orphan+subdata (**+0.30%**)
and 38.450 for capacity+subdata (**-0.72%**). Orphaning reduced core p99 by
1.95% but increased empty/low audio-queue observations; capacity increased
core p99 by 4.99%. Neither variant crosses the performance gate, so the
original upload path remains the release choice. Full evidence is in
`benchmarks/rg351v-amberelec-flycast2021-buffer-streaming-sonic-adventure-2-2026-07-23/REPORT.md`.

The detailed report and raw artifacts are in:

`benchmarks/rg351v-amberelec-flycast2021-diagnostics-sonic-adventure-2-2026-07-22/REPORT.md`

### Corrected diagnostic conclusions

The saved scene performs about 558 draws and 381 texture binds per rendered
frame. Draw submission consumes roughly 13.4–15.8 ms within a 14.0–16.5 ms
GLES render path. Vertex upload is only 0.38–0.47 ms, index upload 0.14–0.20
ms and sorting 0.11–0.13 ms. `glBufferData` occurs only about 2.9 times/frame.

The detailed state diagnostic reports a stable ~530.9 `SetGPState` calls and
3.0–3.2 ms per frame. Texture/sampler work is the largest part at 1.75–1.86
ms/frame, followed by shader work at 0.43–0.48 ms. About 24% of adjacent draws
are compatible, with average compatible run length 1.31. Texture changes
dominate at roughly 380/frame. An order-preserving texture-unit-aware cache was
then tested and yielded only +0.31%, so it is not a useful isolated performance
change for this scene.

Lazy uniforms demonstrably reduce program calls by about 41% and uniform calls
by about 30%, but did not materially reduce measured render/draw time. This is
not currently worth merging alone.

The stable AICA scene normally uses one PCM16 channel and costs only about
0.037–0.047 ms per 32-sample ARM7 block plus 0.018–0.022 ms for
`AICA_Sample32`. No PCM8 or ADPCM channel was observed. Audio is not the first
optimization target for this saved scene.

| Experiment | Result | Decision |
|---|---:|---|
| AmberELEC distributed Flycast 2021 | 2700.67 frames/60 s mean | baseline |
| conservative per-strip merge, startup scene | 2743.33 frames/60 s, +1.58% | superseded by corrected saved-scene result above |
| small SH4 FPU dynarec backport | -0.23% | reject for this workload |
| renderer-only PGO plus FPU | -0.12% | reject |
| clean compiler control | within noise | no compiler-only win |
| extra IPA/interposition/link flags | smoke 1647 vs 1667 | reject |
| whole-program or partial LTO | dynarec crash | do not use on Flycast 2021 |
| whole-core PGO generation | same dynarec crash | do not use |
| modern Flycast at matched settings | 2546 frames, -5.73% vs old core | do not replace the old core on RG351V |
| broader modern vs 2021 comparison | 27.862 vs 31.649 FPS; 2021 +13.6% | optimize 2021 incrementally |
| modern Flycast AmberELEC LTO package tweak | about +0.09% | stable but not perceptible |

The modern Flycast AmberELEC package change remains a separate local change.
It must not be confused with the Flycast 2021 experiments in this document.

## Plan comparison, item by item

| Plan item | Status before reading plan | Current status / evidence | Next action |
|---|---|---|---|
| Phase 0 renderer counters | Implemented and device-tested | Correct saved-scene run measured ~558 draws, ~381 texture binds and only ~2.9 buffer uploads per rendered frame; draw submission dominates | Preserve as diagnostic evidence; do not rank its instrumented throughput |
| 1. Lazy global uniforms | Device-tested alone and combined with state elision | Program calls -41% and uniform calls -30%, but no material render-time reduction; clean standalone screen +1.43%; under the same historical 4.5-second timing the combined screen was 38.04 FPS versus 38.240 for state elision alone | Reject alone and in the tested combination |
| 2. Persistent 16-bit conversion buffers | Device-tested | Clean saved-scene result -0.38%; upload/conversion costs are already small | Reject for this workload |
| 3. GL state per texture unit | Device-tested | Correct per-unit cache produced 33.696 FPS, only +0.31% over baseline | Reject as isolated performance change; retain as infrastructure evidence only |
| 3a. Float texture-parameter cache | Three-run device A/B on top of state elision | 38.222 vs 38.243 FPS (+0.06%); core time +0.03% worse, video time -0.34% | Reject; redundant anisotropy/LOD requests are not a material bottleneck |
| 4. Buffer binding cache | Source-inspected after Phase 0 | Unused cache fields exist and direct calls bypass them, but the representative path requests only about 10 buffer binds/frame versus hundreds of draws; expected gain is below priority threshold | Defer; implement only if a different workload demonstrates high redundant binding traffic |
| 5. Fog/palette fixed storage | Source-inspected after Phase 0 | Both paths use `glTexImage2D`, but saved-scene texture uploads are zero in many one-second windows and only sporadic in others | Defer until a fog/palette-heavy workload demonstrates frequent uploads |
| 6. Static quad buffers | Not tried | Small VMU/crosshair/quad uploads exist | Low priority; only after larger items |
| 7. Buffer streaming variants | Completed: two balanced 90-second runs/build | Orphan+subdata: +0.30% FPS, -0.30% core average, -1.95% core p99; capacity+subdata: -0.72% FPS, +1.10% core average, +4.99% core p99 | Keep original `glBufferData(data)` path; retain orphan patch only as measured evidence and reject capacity strategy |
| 8. Translucent depth-mask second pass | Inspected | Default is already false and game-specific compatibility data can enable it | Do not globally remove it; expose/measure only with visual matrix |
| 9. Cheapest alpha sorting | Already used | Baseline uses `per-strip (fast, least accurate)` | Keep as low-end baseline; do not mix per-triangle results with it |
| 10. Draw/state diagnostics | Completed on device | Phase 0 and detailed SetGP split/run statistics collected; only ~24% adjacent compatibility and texture changes dominate | Use evidence for texture-unit-aware state caching; avoid broad draw reordering |
| 11. Sorted `GL_TRIANGLES` batching | Not tried | Relevant mainly to the more expensive per-triangle path, not the recommended per-strip baseline | Lower priority unless diagnostics or a compatibility title requires per-triangle |
| 12. Triangle-strip batching | Exhaustively isolated; visual regression confirmed; explicit option implemented and benchmarked | +17.70% historical peak; +12.20% through the configurable `inaccurate` option; corrected order/key variant fixes Soul Calibur and Dead or Alive 2 remains correct, but Sonic still has known menu regressions | Fold the order/key corrections into the configurable option, keep it disabled by default and whitelist only manually validated titles |
| 12a. Adjacent exact-state elision | Final matched A/B, five-title visual matrix, stability/source audit and explicit option benchmark complete | Historical matched gain +0.51%; final configurable build +0.90% versus original; disabled path -0.38% within noise; all five games visually correct | Retain as `flycast2021_adjacent_state_elision = enabled`, experimental and disabled by default |
| 13. Conversion vs texture upload timing | Completed for current prioritization | Vertex upload ~0.38–0.47 ms, index upload ~0.14–0.20 ms and texture uploads infrequent in the saved scene | Add deeper texture split only for a different scene that demonstrates frequent uploads |
| 14. Reuse texture storage | Not tried | Candidate pending measurements | Defer until per-game upload data exists |
| 15. Avoid unchanged mip uploads | Not tried | Candidate pending measurements | Defer until per-level counters exist |
| 16. Verify frameskip semantics | Source-inspected | `flycast2021_frame_skipping` sets `settings.pvr.ta_skip`; non-RTT frames skip TA parsing/rendering, RTT is protected. RetroRun presenter frameskip is a different mechanism | Confirm counters and audio/VBlank behavior on device |
| 17. Fixed frameskip 1 | Benchmarked | 25.609 FPS and 32.130 ms average core time versus 33.592 FPS and 22.802 ms baseline | Reject on RG351V for this workload |
| 18. Adaptive max-1 frameskip | Not implemented | RetroRun adaptive presentation skip is not equivalent to core TA skip | Design only after fixed core skip is proven useful |
| 19. Frameskip benchmark | Completed for fixed core skip | Strong regression with zero audio underruns in both runs | Do not pursue until a fundamentally different safe early-skip design exists |
| 20. Audio transient telemetry | Implemented and device-tested | Queue depth/empty/low counters, producer and submit 1/5/10/20 ms tails, callback maximum and compensation totals now expose starvation hidden by the old aggregate underrun counter | Add a small timestamped worst-event ring only if manual crackle cannot be correlated from current data |
| 21. AICA hotspot comparison | Aggregate diagnostic and stable/saved scenes measured | AICA is not the first steady-state bottleneck; frontend under-production/starvation is directly observed | Profile a reproducible crackling startup only if it remains after frontend candidate validation |
| 22. Frontend real-time delivery | Control, prefill, core batch sizes and rate compensation tested; first manual Sonic gate and automated multi-game matrix passed | 512 remains optimal; prefill has no benefit; opt-in 5%/40 ms compensation cuts five-minute Sonic empties 84.2% and three-game aggregate empties 53.7%, with neutral aggregate frames | Keep opt-in and complete a small subjective multi-game pitch/loop/sync matrix |
| 23. AICA low-end optimization | One isolated block-invariant candidate tested | Candidate regressed frames ~2.9% and increased starvation; it is also unsafe if ARM7-visible registers change inside the block | Reject this candidate; modify AICA only after a new scoped hotspot is measured |
| 24. Audio regression matrix | Automated matrix substantially complete; first subjective Sonic test passed | Sonic, Soul Calibur, Crazy Taxi, Dead or Alive 2 and Power Stone pass without drops/crashes; Crazy Taxi/DOA2/Power Stone aggregate frames +0.03%, empty queues -53.7%; manual Sonic audio was almost perfect and acceptable | Complete music-loop, A/V-sync, channel and transition checks on a small game set |

## Phase 0 instrumentation delivered and remaining gaps

The instrumentation-only build is controlled by `FLYCAST_GL_STATS` and prints
one compact `GL perf:` line per second. It compiles cleanly for the target and
the marker is present in the resulting ELF. It currently supplies:

- emulated VBlanks, rendered frames, skipped render frames, presented frames;
- draw calls, draw-elements calls, indices and estimated triangles;
- shader program calls and uniform calls;
- texture binds and active-unit changes;
- array/element buffer binds;
- `glBufferData`/`glBufferSubData` calls and vertex/index bytes;
- texture uploads and bytes, cache hits/misses, live shader count.

It also separates transparent sorting, 32-to-16-bit conversion, vertex upload,
index upload, draw submission and whole GLES render time. The following deeper
breakdowns remain pending and should only be enabled in a separate diagnostic
build, after measuring their profiler overhead:

- raw `glBufferData()` driver-entry time versus surrounding upload work;
- texture conversion/cache work versus raw `glTexImage2D()` and
  `glTexSubImage2D()` time;
- the global all-shader uniform batch, total raw `glUniform*()` time and the
  remaining non-uniform work in `SetGPState()`;
- opaque, punch-through and translucent drawing separately;
- framebuffer resolve/copy and presentation;
- whole emulated frame versus whole rendered frame.

Counters are placed in GL wrappers and clocks are around stages rather than
inside every draw call. Some wrapper counts are requested calls rather than
proven state changes; this is intentional for the first diagnostic pass. A
future per-unit cache can then demonstrate both reduced requested work and
reduced real driver work. Measure diagnostic overhead before trusting its
absolute timing values.

## Experimental patches archived in the Flycast fork

The 28 Flycast/Flycast 2021 patches produced during this campaign are stored,
together with a SHA-256 manifest, in
[`navy1978/flycast2021-lowend`](https://github.com/navy1978/flycast2021-lowend/blob/master/docs/PATCH_ARCHIVE.md)
locally after commit `9f1d885a`. The updated archive includes the renderer, GL
diagnostic, buffer-streaming, AICA/audio, AmberELEC packaging and configurable
SH4-clock experiments. The 2026-07-24 fork audit and SH4 test procedure are
staged in that fork's `docs` directory and are not yet committed.

The shipping implementation at commit `aaf6d44c` is functionally identical to
applying `flycast2021-configurable-render-options.patch` to base revision
`4c293f3`. It already includes the corrected translucent depth order,
`texid1` in the exact merge key, the AmberELEC low-end compatibility changes
and both configurable renderer options. Earlier standalone patches are
preserved only as reproducibility evidence.

The lazy-uniform patch uses one generation per rendered frame and updates a
shader only immediately before its first use in that frame. The legacy
`RPI4_SET_UNIFORM_ATTRIBUTES_BUG` path remains eager.

The persistent-index patch replaces only temporary CPU conversion storage with
resizable vectors; it does not change index order, index type or GL upload
semantics.

## VM builds ready for later device testing

VM: `navy78@192.168.64.6`. Source and binaries are under `/home/navy78`.
The principal renderer test binaries are also collected in
`/home/navy78/flycast-candidates-20260722/` for convenient deployment.

The `_a35_` binaries below were rebuilt with flags passed through the
Makefile's effective `CPUFLAGS` variable. This matters because this old
Makefile resets environment `CFLAGS` and `CXXFLAGS`; the earlier binaries
without `_a35_` in their name compiled successfully but did **not** receive the
complete requested Cortex-A35 flag set and are superseded.

| Candidate | VM binary | SHA-256 |
|---|---|---|
| clean matching A35 control | `/home/navy78/flycast2021_4c293f3_baseline_a35_libretro.so` | `926ebdeb7b23843a1d9db3f86002eba48f059c3ed29fc7d72ae062ec5b049fe2` |
| Phase 0 GL statistics (diagnostic only) | `/home/navy78/flycast2021_4c293f3_phase0_stats_libretro.so` | `4a19b2b0ad0c39b2293579be8acbd42086cb8b12ffb12fba863d1b9c3396768b` |
| strip merge only, A35 | `/home/navy78/flycast2021_4c293f3_stripmerge_a35_libretro.so` | `7faaafa753200e5ad92027bbe618e411d151a7fb5c64652df5e9d2bbf0941be4` |
| lazy uniforms only, A35 | `/home/navy78/flycast2021_4c293f3_lazy_uniforms_a35_libretro.so` | `678938a47053a1c9305e784ee02f587f1fe4404f793fc94e6c0f2873da012d97` |
| persistent index buffers only, A35 | `/home/navy78/flycast2021_4c293f3_persistent_index_a35_libretro.so` | `b63cfa76d58f144fd8ce73451d83ec88d72b37a80ed71c0347290c324901af48` |
| strip merge + lazy + persistent index, A35 | `/home/navy78/flycast2021_4c293f3_stripmerge_lazy_index_a35_libretro.so` | `227dbd72e9b287b4b6dab1c7b1018e780ee46a8d1c87d3a738448d331f0e6b6b` |
| strip-merge diagnostic profile | `/home/navy78/flycast2021_4c293f3_stripmerge_profile_libretro.so` | `d26775255200a8f1149fd2f30af4084b5abb7fb2e03f9784e7c60967283222e9` |
| selectable accurate/fast translucent mode, A35 | `/home/navy78/flycast2021_4c293f3_translucent_option_a35_libretro.so` | `27db28f6ee50d669a9a329eee12646c2a87c53133ce40467b2b7885cfa71cb78` |
| exact post-index translucent batching, A35 | `/home/navy78/flycast2021_4c293f3_postindex_batch_a35_libretro.so` | `9f13b403f37ac24018eca3f997562028fe7deadcadd96d14cc39a8f9e193a9bd` |
| exact presort + original index generation, A35 | `/home/navy78/flycast2021_4c293f3_exact_presort_merge_a35_libretro.so` | `55da68221555e2df5e8eb778ea7c09040c5ab296815c137797622089bd0d4de6` |
| GLES3 primitive restart, A35 | `/home/navy78/flycast2021_4c293f3_primitive_restart_a35_libretro.so` | `f0298977d6c6800310b9a61f6464403041e7e87c9a2d6960f8b937fe03c3bb2f` |
| adjacent exact-state elision, A35 | `/home/navy78/flycast2021_4c293f3_state_elision_a35_libretro.so` | `a1f202c662b0156a41a91522461fb23b78f9441425393aed77d5a69cc7706283` |
| configurable state-elision + inaccurate strip-merge options, A35 | `/home/navy78/flycast2021_4c293f3_configurable_a35_libretro.so` | `c48f8eba3e8c465a8378473cd392d5f2c6d70d491a49b4f8ba72797872aa76ca` |
| state elision + float texture-parameter cache, A35 | `/home/navy78/flycast2021_4c293f3_state_elision_floatcache_a35_libretro.so` | `680d624029e37af00de7d7f33e7d76bd25eb6e382f39058e73f3e944bf7582b0` |
| state elision + 256-frame core audio batch, RGB565 | `/home/navy78/flycast2021_4c293f3_state_elision_audio256_rgb565_a35_libretro.so` | `92010ef1648b8470ea572d94bd51b065df7868b72dffbcc20c2e9d98b911dfdc` |
| state elision + 1024-frame core audio batch, RGB565 | `/home/navy78/flycast2021_4c293f3_state_elision_audio1024_rgb565_a35_libretro.so` | `32fe0c3ab642f4bdad48d0497e9d708801ef3725728f256cb5df678a5c6b1017` |
| state elision + AICA invariant hoist, RGB565 | `/home/navy78/flycast2021_4c293f3_state_elision_aica_hoist_rgb565_a35_libretro.so` | `006ff9ef706bff468dc9acfb9a1a0ac65b4940cc5bc34d183b4a6f2915e4c5bf` |
| integrated configurable SH4 clock, experimental timing hack | `/home/navy78/flycast-candidates-20260724/flycast2021_lowend_integrated_sh4clock_a35_libretro.so` | `7ed7aca13fabddec62d12b257b4a29d7521b1c566de76af31ab91a2f01f72e52` |

The float-parameter cache, 256/1024 batch builds and AICA-hoist build are
measured rejected candidates, not release artifacts. The frontend binary used
for the retained opt-in rate experiment is
`/home/navy78/retrorun_hybrid_audio_stretch`, SHA-256
`1b4b06b53976a3b7441802a8ab4cf509e6343b1061e42f23576da89ea46e30e1`.
The subsequent configuration-file build is
`/home/navy78/retrorun-audio-config-20260723/retrorun-hybrid`, SHA-256
`8afc4ec3dc6bb784c0b9b5d09a5317d9dd5b513ef15108d7d8b8241f05df8c14`.
It was deployed separately as
`/storage/retrorun/retrorun_hybrid_audio_config`; a device smoke test read
5%/40 ms from `retrorun.cfg`, recorded the resolved values in benchmark JSON,
applied compensation and reported zero drops and zero backend underruns. It
did not replace the installed or previous test executable.

All current A35 candidates compiled successfully as AArch64 ELF shared objects
with the AmberELEC identity, RGB565 libretro format and the established target
flags. Compilation is not functional validation.

The integrated SH4-clock option is deliberately outside the normal
optimization invariant:
values other than its 200 MHz default alter emulated timing to reduce or
increase guest CPU work. At 200 MHz it executes the exact original decoder
path. It changes no serialization structures and retains both low-end renderer
options, but it must be evaluated as an explicit compatibility/performance
hack rather than evidence of a faster accurate emulator. Test 200, 180, 160
and optionally 140 MHz in isolation before combining it with inaccurate strip
merge. The patch and detailed test order are documented in the Flycast fork's
`docs/SH4_CLOCK_EXPERIMENT.md`.

VM source worktrees:

- `/home/navy78/flycast2021-lazy-4c293f3`
- `/home/navy78/flycast2021-index-4c293f3`
- `/home/navy78/flycast2021-combined-4c293f3`
- `/home/navy78/flycast2021-phase0-stats-4c293f3`
- `/home/navy78/flycast2021-baseline-a35-4c293f3`
- `/home/navy78/flycast2021-stripmerge-a35-4c293f3`

## Next work when the RG351V is available

1. Check battery before the first run and between long campaigns. If capacity
   is **10% or lower**, stop and ask the developer to connect the charger.
2. Preserve the distributed core and current configuration; never overwrite
   the only copy.
3. Do not repeat translucent batching variants: exact post-index, exact
   presort/original indexing and primitive restart all reproduced the menu
   defect. Preserve `fast` only as an explicitly inaccurate option.
4. Keep the selectable core option default at `accurate`.
5. The state-elision visual/game matrix, stability run, source audit and final
   matched three-run release gate are complete. Visual correctness passed, but
   the corrected +0.51% FPS result is noise and average video time was 0.39%
   worse. Do not integrate it as a performance optimization.
6. Buffer-streaming variants are complete. Orphan-plus-subdata gained only
   0.30%; retained-capacity subdata lost 0.72%. Keep the original upload path.
7. The first manual Sonic Adventure 2 listening gate for the SDL2 5%/40 ms
   candidate passed. Controlled GO2 tuning selected 3% below 20 ms: about 16%
   fewer underruns for about 1.6% fewer presented FPS than unmodified GO2.
   The later Soul Calibur listening check found no substantial improvement
   and still heard the soundtrack slow when emulation fell behind. Keep both
   compensation mechanisms opt-in and keep GO2 compensation disabled by
   default.
8. The automated Crazy Taxi, Dead or Alive 2 and Power Stone audio matrix has
   passed. Further progress now requires a phase-continuous asynchronous
   time-stretcher or slowly controlled resampler; do not repeat percentage and
   watermark micro-tuning, prefill experiments, or 256/1024 core batches.
9. Only a visually and audibly correct candidate gets a final three-run
   release campaign. Do not make the current rate compensation a default.
10. Do not repeat persistent-index, fixed-core-frameskip, LTO, whole-core PGO or
    modern-Flycast replacement experiments already rejected above.
11. Do not import the 2026 Flycast2021 upstream bundle for speed. Corrected
    low-end ablations found the safe-audio group neutral; dynarec, TexCache,
    atomic-only and audio/vblank groups all reduced Sonic throughput.
12. The configurable SH4-clock implementation is complete and retained as an
    opt-in core option. Repeated 200/180 MHz
    testing plus single 160/140 MHz runs found no gain; 180 MHz averaged 0.53%
    slower and 140 MHz regressed further. Keep 200 MHz and do not promote
    downclocking as a performance option for the Sonic/RK3326 workload.

For visual validation include, where locally available and legally owned:

- opaque/high-polygon content;
- heavy transparency;
- render-to-texture effects;
- frequent texture changes;
- one native 30 FPS title;
- one native 60 FPS title;
- the reproducible Sonic Adventure 2 slow scene.

Look specifically for missing translucent polygons, wrong strip connections,
fog/palette errors, broken clipping, RTT corruption and VMU/crosshair issues.

## Acceptance thresholds

Keep a change only if it produces at least one repeatable material result:

- at least 5% lower renderer CPU time;
- at least 3 percentage points higher full emulation speed;
- meaningful audio-underrun reduction;
- elimination of the reproducible crackle with improved queue/callback tail
  behavior and no material latency increase;
- at least 15% fewer draw calls without visual regressions;
- at least 20% fewer uniform/redundant-state calls **and** measurable frame-time
  improvement;
- meaningfully better p95/p99 frame time.

A complicated patch below measurement noise should be rejected. Strip batching
exceeds the performance threshold but fails correctness. Adjacent exact-state
elision passes the five-title visual and stability gates but fails the final
matched performance gate (+0.51% FPS with 0.39% worse average video time), so
it is not suitable as a default optimization. It must nevertheless remain
available behind an explicit experimental core option, disabled by default,
until broader title-specific evidence determines whether it has value outside
the Sonic scene. No renderer-source candidate currently satisfies both
performance and correctness thresholds. VMU/crosshair was structurally
isolated from state elision according to the source audit, but that correctness
result does not alter the default-performance decision.

## Known-good benchmark configuration

```ini
retrorun_audio_backend = sdl2
retrorun_audio_buffer = 512
retrorun_audio_stable_buffer = false
retrorun_force_audio_multithread = false
retrorun_force_video_multithread = true
retrorun_analog_to_digital = none
retrorun_loop_declared_fps = true
retrorun_drm_direct_scanout = false
retrorun_adaptive_frameskip = false
retrorun_frameskip = 0

flycast2021_internal_resolution = 320x240
flycast2021_cpu_mode = dynamic_recompiler
flycast2021_texupscale = off
flycast2021_threaded_rendering = enabled
flycast2021_synchronous_rendering = enabled
flycast2021_alpha_sorting = per-strip (fast, least accurate)
flycast2021_anisotropic_filtering = off
flycast2021_mipmapping = enabled
flycast2021_pvr2_filtering = disabled
flycast2021_enable_rttb = disabled
flycast2021_enable_dsp = disabled
flycast2021_delay_frame_swapping = disabled
flycast2021_frame_skipping = disabled
flycast2021_framerate = fullspeed
flycast2021_sh4clock = 200
flycast2021_adjacent_state_elision = disabled
flycast2021_translucent_strip_merge = disabled
```

For manual Sonic Adventure 2 play on RG351V, the cleaner subjective audio
choice from the controlled backend A/B is:

```ini
retrorun_audio_backend = go2
retrorun_audio_buffer = 512
retrorun_audio_stable_buffer = false
retrorun_force_audio_multithread = false
```

The remaining video and Flycast2021 settings stay unchanged. Occasional
slow-then-fast recovery is a known GO2 trade-off in the hard scene.

RetroRun now also contains an experimental GO2/OpenAL equivalent of the SDL2
low-watermark compensation:

```ini
retrorun_audio_backend = go2
retrorun_go2_audio_stretch_percent = 3
retrorun_go2_audio_stretch_low_ms = 20
```

The implementation tracks usable queued frames across the OpenAL buffer
queue, linearly expands only the next batch when the reserve is below the
threshold, and exposes queue depth plus `adaptive_stretch_frames` in benchmark
JSON. It compiles in both native GO2 and hybrid ARM builds. The percentage
defaults to zero and stable-buffer mode suppresses it.

The original 5%/40 ms device profile reduced underruns by about 16.5%, but
intervened almost continuously and lost 3.15% presented FPS. The corrected
3%/20 ms candidate retained about 15.9% underrun reduction while limiting
extra audio frames from roughly 127,650 to 17,115 per run; its measured FPS
cost was about 1.6%. A later Soul Calibur listening test found no compelling
improvement and still heard a slight audio slowdown. The measured starvation
reduction therefore does not justify enabling the mode by default. It remains
an opt-in diagnostic/compatibility profile rather than the accurate default.

For the RG351V low-end audio compatibility mode validated above, add:

```ini
retrorun_audio_backend = sdl2
retrorun_sdl_audio_stretch_percent = 5
retrorun_sdl_audio_stretch_low_ms = 40
```

Omit these lines, or set the percentage to `0`, for the accurate default path.

For a frameskip experiment, change only the core frameskip option and label the
run clearly. RetroRun `retrorun_frameskip` must remain zero.

## Evidence and companion report

Full prior measurements, rejected builds, raw evidence layout and methodology:

`benchmarks/rg351v-amberelec-flycast2021-build-optimization-sonic-adventure-2-2026-07-21/REPORT.md`

Continue from this handoff and the companion report; do not repeat rejected
compiler experiments, do not update wholesale to modern Flycast, and do not
begin another invasive batching change before completing Phase 0.
