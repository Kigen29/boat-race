#!/usr/bin/env bash
# Runs the real aircraft in isolated Unreal worlds, without graphics.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
LOG="$PROJECT_DIR/Saved/Logs/jet_flight_test.log"
mkdir -p "$(dirname "$LOG")"
status=0
timeout -k 10 240 "$EDITOR_CMD" "$PROJECT_FILE" -unattended -nosplash -nullrhi -nosound -nowrite \
    -ExecCmds="Automation RunTests BoatRace.Jets" \
    -TestExit="Automation Test Queue Empty" -abslog="$LOG" > "$LOG.console" 2>&1 || status=$?
python3 - "$LOG" "$status" <<'PY'
import re
import sys
from pathlib import Path

log = Path(sys.argv[1]).read_text(errors="replace") if Path(sys.argv[1]).exists() else ""
expected = {"FlightEnvelope", "ControlAxesAndMouseAim", "FrameRateIndependence", "CollisionAndRespawn", "AirspaceRecovery"}
passed = set(re.findall(r"Test Completed\. Result=\{Success\} Name=\{([^}]+)\} Path=\{BoatRace\.Jets\.", log))
ok = (int(sys.argv[2]) in (0, 1) and expected == passed
      and "Automation Test Queue Empty 5 tests performed" in log
      and not re.search(r"Fatal error|Assertion failed|Ensure condition failed|Result=\{Fail", log))
if not ok:
    print("JET FLIGHT TEST FAILED; missing:", ", ".join(sorted(expected-passed)))
    print("Engine exit status:", sys.argv[2], "Log:", sys.argv[1])
    for line in log.splitlines():
        if "Error:" in line or "Aim " in line:
            print(line)
    sys.exit(1)
print("JET FLIGHT TEST PASSED:", ", ".join(sorted(passed)))
PY
