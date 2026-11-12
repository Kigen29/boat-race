#!/usr/bin/env bash
# Shared setup for the other scripts. Not meant to be run directly.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/BoatRace.uproject"

# Where Unreal Engine is unpacked. Override with:  export UE_ROOT=/path/to/UnrealEngine
: "${UE_ROOT:=$HOME/UnrealEngine}"

EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
EDITOR_CMD="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd"
[ -x "$EDITOR_CMD" ] || EDITOR_CMD="$EDITOR"

if [ ! -x "$EDITOR" ]; then
	echo "Unreal Engine was not found at: $UE_ROOT" >&2
	echo "Unpack the Linux build of Unreal Engine 5 there, or point UE_ROOT at it:" >&2
	echo "    export UE_ROOT=/path/to/UnrealEngine" >&2
	exit 1
fi
