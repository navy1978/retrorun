#!/bin/sh

# Keep the RG552 Wi-Fi link active by periodically contacting the default
# gateway. Designed for AmberELEC/BusyBox and safe to launch more than once.

PID_FILE="/tmp/rg552-wifi-keepalive.pid"
LOG_FILE="/tmp/rg552-wifi-keepalive.log"
INTERVAL="${RG552_KEEPALIVE_INTERVAL:-10}"

get_gateway()
{
    ip route 2>/dev/null | awk '$1 == "default" { print $3; exit }'
}

is_running()
{
    [ -f "$PID_FILE" ] || return 1
    pid="$(cat "$PID_FILE" 2>/dev/null)"
    [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

start_keepalive()
{
    if is_running; then
        echo "RG552 Wi-Fi keepalive is already running (PID $pid)."
        return 0
    fi

    rm -f "$PID_FILE"
    gateway="${1:-$(get_gateway)}"
    if [ -z "$gateway" ]; then
        echo "Unable to detect the default gateway."
        echo "Connect Wi-Fi first or run: $0 start ROUTER_IP"
        return 1
    fi

    nohup sh "$0" run "$gateway" >>"$LOG_FILE" 2>&1 &
    worker_pid=$!
    echo "$worker_pid" >"$PID_FILE"

    sleep 1
    if kill -0 "$worker_pid" 2>/dev/null; then
        echo "RG552 Wi-Fi keepalive started (PID $worker_pid)."
        echo "Gateway: $gateway; interval: ${INTERVAL}s; log: $LOG_FILE"
        return 0
    fi

    echo "Unable to start RG552 Wi-Fi keepalive. Check $LOG_FILE"
    rm -f "$PID_FILE"
    return 1
}

stop_keepalive()
{
    if ! is_running; then
        echo "RG552 Wi-Fi keepalive is not running."
        rm -f "$PID_FILE"
        return 0
    fi

    kill "$pid" 2>/dev/null
    rm -f "$PID_FILE"
    echo "RG552 Wi-Fi keepalive stopped."
}

show_status()
{
    if is_running; then
        echo "RG552 Wi-Fi keepalive is running (PID $pid)."
        echo "Log: $LOG_FILE"
    else
        echo "RG552 Wi-Fi keepalive is not running."
        return 1
    fi
}

run_keepalive()
{
    gateway="$1"
    failures=0
    was_offline=0

    trap 'rm -f "$PID_FILE"; exit 0' INT TERM EXIT
    echo "$(date): keepalive worker started for gateway $gateway"

    while :; do
        if ping -c 1 -W 2 "$gateway" >/dev/null 2>&1; then
            if [ "$was_offline" -eq 1 ]; then
                echo "$(date): Wi-Fi connection restored after $failures failed checks"
            fi
            failures=0
            was_offline=0
        else
            failures=$((failures + 1))
            if [ "$was_offline" -eq 0 ]; then
                echo "$(date): gateway $gateway is not responding"
            fi
            was_offline=1
        fi
        sleep "$INTERVAL"
    done
}

case "${1:-start}" in
    start)
        start_keepalive "$2"
        ;;
    stop)
        stop_keepalive
        ;;
    restart)
        stop_keepalive
        start_keepalive "$2"
        ;;
    status)
        show_status
        ;;
    run)
        if [ -z "$2" ]; then
            echo "Missing gateway address."
            exit 1
        fi
        run_keepalive "$2"
        ;;
    *)
        echo "Usage: $0 {start [router-ip]|stop|restart [router-ip]|status}"
        exit 1
        ;;
esac
