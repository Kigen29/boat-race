# Racing monorepo

BoatRace and JetFighter are separate Unreal projects in games/BoatRace and
games/JetFighter. Never add dependencies from either game to the other.
Their only shared runtime code is packages/MaritimeCore, an Unreal plugin.
Keep shared code free of boat/jet game-mode, terrain and controller includes;
use callbacks for game-specific coast sampling and wave clocks.

Root Scripts/build.sh, play.sh, test.sh and package.sh select boat, jet or
all (play takes one game). Each game owns its configuration and Scripts.
Shared boat source models live in games/BoatRace/Art; their imports belong to
MaritimeCore under /MaritimeCore. Original jet models live in games/JetFighter/Art
and import into JetFighter's own /Game/Aircraft mount. Regenerate with
Scripts/import_jet_art.sh; source GLBs and licensed font files are committed.
Export cross-module shared functions/classes with MARITIMECORE_API.

Read games/BoatRace/CLAUDE.md for boat design conventions. Use the root README
and JetFighter's README for the current project layout and launch commands.
The active jet plan is games/JetFighter/docs/vision/00_README.md. Older reports
are historical and may contain source paths from before the migration.

After shared code changes, build both editor targets and run Scripts/test.sh
all. After changing a game's appearance, run that game's rendered tour.
Jet tours: Scripts/jet_art_tour.sh and Scripts/jet_check.sh.
Boat tours: Scripts/smoke_test.sh. Wave physics must use OceanWaves everywhere.

Run python3 tools/check_monorepo.py after structural changes. Do not commit
Binaries, Intermediate, Saved, DerivedDataCache, dist, imported .uasset files,
downloaded media or editor-generated Android server settings/tokens.
