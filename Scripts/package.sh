#!/usr/bin/env bash
# Builds a stand-alone Linux copy of the game that runs WITHOUT Unreal Engine
# installed. Send the resulting dist/BoatRace-Linux.tar.gz to the other players.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

DIST="$PROJECT_DIR/dist"

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

One player hosts:      ./host.sh
The other three join:  ./join.sh <host address>
(or run ./BoatRace.sh and use the menu: H host, J join, ENTER solo)

The host presses ENTER to start. 3 laps, pass every gate in order
(red buoy with a cone on the inside, green buoy with a ball on the outside).

W / S   throttle / reverse        A / D   steer
R       back to the last gate     L       leave the race

Same network: use the host's local address (run "hostname -I" on the host).
Over the internet: the host forwards UDP port 7777 on their router to their
machine and gives the others their public address.
HOWTO

chmod +x "$GAME_DIR/host.sh" "$GAME_DIR/join.sh" "$LAUNCHER"
tar -C "$(dirname "$GAME_DIR")" -czf "$DIST/BoatRace-Linux.tar.gz" "$(basename "$GAME_DIR")"

echo
echo "Packaged game:  $GAME_DIR"
echo "Send to players: $DIST/BoatRace-Linux.tar.gz"
