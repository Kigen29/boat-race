# JetFighter

Standalone arcade jet flight prototype. This project loads only its own game
module and the shared MaritimeCore plugin. It has its own configuration,
saved data, logs, tests and packages, independently of BoatRace.

From the repository root:

```bash
Scripts/build.sh jet
Scripts/play.sh jet
Scripts/test.sh jet
Scripts/package.sh jet
```

Or open `games/JetFighter/JetFighter.uproject` after building JetFighterEditor.
The root `UE_ROOT` setting and engine discovery work for both games.

| Input | Action |
|---|---|
| Mouse | Select world aim direction; the aircraft banks and climbs towards it |
| M | Toggle mouse aim / direct stick |
| Up / Down | Push / pull the nose |
| A / D | Roll left / right |
| Q / E | Rudder left / right |
| W / S | Increase / decrease throttle; holds the chosen setting |
| Shift / Space | Afterburner / airbrake |
| F | Assisted / Expert flight |
| R | Reset in the air |
| N / D-pad right | Select the next navigation landmark |
| Esc / Start | Pause / resume |
| Enter / A while paused | Resume |
| Q / B while paused | Quit JetFighter |

Gamepad: left stick flies, right stick rudder, triggers change throttle,
LB boosts, RB brakes, X toggles assists, Y resets. Manual inputs override
mouse aim and preserve the new direction when released. The amber marker is
the selected aim direction; cyan shows the flight path. An amber arrow points
towards an aim point outside the view.

Sea, terrain and solid-obstacle impacts respawn after three seconds. Outside a 6 km radius or above
6 km altitude, return within ten seconds to avoid resetting. Assisted mode
trims lift and limits angle of attack; Expert allows stalls. All tuning lives
in `Config/DefaultGame.ini` under `/Script/JetFighter.JetPawn`.

## Island airspace

Four main islands and two small stacks surround the open spawn corridor.
East Spire rises to 950 m; West Ridge, North Island and the South Airstrip
provide distinct navigation points. Summit beacons and runway markings make
them readable from the air. Press N or D-pad right to cycle landmarks; the
HUD shows the selected point's bearing, distance and projected diamond.
The airstrip is a navigation landmark; landing and takeoff are not implemented.

The terrain's collision height is interpolated from the exact triangles in
its visible 80,000-triangle mesh. Pull-up warnings measure clearance over
both hills and waves. Cloud clusters are seeded and drift on the world clock.
Their count, seed and wind speed live in the JetScenery config section.

The jet-specific water renderer expands its fixed-size grid in powers of two
with altitude. It blends detailed waves into distant flat water at the mesh
edge and above 600 m, suppressing stretched wave patches and a square seam.
Above that height it draws only the distant surface, avoiding overlapping
water sheets. Low-flight contact still uses the shared OceanWaves function. BoatRace's
water renderer and gameplay are unchanged.

## Verification and next work

`Scripts/flight_check.py` mirrors the force/rate and aim equations.
`Scripts/jet_flight_test.sh` runs eight tests against the actual Unreal code:
five flight/control/collision tests, terrain-to-mesh agreement, ground-impact
warnings/respawn, and altitude-water budget/transition checks.
`Scripts/jet_flight_tour.sh` renders level flight, banking, climbing, a sea
impact and respawn. `Scripts/jet_check.sh 55` surveys the island world at four
camera altitudes, with a 55 fps minimum. `Scripts/jet_pause_test.sh` verifies
that pausing freezes the aircraft and resuming restores movement. Logs and
screenshots go to this project's Saved directory.

The airframe is temporary geometry; flight is local only. Next are hands-on
flight tuning and a 3D ring course, followed by LAN flight, weapons, dogfight
rules, AI and original aircraft. Research in docs predates the monorepo and
refers to the original paths.

World milestone validated on 20 November 2026: editor build, all 15 numerical
flight checks, eight engine flight/world tests, and the altitude survey pass.
The four survey stops measured 110–119 fps at 1280 × 720. The aerial views
include the airstrip and islands, and were inspected for water seams.
