# Boat Race v2: feature plan

Everything below is built in C++ in this project, so it all arrives through
`Scripts/build.sh` with no editor work needed.

Real-world reference: UIM F1H2O powerboat racing (the series Red Bull teams
compete in). Borrowed from the real rulebook: a circuit marked by turn buoys,
a **90 m minimum course width**, a start line of two tall white pillar buoys,
orange turn buoys with one yellow, and a standing start.

## Stage 1 — A sea that moves (the foundation)

The complaint "it's too calm" is really that the water is a flat plane. Fixing
that first, because waves are what every other feature then reacts to.

| Feature | How |
|---|---|
| Real waves | Six sine components with physically correct speeds (deep-water dispersion, so long swells outrun short chop). Significant wave height about 1.9 m: lively for a 7 m boat |
| One source of truth | The same wave function drives the water mesh, the boat's buoyancy, the torpedoes and the floating crates, so what you see is what you hit |
| Visible surface | The water near you is a real mesh of 7,200 triangles displaced every frame, with exact analytic normals for lighting; flat ocean beyond 420 m |
| Sea state dial | `WaveRoughness` in `Config/DefaultGame.ini`: `0` glass, `1` default, `2` storm |
| Synchronised | Waves are driven by the server's clock, so all four players see the same sea |

## Stage 2 — Real physics

The boat stops being a sliding box and becomes a rigid body in Unreal's Chaos
physics, 1,200 kg, with a low and slightly aft centre of mass.

| Feature | How |
|---|---|
| Buoyancy | Four probes at the hull corners each push up in proportion to how deep they are. Pitch, roll, heave and the slam off a wave crest all emerge from that rather than being animated |
| Hydrodynamics | Quadratic drag along the hull, much stiffer drag sideways (so it grips but can be made to slide), wave-slope influence |
| Steering | The rudder only bites when water flows past it, and not at all when the boat is airborne. A control loop targets a yaw rate, which keeps handling predictable instead of leaving it to blind physics tuning |
| Collisions | Boat-to-boat ramming, obstacle impacts and explosion knockback are all real impulses on the body |
| Self-righting | Corner buoyancy rights the hull naturally; an assist kicks in past 70 degrees so a capsize doesn't end your race |
| Tuning without recompiling | Mass, buoyancy, drag, thrust and turn rate all live in `Config/DefaultGame.ini` |

## Stage 3 — Weapons

Server-authoritative, the way shooters are built: your machine asks to fire,
the server decides what was hit and what it cost. Nobody can shoot you on their
screen alone, and nobody can edit their own damage numbers.

| Feature | How |
|---|---|
| Torpedo | Travels 55 m/s riding the wave surface, 6 s range, explodes on any boat or obstacle |
| Mine | Dropped behind you, arms after 1.2 s, detonates within 3.5 m. Best used entering a turn |
| Explosions | 9 m blast radius with damage and impulse falling off by distance: a near miss shoves you off line, a direct hit wrecks you |
| Ammo | Floating crates on the course, 3 torpedoes or 2 mines each, respawning 15 s after being taken |
| Health and wrecking | 100 hit points. At zero the engine dies and you drift for 4 s, then restart at your last gate with 3 s of immunity. The cost is lost time, so shooting is a racing tactic |
| Feedback | Health bar, ammo counts, hit markers, and a kill feed naming who wrecked whom |
| Friendly fire | Your own blast can push you; it cannot damage you |

## Stage 4 — A bigger, more hostile course

| Feature | How |
|---|---|
| Scale | 2.8 km lap, 16 gates, inside a 1.4 km by 1.1 km bay (the old course was 1.17 km) |
| Course width | 90 m between buoys, per the UIM minimum |
| Rock pillars | Nine, just off the racing line, solid |
| Jump ramps | Four floating wedges that launch you into the air. Landing is wherever the physics puts you |
| Moving barges | Two crossing the course on fixed timing, identical on every machine |
| Whirlpools | Three, dragging you inward and around |
| Floating debris | Twelve barrels the server simulates, so they scatter when hit |
| Laps | 2 by default, since a lap is now about 100 s |

## Stage 5 — Playing together

A guide (`ONBOARDING.md`) covering the host-and-join flow, what each friend
needs installed, finding the host's address, firewall and router, and what to
do when a join fails.

## What is deliberately not in this round

- **Sound.** None of it. It needs audio assets and editor setup.
- **Spray and foam particles.** Niagara effects are authored in the editor, not
  in code. Wave motion and wakes read well without them, but the spray a real
  powerboat throws is missing.
- **A proper shoreline.** The bay edge is still a boundary you cannot cross
  rather than a coast you can see.
- **Anti-cheat.** Each player's own boat position is trusted, which is right
  for friends and wrong for strangers.

## The honest risk

None of this has been compiled or run; I have no Unreal Engine and no GPU here.
Physics in particular is tuned by calculation rather than by feel, so expect to
adjust numbers in `Config/DefaultGame.ini` once you have driven it. The two
most likely complaints are "the boat is too floaty" and "it turns too slowly",
and both are one config line each.

On your laptop's Intel graphics, the waves are the cheap part (7,200 triangles)
but four boats, explosions and debris together may be heavy. `WaveRoughness`
and `r.ScreenPercentage` are the dials to turn down.
