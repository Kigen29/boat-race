#!/usr/bin/env bash
# Exercise the same gun and flight simulation as normal input; no fake hits.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
LOG="$PROJECT_DIR/Saved/Logs/jet_combat_tour.log"
SHOTS="$PROJECT_DIR/Saved/Screenshots/LinuxEditor"
mkdir -p "$(dirname "$LOG")"
rm -f "$SHOTS"/JetCombat_*.png
status=0
timeout -k 10 180 "$EDITOR" "$PROJECT_FILE" /Engine/Maps/Entry -game \
    -windowed -ResX=1280 -ResY=720 -unattended -nosound -nowrite -JetCombatTour \
    -abslog="$LOG" "$@" > "$LOG.console" 2>&1 || status=$?
python3 - "$LOG" "$SHOTS" "$status" <<'PY'
from pathlib import Path
import re,sys
log=Path(sys.argv[1]).read_text(errors='replace') if Path(sys.argv[1]).exists() else ''
expected=('Ready','Firing','Destroyed','Overheat','Restart')
missing=[n for n in expected if not (Path(sys.argv[2])/f'JetCombat_{n}.png').exists()]
records={m[0]:tuple(map(int,m[1:4]))+(float(m[4]),int(m[5])) for m in re.findall(
 r'Jet combat tour: (\w+), shots=(\d+), hits=(\d+), dead=(\d), heat=([\d.]+), overheat=(\d)',log)}
ok=int(sys.argv[3])==0 and not missing and set(records)==set(expected) and 'Jet combat tour finished.' in log
if ok:
 ok=(records['Ready'][:3]==(0,0,0) and records['Firing'][0]>0 and records['Firing'][1]>0
     and records['Destroyed'][1:3]==(10,1) and records['Overheat'][4]==1
     and records['Restart'][:3]==(0,0,0) and records['Restart'][3]==0)
ok=ok and not re.search(r'Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception',log)
for line in log.splitlines():
 if 'Jet combat tour:' in line or 'Jet training:' in line: print(line)
print('JET COMBAT TOUR PASSED' if ok else f'JET COMBAT TOUR FAILED: {sys.argv[1]} / missing {missing}')
sys.exit(0 if ok else 1)
PY
