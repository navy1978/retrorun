#!/bin/sh
set -eu

PRODUCTION=/roms/retrorun-production
TARGET=/usr/bin/retrorun.sh

printf 'disabled\n' > "$PRODUCTION/DOA_V10_STATE"
if grep -q " $TARGET " /proc/mounts; then
    umount "$TARGET"
fi

echo "DOA2 v10 routing disabled; the firmware RetroRun wrapper is active"
