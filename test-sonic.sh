#!/bin/sh

set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CORE="$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
DEFAULT_ROM="$HOME/Downloads/sega-mega-drive-genesis-rom-set/Sega - Mega Drive - Genesis/Sonic The Hedgehog (USA, Europe).md"

if [ "$#" -gt 1 ]; then
    echo "Uso: $0 [/percorso/di/sonic.{md,bin,gen,zip}]" >&2
    exit 2
fi

ROM=${1:-$DEFAULT_ROM}

if [ ! -x "$PROJECT_DIR/retrorun" ]; then
    echo "RetroRun non trovato. Esegui prima 'make' in: $PROJECT_DIR" >&2
    exit 1
fi

if [ ! -f "$CORE" ]; then
    echo "Core Genesis Plus GX non trovato: $CORE" >&2
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
