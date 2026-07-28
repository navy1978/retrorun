#!/bin/sh
set -eu

PRODUCTION=/roms/retrorun-production
SOURCE="$PRODUCTION/retrorun.sh.patched"
TARGET=/usr/bin/retrorun.sh
STATE="$PRODUCTION/DOA_V10_STATE"

count=0
while { [ ! -f "$SOURCE" ] || [ ! -f "$STATE" ]; } && [ "$count" -lt 30 ]; do
    sleep 1
    count=$((count + 1))
done

[ -f "$SOURCE" ] || exit 1
[ -f "$STATE" ] || exit 1
[ "$(cat "$STATE")" = enabled ] || exit 0

if ! grep -q " $TARGET " /proc/mounts; then
    mount --bind "$SOURCE" "$TARGET"
fi
