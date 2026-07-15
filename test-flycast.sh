#!/bin/sh

set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CORE="$HOME/Library/Application Support/RetroArch/cores/flycast_libretro.dylib"
DEFAULT_ROM="$HOME/Library/Application Support/OpenEmu/Game Library/roms/Sega Dreamcast/Soulcalibur (Europe) (EnFrDeEs)/Soulcalibur (Europe) (En,Fr,De,Es).cue"

if [ "$#" -gt 1 ]; then
    echo "Uso: $0 [/percorso/del/gioco.{cue,gdi,cdi,chd}]" >&2
    exit 2
fi

ROM=${1:-$DEFAULT_ROM}

if [ ! -x "$PROJECT_DIR/retrorun" ]; then
    echo "RetroRun non trovato. Esegui prima 'make' in: $PROJECT_DIR" >&2
    exit 1
fi

if [ ! -f "$CORE" ]; then
    echo "Core Flycast non trovato: $CORE" >&2
    exit 1
fi

if [ ! -f "$ROM" ]; then
    echo "Gioco non trovato: $ROM" >&2
    exit 1
fi

mkdir -p "$PROJECT_DIR/saves" "$PROJECT_DIR/system"

exec "$PROJECT_DIR/retrorun" \
    -s "$PROJECT_DIR/saves" \
    -d "$PROJECT_DIR/system" \
    "$CORE" \
    "$ROM"
