#!/usr/bin/env bash
# Actual weapon actors in isolated engine worlds; no graphics or saved progress.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
LOG="$PROJECT_DIR/Saved/Logs/combat_test.log"
mkdir -p "$(dirname "$LOG")"
status=0
timeout 240 "$EDITOR_CMD" "$PROJECT_FILE" -unattended -nosplash -nullrhi -nosound -nowrite \
	-ExecCmds="Automation RunTests BoatRace.Combat" \
	-TestExit="Automation Test Queue Empty" -abslog="$LOG" > "$LOG.console" 2>&1 || status=$?
python3 - "$LOG" "$status" <<'PY'
import re
import sys
from pathlib import Path

log = Path(sys.argv[1]).read_text(errors="replace")
expected = {"SolidCover", "MissileCrossesTarget", "FlaresAndLostTarget", "DamageAndProtection", "HomingAndUnmatchedLaunch"}
passed = set(re.findall(r"Test Completed\. Result=\{Success\} Name=\{([^}]+)\} Path=\{BoatRace\.Combat\.", log))
missing = expected - passed
# This preview engine's -TestExit uses RequestExit(1); require the complete
# success log as well as an ordinary exit, so timeouts/crashes cannot pass.
if (int(sys.argv[2]) not in (0, 1) or "Automation Test Queue Empty 5 tests performed" not in log
        or missing or re.search(r"Fatal error|Assertion failed|Ensure condition failed|Result=\{Fail", log)):
    print("COMBAT TEST FAILED; missing successful tests:", ", ".join(sorted(missing)))
    print("Engine exit status:", sys.argv[2])
    print("Log:", sys.argv[1])
    sys.exit(1)
print("COMBAT TEST PASSED:", ", ".join(sorted(expected)))
PY
