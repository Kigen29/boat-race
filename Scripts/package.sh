#!/usr/bin/env bash
# Builds a stand-alone Linux copy of the game that runs WITHOUT Unreal Engine
# installed. Send the resulting dist/BoatRace-Linux.tar.gz to the other players.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

DIST="$PROJECT_DIR/dist"

# Packaging takes a long time, so check first that this folder is the current
# one. A second, older copy of the project is an easy mistake to make, and the
# only symptom is a compile error that was fixed somewhere else.
echo "Project: $PROJECT_DIR"
if [ ! -d "$PROJECT_DIR/.git" ]; then
	echo
	echo "This folder is not linked to the git repository, so it may be an older"
	echo "copy. If you have another folder with a .git directory in it, package"
	echo "from that one instead."
	echo
	printf 'Carry on anyway? [y/N] '
	read -r reply
	case "$reply" in [yY]*) ;; *) echo "Stopped."; exit 1 ;; esac
elif command -v git >/dev/null 2>&1 && git -C "$PROJECT_DIR" rev-parse --abbrev-ref '@{upstream}' >/dev/null 2>&1; then
	git -C "$PROJECT_DIR" fetch -q origin 2>/dev/null || true
	BEHIND="$(git -C "$PROJECT_DIR" rev-list --count 'HEAD..@{upstream}' 2>/dev/null || echo 0)"
	if [ "${BEHIND:-0}" != "0" ]; then
		echo
		echo "This folder is $BEHIND commit(s) behind the repository. Run 'git pull'"
		echo "first, or you will package an out-of-date build."
		exit 1
	fi
	echo "Up to date with the repository."
fi

"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
	-project="$PROJECT_FILE" -noP4 -utf8output \
	-platform=Linux -clientconfig=Development \
	-build -cook -map=/Engine/Maps/Entry -stage -pak \
	-archive -archivedirectory="$DIST" "$@"

LAUNCHER="$(find "$DIST" -maxdepth 2 -name 'BoatRace.sh' | head -n 1)"
if [ -z "$LAUNCHER" ]; then
	echo "Packaging finished but BoatRace.sh was not found under $DIST" >&2
	exit 1
fi
GAME_DIR="$(dirname "$LAUNCHER")"

cat > "$GAME_DIR/host.sh" <<'HOST'
#!/usr/bin/env bash
# Host a race: you play, and up to three others join you on UDP port 7777.
cd "$(dirname "${BASH_SOURCE[0]}")"
exec ./BoatRace.sh "/Engine/Maps/Entry?listen" -log -windowed -ResX=1600 -ResY=900 "$@"
HOST

cat > "$GAME_DIR/join.sh" <<'JOIN'
#!/usr/bin/env bash
# Join a race:  ./join.sh <host address>
cd "$(dirname "${BASH_SOURCE[0]}")"
if [ -z "${1:-}" ]; then
	echo "Usage: ./join.sh <host address>   e.g. ./join.sh 192.168.1.20" >&2
	exit 1
fi
exec ./BoatRace.sh "$1" -log -windowed -ResX=1600 -ResY=900 "${@:2}"
JOIN

cat > "$GAME_DIR/HOW_TO_PLAY.txt" <<'HOWTO'
BOAT RACE - up to 4 players

One player hosts: press H on the menu (or ./host.sh).
On the same Wi-Fi the others just open the game: the host's race is listed
on the menu, press 1 to join. (J joins by typing an address; ./join.sh too.)

The host presses ENTER to start. 3 laps, pass every gate in order
(red buoy with a cone on the inside, green buoy with a ball on the outside).

W / S   throttle / reverse        A / D   steer
R       back to the last gate     L       leave the race

If the race is not listed: same Wi-Fi? Host firewall: sudo ufw allow 7777:7778/udp
Over the internet: the host forwards UDP port 7777 on their router to their
machine and gives the others their public address.
HOWTO

chmod +x "$GAME_DIR/host.sh" "$GAME_DIR/join.sh" "$LAUNCHER"
tar -C "$(dirname "$GAME_DIR")" -czf "$DIST/BoatRace-Linux.tar.gz" "$(basename "$GAME_DIR")"

echo
echo "Packaged game:  $GAME_DIR"
echo "Send to players: $DIST/BoatRace-Linux.tar.gz"
