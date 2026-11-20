#!/usr/bin/env bash
set -euo pipefail
MONOREPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case "${1:-boat}" in
    jet|jets|jetfighter|JetFighter) shift; exec bash "$MONOREPO_ROOT/games/JetFighter/Scripts/play.sh" "$@" ;;
    boat|boats|boatrace|BoatRace) if [ "$#" -gt 0 ]; then shift; fi ;;
esac
exec bash "$MONOREPO_ROOT/games/BoatRace/Scripts/play.sh" "$@"
