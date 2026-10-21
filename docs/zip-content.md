# ZIP game content

ZIP handling lives in RetroRun's core loader. Launchers continue to pass the
original archive path as the normal ROM argument. No external `unzip` process
or archive-handling logic in a launcher is required.

For cores with `retro_system_info.block_extract == true`, RetroRun leaves the
original archive intact. Otherwise it selects the single archive member whose
extension matches the core's `valid_extensions` (case-insensitive). Directory
entries, `__MACOSX` and hidden metadata files are ignored. An archive containing
multiple matching files is rejected rather than choosing an arbitrary game.

Extraction streams through a 32 KiB buffer into a private temporary directory
inside the configured save directory (`-s`). Set `RETRORUN_EXTRACT_DIR` to an
existing writable directory to override that location. Use a disk-backed path
on handhelds: `/tmp` is commonly a RAM filesystem. Original archives are never
modified. Temporary files remain available until core shutdown and are then
removed, including normal load failures. A forced kill or power loss may leave
an abandoned temporary directory.

For `need_fullpath == true`, the core receives the temporary file path and
RetroRun does not allocate a complete ROM buffer. For `need_fullpath == false`,
the libretro API requires a memory buffer: RetroRun reads the extracted file,
passes its bytes during `retro_load_game`, and releases that buffer immediately
afterward, as with uncompressed content. Extraction itself does not retain a
second ROM-sized buffer. The core's own ROM allocation is outside frontend
control.

Save RAM, save-state names, configuration and decoration selection continue to
use the original command-line archive path, never the random temporary path.
ZIP-specific RetroAchievements identification has not been validated yet.

This initial implementation supports single-file ROMs in stored or deflated
ZIPs, including ZIP64 directory metadata. It checks extracted size and CRC.
Encrypted archives, unsafe member paths, empty content, more than 65,536 entries,
and extracted ROMs larger than 512 MiB are rejected. Frontend extraction of
CUE/GDI/M3U descriptors is intentionally rejected: those require their companion
files. Cores which block frontend extraction retain their native archive policy.
This does not add 7z/RAR support or select cores automatically.

The small ZIP reader in `deps/minizip` is pinned, unmodified MiniZip source from
zlib 1.3.1; provenance and hashes are in `SOURCE.json`, and license notices are
retained in the source headers. It uses the existing zlib dependency. No new
runtime library package, background thread or service is needed.

Build and verify on Linux:

```sh
make PLATFORM=linux-go2 config=release -j4
make -C tests run
make -C tests clean
make -C tests SANITIZER='-fsanitize=address,undefined -fno-omit-frame-pointer' run
```

On older GCC installations, pass `CXX=g++-10` to the tests. Synthetic ZIP fixtures
cover stored/deflated content, nested names, extension case, ignored metadata,
ambiguous archives, unsafe paths, corruption/CRC, encryption, size limits,
unwritable temporary locations, native archive passthrough and cleanup.
