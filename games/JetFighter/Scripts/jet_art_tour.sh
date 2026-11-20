#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
bash "$PROJECT_DIR/Scripts/jet_flight_tour.sh" -JetArtTour "$@"
python3 - "$PROJECT_DIR/Saved/Logs/jet_flight_tour.log" <<'PY'
from pathlib import Path
import sys
text=Path(sys.argv[1]).read_text(errors='replace')
for name in ('AF-01 KITE','AF-02 ROOK','AF-03 MANTA'):
    if f'Jet airframe: {name}, model=imported' not in text:
        raise SystemExit('ART TOUR FAILED: missing imported '+name)
print('JET ART TOUR PASSED: all three original aircraft rendered')
PY
