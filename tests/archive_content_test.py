#!/usr/bin/env python3
"""Synthetic fixtures only: no game assets, network or external unzip process."""
import pathlib
import struct
import subprocess
import sys
import tempfile
import zipfile

with tempfile.TemporaryDirectory(prefix="rr-archive-test-") as tmp:
    root = pathlib.Path(tmp)
    data = b"NES\x1afake test content"

    def archive(name, members, compression=zipfile.ZIP_DEFLATED):
        with zipfile.ZipFile(root / name, "w", compression=compression) as out:
            for path, content in members:
                out.writestr(path, content)

    (root / "raw.nes").write_bytes(data)
    (root / "invalid.zip").write_bytes(b"not a zip")
    archive("stored.zip", [("game.nes", data)], zipfile.ZIP_STORED)
    archive("deflated.ZIP", [("game.NES", data)])
    archive("nested.zip", [("folder/game.nes", data)])
    archive("metadata.zip", [("__MACOSX/._game.nes", b"metadata"),
                             ("README.txt", b"readme"), ("game.nes", data)])
    archive("empty.zip", [])
    archive("no-match.zip", [("other.sfc", data)])
    archive("multiple.zip", [("one.nes", data), ("two.nes", data)])
    archive("unsafe.zip", [("../game.nes", data)])
    archive("disc.zip", [("game.cue", b"FILE game.bin BINARY")])
    corrupt = bytearray((root / "stored.zip").read_bytes())
    corrupt[corrupt.index(data)] ^= 1
    (root / "corrupt.zip").write_bytes(corrupt)
    encrypted = bytearray((root / "stored.zip").read_bytes())
    struct.pack_into("<H", encrypted, 6, 1)
    central = encrypted.index(b"PK\x01\x02")
    struct.pack_into("<H", encrypted, central + 8, 1)
    (root / "encrypted.zip").write_bytes(encrypted)
    oversized = bytearray((root / "stored.zip").read_bytes())
    central = oversized.index(b"PK\x01\x02")
    struct.pack_into("<I", oversized, central + 24, 512 * 1024 * 1024 + 1)
    (root / "oversized.zip").write_bytes(oversized)
    subprocess.run([sys.argv[1], str(root)], check=True)
