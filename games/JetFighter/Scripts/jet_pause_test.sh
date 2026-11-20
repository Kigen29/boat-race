#!/usr/bin/env bash
# Standalone pause freezes the aircraft, renders a menu, then resumes flight.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
LOG="$PROJECT_DIR/Saved/Logs/jet_pause_test.log"
SHOTS="$PROJECT_DIR/Saved/Screenshots/LinuxEditor"
mkdir -p "$(dirname "$LOG")"
rm -f "$SHOTS"/JetPause_*.png
status=0
timeout -k 10 180 "$EDITOR" "$PROJECT_FILE" /Engine/Maps/Entry -game \
    -windowed -ResX=1280 -ResY=720 -unattended -nosound -nowrite -JetPauseCheck \
    -abslog="$LOG" > "$LOG.console" 2>&1 || status=$?
python3 - "$LOG" "$SHOTS" "$status" <<'PY'
import re, sys
from pathlib import Path
log = Path(sys.argv[1]).read_text(errors='replace') if Path(sys.argv[1]).exists() else ''
ok = (int(sys.argv[3]) == 0 and all(f'Jet pause check: stage {n} PASS' in log for n in range(3))
      and "Jet pause check: active gun frozen=1" in log
      and all((Path(sys.argv[2])/f'JetPause_{name}.png').is_file() for name in ('Paused','Resumed'))
      and not re.search(r'Fatal error|Assertion failed|Ensure condition failed|Jet pause check: stage \d FAIL', log))
print('JET PAUSE TEST PASSED' if ok else 'JET PAUSE TEST FAILED: '+sys.argv[1])
sys.exit(0 if ok else 1)
PY
