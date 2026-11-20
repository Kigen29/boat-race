#!/usr/bin/env bash
MONOREPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case "${1:-all}" in
    boat|boats|boatrace|BoatRace) GAME_DIR=BoatRace ;;
    jet|jets|jetfighter|JetFighter) GAME_DIR=JetFighter ;;
    all) GAME_DIR=all ;;
    *) echo "Choose boat, jet or all." >&2; exit 2 ;;
esac
