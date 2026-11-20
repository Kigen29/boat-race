# First gun-training encounter

Implemented 20 November 2026. Launch with `Scripts/play.sh jet`.
The aircraft starts airborne with an orange stationary training drone 600 m
ahead. Follow the target brackets, align the actual nose sight and hold the
left mouse button or gamepad A. The amber mouse-aim circle is a steering
request; the gun fires along the aircraft's nose, not directly at that circle.

The drone's hull bar and numeric hull fall on actual hits. A brief cyan hit
marker confirms damage, an impact flash appears at contact, and zero hull
hides/disables the drone and produces a short blast and falling debris. The
training banner changes to complete. T / D-pad down restarts the encounter,
returning the aircraft to its safe initial flight state and creating a fresh
drone. R remains the flight-only reset. There is no automatic target respawn.

## Current tuning

| Parameter | Value |
|---|---:|
| Gun cadence | 15 rounds/second |
| Muzzle velocity | 1200 m/s plus aircraft velocity |
| Maximum world travel | 900 m |
| Round collision radius | 0.25 m |
| Damage | 12 hull per confirmed impact |
| Drone hull | 120; ten hits to destroy |
| Overheat | 38 consecutive shots |
| Cooling | 25 percentage points/second when released, disabled or overheated |
| Unlock threshold | 35% heat |
| Simulation | 120 Hz; catch-up capped at 0.1 s/frame |
| Live round cap | 64 |

Sweeps catch thin solid obstacles between simulation steps. An additional
surface query checks the same terrain/wave height used by flight contact;
whichever collision occurs first blocks the shot. Bullet lifetime is bounded
by travelled distance. Rendering uses a bounded set of instanced mesh tracers
and a short muzzle flash; neither the HUD nor a tracer decides damage.

Health rejects friendly fire, nonpositive/nonfinite damage, duplicate/stale
shot IDs and hits after destruction. The current monotonic shot sequence is
for this encounter's **single local gun owner**. Multi-owner/projectile-ID
tracking, replicated authority and anti-cheat are not implemented by this API.
The damage component is the foundation for those changes, not a claim of LAN
combat. A crashed player cannot fire; pause stops the gun's advancement.

## Verification

`Scripts/jet_flight_test.sh -RequireJetArt` includes three combat tests:

- Damage ownership, duplicate/lethal hits and hull reset.
- Cadence at 30/60/120 fps, heat lock/recovery, round expiry and disabled fire.
- Swept hits on the actual drone, ten-hit destruction, encounter reset, a
  2 cm obstruction and analytic terrain contact.

`Scripts/jet_combat_tour.sh` flies and fires through the same runtime methods
as normal input; it does not directly reduce target hull. It captures Ready,
Firing, Destroyed, Overheat and Restart states and validates damage and heat
from the runtime log. Screenshots/logs are generated in Saved. The pause check also starts live rounds before pausing and verifies frozen
round count, shot count and heat. Existing flight, art and water checks remain
available.

## Scope still ahead

The drone is stationary and unarmed, and has a spherical training hit volume.
There is no aim-lead solution, moving AI pilot, player combat hull, weapon
audio, subsystem damage, missile, lock, flare or multiplayer combat yet. Gun
impacts on scenery stop the round but do not yet spawn material-specific
impact effects. The current effects are simple runtime meshes, to be replaced
by authored effects later. Next: targeting and missiles, countermeasures and
then a manoeuvring opponent using the existing flight limits.
