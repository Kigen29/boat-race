#!/usr/bin/env bash
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
DISPLAY_ARGS=(-fullscreen)
if [ "${JETFIGHTER_WINDOWED:-${BOATRACE_WINDOWED:-0}}" = 1 ]; then
    DISPLAY_ARGS=(-windowed -ResX=1600 -ResY=900)
fi
export SDL_VIDEO_WAYLAND_SCALE_TO_DISPLAY=1
exec "$EDITOR" "$PROJECT_FILE" /Engine/Maps/Entry -game -log "${DISPLAY_ARGS[@]}" "$@"
