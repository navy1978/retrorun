libchdr v0.3.0

Source: https://github.com/rtissera/libchdr
Commit: 93d8c239ff0d4e8d7722985992649fce12d2463b

Snapshot of include/, src/ and deps/, excluding CMakeLists.txt and .gitignore,
with the two upstream backports listed below.
Built by build/disc-reader.mk with system zlib, bundled LZMA and Zstandard,
and the dr_flac backend. License notices are retained in this directory.

Applied upstream fixes (patches/):
- https://github.com/rtissera/libchdr/commit/1a10dd39ee18d07f3fda2f887d694302c8e4efc7
- https://github.com/rtissera/libchdr/commit/d147f767ba17f123edbbe55125108a240dbe84d1

These remove undefined shifts found by the sanitizer tests and validate map bit
widths. upstream-sha256.json records files before these explicit backports.
