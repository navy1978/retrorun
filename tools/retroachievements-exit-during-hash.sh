#!/bin/sh
set -u

frontend=${1:?frontend path required}
core=${2:?core path required}
content=${3:?content path required}
log=${4:-/tmp/retrorun-ra-exit-during-hash.log}

cd "$(dirname "$frontend")" || exit 1

"$frontend" \
    --benchmark 60 \
    -s /storage/retrorun \
    -d /storage/roms/bios \
    "$core" "$content" >"$log" 2>&1 &
pid=$!

while kill -0 "$pid" 2>/dev/null; do
    if grep -q "hashing unsupported image fallback in background" "$log"; then
        break
    fi
    sleep 0.02
done

if ! kill -0 "$pid" 2>/dev/null; then
    echo "RetroRun exited before background hashing began" >&2
    wait "$pid"
    exit $?
fi

echo "Sending TERM to PID $pid while background hashing is active"
kill -TERM "$pid"

status=0
wait "$pid" || status=$?
echo "RetroRun exit status: $status"

if kill -0 "$pid" 2>/dev/null; then
    echo "RetroRun process is still active after wait" >&2
    exit 1
fi

exit "$status"
