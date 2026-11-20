#!/usr/bin/env bash
exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/games/JetFighter/Scripts/import_jet_art.sh" "$@"
