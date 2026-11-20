#!/usr/bin/env bash
# Turns the heavy rendering off for integrated graphics, and makes the water
# mesh cheaper. Safe to run more than once.
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if grep -q "BOAT RACE LOW SPEC" "$PROJECT_DIR/Config/DefaultEngine.ini"; then
	echo "Low-spec settings are already applied."
	exit 0
fi

cat >> "$PROJECT_DIR/Config/DefaultEngine.ini" <<'INI'

; ---- BOAT RACE LOW SPEC ----
[SystemSettings]
r.DynamicGlobalIlluminationMethod=0
r.ReflectionMethod=2
r.GenerateMeshDistanceFields=0
r.Shadow.Virtual.Enable=0
r.AntiAliasingMethod=1
r.DefaultFeature.MotionBlur=0
r.VolumetricCloud=0
r.VolumetricFog=0
r.ScreenPercentage=70
r.ViewDistanceScale=0.7
INI

python3 - "$PROJECT_DIR/Config/DefaultGame.ini" <<'PY'
import sys
path = sys.argv[1]
text = open(path).read()
text = text.replace("GridCells=60", "GridCells=40")
text = text.replace("UpdateInterval=0.0", "UpdateInterval=0.033")
open(path, "w").write(text)
PY

echo "Low-spec settings applied:"
echo "  - Lumen, virtual shadows, volumetric cloud and fog off"
echo "  - 70% screen percentage"
echo "  - water mesh 40x40 cells, rebuilt 30 times a second"
echo
echo "To undo: delete the BOAT RACE LOW SPEC block at the end of"
echo "Config/DefaultEngine.ini and set GridCells back to 60 in Config/DefaultGame.ini."
