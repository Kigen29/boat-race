#!/usr/bin/env bash
# Screenshots of every submenu, Back navigation and live course changes.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
SHOTS="$PROJECT_DIR/Saved/Screenshots/LinuxEditor"
LOG="$PROJECT_DIR/Saved/Logs/menu_test.log"
mkdir -p "$(dirname "$SHOTS")" "$(dirname "$LOG")"
if [ -d "$SHOTS" ]; then
	BACKUP=$(mktemp -d "$PROJECT_DIR/Saved/Screenshots/previous-menu-test.XXXXXX")
	mv "$SHOTS" "$BACKUP/LinuxEditor"
fi
status=0
timeout 180 "$EDITOR" "$PROJECT_FILE" -game -windowed -ResX=1280 -ResY=720 \
	-MenuTour -Autopilot -nowrite -abslog="$LOG" > "$LOG.console" 2>&1 || status=$?
python3 - "$LOG" "$SHOTS" "$status" <<'PY'
from pathlib import Path
import re
import sys

log = Path(sys.argv[1]).read_text(errors="replace")
shots = Path(sys.argv[2])
missing = [f"MenuTour_{n:02d}.png" for n in range(12) if not (shots / f"MenuTour_{n:02d}.png").is_file()]
expected = ("0 Bay Circuit forward", "1 Harbour Sprint forward", "1 Harbour Sprint reverse", "2 Canyon Run reverse")
missing_layouts = [name for name in expected if "Circuit ready: " + name not in log]
ok = (sys.argv[3] == "0" and not missing and not missing_layouts
      and not re.search(r"Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception", log))
print("MENU TEST PASSED: 12 screenshots, submenu Back paths and live course/direction changes" if ok
      else f"MENU TEST FAILED: missing screenshots {missing}; missing layouts {missing_layouts}; exit {sys.argv[3]}; log {sys.argv[1]}")
sys.exit(0 if ok else 1)
PY
