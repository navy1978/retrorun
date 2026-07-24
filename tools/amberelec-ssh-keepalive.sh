#!/bin/sh

# Keep remote benchmark sessions reachable when EmulationStation is stopped.
# Run this script from an independent systemd service, not from the SSH session
# or the EmulationStation service hierarchy.

interval=${KEEPALIVE_INTERVAL:-10}

while :; do
    if ! systemctl is-active --quiet sshd; then
        systemctl reset-failed sshd 2>/dev/null || true
        systemctl restart sshd 2>/dev/null || true
    fi

    sleep "$interval"
done
