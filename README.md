# Boat Race

Four-player armed powerboat racing for Unreal Engine 5, written entirely in
C++, with art generated in Higgsfield.

A 3 km, sixteen-gate circuit in a stormy bay. The boats are rigid bodies
floating on a real wave field, with torpedoes and mines to make the racing
untidy. One player hosts, three others join by address.

## Status

- **Version 0.2 compiles** on Unreal Engine 5.9.0 preview-1 under Ubuntu, with
  no warnings. It has not been playtested, so the handling, the difficulty and
  the sea state are calculated rather than felt.
- The physics was checked by simulating it numerically rather than by feel
  (`Scripts/physics_check.py`), and the circuit by generating and testing its
  geometry (`Scripts/course_layout.py`). Both found real bugs, which is why
  they are in the project.
- No sound, no spray particles. See [What is missing](#what-is-missing).

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

## Documentation

| | |
|---|---|
| **[ONBOARDING.md](ONBOARDING.md)** | **Getting four people into a race**: sharing the game over Wi-Fi in one line, building the Windows copy so Windows and Linux friends race together, joining from the menu list, and what to do when a join fails |
| [PLAN.md](PLAN.md) | What was built and why, stage by stage, with the real-world powerboat racing it borrows from |
| [HIGGSFIELD_ASSETS.md](HIGGSFIELD_ASSETS.md) | The generated art: which models, the exact prompts, what each cost, and how to replace any of it |
| [CLAUDE.md](CLAUDE.md) | Working context for an AI coding agent: current state, design rules, verification scripts |

## Controls

| Key | Action |
|---|---|
| W / S | Throttle / reverse |
| A / D | Steer |
| SPACE or left mouse | Fire a torpedo |
| Q or right mouse | Drop a mine |
| R | Back to your last gate |
| SHIFT / RB | Boost (fills slowly, and fast while airborne off a ramp) |
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
Y reset, Start to begin.

## What is in it

**The sea.** Six wave components with physically correct speeds, about 1.9 m
significant wave height. The same wave function drives the water you see, the
boats' buoyancy, the torpedoes and the floating crates, so what you see is what
you hit. The surface near you is a real mesh displaced every frame.

**The boats.** 1,200 kg rigid bodies. Four buoyancy probes at the hull corners
produce all the pitch, roll, heave and slamming; the hull planes at speed,
leans into turns, and loses its engine when the propeller leaves the water.
Ramming, obstacles and explosions are real impulses.

**The circuit.** 3 km, sixteen gates, 90 m between buoys (the UIM minimum for
F1H2O powerboat racing). Nine rock spires, four jump ramps, two barges crossing
the course, three whirlpools, twelve floating barrels.

**The weapons.** Torpedoes that skim the surface and mines you drop behind you,
both decided by the server. A wreck costs you about eight seconds, which is the
point: shooting is a way to gain track position, not a score.

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

- **Sound.** None at all.
- **Spray and wake particles.** Niagara effects are authored in the editor, not
  in code, so the boats move convincingly but throw no water.
- **A shoreline.** The edge of the bay is a boundary that pushes you back
  rather than a coast.
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
