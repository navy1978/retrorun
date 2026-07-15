#!/bin/sh

set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CORE="$HOME/Library/Application Support/RetroArch/cores/bsnes2014_performance_libretro.dylib"
ROM="$HOME/Downloads/Super Ghouls 'N Ghosts (USA).sfc"

if [ ! -x "$PROJECT_DIR/retrorun" ]; then
    echo "RetroRun non trovato. Esegui prima 'make' in: $PROJECT_DIR" >&2
    exit 1
fi

if [ ! -f "$CORE" ]; then
    echo "Core bsnes2014 non trovato: $CORE" >&2
    exit 1
fi

if [ ! -f "$ROM" ]; then
    echo "ROM non trovata: $ROM" >&2
    exit 1
fi

mkdir -p "$PROJECT_DIR/saves" "$PROJECT_DIR/system"

exec "$PROJECT_DIR/retrorun" \
    -s "$PROJECT_DIR/saves" \
    -d "$PROJECT_DIR/system" \
    "$CORE" \
    "$ROM"
