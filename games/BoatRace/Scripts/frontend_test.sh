#!/usr/bin/env bash
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
LOG="$PROJECT_DIR/Saved/Logs/frontend_test.log"
mkdir -p "$(dirname "$LOG")"
status=0
timeout 180 "$EDITOR_CMD" "$PROJECT_FILE" -unattended -nullrhi -nosound -nosplash -nowrite \
    -ExecCmds="Automation RunTests BoatRace.Frontend" -TestExit="Automation Test Queue Empty" \
    -abslog="$LOG" > "$LOG.console" 2>&1 || status=$?
python3 - "$LOG" "$status" <<'PY'
import re
import sys
from pathlib import Path
log = Path(sys.argv[1]).read_text(errors="replace")
passed = set(re.findall(r"Test Completed\. Result=\{Success\} Name=\{([^}]+)\} Path=\{BoatRace\.Frontend\.", log))
ok = (int(sys.argv[2]) in (0,1) and {"BackNavigation", "Circuits"} <= passed
      and "Automation Test Queue Empty 2 tests performed" in log
      and not re.search(r"Fatal error|Assertion failed|Ensure condition failed|Result=\{Fail", log))
print("FRONTEND TEST PASSED" if ok else "FRONTEND TEST FAILED: " + sys.argv[1])
if not ok:
    print("Engine exit status:", sys.argv[2], "Completed:", ", ".join(sorted(passed)))
sys.exit(0 if ok else 1)
PY
