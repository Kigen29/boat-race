# MaritimeCore

Runtime Unreal plugin shared by BoatRace and JetFighter. It contains the
analytic OceanWaves surface, RaceWeather data, BoatArt material/model helpers
(the legacy name is retained), and AWaterSurface's camera-following sea mesh.
Public APIs use MARITIMECORE_API; neither game is a dependency of this plugin.

AWaterSurface accepts ShoreClearanceMetres and WaveClock callbacks. BoatRace
sets them before FinishSpawning; JetFighter uses the open-sea path. With no
clock callback it uses the game state's server clock, or local world time.
All collision and buoyancy surface tests still call OceanWaves directly.

Generated assets mount at /MaritimeCore/Models and /MaritimeCore/Higgsfield.
Use games/BoatRace/Scripts/import_assets.sh to rebuild them from the committed
boat art sources and optional downloaded media. Do not commit generated
Content, Binaries or Intermediate files. Content README files are retained.

Validate shared changes with both game builds, their tests and rendered tours.
