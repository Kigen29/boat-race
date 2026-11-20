#!/usr/bin/env bash
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
LOG="$PROJECT_DIR/Saved/Logs/jet_art_import.log"
mkdir -p "$(dirname "$LOG")"
status=0
timeout -k 10 240 "$EDITOR" "$PROJECT_FILE" -ExecutePythonScript="$PROJECT_DIR/Scripts/import_jet_art.py" \
    -unattended -nosplash -nullrhi -nowrite -abslog="$LOG" > "$LOG.console" 2>&1 || status=$?
python3 - "$LOG" "$status" <<'PY'
from pathlib import Path
import re,sys
log=Path(sys.argv[1]).read_text(errors='replace') if Path(sys.argv[1]).exists() else ''
ok=(int(sys.argv[2])==0 and all('JET ART READY: '+n in log for n in ('JetKite','JetRook','JetManta'))
    and not re.search(r'Traceback|Fatal error|Assertion failed',log))
print('JET ART IMPORT PASSED' if ok else 'JET ART IMPORT FAILED: '+sys.argv[1])
if not ok:
 for line in log.splitlines():
  if 'Error:' in line or 'Exception' in line or 'Traceback' in line:print(line)
sys.exit(0 if ok else 1)
PY
