#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/select_game.sh" "${1:-all}"
if [ "$#" -gt 0 ]; then shift; fi
if [ "$GAME_DIR" = all ]; then
    bash "$MONOREPO_ROOT/games/BoatRace/Scripts/build.sh" "$@"
    bash "$MONOREPO_ROOT/games/JetFighter/Scripts/build.sh" "$@"
else
    exec bash "$MONOREPO_ROOT/games/$GAME_DIR/Scripts/build.sh" "$@"
fi
