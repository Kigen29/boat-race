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

exec "$EDITOR" "${ARGS[@]}" -game -log -windowed -ResX=1600 -ResY=900 ${EXTRA[@]+"${EXTRA[@]}"}
