#!/usr/bin/env bash
# Builds every 3D model in Blender (Scripts/make_models.py) into Art/Models/,
# with preview renders in Saved/ModelPreviews/. Then Scripts/import_assets.sh
# brings them into Unreal under /Game/Models.
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLENDER="${BLENDER:-$(command -v blender || true)}"
if [ -z "$BLENDER" ]; then
	echo "Blender was not found. Install it (sudo snap install blender --classic) or set BLENDER=/path/to/blender." >&2
	exit 1
fi
cd "$PROJECT_DIR"
"$BLENDER" -b --factory-startup -P Scripts/make_models.py -- Art/Models Saved/ModelPreviews "$@" \
	| grep -E '^model|^exported|^previews|Error|Traceback' || true
ls Art/Models/*.glb >/dev/null
