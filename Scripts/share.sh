#!/usr/bin/env bash
# Hands the game to friends on the same Wi-Fi. Run it on the machine that has
# dist/BoatRace-Linux.tar.gz (make that with Scripts/package.sh), then each
# friend pastes the one line it prints into a terminal. Ctrl+C stops sharing.
#
# Why a tiny web server rather than a USB stick or a chat upload: the package
# is ~420 MB, every Ubuntu machine already has python3 and wget, and the line
# a friend pastes also installs the game and gives it an icon, so there is
# nothing to unpack or find afterwards.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="$PROJECT_DIR/dist"
PACKAGE="$DIST/BoatRace-Linux.tar.gz"
PORT="${1:-8000}"

if [ ! -f "$PACKAGE" ]; then
	echo "No package yet. Build it first:  Scripts/package.sh" >&2
	exit 1
fi

# The address friends can reach: the one this machine uses for its default
# route, which skips Docker bridges and VPN tunnels that hostname -I lists first.
ADDRESS="$(ip -4 route get 1.1.1.1 2>/dev/null | awk '{for (i = 1; i < NF; i++) if ($i == "src") { print $(i + 1); exit }}')"
[ -n "$ADDRESS" ] || ADDRESS="$(hostname -I | awk '{print $1}')"
if [ -z "$ADDRESS" ]; then
	echo "This machine does not seem to be on a network." >&2
	exit 1
fi

SERVE="$(mktemp -d)"
trap 'rm -rf "$SERVE"' EXIT
ln -s "$PACKAGE" "$SERVE/BoatRace-Linux.tar.gz"

# What a friend's machine runs. Kept to tools a stock Ubuntu has.
cat > "$SERVE/install.sh" <<INSTALL
#!/usr/bin/env bash
set -euo pipefail
FROM="http://$ADDRESS:$PORT"
GAME="\$HOME/BoatRace"
echo "Downloading Boat Race (about $(( $(stat -c %s "$PACKAGE") / 1048576 )) MB)..."
TMP="\$(mktemp)"
if command -v wget >/dev/null; then wget -q --show-progress -O "\$TMP" "\$FROM/BoatRace-Linux.tar.gz"
else curl -# -o "\$TMP" "\$FROM/BoatRace-Linux.tar.gz"; fi
rm -rf "\$GAME"
mkdir -p "\$GAME"
tar -xzf "\$TMP" -C "\$GAME" --strip-components=1
rm -f "\$TMP"
mkdir -p "\$HOME/.local/share/applications"
cat > "\$HOME/.local/share/applications/boatrace.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Boat Race
Comment=Four-player powerboat racing
Exec=\$GAME/BoatRace.sh -windowed -ResX=1600 -ResY=900
Path=\$GAME
Terminal=false
Categories=Game;
DESKTOP
echo
echo "Installed. Open 'Boat Race' from your apps (or run \$GAME/BoatRace.sh)."
echo "On the menu, press the number next to your friend's race to join."
INSTALL

echo
echo "Sharing Boat Race on your Wi-Fi. Each friend opens a terminal and pastes:"
echo
echo "    wget -qO- http://$ADDRESS:$PORT/install.sh | bash"
echo
echo "Leave this running until everyone has it, then press Ctrl+C."
echo
cd "$SERVE"
exec python3 -m http.server "$PORT" --bind "$ADDRESS"
