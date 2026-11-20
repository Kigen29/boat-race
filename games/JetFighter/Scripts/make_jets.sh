#!/usr/bin/env bash
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLENDER="${BLENDER:-$(command -v blender || true)}"
# Snap's launcher cannot enter some container sandboxes; the installed binary
# itself can run without changing host confinement or system services.
if [ "$BLENDER" = /snap/bin/blender ] && [ -x /snap/blender/current/blender ]; then BLENDER=/snap/blender/current/blender; fi
[ -n "$BLENDER" ] || { echo 'Set BLENDER to an installed Blender executable.' >&2; exit 1; }
exec "$BLENDER" -b -noaudio --factory-startup --python-exit-code 1 -P "$PROJECT_DIR/Scripts/make_jets.py" -- "$PROJECT_DIR/Art/Models" "$PROJECT_DIR/Saved/JetArt"
