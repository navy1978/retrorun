# RetroAchievements and large content files

## Symptom

With RetroAchievements enabled, launching a large Dreamcast CDI image can make
RetroRun appear frozen while the game is being identified.

## Root cause

RetroRun already performs RetroAchievements HTTP requests on worker threads,
but the completion callbacks are dispatched by `achievements_frame()` on the
emulation/render thread. After login, the callback calls
`rc_client_begin_identify_and_load_game()`.

Game identification is not asynchronous in the bundled rcheevos version. That
function calculates the first content hash before it returns.

The bundled rcheevos extension table supports Dreamcast GDI, CUE and CHD
content through its optical-disc readers. It has no CDI entry or CDI reader.
For an unknown extension it deliberately falls back to its generic
`rc_hash_whole_file()` path. Despite that function's name, this rcheevos
version caps the hash at the first 64 MiB (`MAX_BUFFER_SIZE`). Reading and
hashing those 64 MiB synchronously from slow storage is still enough to stall
RetroRun's emulation thread and make startup appear frozen.

## RetroArch comparison

The inspected RetroArch source uses the same rcheevos entry point:

```c
rc_client_begin_identify_and_load_game(...)
```

Its bundled rcheevos also has no CDI mapping. RetroArch adds lifecycle
generation checks around the asynchronous server response, but the initial
hash generation is still synchronous in `rc_client_begin_identify_and_load_game`.
It does not provide evidence that CDI images are identified using a native
Dreamcast hash.

For supported Dreamcast descriptors, rcheevos reads the relevant track data
and hashes Dreamcast boot metadata instead of hashing the entire disc image.
GDI, CUE or CHD is consequently preferable to CDI when RetroAchievements is
required.

## RetroRun mitigation

For CDI images larger than the normal 64MB buffering threshold, RetroRun now:

1. keeps the image file-backed;
2. calculates rcheevos's whole-file fallback hash on a worker thread;
3. continues emulation and presentation while hashing;
4. submits the completed fallback hash to `rc_client_begin_load_game()` on the main
   thread;
5. reports the elapsed hash time in the log;
6. shows `Hashing game in background` in the achievements view.

This preserves the previous 64 MiB fallback hash instead of silently changing
the game identity. It prevents the startup/render-thread stall, but cannot
make rcheevos understand the CDI track layout. If RetroAchievements has no
fallback hash registered for that exact CDI image, no achievement set will be
found.

## Verification

The host build and existing unit tests must pass:

```sh
make test
make macos-sdl2
```

An ARM64 hybrid candidate was built successfully on the Ubuntu ARM VM:

```text
/home/navy78/retrorun-candidates-20260724/retrorun_hybrid_ra_background_hash
SHA-256: 5019717a7f74ee19474ef7e7d24370a8560eb26e3bd7a137ee5f620df0c2204c
```

The ELF contains all three expected status/log strings. This proves the target
code is linked into the ARM build, but it is not a substitute for the device
regression test.

The on-device regression test should use a CDI image larger than 1GB and
confirm all of the following:

- gameplay starts and remains responsive during identification;
- the log prints `hashing unsupported image fallback in background`;
- the log eventually prints `RetroAchievements background hash` with
  `elapsed_ms`;
- exiting during or after identification is safe;
- GDI, CUE, CHD and cartridge-sized content retain their existing behaviour.

On 2026-07-24 the candidate was copied to the RG351V as a separate executable:

```text
/storage/retrorun/retrorun_hybrid_ra_background_hash
SHA-256: 5019717a7f74ee19474ef7e7d24370a8560eb26e3bd7a137ee5f620df0c2204c
```

It did not replace the installed frontend. The integration test was completed
after RetroAchievements was configured on the device:

- login succeeded, a token was persisted, and the plaintext password was
  removed;
- the 842 MiB Soul Calibur CDI selected background fallback hashing;
- the 64 MiB fallback hash completed in 1,145 ms;
- RetroRun presented 2,833 frames during the 60-second test and exited
  normally;
- a normal benchmark exit requested 0.5 seconds after startup safely overlapped
  the hash worker and left no residual process;
- a GDI control retained the existing optical reader and did not select the CDI
  background fallback.

The largest available CDI is the 842 MiB Soul Calibur image. Although it is
smaller than the preferred 1 GiB test case, it is still well above rcheevos's
64 MiB fallback threshold and is sufficient to exercise the background path.

Detailed evidence is in
`benchmarks/rg351v-amberelec-retroachievements-large-content-2026-07-24/`.

## Remaining limitation

Shutdown currently waits for an in-progress hash worker to finish before
destroying rcheevos state. Startup no longer freezes, but exiting immediately
during the 64 MiB fallback hash can still wait for that read to finish. A
future improvement could add a cancellable file-reader wrapper or native CDI
track support to rcheevos.
