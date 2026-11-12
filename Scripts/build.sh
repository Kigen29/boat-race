#!/usr/bin/env bash
# Compiles the game's C++ module for the editor. Run this first, and again after any code change.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

"$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" BoatRaceEditor Linux Development \
	-project="$PROJECT_FILE" -waitmutex "$@"

echo
echo "Build finished. Next:  Scripts/fetch_assets.sh && Scripts/import_assets.sh   then   Scripts/play.sh"
