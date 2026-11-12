#!/usr/bin/env bash
# Imports HiggsfieldAssets/ into the project (/Game/Higgsfield) and builds the
# water material, using Unreal's own Python scripting. Run Scripts/build.sh first.
#
# If this headless run gives trouble, do the same thing from inside the editor:
#   Scripts/editor.sh  ->  Tools > Execute Python Script...  ->  Scripts/import_assets.py
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

if [ ! -s "$PROJECT_DIR/HiggsfieldAssets/SM_Boat.glb" ]; then
	echo "HiggsfieldAssets/SM_Boat.glb is missing. Run Scripts/fetch_assets.sh first." >&2
	exit 1
fi

"$EDITOR_CMD" "$PROJECT_FILE" -run=pythonscript -script="$PROJECT_DIR/Scripts/import_assets.py" -unattended -nosplash "$@"

echo
echo "Imported assets:"
ls -1 "$PROJECT_DIR/Content/Higgsfield" 2>/dev/null || true
echo
echo "Expected: SM_Boat.uasset, M_Water.uasset, T_Title.uasset, T_WaterNormal.uasset"
