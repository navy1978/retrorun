#!/bin/sh
set -eu

TEST_ROOT=/roms/retrorun-test
SONIC_ROOT="$TEST_ROOT/sonic-v10-menu-compare-20260728"
DOA_VAULT="$TEST_ROOT/doa-adaptive-v2"
FRONTEND="$DOA_VAULT/v10-vblank-vault-20260728/retrorun-rg351v-v28"
CORE="$DOA_VAULT/v28-vault-20260728/flycast-adaptive-v9.so"
CONFIG="$SONIC_ROOT/sonic-c1-catalog-audio.cfg"
ROM_PATH=${1:?missing ROM path}

for required in "$FRONTEND" "$CORE" "$CONFIG" "$ROM_PATH"; do
    if [ ! -e "$required" ]; then
        echo "Sonic C1: missing $required" >&2
        exit 1
    fi
done

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

exec "$FRONTEND" \
    -c "$CONFIG" \
    -d /roms/bios \
    -s /roms/dreamcast \
    "$CORE" "$ROM_PATH"
