#!/usr/bin/env bash
# Matching in-game sunny screenshots of the three environments, with no save writes.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
SHOTS="$PROJECT_DIR/Saved/Screenshots/LinuxEditor"
OUT="$PROJECT_DIR/Saved/Screenshots/EnvironmentComparison"
mkdir -p "$OUT" "$PROJECT_DIR/Saved/Logs"
NAMES=(Bay_Circuit Harbour_Sprint Canyon_Run)
CIRCUITS=("$@")
if [ "$#" -eq 0 ]; then CIRCUITS=(0 1 2); fi
for circuit in "${CIRCUITS[@]}"; do
	if ! [[ "$circuit" =~ ^[0-2]$ ]]; then
		echo "Usage: bash Scripts/environment_test.sh [0 1 2]" >&2
		exit 1
	fi
	if [ -d "$SHOTS" ]; then
		BACKUP=$(mktemp -d "$PROJECT_DIR/Saved/Screenshots/previous-environment.XXXXXX")
		mv "$SHOTS" "$BACKUP/LinuxEditor"
	fi
	LOG="$PROJECT_DIR/Saved/Logs/environment_${circuit}.log"
	echo "Capturing ${NAMES[$circuit]}..."
	timeout 90 "$EDITOR" "$PROJECT_FILE" -game -windowed -ResX=1280 -ResY=720 \
		-PhotoTour -Circuit="$circuit" -ExitAfter=27 -nowrite -abslog="$LOG" > "$LOG.console" 2>&1
	python3 - "$LOG" "$SHOTS/Tour_01_Sunny.png" "$OUT/${NAMES[$circuit]}.png" "$circuit" <<'PY'
from pathlib import Path
import re
import shutil
import sys
log = Path(sys.argv[1]).read_text(errors="replace")
shot = Path(sys.argv[2])
fps = re.search(r"Photo tour: Tour_01_Sunny, (\d+) fps", log)
assert "Circuit ready: " + sys.argv[4] + " " in log, "Wrong circuit"
assert not re.search(r"Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception", log), "Crash or assertion"
assert shot.is_file() and fps and int(fps[1]) >= 25, "Missing sunny screenshot or low frame rate"
shutil.copy2(shot, sys.argv[3])
print("Verified", Path(sys.argv[3]).name, fps[1] + " fps")
PY
done
echo "ENVIRONMENT TEST PASSED: $OUT"
