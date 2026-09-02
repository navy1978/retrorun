#!/bin/sh

# Keep the RG552 Wi-Fi and SSH services reachable on AmberELEC. Recovery is
# deliberately staged: reconnect, cycle the Wi-Fi radio, restart the network
# stack, and finally reboot only after a sustained outage.

PID_FILE="/tmp/rg552-wifi-keepalive.pid"
LOCK_DIR="/tmp/rg552-wifi-keepalive.lock"
LOG_FILE="/tmp/rg552-wifi-keepalive.log"
STATE_DIR="${RG552_KEEPALIVE_STATE_DIR:-/storage/.config/retrorun-tools}"
WIFI_SERVICE_FILE="$STATE_DIR/rg552-wifi-service"
REBOOT_STAMP_FILE="$STATE_DIR/rg552-wifi-last-reboot"

INTERVAL="${RG552_KEEPALIVE_INTERVAL:-10}"
RECOVER_AFTER="${RG552_KEEPALIVE_RECOVER_AFTER:-2}"
RESTART_AFTER="${RG552_KEEPALIVE_RESTART_AFTER:-6}"
REBOOT_AFTER="${RG552_KEEPALIVE_REBOOT_AFTER:-12}"
REBOOT_COOLDOWN="${RG552_KEEPALIVE_REBOOT_COOLDOWN:-900}"
WIFI_INTERFACE="${RG552_WIFI_INTERFACE:-wlan0}"
lock_owned=0

positive_integer_or_default()
{
    value="$1"
    default_value="$2"
    case "$value" in
        ''|*[!0-9]*|0)
            printf '%s\n' "$default_value"
            ;;
        *)
            printf '%s\n' "$value"
            ;;
    esac
}

validate_settings()
{
    INTERVAL="$(positive_integer_or_default "$INTERVAL" 10)"
    RECOVER_AFTER="$(positive_integer_or_default "$RECOVER_AFTER" 2)"
    RESTART_AFTER="$(positive_integer_or_default "$RESTART_AFTER" 6)"
    REBOOT_AFTER="$(positive_integer_or_default "$REBOOT_AFTER" 12)"
    REBOOT_COOLDOWN="$(positive_integer_or_default "$REBOOT_COOLDOWN" 900)"

    if [ "$RECOVER_AFTER" -ge "$RESTART_AFTER" ]; then
        RECOVER_AFTER=2
        RESTART_AFTER=6
    fi
    if [ "$RESTART_AFTER" -ge "$REBOOT_AFTER" ]; then
        REBOOT_AFTER=$((RESTART_AFTER * 2))
    fi
}

log_message()
{
    printf '%s %s\n' "$(date '+%Y-%m-%dT%H:%M:%S%z')" "$*"
}

get_gateway()
{
    ip route 2>/dev/null | awk -v dev="$WIFI_INTERFACE" \
        '$1 == "default" && $5 == dev { print $3; exit }'
}

get_connected_wifi_service()
{
    connmanctl services 2>/dev/null | awk \
        '$NF ~ /^wifi_/ && $1 ~ /^\*/ { print $NF; exit }'
}

remember_wifi_service()
{
    service="$1"
    case "$service" in
        wifi_*)
            mkdir -p "$STATE_DIR" 2>/dev/null || true
            printf '%s\n' "$service" > "$WIFI_SERVICE_FILE" 2>/dev/null || true
            ;;
    esac
}

get_wifi_service()
{
    connected_service="$(get_connected_wifi_service)"
    if [ -n "$connected_service" ]; then
        remember_wifi_service "$connected_service"
        printf '%s\n' "$connected_service"
        return 0
    fi

    if [ -r "$WIFI_SERVICE_FILE" ]; then
        saved_service="$(sed -n '1p' "$WIFI_SERVICE_FILE" 2>/dev/null)"
        case "$saved_service" in
            wifi_*)
                printf '%s\n' "$saved_service"
                return 0
                ;;
        esac
    fi

    # At installation time this normally is not needed because the connected
    # service is persisted. When booting offline, only select a ConnMan service
    # marked Favorite; never auto-connect to an arbitrary visible open network.
    for candidate in $(connmanctl services 2>/dev/null | awk '$NF ~ /^wifi_/ { print $NF }'); do
        if connmanctl services "$candidate" 2>/dev/null | grep -q 'Favorite = True'; then
            remember_wifi_service "$candidate"
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

ensure_sshd()
{
    if ! systemctl is-active --quiet sshd.service; then
        log_message "sshd is inactive; restarting it"
        systemctl reset-failed sshd.service 2>/dev/null || true
        systemctl restart sshd.service 2>/dev/null || true
    fi
}

prepare_wifi()
{
    ip link set "$WIFI_INTERFACE" up 2>/dev/null || true
    if command -v iw >/dev/null 2>&1; then
        iw dev "$WIFI_INTERFACE" set power_save off 2>/dev/null || true
    fi
    connmanctl enable wifi >/dev/null 2>&1 || true
}

connect_saved_wifi()
{
    wifi_service="$(get_wifi_service 2>/dev/null || true)"
    if [ -n "$wifi_service" ]; then
        log_message "requesting ConnMan connection to $wifi_service"
        connmanctl connect "$wifi_service" >/dev/null 2>&1 || true
    else
        log_message "no saved ConnMan Wi-Fi service is available"
    fi
}

cycle_wifi_radio()
{
    log_message "cycling the ConnMan Wi-Fi radio"
    connmanctl disable wifi >/dev/null 2>&1 || true
    sleep 2
    connmanctl enable wifi >/dev/null 2>&1 || true
    sleep 2
    prepare_wifi
    connect_saved_wifi
}

restart_network_stack()
{
    log_message "restarting wpa_supplicant and ConnMan"
    systemctl restart wpa_supplicant.service 2>/dev/null || true
    systemctl restart connman.service 2>/dev/null || true

    # ConnMan needs a few seconds after restart before services become visible.
    attempt=0
    while [ "$attempt" -lt 10 ]; do
        attempt=$((attempt + 1))
        sleep 1
        prepare_wifi
        wifi_service="$(get_wifi_service 2>/dev/null || true)"
        if [ -n "$wifi_service" ]; then
            connmanctl connect "$wifi_service" >/dev/null 2>&1 || true
            return 0
        fi
    done
    log_message "ConnMan did not expose a saved Wi-Fi service after restart"
}

reboot_allowed()
{
    now="$(date +%s 2>/dev/null || printf '0')"
    case "$now" in
        ''|*[!0-9]*) now=0 ;;
    esac

    last=0
    if [ -r "$REBOOT_STAMP_FILE" ]; then
        last="$(sed -n '1p' "$REBOOT_STAMP_FILE" 2>/dev/null)"
        case "$last" in
            ''|*[!0-9]*) last=0 ;;
        esac
    fi

    # Also suppress another reboot when the clock moved backwards or was not
    # restored yet. Network recovery continues while the cooldown is active.
    [ "$now" -gt 0 ] || return 1
    [ "$now" -ge "$last" ] || return 1
    [ $((now - last)) -ge "$REBOOT_COOLDOWN" ]
}

request_guarded_reboot()
{
    if ! reboot_allowed; then
        log_message "reboot suppressed by the ${REBOOT_COOLDOWN}s anti-loop cooldown"
        return 1
    fi

    now="$(date +%s 2>/dev/null || printf '0')"
    mkdir -p "$STATE_DIR" 2>/dev/null || true
    printf '%s\n' "$now" > "$REBOOT_STAMP_FILE" 2>/dev/null || true
    sync
    log_message "connectivity remained down; rebooting RG552 as last resort"
    systemctl reboot
    return 0
}

is_running()
{
    [ -r "$PID_FILE" ] || return 1
    pid="$(sed -n '1p' "$PID_FILE" 2>/dev/null)"
    case "$pid" in
        ''|*[!0-9]*) return 1 ;;
    esac
    kill -0 "$pid" 2>/dev/null
}

acquire_lock()
{
    if mkdir "$LOCK_DIR" 2>/dev/null; then
        printf '%s\n' "$$" > "$PID_FILE"
        lock_owned=1
        return 0
    fi

    if is_running; then
        return 1
    fi

    rm -f "$PID_FILE"
    rmdir "$LOCK_DIR" 2>/dev/null || return 1
    mkdir "$LOCK_DIR" 2>/dev/null || return 1
    printf '%s\n' "$$" > "$PID_FILE"
    lock_owned=1
}

cleanup_worker()
{
    exit_status=$?
    trap - 0 INT TERM HUP
    if [ "$lock_owned" -eq 1 ] && [ "$(sed -n '1p' "$PID_FILE" 2>/dev/null)" = "$$" ]; then
        rm -f "$PID_FILE"
        rmdir "$LOCK_DIR" 2>/dev/null || true
    fi
    exit "$exit_status"
}

start_keepalive()
{
    if is_running; then
        echo "RG552 Wi-Fi keepalive is already running (PID $pid)."
        return 0
    fi

    rm -f "$PID_FILE"
    rmdir "$LOCK_DIR" 2>/dev/null || true
    fallback_gateway="${1:-$(get_gateway)}"
    nohup sh "$0" run "$fallback_gateway" >>"$LOG_FILE" 2>&1 &
    worker_pid=$!

    sleep 1
    if is_running; then
        echo "RG552 Wi-Fi keepalive started (PID $pid)."
        echo "Interval: ${INTERVAL}s; log: $LOG_FILE"
        return 0
    fi

    echo "Unable to start RG552 Wi-Fi keepalive (worker PID $worker_pid)."
    return 1
}

stop_keepalive()
{
    if ! is_running; then
        echo "RG552 Wi-Fi keepalive is not running."
        rm -f "$PID_FILE"
        rmdir "$LOCK_DIR" 2>/dev/null || true
        return 0
    fi

    worker_pid="$pid"
    kill "$worker_pid" 2>/dev/null || true
    count=0
    while kill -0 "$worker_pid" 2>/dev/null && [ "$count" -lt 10 ]; do
        count=$((count + 1))
        sleep 1
    done
    if kill -0 "$worker_pid" 2>/dev/null; then
        echo "RG552 Wi-Fi keepalive did not stop (PID $worker_pid)."
        return 1
    fi
    echo "RG552 Wi-Fi keepalive stopped."
}

show_status()
{
    if is_running; then
        echo "RG552 Wi-Fi keepalive is running (PID $pid)."
        echo "Log: $LOG_FILE (or systemd journal when launched as a service)"
        return 0
    fi
    echo "RG552 Wi-Fi keepalive is not running."
    return 1
}

run_keepalive()
{
    fallback_gateway="$1"
    failures=0
    checks=0

    if ! acquire_lock; then
        log_message "another keepalive worker is already running"
        return 0
    fi
    trap cleanup_worker 0
    trap 'exit 0' INT TERM HUP

    prepare_wifi
    wifi_service="$(get_wifi_service 2>/dev/null || true)"
    log_message "watchdog started: interface=$WIFI_INTERFACE service=${wifi_service:-unknown} interval=${INTERVAL}s"

    while :; do
        ensure_sshd
        gateway="$(get_gateway)"
        [ -n "$gateway" ] || gateway="$fallback_gateway"

        if [ -n "$gateway" ] && ping -c 1 -W 2 "$gateway" >/dev/null 2>&1; then
            if [ "$failures" -gt 0 ]; then
                log_message "Wi-Fi restored after $failures failed checks"
            fi
            failures=0
            connected_service="$(get_connected_wifi_service)"
            [ -z "$connected_service" ] || remember_wifi_service "$connected_service"
        else
            failures=$((failures + 1))
            if [ "$failures" -eq 1 ]; then
                log_message "connectivity check failed: gateway=${gateway:-missing}"
            fi

            if [ "$failures" -eq "$RECOVER_AFTER" ]; then
                cycle_wifi_radio
            elif [ "$failures" -ge "$REBOOT_AFTER" ] &&
                 [ $((failures % REBOOT_AFTER)) -eq 0 ]; then
                request_guarded_reboot || restart_network_stack
            elif [ "$failures" -ge "$RESTART_AFTER" ] &&
                 [ $((failures % RESTART_AFTER)) -eq 0 ]; then
                restart_network_stack
            fi
        fi

        checks=$((checks + 1))
        if [ "$checks" -ge 12 ]; then
            log_message "heartbeat: sshd=$(systemctl is-active sshd.service 2>/dev/null) gateway=${gateway:-missing} failures=$failures"
            checks=0
        fi
        sleep "$INTERVAL"
    done
}

validate_settings

case "${1:-start}" in
    start)
        start_keepalive "$2"
        ;;
    stop)
        stop_keepalive
        ;;
    restart)
        stop_keepalive || exit 1
        start_keepalive "$2"
        ;;
    status)
        show_status
        ;;
    run)
        run_keepalive "$2"
        ;;
    *)
        echo "Usage: $0 {start [router-ip]|stop|restart [router-ip]|status|run [router-ip]}"
        exit 1
        ;;
esac
