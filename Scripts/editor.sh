#!/usr/bin/env bash
# Opens the project in the Unreal Editor.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
exec "$EDITOR" "$PROJECT_FILE" "$@"
