# JetFighter: playable combat design

Design baseline: 20 November 2026. This is our proposed game, informed by the
reference research. Balance values in this document are **starting hypotheses**,
not measured Sky Warriors settings and not a claim that the systems already
exist in code.

## 1. The game we are building

An accessible PC aerial-combat game about reading terrain, getting a good
firing position, breaking enemy pressure and helping a small squad complete an
objective. The aircraft should look mechanically plausible, feel powerful,
and remain enjoyable with mouse/keyboard or a gamepad. A player should be able
to fly in one minute and still have positioning, timing and energy management
to learn after many matches.

The first complete release supports one player with bots and up to four human
players over LAN. The default competitive format is 2v2, with bots filling
empty seats. A future six-player experiment must demonstrate acceptable
networking and laptop performance before replacing that scope. We are not
starting with public ranked matchmaking, an account service or a mobile port.

### Experience pillars

1. **A visible aircraft with personality.** A silhouette, engine arrangement,
   canopy, painted surfaces, exhaust and audio identify the selected vehicle.
2. **A fight with readable cause and effect.** The player knows who is targeted,
   whether a weapon can fire, what incoming threat exists and why damage occurred.
3. **Terrain changes a decision.** Valleys, shorelines and cloud cover create
   alternative approaches; there is always room to pull out of a bad approach.
4. **Objectives bring the fight together.** Empty airspace and five minutes of
   searching are failures, even if the scenery is attractive.
5. **The laptop is a real target.** Art and effects are accepted in measured
   flight scenes, including multiple aircraft, rather than only beauty renders.

## 2. State of the actual implementation

| System | Present state | Next requirement |
|---|---|---|
| Standalone project | Implemented, separate from BoatRace | Keep boundaries and packaging checks |
| Flight physics | Fixed-step lift, drag, thrust, G demand and stalls | Human playtest and class tuning |
| Controls | Mouse aim, direct stick, keyboard, gamepad | Remapping, inversion and sensitivity UI |
| Aircraft visuals | Three original selectable art prototypes | UV/PBR production pass and articulated controls |
| Aircraft roles | Roster metadata identifies roles | Different flight/combat profiles are not yet implemented |
| HUD font | Rajdhani regular/semibold with fallback | Larger UI system and packaged-font verification |
| Terrain | Procedural island mesh with matching contact sampling | Authored ridges, channels, lakes and mesh kits |
| Water/clouds | Aerial water LOD and drifting visual puffs | Local water bodies and gameplay cloud volumes |
| Navigation | Cycle named landmarks, bearing/distance/marker | Objectives, radar and team pings |
| Crash/pause | Sea/terrain/solid contact, respawn, pause | Combat deaths, safe spawns and results |
| Nose gun / training hull | Implemented: swept rounds, heat, stationary drone and restart | Aim lead, production effects and moving targets |
| Missiles/flares | Not implemented for jets | Targeting, seeker and countermeasure simulation |
| AI/LAN combat | Not implemented | Same-physics pilots and server-authoritative combat |
| Matches/progression | Not implemented | Actual phases, objectives, scores and local saves |

A visible button, aircraft role label, missile store mesh or concept screen is
not evidence of a working gameplay system. The production checklist requires
an action, state transition, world feedback and a repeatable test for each
feature before it can be called implemented.

## 3. The first ten minutes

### First launch

Open a compact, real hangar scene. The selected aircraft is visible and can be
rotated. The primary actions are **Training**, **Quick Battle**, **Host LAN**,
**Join LAN**, **Aircraft**, and **Settings**. Hide unfinished actions rather
than presenting an attractive screen full of inactive buttons. During the
first slice only Training, Aircraft and Settings need to be enabled.

The first prompt offers standard mouse aim and gamepad. It demonstrates bank,
pull and throttle with a short moving cue, not a long text manual. An optional
inversion question shows a preview arrow. The player can skip and change it
later. The tutorial saves completion locally without requiring an account.

### Training encounter

1. Start airborne in a safe lane facing the play area. The selected model,
   exhaust, engine sound and flight instruments are visible immediately.
2. Follow a broad curved flight route around a ridge. Rings can teach the
   controls, but are optional assistance, not the main game.
3. Acquire a slow flying drone. A target marker changes from selected to
   acquiring to locked; each state has a different shape and sound.
4. Track it with the nose gun. Tracers originate at a real muzzle, impact the
   target and reduce its health. At zero health it visibly breaks up and the
   drill advances. A floating text message by itself does not pass this step.
5. Fire a standard guided missile. The player sees the launch, trail,
   interception or miss, damage, feedback and rack/cooldown change.
6. Receive a scripted incoming threat in a forgiving corridor. The directional
   warning and tone teach a break turn plus flare timing. Failed attempts
   reset quickly without an economy penalty.
7. Approach a ridge and a cloud bank to learn the difference between solid
   cover and acquisition cover. Both effects must be demonstrable in the
   actual weapon logic.
8. Complete a small relay strike, then see a results screen explaining hits,
   evasion, damage, objective contribution and completion time.

The first combat slice ends here. It is a small repeatable game, not another
empty flight demonstration with menu promises attached.

## 4. Aircraft handling and camera

Keep the existing 120 Hz flight integrator. Controls produce the same
normalised pitch, roll and yaw demands whether they come from a human or AI.
Assisted mode trims lift, limits excessive angle of attack, and helps level
the wings on release. Expert mode exposes stalls and more demanding energy
management. Neither mode grants hidden damage reduction.

Class profiles multiply or replace config data through a `FJetFlightProfile`.
Do not duplicate the integrator for each aircraft. A profile includes thrust,
boost thrust, drag, induced drag, lift curve, G limit, roll rate, yaw response,
brake authority, inertia/rate response and camera offsets. The default light
fighter remains the numerical-test baseline. Every extreme profile gets the
same frame-rate, stall, turn and recovery checks.

The chase camera follows the aircraft with restrained positional and rotational
lag, while a horizon/flight-path marker explains actual orientation. Boost
widens FOV modestly; optional shake and speed streaks communicate thrust. Avoid
large forced camera swings and persistent blur. Expert players can reduce
horizon levelling or use a closer view. A target-look control may briefly look
towards the selected enemy, but never secretly steer the aircraft.

Mouse aim should select a world direction, not be an unbounded virtual stick
that drifts when the mouse stops. Manual stick input overrides it cleanly.
Screen-edge indicators show an aim direction or threat that leaves the view.
Automatic target tracking, if offered, is separately labelled and does not
turn a pilot's aircraft without a setting they chose.

### Default action map for the combat slice

| Action | Keyboard/mouse | Gamepad starting proposal |
|---|---|---|
| Aim/fly | Mouse; direct axes available | Left stick pitch/roll, right stick rudder/look as selected |
| Throttle | W/S | Triggers, retaining existing flight bindings |
| Gun | Hold left mouse / Ctrl | Hold A; remappable |
| Missile | Right mouse / E after resolving rudder conflict | B |
| Cycle combat target | T / mouse wheel | D-pad up |
| Countermeasures | Q or a dedicated remapped binding | D-pad down |
| Boost/brake | Shift/Space | LB/RB |
| Aircraft selection | Hangar; V remains development shortcut | Hangar; D-pad left remains development shortcut |
| Team ping | Middle mouse | Right-stick press |
| Pause | Escape | Start |

These are proposals. Existing Q/E rudder must be reconciled explicitly; do
not bind rudder and a weapon to the same input. Remapping should show conflicts
before applying a change. Gameplay prompts follow the active device.

## 5. Target selection and weapon state

`Selected` means the pilot chose an actor. `Acquiring` means it satisfies the
weapon's range/cone/sensor conditions and lock progress is building. `Locked`
means the weapon can launch a guided shot. `Blocked` explains terrain,
cloud-acquisition protection, minimum range or target loss. `Incoming` is an
independent defensive state, not an alternative selected-target colour.

Cycle targets by a stable combination of forward angle, range and visibility.
Prefer living hostile aircraft, then relevant objective targets. A friendly
cannot enter a damaging hostile lock. Selected targets retain identity briefly
through a bank so the marker does not jump to a different opponent every frame.
The HUD shows target callsign/type, range, health and lock progress. When
occluded, it can show a last-known marker with a different style, not pretend
the player currently has a valid shot.

## 6. Gun, missiles and defensive tools

### Nose gun

Use deterministic straight-moving projectiles with segment sweeps between
fixed steps. A lead pipper must use the same projectile speed as the gun.
Visual tracers may be sparse, but every damaging shot has an authoritative
trajectory. Avoid a probability roll substituting for aim.

Starting targets: 1,200 m/s projectile speed, 900 m effective range, 15 shots/s,
12 damage/shot against a nominal 1,000 hull, and about 2.5 seconds of continuous
fire before overheating. Cooling and overheat recovery remain visible. These
values imply several seconds of accurate tracking; they must be tuned in
actual pursuit fights, not accepted from arithmetic alone.

Place the muzzle at an authored socket/anchor. A wing-mounted gun must fire
from that wing, not the world origin or chase camera. Friendly-fire defaults
off. A mountain, hangar or bridge must block a shot. There is no shot through
terrain simply because a target marker is visible.

### Guided missile family

Start with one reliable standard missile before adding variants. Proposed
initial envelope: acquisition 0.8–1.2 s, launch range roughly 150–1,800 m,
350–450 m/s travel speed, ten-second maximum life, limited lateral acceleration,
and about 350 hull damage. Tune the tail-chase, crossing and head-on cases
separately. A target near maximum range should sometimes defeat a shot by
position and energy, rather than every lock promising a kill.

The missile has finite steering, visible trail, launch audio, a swept proximity
fuse and solid-surface collision. It loses a destroyed target safely and never
hits a recycled actor ID after respawn. Damage is attributed to a weapon and
attacker. Cockpit warning frequency should reflect actual closing danger, not
simply remain at maximum volume for ten seconds.

After the standard missile works, add tradeoffs: a shorter-range agile missile,
a faster less agile interceptor shot, and a heavy strike store for objectives.
Do not ship variants as recoloured models with identical behaviour or as a
ladder of universally stronger consumables.

### Countermeasures and cover

| Defence | Proposed effect | Required limitation |
|---|---|---|
| Hard terrain/structure | Blocks sensor line of sight and physically stops weapons | Visibility and collision agree; no invisible cover |
| Cloud volume | After a short entry delay, inhibits new compatible acquisition | Does not erase an already flying missile or hide every HUD marker |
| Flare burst | Offers a compatible incoming seeker an alternate heat target | Timing, geometry and a break turn matter; radar-only weapons ignore it |
| Chaff/electronic tool | Later acquisition/radar disruption mechanic | Added only after a distinct radar weapon exists |
| Airbrake/energy turn | Changes closure and can force overshoot | Costs speed and can create a stall risk |

Initial flare experiment: two burst charges with regeneration and a short
activation window. Training must demonstrate both a successful timed defence
and a failed late/poorly positioned attempt. Avoid random outcomes that make
identical practice attempts inexplicable. Networked missile/decoy decisions
come from the server and are replayable in tests.

Gameplay cloud volumes derive from the same cluster data as their visible
puffs. A pilot should not be protected outside an apparently clear sky while
another unmarked region removes their lock. Visual quality may scale down;
the gameplay volume does not change with a client's graphics preset.

## 7. Damage, deaths and respawn

Keep a simple first model: one hull pool and optional short spawn protection.
Damage zones, disabled engines and asymmetric controls are later additions,
not prerequisites to making one match fun. Visible damage has stages: normal,
scorched, smoking, then destroyed. A destroyed aircraft cannot fire or score
new collisions. Fire/smoke, breakup pieces and audio explain the event.

Respawn after a short interval, initially the existing three seconds. Pick
from validated airborne spawns with safe terrain clearance, an unobstructed
forward route, sufficient distance from opponents and fair access to the
current objective. Brief protection ends on firing and is visible. Never spawn
someone inside a canyon wall, facing a cliff, or immediately inside a hostile
missile's proximity radius.

The death panel names the attacker, weapon/collision cause and assists, with a
short camera beat or optional skip. A cliff crash should say collision, not
claim a random enemy missile. Scores and kill credit are idempotent across
late packets and reconnects.

## 8. Match modes and scoring

### Training Range — first shippable slice

One human plus scripted/same-physics drones. Flight and combat exercises,
instant retry, no resource costs, configurable difficulty, proper results.
This is also the safe place to test an aircraft and preview a loadout.

### Relay Strike — initial 2v2 flagship

Teams fight over physically present relay installations exposed in a changing
sequence. Each site has a visible dish/mast, damage state, approach routes and
an objective marker. A site activates for a limited period; destroying it earns
team points and exposes the next site after a reset interval. Opposing fighters
can intercept the attacker or approach another angle. At least one site sits
near a channel, one on a ridge and one near open water, encouraging different
routes rather than circling a single stationary object for the entire match.

Starting round rules: four minutes, 15 team points, aircraft kill worth one,
relay destruction worth three. These are our tuning hypotheses. Only the
server can advance the active site, award points or end the round. At a tie,
use a clearly announced objective-based overtime; do not add an arbitrary
worldwide missile barrage.

Personal merit is separate from team victory points. Split a kill's merit
between recent damage contribution and the final blow so an assist matters.
Record objective damage/assists too. Cap farming opportunities: friendly fire,
protected targets and repeated damage healed by a scripted target do not
produce unlimited merit.

### Convoy Intercept — next substantial mode

Attackers try to stop a small group of original transport aircraft travelling
a visible route. Defenders escort them through two or three terrain corridors.
Destroying an engine or a transport has a real world effect; escape and failure
conditions are visible. Routes, health, airspeed and defence difficulty are
configured for the small lobby. A carrier landing or runway takeoff sequence
is not required for the first escort mode.

### Later modes

Free flight, time trial/ring challenges and plain team dogfight are useful
extras after combat is healthy. A dynamic campaign, automatic aircraft Arena,
large-scale base warfare and mixed BoatRace/JetFighter combat remain outside
the first release. The monorepo's separate-game design stays intact.

## 9. Bot pilots

Bots use the same flight profile and command structure as players. A state
machine starts with transit, acquire, pursue, attack pass, break away, defend,
avoid terrain and respawn. Predict terrain several seconds ahead, and test
pull-out clearance before committing to a dive. A new head-on pass is followed
by separation rather than endless immediate 180-degree jousting.

Difficulty changes reaction delay, aim error, situational awareness and
countermeasure timing. It does not increase thrust, ignore collision or give
unlimited flares. A bot can lose energy, miss, get obstructed and die. Record
its state and chosen commands in a debug view so failure can be diagnosed.

For escort targets, initially use a deterministic route autopilot with safe
terrain-aware waypoints. A slow transport is not a fighter AI with its speed
arbitrarily reduced. Each bot role needs a repeatable encounter and minimum
survival/avoidance criteria, not only an apparent ability to follow waypoints.

## 10. LAN and authority

Reuse the proven **pattern**, not an illegal cross-game module dependency.
JetFighter owns its rules and aircraft; MaritimeCore remains game-independent.
A generic shared combat component is extracted only when both projects truly
need it and both regression suites can be protected.

The owning client predicts its aircraft. Send timestamped position, velocity,
orientation, input/control state and sequence numbers within a jet-specific
speed/altitude envelope. Remote aircraft interpolate snapshots and handle
respawn/teleport explicitly. Do not reuse boat speed clamps, 2D bearings or a
sea-skimming missile constraint.

The server owns health, ammunition/heat eligibility, missile launch, target
validation, decoys, damage, objectives, scores and respawn. Client tracers and
hit previews are cosmetic until acknowledged. Optional bounded lag compensation
can use recent target poses; its time window must be capped and observable.
This is friendly-LAN validation, not a promise of public anti-cheat security.

Tests must include joins in progress, packet delay/loss, death while locking,
owner disconnect, host departure, respawn while a missile is alive and a shot
blocked on the server. LAN discovery advertises the game/protocol version so
boat and jet servers cannot be confused. No cloud team-code system is promised
without an actual relay/backend design.

## 11. Local progression, hangar and settings

Start with earned unlocks, cosmetic paint, training records and saved controls.
Aircraft roles have tradeoffs at equal overall power. In a casual LAN lobby,
normalise competitive upgrades by default or let the host choose a clearly
marked unrestricted setting. A paint scheme does not silently multiply damage.

The hangar is a real 3D viewing space with original aircraft, lighting, loadout
anchors, service props and a test-flight action. It shows actual profile values
and the selected equipment. Purchased/unlocked/equipped states are persistent
and have different UI treatment. Buttons must save and apply state; card art
alone is not a working garage.

Settings cover bindings, mouse and stick sensitivity, deadzones, inversion,
camera lag/shake, FOV limits, HUD scale, colour options, warning volume,
subtitles, graphics and reset-to-defaults. Presets retain the current integrated
GPU path. Unavailable features do not create nonfunctional settings sliders.

## 12. Completion gates

A gameplay feature passes only when a tester can perform it in the running
game, observe its world/UI/audio effect, and repeat its failure/success case.
The sequence for a weapon is **select → acquire/aim → fire → travel → hit/miss
→ damage → score/feedback → cool/reload**, including blocked and interrupted
cases. The sequence for a match is **choose → host/join → ready → countdown →
play objective → death/respawn → win/lose → results → replay/leave**.

Performance, readability and networking are acceptance criteria throughout.
The production roadmap assigns files, dependencies, test scenes and measurable
exit gates so this document can turn into implementation work instead of
remaining a list of attractive nouns.
