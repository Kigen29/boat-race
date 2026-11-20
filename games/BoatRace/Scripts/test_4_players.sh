#!/usr/bin/env bash
# Starts a host and three clients on this one machine, tiled 2 x 2, to prove
# that four players can race at once. Needs a capable GPU: it runs four copies
# of the game. Close the windows (or Ctrl+C here) to stop.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

W=960
H=540
COMMON=(-game -windowed -ResX=$W -ResY=$H -log -nosplash)

trap 'kill $(jobs -p) 2>/dev/null || true' EXIT

"$EDITOR" "$PROJECT_FILE" "/Engine/Maps/Entry?listen?Name=Host" "${COMMON[@]}" -WinX=0 -WinY=0 &
echo "Host starting. Waiting ${HOST_WAIT:=25} s before the clients connect (raise HOST_WAIT on a slow first run)."
sleep "$HOST_WAIT"

"$EDITOR" "$PROJECT_FILE" "127.0.0.1?Name=Player2" "${COMMON[@]}" -WinX=$W -WinY=0 &
"$EDITOR" "$PROJECT_FILE" "127.0.0.1?Name=Player3" "${COMMON[@]}" -WinX=0 -WinY=$H &
"$EDITOR" "$PROJECT_FILE" "127.0.0.1?Name=Player4" "${COMMON[@]}" -WinX=$W -WinY=$H &

echo "In the Host window (top-left) press ENTER to start the race."
wait
