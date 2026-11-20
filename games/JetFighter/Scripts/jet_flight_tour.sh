#!/usr/bin/env bash
# Render the actual jet, its aim controller, impact and automatic respawn.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
LOG="$PROJECT_DIR/Saved/Logs/jet_flight_tour.log"
SHOTS="$PROJECT_DIR/Saved/Screenshots/LinuxEditor"
mkdir -p "$(dirname "$LOG")"
rm -f "$SHOTS"/JetFlight_*.png
status=0
timeout -k 10 240 "$EDITOR" "$PROJECT_FILE" "/Engine/Maps/Entry" -game \
    -windowed -ResX=1280 -ResY=720 -unattended -nosound -nowrite -JetFlightTour \
    -abslog="$LOG" "$@" > "$LOG.console" 2>&1 || status=$?
python3 - "$LOG" "$SHOTS" "$status" <<'PY'
import re
import sys
from pathlib import Path

log = Path(sys.argv[1]).read_text(errors="replace") if Path(sys.argv[1]).exists() else ""
shots = Path(sys.argv[2])
expected = {"Level", "Bank", "Climb", "Impact", "Respawn"}
missing = {name for name in expected if not (shots / f"JetFlight_{name}.png").exists()}
ok = (int(sys.argv[3]) == 0 and not missing and "Jet flight tour finished." in log
      and "Jet flight tour: Impact, crashed=1" in log and "Jet flight tour: Respawn, crashed=0" in log
      and not re.search(r"Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception", log))
for line in log.splitlines():
    if "Jet flight tour:" in line:
        print(line)
print("JET FLIGHT TOUR PASSED" if ok else "JET FLIGHT TOUR FAILED: " + sys.argv[1])
if missing:
    print("Missing screenshots:", ", ".join(sorted(missing)))
sys.exit(0 if ok else 1)
PY
