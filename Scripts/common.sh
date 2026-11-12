#!/usr/bin/env bash
# Shared setup for the other scripts. Not meant to be run directly.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/BoatRace.uproject"

# Where Unreal Engine is unpacked. Override with:  export UE_ROOT=/path/to/UnrealEngine
# setup.sh records UE_ROOT in ~/.bashrc, but scripts started from a file
# manager, a desktop launcher or a non-interactive shell never read that file,
# so without this search they fall back to a folder that usually is not there.
if [ -z "${UE_ROOT:-}" ] || [ ! -x "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" ]; then
	if [ -x "$HOME/UnrealEngine/Engine/Binaries/Linux/UnrealEditor" ]; then
		UE_ROOT="$HOME/UnrealEngine"
	else
		FOUND="$(find "$HOME" -maxdepth 6 -type f -name UnrealEditor -path '*/Engine/Binaries/Linux/*' 2>/dev/null | head -n1 || true)"
		UE_ROOT="${FOUND:+${FOUND%/Engine/Binaries/Linux/UnrealEditor}}"
		: "${UE_ROOT:=$HOME/UnrealEngine}"
	fi
fi
export UE_ROOT

EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
EDITOR_CMD="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd"
[ -x "$EDITOR_CMD" ] || EDITOR_CMD="$EDITOR"

if [ ! -x "$EDITOR" ]; then
	echo "Unreal Engine was not found at: $UE_ROOT" >&2
	echo "Unpack the Linux build of Unreal Engine 5 there, or point UE_ROOT at it:" >&2
	echo "    export UE_ROOT=/path/to/UnrealEngine" >&2
	exit 1
fi
