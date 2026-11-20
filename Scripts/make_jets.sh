#!/usr/bin/env bash
exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/games/JetFighter/Scripts/make_jets.sh" "$@"
