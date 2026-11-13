#!/usr/bin/env bash
# Two copies of the game on this machine, no graphics: a host and a client
# that joins it by address. The host starts a race by itself, rivals fill
# the other seats, the client's boat drives itself. Passes if the client
# joined, the race started with four boats, and the server counted the
# client's boat through gates (which only happens if its moves replicate).
#   Scripts/net_test.sh        about two minutes
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
LOGS="$PROJECT_DIR/Saved/Logs"
HOST_LOG="$LOGS/net_host.log"; CLIENT_LOG="$LOGS/net_client.log"
rm -f "$HOST_LOG" "$CLIENT_LOG"
COMMON=(-game -nullrhi -nosound -unattended -log)
"$EDITOR_CMD" "$PROJECT_FILE" "/Engine/Maps/Entry?listen" "${COMMON[@]}" -AutoRace=25 -ExitAfter=120 -abslog="$HOST_LOG" >/dev/null 2>&1 &
HOST=$!
sleep 15
"$EDITOR_CMD" "$PROJECT_FILE" 127.0.0.1 "${COMMON[@]}" -Autopilot -ExitAfter=100 -abslog="$CLIENT_LOG" >/dev/null 2>&1 &
CLIENT=$!
wait $CLIENT; wait $HOST

fail=0
check() { if grep -qE "$2" "$1"; then echo "  ok    $3"; else echo "  FAIL  $3"; fail=1; fi; }
echo "Host:"
check "$HOST_LOG" "joined in slot 2" "a second player joined"
check "$HOST_LOG" "2 computer rival" "rivals took the two empty seats"
check "$HOST_LOG" "Countdown started with 4 player" "the race started with four boats"
check "$HOST_LOG" "passed gate [0-9]+" "boats are passing gates"
# Both copies share this computer's name; the client is the one in slot 2,
# and the host's own boat sits still, so slot 2 gates are the client's.
check "$HOST_LOG" "\(slot 2\) passed gate" "the client's own boat scored gates (its moves replicate)"
echo "Client:"
check "$CLIENT_LOG" "Bay built" "built the bay"
check "$CLIENT_LOG" "Weather now" "received the weather"
for LOG in "$HOST_LOG" "$CLIENT_LOG"; do
	if grep -qE "Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception" "$LOG"; then
		echo "  FAIL  crash or assert in $(basename "$LOG")"; fail=1
	fi
done
[ "$fail" = 0 ] && echo "NETWORK TEST PASSED" || echo "NETWORK TEST FAILED (logs: $HOST_LOG, $CLIENT_LOG)"
exit $fail
