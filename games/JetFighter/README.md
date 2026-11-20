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
| Esc / Start | Pause / resume |
| Enter / A while paused | Resume |
| Q / B while paused | Quit JetFighter |

Gamepad: left stick flies, right stick rudder, triggers change throttle,
LB boosts, RB brakes, X toggles assists, Y resets. Manual inputs override
mouse aim and preserve the new direction when released. The amber marker is
the selected aim direction; cyan shows the flight path. An amber arrow points
towards an aim point outside the view.

Sea/solid impacts respawn after three seconds. Outside a 6 km radius or above
6 km altitude, return within ten seconds to avoid resetting. Assisted mode
trims lift and limits angle of attack; Expert allows stalls. All tuning lives
in `Config/DefaultGame.ini` under `/Script/JetFighter.JetPawn`.

`Scripts/flight_check.py` mirrors the force/rate and aim equations.
`Scripts/jet_flight_test.sh` runs five tests against the actual Unreal pawn:
flight envelope, control axes and mouse aim, frame-rate independence,
collision/respawn and boundary recovery. Tests are named `JetFighter.Flight`.
`Scripts/jet_flight_tour.sh` renders level flight, banking, climbing, a sea
impact and respawn. `Scripts/jet_check.sh` surveys four camera altitudes.
`Scripts/jet_pause_test.sh` renders pause/resume and verifies that pausing
freezes the aircraft and resuming restores movement. Logs and screenshots go
to this project's `Saved` directory.

The airframe is temporary geometry; flight is local only. Next are terrain,
landmarks, sky/water performance and hands-on flight tuning, followed by 3D
ring racing and LAN flight, weapons, dogfight rules, AI and original aircraft.
Research in `docs/` predates the monorepo and refers to the original paths.
