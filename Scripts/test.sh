#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/select_game.sh" "${1:-all}"
if [ "$GAME_DIR" = BoatRace ] || [ "$GAME_DIR" = all ]; then
    python3 "$MONOREPO_ROOT/games/BoatRace/Scripts/physics_check.py"
    python3 "$MONOREPO_ROOT/games/BoatRace/Scripts/course_layout.py"
    bash "$MONOREPO_ROOT/games/BoatRace/Scripts/combat_test.sh"
    bash "$MONOREPO_ROOT/games/BoatRace/Scripts/frontend_test.sh"
fi
if [ "$GAME_DIR" = JetFighter ] || [ "$GAME_DIR" = all ]; then
    python3 "$MONOREPO_ROOT/games/JetFighter/Scripts/flight_check.py"
    bash "$MONOREPO_ROOT/games/JetFighter/Scripts/jet_flight_test.sh"
fi
