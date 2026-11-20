# Racing monorepo

BoatRace and JetFighter are separate Unreal projects in games/BoatRace and
games/JetFighter. Never add dependencies from either game to the other.
Their only shared runtime code is packages/MaritimeCore, an Unreal plugin.
Keep shared code free of boat/jet game-mode, terrain and controller includes;
use callbacks for game-specific coast sampling and wave clocks.

Root Scripts/build.sh, play.sh, test.sh and package.sh select boat, jet or
all (play takes one game). Each game owns its configuration and Scripts.
Asset source models live in games/BoatRace/Art; imported assets belong to the
MaritimeCore plugin under /MaritimeCore, not either project's /Game mount.
Export cross-module shared functions/classes with MARITIMECORE_API.

Read games/BoatRace/CLAUDE.md for boat design conventions. Use the root README
and JetFighter's README for the current project layout and launch commands.
The old jet research in JetFighter/docs is historical and contains source
paths from before the migration.

After shared code changes, build both editor targets and run Scripts/test.sh
all. After changing a game's appearance, run that game's rendered tour.
Jet tours: Scripts/jet_flight_tour.sh and Scripts/jet_check.sh.
Boat tours: Scripts/smoke_test.sh. Wave physics must use OceanWaves everywhere.

Run python3 tools/check_monorepo.py after structural changes. Do not commit
Binaries, Intermediate, Saved, DerivedDataCache, dist, imported .uasset files,
downloaded media or editor-generated Android server settings/tokens.
