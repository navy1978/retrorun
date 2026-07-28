#!/bin/sh
set -eu

VAULT=/roms/retrorun-test/doa-adaptive-v2/v10-vblank-vault-20260728
FRONTEND="$VAULT/retrorun-rg351v-v28"
CORE="$VAULT/flycast-rg351v-adaptive-v10.so"
CONFIG="$VAULT/doa-adaptive-v23-fast-aica.cfg"
ROM_PATH=${1:?missing ROM path}

for required in "$FRONTEND" "$CORE" "$CONFIG" "$ROM_PATH"; do
    if [ ! -e "$required" ]; then
        echo "DOA2 v10: missing $required" >&2
        exit 1
    fi
done

if [ "${RETRORUN_DOA_V10_DRY_RUN:-0}" = 1 ]; then
    printf 'DOA2_V10_SELECTED\nfrontend=%s\ncore=%s\nconfig=%s\nrom=%s\n' \
        "$FRONTEND" "$CORE" "$CONFIG" "$ROM_PATH"
    exit 0
fi

CPU=/sys/devices/system/cpu/cpufreq/policy0
DMC=/sys/class/devfreq/dmc
GPU=/sys/class/devfreq/ff400000.gpu

echo performance > "$CPU/scaling_governor"
echo 1296000 > "$CPU/scaling_max_freq"
echo 1296000 > "$CPU/scaling_min_freq"
echo userspace > "$DMC/governor"
echo 786000000 > "$DMC/max_freq"
echo 786000000 > "$DMC/min_freq"
echo userspace > "$GPU/governor"
echo 520000000 > "$GPU/max_freq"
echo 520000000 > "$GPU/min_freq"

exec env \
    RETRORUN_GO2_AUDIO_WSOLA_PERCENT=25 \
    RETRORUN_GO2_AUDIO_WSOLA_EMERGENCY_PERCENT=33 \
    RETRORUN_GO2_AUDIO_WSOLA_EMERGENCY_MS=65 \
    RETRORUN_GO2_AUDIO_WSOLA_LOW_MS=60 \
    RETRORUN_GO2_AUDIO_WSOLA_HIGH_MS=120 \
    RETRORUN_GO2_AUDIO_WSOLA_WINDOW_FRAMES=1024 \
    RETRORUN_GO2_AUDIO_DYNAMIC_PITCH=0 \
    RETRORUN_GO2_AUDIO_PLAYBACK_PERCENT=96 \
    "$FRONTEND" \
    -c "$CONFIG" \
    -d /roms/bios \
    -s /roms/dreamcast \
    "$CORE" "$ROM_PATH"
