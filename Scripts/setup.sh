#!/usr/bin/env bash
# One command to get from a fresh copy of this project to a playable game.
#
#   cd <project folder>
#   bash Scripts/setup.sh
#
# Finds Unreal Engine, links the folder to the GitHub repo, picks graphics
# settings to suit the machine, compiles, downloads and imports the Higgsfield
# assets, and tells you how to play. Safe to run again at any point: every step
# checks whether it is already done.
#
#   --low      force the low graphics preset
#   --high     force the full preset (Lumen, virtual shadows, volumetric cloud)
#   --no-git   skip linking the folder to the repository
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR"

REPO_URL="https://github.com/Kigen29/boat-race.git"
FORCE_PRESET=""
DO_GIT=1
for arg in "$@"; do
	case "$arg" in
		--low)    FORCE_PRESET=low ;;
		--high)   FORCE_PRESET=high ;;
		--no-git) DO_GIT=0 ;;
		*) echo "Unknown option: $arg"; exit 2 ;;
	esac
done

step()  { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
need()  { [ -f "$1" ] || die "$1 is missing from this folder.
That means this copy is older than the repository. Update it with:
    git restore .            (if the folder is linked to the repo)
or re-download the project. Compiling now would build out-of-date code."; }
ok()    { printf '    \033[32m%s\033[0m\n' "$*"; }
warn()  { printf '    \033[33m%s\033[0m\n' "$*"; }
die()   { printf '\n\033[31m%s\033[0m\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
step "Looking for Unreal Engine"
# ---------------------------------------------------------------------------
if [ -n "${UE_ROOT:-}" ] && [ -x "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" ]; then
	ok "Using UE_ROOT=$UE_ROOT"
else
	FOUND="$(find "$HOME" -maxdepth 6 -type f -name UnrealEditor -path '*/Engine/Binaries/Linux/*' 2>/dev/null | head -n1)"
	[ -n "$FOUND" ] || die "Unreal Engine was not found under $HOME.
Download the Linux build from https://www.unrealengine.com/linux, unzip it,
then run:  export UE_ROOT=/path/to/that/folder  and try again."
	export UE_ROOT="${FOUND%/Engine/Binaries/Linux/UnrealEditor}"
	ok "Found it at $UE_ROOT"
	if ! grep -q "UE_ROOT=" "$HOME/.bashrc" 2>/dev/null; then
		echo "export UE_ROOT=\"$UE_ROOT\"" >> "$HOME/.bashrc"
		ok "Remembered it in ~/.bashrc for new terminals"
	fi
fi

# ---------------------------------------------------------------------------
step "Checking the graphics driver"
# ---------------------------------------------------------------------------
PRESET="$FORCE_PRESET"
if command -v vulkaninfo >/dev/null 2>&1; then
	GPU="$(vulkaninfo --summary 2>/dev/null | grep -m1 'deviceName' | cut -d= -f2- | sed 's/^ *//')"
	if [ -n "$GPU" ]; then
		ok "Vulkan device: $GPU"
	else
		warn "vulkaninfo found no device. Unreal cannot render without one."
	fi
	if [ -z "$PRESET" ]; then
		# Integrated graphics cannot carry Lumen and virtual shadow maps.
		if vulkaninfo --summary 2>/dev/null | grep -q 'INTEGRATED_GPU'; then
			PRESET=low
		else
			PRESET=high
		fi
	fi
else
	warn "vulkaninfo is not installed (sudo apt install vulkan-tools)."
	warn "Cannot tell what the graphics are; assuming the low preset."
	[ -n "$PRESET" ] || PRESET=low
fi

if [ "$PRESET" = low ]; then
	if grep -q "BOAT RACE LOW SPEC" Config/DefaultEngine.ini 2>/dev/null; then
		ok "Low graphics preset already applied"
	else
		need Scripts/low_spec.sh
		bash Scripts/low_spec.sh >/dev/null && ok "Applied the low graphics preset"
	fi
else
	ok "Keeping the full graphics preset"
fi

# ---------------------------------------------------------------------------
if [ "$DO_GIT" = 1 ]; then
step "Linking this folder to the repository"
	if [ -d .git ]; then
		ok "Already a git repository"
	elif ! command -v git >/dev/null 2>&1; then
		warn "git is not installed; skipping"
	else
		git init -q && git branch -M main
		git remote add origin "$REPO_URL" 2>/dev/null || git remote set-url origin "$REPO_URL"
		if git fetch -q origin main 2>/dev/null; then
			# Adopts the published history without touching any of your files.
			git reset --mixed -q origin/main
			git branch --set-upstream-to=origin/main main >/dev/null 2>&1
			ok "Linked to $REPO_URL"
			CHANGED="$(git status --porcelain | wc -l)"
			if [ "$CHANGED" = 0 ]; then
				ok "Your files match the repository"
			else
				printf '\n\033[33m    %s file(s) here differ from the repository.\033[0m\n' "$CHANGED"
				git status --short | head -12
				[ "$CHANGED" -gt 12 ] && echo "    ... and $((CHANGED - 12)) more"
				cat <<'STALE'

    Building now would compile whichever of these is older. To take the
    repository's version of every tracked file (your own edits to them are
    discarded; untracked files and build output are left alone):

        git restore .  &&  bash Scripts/setup.sh

STALE
				die "Stopping so that stale code is not built."
			fi
		else
			warn "Could not reach GitHub; left the folder unlinked"
		fi
	fi
fi

# ---------------------------------------------------------------------------
step "Compiling the game"
# ---------------------------------------------------------------------------
need Scripts/build.sh
BUILD_LOG="$(mktemp)"
if bash Scripts/build.sh > "$BUILD_LOG" 2>&1; then
	ok "Compiled"
else
	echo
	printf '\033[31mThe build failed. The errors are:\033[0m\n\n'
	grep -E 'error:|Error:|error C' "$BUILD_LOG" | head -40
	echo
	echo "Full log: $BUILD_LOG"
	die "Send the lines above to Claude and it can fix them."
fi
rm -f "$BUILD_LOG"

# ---------------------------------------------------------------------------
step "Higgsfield assets"
# ---------------------------------------------------------------------------
if [ -s HiggsfieldAssets/SM_Boat.glb ]; then
	ok "Already downloaded"
else
	bash Scripts/fetch_assets.sh >/dev/null 2>&1 \
		&& ok "Downloaded the boat model, menu art and water map" \
		|| warn "Download failed. The game still runs with its placeholder boat."
fi

if ls Content/Higgsfield/*.uasset >/dev/null 2>&1; then
	ok "Already imported"
elif [ -s HiggsfieldAssets/SM_Boat.glb ]; then
	echo "    Importing into Unreal, this opens the editor and takes a few minutes..."
	if bash Scripts/import_assets.sh >/dev/null 2>&1 && ls Content/Higgsfield/*.uasset >/dev/null 2>&1; then
		ok "Imported the boat, the water material and the menu art"
	else
		warn "Import did not finish. The game still runs with placeholders."
		warn "To retry by hand: Scripts/editor.sh, then Tools > Execute Python Script"
		warn "and choose Scripts/import_assets.py"
	fi
fi

# ---------------------------------------------------------------------------
printf '\n\033[1m==> Ready\033[0m\n'
cat <<'NEXT'

    Scripts/play.sh            play, with the menu
    Scripts/play.sh host       start hosting straight away
    Scripts/play.sh join ADDR  join someone else's race

    On the menu:  H host    J join    ENTER solo practice
    Driving:      W/S throttle   A/D steer   SPACE torpedo   Q mine   R reset

    To race with three friends, see ONBOARDING.md.
NEXT
