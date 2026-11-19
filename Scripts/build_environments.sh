#!/usr/bin/env bash
# Generate only the nine themed scenery models, then import with editor subsystems.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
BLENDER="${BLENDER:-$(command -v blender || true)}"
if [ -z "$BLENDER" ]; then
	echo "Set BLENDER to a Blender executable." >&2
	exit 1
fi
cd "$PROJECT_DIR"
BOATRACE_ENV_ONLY=1 "$BLENDER" -b -noaudio --factory-startup --python-exit-code 1 \
	-P Scripts/make_models.py -- Art/Models
BOATRACE_ENV_ONLY=1 "$EDITOR" "$PROJECT_FILE" \
	-ExecutePythonScript="$PROJECT_DIR/Scripts/import_assets.py" -unattended -nosplash -nullrhi -nowrite
