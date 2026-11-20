#!/usr/bin/env bash
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
DIST="$PROJECT_DIR/dist"
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
    -project="$PROJECT_FILE" -noP4 -utf8output -platform=Linux -clientconfig=Development \
    -build -cook -map=/Engine/Maps/Entry -stage -pak -archive -archivedirectory="$DIST" "$@"
LAUNCHER="$(find "$DIST" -maxdepth 2 -name 'JetFighter.sh' -print -quit)"
[ -n "$LAUNCHER" ] || { echo "JetFighter.sh missing under $DIST" >&2; exit 1; }
GAME_DIR="$(dirname "$LAUNCHER")"
tar -C "$(dirname "$GAME_DIR")" -czf "$DIST/JetFighter-Linux.tar.gz" "$(basename "$GAME_DIR")"
echo "Packaged: $DIST/JetFighter-Linux.tar.gz"
