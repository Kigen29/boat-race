#!/usr/bin/env bash
# Plays the game end to end (the -PhotoTour: title screen, a race with
# rivals, every weather, a blast, the pause menu) and fails if it crashed,
# missed a screenshot or ran too slowly. About four minutes.
#   Scripts/smoke_test.sh [minimum fps, default 25]
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
MIN_FPS="${1:-25}"
SHOTS="$PROJECT_DIR/Saved/Screenshots/LinuxEditor"
LOG="$PROJECT_DIR/Saved/Logs/smoke_test.log"
rm -rf "$SHOTS"
mkdir -p "$(dirname "$LOG")"
timeout 600 "$EDITOR" "$PROJECT_FILE" -game -log -windowed -ResX=1280 -ResY=720 -PhotoTour -abslog="$LOG" >/dev/null 2>&1 || true

fail=0
count=$(ls "$SHOTS"/*.png 2>/dev/null | wc -l)
echo "screenshots: $count (want 14)"; [ "$count" -ge 14 ] || fail=1
if grep -qE "Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception" "$LOG"; then
	echo "CRASH OR ASSERT:"; grep -E "Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception" "$LOG" | head -5; fail=1
fi
grep -q "models imported" "$LOG" || { echo "models were not imported"; fail=1; }
low=$(grep -oE "Photo tour: [^,]+, [0-9]+ fps" "$LOG" | awk '{print $(NF-1)}' | sort -n | head -1)
echo "lowest fps at a tour stop: ${low:-none} (want >= $MIN_FPS)"
[ -n "$low" ] && [ "$low" -ge "$MIN_FPS" ] || fail=1
[ "$fail" = 0 ] && echo "SMOKE TEST PASSED" || echo "SMOKE TEST FAILED (log: $LOG)"
exit $fail
