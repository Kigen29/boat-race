#!/usr/bin/env bash
exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/games/BoatRace/Scripts/net_combat_test.sh" "$@"
