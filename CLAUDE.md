# Boat Race — working notes for Claude

Four-player armed powerboat racing for Unreal Engine 5. Code-only: there is no
`.umap` and there are no Blueprint assets. The game boots the engine's empty
`Entry` map and builds the sea, the circuit, the scenery and the boats in C++.

## Where things stand

- **v3 compiles and runs** on Unreal 5.9 preview-1 (13 Nov 2026), 60–80 fps on
  the Intel laptop. Added: land around the bay with cliffs, beaches, narrows
  and barge channels; Blender models for everything; nine weathers; synthesised
  sound and music; computer rivals; start lights and race banners; explosions,
  fire, spray and wake.
- Checked by `-PhotoTour` screenshots (see **Verification**), not by playing a
  full race by hand. Handling and rival difficulty are still unplayed.
- Multiplayer across two machines is untested since v2.

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

Windows copy: only on a Windows PC (Unreal cannot cross-compile to Windows
from Linux, and the Linux engine here has no Win64 libraries):
`Scripts\package_windows.bat` -> `dist\BoatRace-Windows.zip`, which
`share.sh` offers on a download page when it is in `dist/`. Without one, the
page offers `Install Boat Race.bat`, which runs `Scripts/windows_install.ps1`
(address filled in) to install the Linux copy inside WSL. Untested on Windows.

`setup.sh` stops rather than building a folder that is behind the repository.
If it says files differ, `git restore .` takes the repository's version.

## Architecture

| File | Job |
|---|---|
| `BayTerrain` | The land: one height function from the shore table in `BoatTrackData.h`. Builds the mesh (solid near the course), plants trees and rocks, places the grandstand, cottages, lighthouse, bridge and duck, and tells boats which way the water is |
| `BoatArt` | Loads `/Game/Models/SM_*` and gives every material slot a finish from its name (`Leaf_5CB646`): the part decides the master material (clear-coat `M_Paint`, livery `M_Livery`, grained `M_Surface` for metal/rubber/wood/rock, `M_Glass`, `M_Foam`) and the hex its colour. `Paint_*`/`Livery*` take a player's colour, `Lamp*` glow; snow and night are tints |
| `BoatWake` | The wake ribbon: white water behind the transom plus the two Kelvin arms (19.5°), laid down by distance travelled and drawn on the waves |
| `RaceWeather` | The nine weathers as data, plus wind and lightning timing from the shared clock. The host's choice is one replicated byte on the game state |
| `BoatAudio` | Every sound synthesised in code at startup (no audio files) and played through procedural waves; a world subsystem runs the music, rain, wind and sea beds |
| `BoatRivalController` | Computer rivals: an AI controller with a player state that drives the boat by setting its throttle and rudder |
| `OceanWaves` | Six sine components, vertical displacement only. Pure maths on (position, time), so every machine agrees with nothing sent |
| `WaterSurface` | The visible sea: a procedural mesh grid displaced every frame, following the camera in whole cells |
| `BoatTrack` + `BoatTrackData.h` | Circuit and scenery as data. **The .h is generated — edit `Scripts/course_layout.py`, not the header** |
| `BoatPawn` | 1200 kg rigid body: four buoyancy probes, hydrodynamics, engine, rudder, networking, weapons, damage |
| `BoatProjectile` | Torpedoes, mines, explosions, ammunition crates |
| `RaceEnvironment` | Sky, light, weather (rain, snow, gale, clouds, lightning), pylons, rope lanes, sea stacks, ramps, whirlpools, barges, logs, start gantry and its lights |
| `BoatRaceGameMode` | Server rules: four-player cap, grid, countdown, laps, wreck recovery, results |
| `BoatRaceHUD` | Everything on screen, drawn on the canvas — no UI assets. Text is the engine's Roboto through Slate's font cache (sharp at any size); a compass tape and a world-space waypoint over the next gate; the menu panels |
| `BoatRacePlayerController` | Menus as data (`BuildMenuItems`): title screen, pause, settings, controls; keys, gamepad and mouse. While a menu is open the boat ignores the driving keys |
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
- **Models are made in Blender by `Scripts/make_models.py`** (boats in
  `Scripts/make_boats.py`: subdivision hulls with strakes, chines, steps and
  a livery texture in `Art/Textures`), never by hand:
  metres, +X forward, origin at the waterline, materials named `Part_RRGGBB`.
  Anything solid gets a plain invisible collision shape; the model is looks only.
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

Visual check, after anything that changes how the game looks:

```bash
Scripts/play.sh menu -PhotoTour     # races, visits every weather, sets off a blast, quits
```

Screenshots land in `Saved/Screenshots/LinuxEditor/`. The tour drives the
player's boat itself; keys pressed in its window still count.

`course_layout.py` checks that gate lines never cross, that no obstacle blocks
a gate, that the barges cross the racing line, and that driving the line scores
every gate in order while driving it backwards scores none. It also checks the
shore: every buoy, the whole width of the course, every obstacle, the barge
runs and the grid must be water.

## Conventions

- Comments explain **why**, not what. British spelling in prose and comments.
- No new engine plugins without good reason; `ProceduralMeshComponent` is the
  only one beyond the defaults.
- Never commit `Binaries/`, `Intermediate/`, `Saved/`, `dist/`, imported
  `.uasset` files or the Higgsfield media — all regenerate from the scripts.

## Known gaps, roughly in order of value

1. **Not played by hand.** Rival skill (`RivalSkill`), handling (`TurnRate`,
   `WaveRoughness`, `BuoyancyMultiplier`) and crash damage are guesses.
2. **Multiplayer not retested** across machines since the rivals and weather
   were added.
3. **Effects are mesh puffs, not Niagara.** Spray, smoke and fire are pooled
   soft spheres; good enough on integrated graphics, not film quality.
4. **No crowd or commentator.** The grandstand is empty; sound is synthesised.
5. Each player's own boat position is trusted — fine for friends, not for
   strangers.

## Higgsfield assets

Generated 11 Nov 2026; `Scripts/fetch_assets.sh` downloads them and
`Scripts/import_assets.sh` imports them to `/Game/Higgsfield`. The game falls
back to placeholder shapes and plain water when they are absent, so never make
them a hard requirement. Prompts and costs are in `HIGGSFIELD_ASSETS.md`.
