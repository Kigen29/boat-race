# Boat Race — working notes for Claude

Four-player armed powerboat racing for Unreal Engine 5. Code-only: there is no
`.umap` and there are no Blueprint assets. The game boots the engine's empty
`Entry` map and builds the sea, the circuit, the scenery and the boats in C++.

## Where things stand

- **v2 has never been compiled or run.** It was written in a cloud workspace
  with no Unreal Engine and no GPU. v1 did compile cleanly against Unreal 5.9
  preview-1, so the project layout and build rules are known good.
- The physics and the course geometry were verified by simulation instead of by
  playing (see **Verification** below). Both found real bugs.
- Nothing has been playtested. Handling, difficulty and the sea state are
  guesses from calculation, not from feel.

## The machine this is built on

| | |
|---|---|
| OS | Ubuntu (recent), x86-64 |
| Graphics | Intel Meteor Lake integrated, Vulkan via Mesa — **no dedicated GPU** |
| Memory | 14 GB, shared with the graphics |
| Engine | Unreal Engine 5.9.0 preview-1, unpacked under `~/Downloads/` |

Integrated graphics means the low preset is required: `Scripts/low_spec.sh`
turns off Lumen, virtual shadow maps, volumetric cloud and fog, drops to 70%
screen percentage and shrinks the water mesh. `Scripts/setup.sh` applies it
automatically when Vulkan reports an integrated device.

## Build and run

```bash
bash Scripts/setup.sh          # engine lookup, graphics preset, build, assets
Scripts/play.sh                # play (H host, J join, ENTER solo)
Scripts/play.sh host           # host straight away
Scripts/test_4_players.sh      # four windows on one machine (needs a real GPU)
```

`setup.sh` stops rather than building a folder that is behind the repository.
If it says files differ, `git restore .` takes the repository's version.

## Architecture

| File | Job |
|---|---|
| `OceanWaves` | Six sine components, vertical displacement only. Pure maths on (position, time), so every machine agrees with nothing sent |
| `WaterSurface` | The visible sea: a procedural mesh grid displaced every frame, following the camera in whole cells |
| `BoatTrack` + `BoatTrackData.h` | Circuit and scenery as data. **The .h is generated — edit `Scripts/course_layout.py`, not the header** |
| `BoatPawn` | 1200 kg rigid body: four buoyancy probes, hydrodynamics, engine, rudder, networking, weapons, damage |
| `BoatProjectile` | Torpedoes, mines, explosions, ammunition crates |
| `RaceEnvironment` | Sky, weather, light, buoys, rock spires, ramps, whirlpools, barges, debris |
| `BoatRaceGameMode` | Server rules: four-player cap, grid, countdown, laps, wreck recovery, results |
| `BoatRaceHUD` | Everything on screen, drawn on the canvas — no UI assets |
| `LanDiscovery` | Hosts announce on UDP 7778 once a second; the menu lists what it hears, number keys join |

### Rules that hold the design together

- **One wave function.** The water mesh, buoyancy, torpedoes and crates all
  call `OceanWaves`. Never approximate the surface anywhere else, or things
  will float above or sink below the water you can see.
- **Anything derivable from the shared clock is never replicated.** Waves,
  barges and buoys ride `GetServerNow()`. Only boats, weapons and crates are
  sent over the network.
- **The owner simulates its own boat; everyone else holds a kinematic copy.**
  The server counts gates from reported positions and decides all damage.
- **Tuning lives in `Config/DefaultGame.ini`,** not in code. Every handling
  number is a `UPROPERTY(Config)`. All four players need the same values.

## Verification — run these after touching physics or the course

```bash
python3 Scripts/physics_check.py    # mirrors BoatPawn.cpp as a rigid body
python3 Scripts/course_layout.py    # regenerates and checks BoatTrackData.h
```

`physics_check.py` must end in `FAILURES: 0`. It checks that the boat floats
and settles, reaches its top speed, turns without capsizing, and behaves the
same from 120 fps down to 20 fps. It has already caught a propeller mounted
above the waterline (top speed zero) and drag following the hull's tilted axes
(the boat dragged itself under in turns).

`course_layout.py` checks that gate lines never cross, that no obstacle blocks
a gate, that the barges cross the racing line, and that driving the line scores
every gate in order while driving it backwards scores none.

## Conventions

- Comments explain **why**, not what. British spelling in prose and comments.
- No new engine plugins without good reason; `ProceduralMeshComponent` is the
  only one beyond the defaults.
- Never commit `Binaries/`, `Intermediate/`, `Saved/`, `dist/`, imported
  `.uasset` files or the Higgsfield media — all regenerate from the scripts.

## Known gaps, roughly in order of value

1. **It has not been compiled.** Expect UHT or API errors on 5.9 preview first.
2. **No sound at all.**
3. **No spray, wake or foam.** Needs Niagara, which is authored in the editor.
4. **No shoreline.** The bay edge is a force that pushes boats back.
5. Handling is unplayed: `TurnRate`, `WaveRoughness` and `BuoyancyMultiplier`
   are the first dials to try.
6. Each player's own boat position is trusted — fine for friends, not for
   strangers.

## Higgsfield assets

Generated 11 Nov 2026; `Scripts/fetch_assets.sh` downloads them and
`Scripts/import_assets.sh` imports them to `/Game/Higgsfield`. The game falls
back to placeholder shapes and plain water when they are absent, so never make
them a hard requirement. Prompts and costs are in `HIGGSFIELD_ASSETS.md`.
