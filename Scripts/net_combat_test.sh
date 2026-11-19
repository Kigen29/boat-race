#!/usr/bin/env bash
# Controlled host/client combat; about a minute, no graphics or save writes.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
LOGS="$PROJECT_DIR/Saved/Logs"
mkdir -p "$LOGS"
HOST_LOG="$LOGS/net_combat_host.log"
CLIENT_LOG="$LOGS/net_combat_client.log"
COMMON=(-game -nullrhi -nosound -unattended -nowrite -Autopilot -ExitAfter=95)
HOST=""
CLIENT=""
cleanup() {
	[ -z "$CLIENT" ] || kill "$CLIENT" 2>/dev/null || true
	[ -z "$HOST" ] || kill "$HOST" 2>/dev/null || true
}
trap cleanup EXIT
timeout 110 "$EDITOR_CMD" "$PROJECT_FILE" "/Engine/Maps/Entry?listen" "${COMMON[@]}" \
	-NetCombatTest -port=7787 -abslog="$HOST_LOG" > "$HOST_LOG.console" 2>&1 &
HOST=$!
sleep 12
timeout 110 "$EDITOR_CMD" "$PROJECT_FILE" 127.0.0.1:7787 "${COMMON[@]}" \
	-abslog="$CLIENT_LOG" > "$CLIENT_LOG.console" 2>&1 &
CLIENT=$!
host_status=0
client_status=0
wait "$CLIENT" || client_status=$?
wait "$HOST" || host_status=$?
python3 - "$HOST_LOG" "$CLIENT_LOG" "$host_status" "$client_status" <<'PY'
from pathlib import Path
import re
import sys

host, client = [Path(p).read_text(errors="replace") for p in sys.argv[1:3]]
checks = {
    "gun damage and release agree": "Net combat verified: gun damage and trigger release" in host,
    "missile target, ammo and damage agree": "Net combat verified: missile lock ammo and damage" in host,
    "client flares divert missile without damage": "Net combat verified: client flares and no blast damage" in host,
    "wreck recovery and protection replicate": "Net combat verified: wreck recovery and respawn protection" in host,
    "server completed": "NETWORK COMBAT SERVER PASSED" in host,
    "client completed": "NETWORK COMBAT CLIENT PASSED" in client,
    "processes exited cleanly": sys.argv[3:] == ["0", "0"],
    "no crash or assertion": not re.search(r"Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception|NETWORK COMBAT FAILED", host + client),
}
for label, passed in checks.items():
    print("  ok  " if passed else " FAIL ", label)
passed = all(checks.values())
print("NETWORK COMBAT TEST PASSED" if passed else "NETWORK COMBAT TEST FAILED; logs: " + ", ".join(sys.argv[1:3]))
sys.exit(0 if passed else 1)
PY
