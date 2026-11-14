#!/usr/bin/env bash
# Runs the game straight from the project (no packaging needed).
#
#   Scripts/play.sh                 main menu (H host, J join, ENTER solo)
#   Scripts/play.sh host            start hosting immediately
#   Scripts/play.sh join 10.0.0.5   join the host at that address
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

MODE="${1:-menu}"
ARGS=("$PROJECT_FILE")
EXTRA=()
case "$MODE" in
	host)
		ARGS+=("/Engine/Maps/Entry?listen")
		EXTRA=("${@:2}")
		;;
	join)
		if [ -z "${2:-}" ]; then
			echo "Usage: Scripts/play.sh join <host address>" >&2
			exit 1
		fi
		ARGS+=("$2")
		EXTRA=("${@:3}")
		;;
	menu|solo)
		EXTRA=("${@:2}")
		;;
	*)
		echo "Unknown mode '$MODE'. Use: host | join <address> | menu" >&2
		exit 1
		;;
esac

# Fullscreen at the screen's own resolution: a smaller window on a scaled
# (HiDPI) desktop gets stretched by the compositor and looks soft. The game
# renders fewer pixels itself and upscales them properly (TSR).
# BOATRACE_WINDOWED=1 Scripts/play.sh gives the old 1600 x 900 window.
export SDL_VIDEO_WAYLAND_SCALE_TO_DISPLAY=1
if [ "${BOATRACE_WINDOWED:-0}" = 1 ]; then
	DISPLAY_ARGS=(-windowed -ResX=1600 -ResY=900)
else
	DISPLAY_ARGS=(-fullscreen)
fi
exec "$EDITOR" "${ARGS[@]}" -game -log "${DISPLAY_ARGS[@]}" ${EXTRA[@]+"${EXTRA[@]}"}
