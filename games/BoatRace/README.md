# Boat Race

Four-player armed powerboat racing for Unreal Engine 5, written entirely in
C++, with art generated in Higgsfield.

Three sixteen-gate circuits, each playable forwards or reversed. The boats are rigid bodies
floating on a real wave field, with torpedoes and mines to make the racing
untidy. One player hosts, three others join by address.

## Status

- **The current C++ changes compile** on Unreal Engine 5.9.0 preview-1 under
  Ubuntu (verified 16 November 2026). Handling, difficulty and combat balance
  still need full races played by hand.
- The physics was checked by simulating it numerically rather than by feel
  (`Scripts/physics_check.py`), and the circuit by generating and testing its
  geometry (`Scripts/course_layout.py`). Both found real bugs, which is why
  they are in the project.
- The working version includes synthesised sound, wakes, scenery and combat
  upgrades. See [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md) for the current
  verification and improvement priorities.

After building, `bash Scripts/combat_test.sh` runs the engine combat checks:
solid cover, missile homing and hits between frames, flares, wrecks, shields
and respawn protection. It runs without graphics and writes its log to
`Saved/Logs/combat_test.log`.

`bash Scripts/net_combat_test.sh` runs a host and client locally to compare
gun damage and release, missile lock/ammo/damage, client flares and wreck
recovery. It holds the boats still for repeatable shots and does not write
player progress. Logs are in `Saved/Logs/net_combat_host.log` and
`Saved/Logs/net_combat_client.log`. It complements the moving-boat check in
`Scripts/net_test.sh`; Wi-Fi across two computers still needs a separate test.

`bash Scripts/frontend_test.sh` checks Back navigation from every submenu and
all six circuit/direction layouts. `bash Scripts/net_test.sh 2` checks hosting,
joining and scoring gates on Canyon Run. For visual checks, use
`bash Scripts/smoke_test.sh 25 -Circuit=1` (Harbour) or `-Circuit=2` (Canyon).
Previous screenshots are archived instead of deleted.
`bash Scripts/menu_test.sh` photographs the menus and verifies live circuit
and direction changes through the server RPC, including the return paths.

Escape returns from Garage, Leaderboard, Settings and Controls to the menu
that opened them; Escape from Pause resumes play. The on-screen Back action
and gamepad Back follow the same path.

## Quick start

One command does everything: finds Unreal Engine, picks graphics settings for
the machine, compiles, and downloads and imports the art.

```bash
bash Scripts/setup.sh
Scripts/play.sh
```

Or step by step, with Unreal Engine 5 unpacked and `UE_ROOT` pointing at it:

```bash
Scripts/build.sh          # compile
Scripts/fetch_assets.sh   # download the Higgsfield boat, art and water map
Scripts/import_assets.sh  # import them and build the materials
Scripts/play.sh           # play
```

On integrated graphics, run `Scripts/low_spec.sh` first.

To race with three friends on the same Wi-Fi: `Scripts/package.sh`, then
`Scripts/share.sh`, and each friend pastes the line it prints. Details in
[ONBOARDING.md](ONBOARDING.md).

## Monorepo layout

BoatRace is now this standalone project under `games/BoatRace`. JetFighter
is a separate project in `games/JetFighter`, launched with the repository
root's `Scripts/play.sh jet`. Its mode is no longer on the boat title screen.
Shared water, weather and art helpers live in `packages/MaritimeCore`.
Boat-only commands in this guide are relative to this project directory.
Generated shared assets belong to `../../packages/MaritimeCore/Content`.

## Documentation

| | |
|---|---|
| **[ONBOARDING.md](ONBOARDING.md)** | **Getting four people into a race**: sharing the game over Wi-Fi in one line, building the Windows copy so Windows and Linux friends race together, joining from the menu list, and what to do when a join fails |
| [PLAN.md](PLAN.md) | What was built and why, stage by stage, with the real-world powerboat racing it borrows from |
| [RACING_RESEARCH.md](RACING_RESEARCH.md) | Comparison with Hydro Thunder and Riptide GP, the additions made, and remaining gaps |
| [HIGGSFIELD_ASSETS.md](HIGGSFIELD_ASSETS.md) | The generated art: which models, the exact prompts, what each cost, and how to replace any of it |
| [CLAUDE.md](CLAUDE.md) | Working context for an AI coding agent: current state, design rules, verification scripts |

## Controls

| Key | Action |
|---|---|
| W / S | Throttle / reverse |
| A / D | Steer |
| SPACE or left mouse | Fire a torpedo |
| Q or right mouse | Drop a mine |
| CTRL or X / left bumper | Hold to fire the deck machine gun; release to cool it |
| R | Back to your last gate |
| SHIFT / RB | Nitrous boost (fills slowly, and fast while airborne off a ramp) |
| E | Fire a missile; hold the sights on an opponent first for a homing lock |
| T / D-pad right | Switch missile target |
| F | Drop flares: any missile locked on you chases them instead |
| G | Your copilot lobs a grenade over the side |
| C / B | Cycle camera view / look behind |
| Pause menu > Photo mode | Free camera with no HUD: WASD fly, mouse look, Q/E down/up, SHIFT fast, F12 save a picture, ESC back |
| ESC | Pause menu: resume, restart, back to last gate, settings, controls, leave, quit |
| Arrows / mouse | Move through menus; ENTER or click to choose; left / right change a value |
| ENTER | Host: start the race |
| TAB | Menu or lobby (host): next weather. SHIFT+TAB goes back |
| M | Music on / off |
| H / J | Menu: host / join by typed address |
| 1–4 | Menu: join a race found on your Wi-Fi |
| L | Leave the race |

Gamepad: triggers for throttle, left stick to steer, A torpedo, X mine,
B missile, D-pad down flares, D-pad up grenade, Y reset, Start to begin.

After a race the finish is replayed in slow motion from the bank; ENTER skips
to the results.

## What is in it

**The sea.** Six wave components with physically correct speeds, about 1.9 m
significant wave height. The same wave function drives the water you see, the
boats' buoyancy, the torpedoes and the floating crates, so what you see is what
you hit. The surface near you is a real mesh displaced every frame.

**The boats.** 1,200 kg rigid bodies. Four buoyancy probes at the hull corners
produce all the pitch, roll, heave and slamming; the hull planes at speed,
leans into turns, and loses its engine when the propeller leaves the water.
Ramming, obstacles and explosions are real impulses.

**The circuits.** Bay Circuit (3.0 km), Harbour Sprint (2.48 km) and Canyon
Run (3.28 km), each with sixteen gates and its own racing line, shoreline,
obstacles and barge routes. Race setup shows a route preview, course profile
and personal-best lap. There are 90 m between buoys (the UIM minimum for
F1H2O powerboat racing). Nine rock spires, four jump ramps, two barges crossing
the course, three whirlpools, explosive red barrels (ram one, or shoot it,
and it goes off and sets off its neighbours) and a field of drifting sea
mines just off the racing line. Choose **Circuit** and **Direction** in race
setup: all three courses can be reversed, for six layouts. Every course and
direction has independent lap/race records and a ghost; original bay saves
remain compatible.

The settings are visibly different: Bay Circuit is a low tropical coast with
palms, sandy banks and turquoise water; Harbour Sprint has concrete quays,
container yards, warehouses, cargo cranes and steel navigation towers; Canyon
Run has tall sandstone walls, mesas, cacti, a natural arch and teal water.
Terrain relief, vegetation, skyline, water/lighting palette and minimap land
colours change with the circuit.

After the normal art setup, `bash Scripts/build_environments.sh` regenerates
and imports the nine themed scenery models. `bash Scripts/environment_test.sh`
captures matching sunny game screenshots in `Saved/Screenshots/EnvironmentComparison`.

**The weapons.** A forward deck machine gun with heat and cooling,
torpedoes that skim the surface, mines you drop behind you,
homing missiles (with a lock warning and flares to shake them), and grenades
your copilot throws, all decided by the server. Crates hand out more. A wreck costs you about eight seconds, which is the
point: shooting is a way to gain track position, not a score.

**The garage.** A steel boathouse over its own water slip, in a cove off
the course: the title screen looks out of it at your boat, and the garage
walks round the boat on the quay as you choose. Four boats, each its own Blender model and handling: the
Interceptor (all-rounder, free), the Bullet (9 m cigarette boat, fastest flat
out), the Hornet (F1 tunnel catamaran, quickest and sharpest, fragile) and the
Bulldog (armoured patrol RIB). Twenty paints, six nitrous flame colours, and
engine, handling, armour and nitrous upgrades, three levels each. The garage
shows the boat on the water on a turntable before you buy.

**Coins and power-ups.** Gold coins in rows along every leg (yours alone; they
come back after 25 s), plus coins for your finishing place, hits and a clean
race. Crates also hold power-ups: hull repair, a full nitrous tank and a coin
magnet. After each race a score card adds it up, and the best ten scores go on
the leaderboard (main menu). Coins, boats and records live in this machine's
Game.ini.

**The HUD.** Position, lap, clock, best lap, the running order with time gaps,
speed, hull and ammunition, a map of the circuit and a kill feed.

## Tuning without recompiling

`Config/DefaultGame.ini` holds everything worth changing, commented. The dials
most likely to need it:

| Setting | If... |
|---|---|
| `WaveRoughness` | The sea is too rough to race on (try 0.6) or too calm (1.4) |
| `Weather` | 0 storm, 1 overcast, 2 clear with a low sun |
| `TurnRate` / `TurnResponse` | Steering feels slow or twitchy |
| `BuoyancyMultiplier` | The boat sits too high (lower) or swamps (raise) |
| `PlaningLiftFraction` | The nose does not lift enough at speed |
| `GridCells` / `UpdateInterval` | The water mesh is costing frame rate |
| `TotalLaps` | Races are too short or too long |

All four players must use the same values.

## What is missing

- **Full-race playtesting.** Handling, rival difficulty and combat balance need
  hands-on racing; automated checks cannot establish how the game feels.
- **Cross-device multiplayer verification.** A local host/client check does
  not establish reliability over Wi-Fi between two machines.
- **Effects polish.** Spray, smoke and fire use inexpensive mesh effects;
  further detail needs to stay within the integrated graphics budget.
- **Anti-cheat.** Each player's own boat position is trusted, which is right
  for friends and wrong for strangers.

## How it works

No `.umap` and no Blueprints. The game boots the engine's empty `Entry` map and
builds everything in code.

| File (`Source/BoatRace/`) | Job |
|---|---|
| `OceanWaves` | The wave field. Pure maths on position and time, so every machine agrees without sending anything |
| `WaterSurface` | The visible sea: a displaced mesh that follows the camera |
| `BoatTrack` / `BoatTrackData.h` | The circuit and the scenery as data. The data file is generated and checked by `Scripts/course_layout.py` |
| `BoatPawn` | The boat: buoyancy, hydrodynamics, engine, rudder, networking, weapons, damage |
| `BoatProjectile` | Torpedoes, mines, explosions and ammunition crates |
| `RaceEnvironment` | Sky, weather, light, buoys, rock spires, ramps, whirlpools, barges, debris |
| `BoatRaceGameMode` | Server rules: four-player cap, grid, countdown, laps, wreck recovery, results |
| `BoatRaceGameState` | Race phase, clocks, kill feed |
| `BoatRacePlayerState` | Each player's laps, gates, times and hits |
| `BoatRacePlayerController` | Host, join, start, reset, leave |
| `BoatRaceHUD` | Everything on screen |

**Networking.** Each player simulates their own boat, so the controls never
wait on the network, and reports it to the host 30 times a second. Everyone
else sees a smoothed, extrapolated copy that is still solid, so ramming works
both ways. The host alone counts gates and laps and decides what weapons hit.
Anything that can be worked out from the shared clock — the waves, the barges,
the buoys riding the swell — is never sent at all.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `Unreal Engine was not found` | `export UE_ROOT=/path/to/your/engine` |
| "Cannot find a compatible Vulkan device" | No usable GPU or driver; install `vulkan-tools` and check `vulkaninfo --summary` |
| Compile errors | Expected risk. Send the lines containing `error:` |
| The boat will not pull away from a standstill | `PropellerDepth` is too shallow for the draft; raise it |
| The boat rolls over in turns | Lower `MaxHeelDegrees`, or `LateralDragRatio` |
| The boat is too floaty, or sinks | `BuoyancyMultiplier`: 2.6 is the default, below ~1.5 it swamps |
| Water is flat and plain-coloured | `M_Water` was not built. Re-run `Scripts/import_assets.sh` |
| Boat model faces backwards | `CustomMeshYawOffset=180` |
| Very low frame rate | `Scripts/low_spec.sh` |
