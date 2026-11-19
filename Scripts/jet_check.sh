#!/usr/bin/env bash
# Opens the jet game (phase two), parks the camera at 30 m, 300 m, 1.5 km and
# 4 km over the sea, measures the frame rate and takes a screenshot at each,
# then quits. Fails on a crash, a missing stop or a frame rate below the minimum.
# About half a minute.
#   Scripts/jet_check.sh [minimum fps, default 30]
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
MIN_FPS="${1:-30}"
SHOTS="$PROJECT_DIR/Saved/Screenshots/LinuxEditor"
LOG="$PROJECT_DIR/Saved/Logs/jet_check.log"
mkdir -p "$(dirname "$LOG")"
rm -f "$SHOTS"/Jet_*.png
timeout 300 "$EDITOR" "$PROJECT_FILE" "/Engine/Maps/Entry?game=Jets" -game -log -windowed -ResX=1280 -ResY=720 \
	-JetCheck -nowrite -abslog="$LOG" "${@:2}" >/dev/null 2>&1 || true

fail=0
grep -q "Game class is 'JetGameMode'" "$LOG" || { echo "the jet game mode did not load"; fail=1; }
if grep -qE "Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception" "$LOG"; then
	echo "CRASH OR ASSERT:"; grep -E "Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception" "$LOG" | head -5; fail=1
fi
grep -oE "Jet check: [0-9]+ m, [0-9]+ fps" "$LOG"
stops=$(grep -cE "Jet check: [0-9]+ m" "$LOG")
echo "stops: $stops (want 4)"; [ "$stops" -ge 4 ] || fail=1
low=$(grep -oE "Jet check: [0-9]+ m, [0-9]+ fps" "$LOG" | awk '{print $(NF-1)}' | sort -n | head -1)
echo "lowest fps: ${low:-none} (want >= $MIN_FPS)"
[ -n "$low" ] && [ "$low" -ge "$MIN_FPS" ] || fail=1
[ "$fail" = 0 ] && echo "JET CHECK PASSED (screenshots: $SHOTS/Jet_*.png)" || echo "JET CHECK FAILED (log: $LOG)"
exit $fail
