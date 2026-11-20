# Production roadmap

Status: 20 November 2026. This is a proposed dependency order, not a promise of
calendar delivery. Current development is a local flight prototype, not a
networked combat game. Keep BoatRace independently buildable throughout.

## Milestone 0 — Make the current foundation reproducible

Implemented in this pass: three original aircraft meshes, a stable airframe
roster, in-flight visual selection, deterministic source/export/import scripts,
three generated LODs per aircraft and licensed Rajdhani HUD font files. All
three airframes currently use the same flight model. Selection does not yet
constitute a hangar, unlock system, weapon loadout or distinct performance.

Required closeout: build the editor; run numerical and engine flight tests;
verify all three imported models in rendered views; inspect HUD text and
pause states; survey sea and islands at multiple altitudes; document fresh
checkout asset generation. Preserve original source files and license records.
A successful font load log does not prove the HUD rendered.

## Milestone 1 — A complete local combat training slice

The first gun/drone portion is implemented: local hull ownership, swept shots,
heat, impact/destruction feedback and restart. [Current encounter](../COMBAT_TRAINING.md).
Targeting, missiles, flares and manoeuvring AI remain next. A ring race can be
optional flight training later; it should not
stand between the current prototype and the user's combat-game objective.

Build in this order:

1. Introduce a team/damage interface, authoritative health component and hit
   event data, with clear death and reset ownership. First target is a visible
   stationary training drone with a hull bar.
2. Add a nose gun with timed shots, swept projectile collision, heat, tracers,
   impact effects and damage feedback. Simulation owns the hit; visual trails
   represent it. Verify terrain blocks fire and frame rate does not alter DPS.
3. Add target enumeration and selection, range/cone/line-of-sight eligibility,
   acquisition progress, lock loss and seeker state. A reticle alone is not a
   targeting system. Add a single standard missile before variants.
4. Give the missile an actual trajectory, turn-rate limit, finite lifetime,
   swept collision and explosion event. The player must be able to evade it;
   avoid instant guaranteed hits disguised by a flying mesh.
5. Add incoming warning and a finite flare inventory. Only compatible seekers
   react; selection uses documented probability/distance/angle rules and a
   deterministic seed for tests. A flare puff without seeker effects fails.
6. Add a manoeuvring drone using the same forces and rate limits as the player.
   It patrols, acquires, pursues, fires, evades and disengages. Start with one
   opponent and truthful perception rather than omniscient difficulty cheats.
7. Complete a training loop: objective prompts, target destruction, player
   death, respawn, success state and retry. Pause freezes simulation/effects.

Suggested source boundaries: JetCombatComponent, JetProjectile,
JetMissile, JetTargetingComponent, JetAIController and JetTrainingDirector
under Source/JetFighter. These names are proposals, not files already present.
Keep flight integration in JetPawn; route weapon commands through components.
Damage events should carry instigator, team, weapon, location and sequence ID
so later networking does not require rewriting every visual response.

Acceptance: a new player can destroy a drone with guns, lock and fire a missile,
see an incoming missile, use a flare, take damage, crash and retry without the
console. Include engine tests for obstruction, duplicate-hit prevention,
missile lifetime, target death during lock and inventory exhaustion. A short
scripted rendered combat tour must demonstrate those events in the world.

## Milestone 2 — Stonegate Strait, a first real combat map

Build one coherent coastal mountain channel before spreading effort across
five unfinished biomes. Use the art bible's Stonegate layout and the design
document's Relay Strike requirements together.

First produce a navigable greybox with ridges, multiple passes, broad central
engagement space, side exits, team approaches and objective sites. Instrument
route width, first-contact time, escape paths and safe respawn volumes.
Gameplay signs of a good map are repeated use of alternate routes and real
line-of-sight breaks, not the number of mountain meshes.

Then replace the coarse dome terrain with an authored height/route source,
chunked render LODs and collision sampling the corresponding visible surface.
Near-route rock meshes supply cliffs and overhangs where a heightfield cannot.
Build shoreline foam, rock/sand/vegetation transitions, runway buildings,
relay installations and a small number of distinctive landmarks. Every relay
must visibly change state when disabled or destroyed.

File boundaries: evolve JetTerrain into a terrain data source plus chunk
renderer; keep gameplay sampling separate from presentation LOD. Extend
JetScenery for placed landmarks and pooled effects. Add a water-body definition
and query used by both contact physics and the jet water presentation. Do not
change BoatRace's wave phase, boat contact or its world by accident.

Elevated lakes are a second step within this milestone: implement body polygon,
mean level, shoreline, independent wave scale and relative-height shading.
One elevated-water test basin proves the architecture before Azure Basin is
produced. A flat blue plane over terrain without valid contact queries fails.

Acceptance: fly each main route at intended speed without surprise invisible
walls; fire through clear gaps and confirm cliffs block weapons; crash at
shore, ridge and lake edge; inspect far water and terrain LODs from 6 km;
verify objectives remain legible on bright and shaded backgrounds. Repeat
performance sampling with the training opponent and effects active.

## Milestone 3 — Relay Strike as a complete local match

Implement team assignment, timed round state, phased neutral relay sites,
scoring, overtime, victory/defeat, scoreboard and replay. Neutral objectives
concentrate action while multiple approaches preserve manoeuvre choices.
The proposed 15-point/four-minute rules are ours and require playtesting.

Add enough bots for a 2v2 match, spawn selection based on enemy distance and
line of sight, temporary clearly signalled spawn protection and post-death
feedback explaining the cause. Define disconnect-independent local round
ownership now. Bot difficulty changes reaction and aim error, not impossible
turn rates, hidden health or unlimited missiles.

Acceptance: both teams can win; a destroyed target scores once; simultaneous
kills resolve consistently; a timeout and tie have explicit outcomes; respawn
cannot award repeated free points; replay fully resets objectives and effects.
The scoreboard must reflect the same events as the visible destroyed sites.

## Milestone 4 — Four-player LAN, with explicit authority

Do not advertise multiplayer because pawns merely appear on two screens.
Implement host/join UI and session lifecycle, team/player readiness and host
start. Replicate flight state with bounded prediction/interpolation, then
weapons, damage, objectives, scores, respawns and match transitions.

The server validates fire cadence, weapon inventory, damage and objective
transitions. Missile spawn IDs and shot IDs prevent duplicate effects and
hits. Clients may predict local presentation, but confirmed damage comes from
the server. Friendly LAN movement trust must be documented; this is not a
claim of competitive internet anti-cheat. Keep state snapshots versioned.

Test two processes first, then four actual peers. Include host quit, client
quit, late join policy, packet loss, latency, repeated restart and mismatched
content version. Under artificial latency, a client cannot create extra
missiles by retrying commands or score twice for one relay. The host's view
and all clients must agree on winner and remaining inventory.

Acceptance: four people can launch from fresh builds, join one lobby, play
three rounds and return to lobby without console commands or manual config
edits. Capture server/client logs and network/performance measurements.

## Milestone 5 — Distinct aircraft and a production hangar

After the combat loop works, tune Kite, Rook and Manta into meaningful roles.
Introduce data assets for mass/thrust/drag, rate limits, speed envelope,
hull, weapon mounts and camera offsets. Preserve a balanced starter aircraft;
role differences must offer tradeoffs, not a paid or unlocked strict upgrade.

Replace prototype materials with production UVs/atlases and authored surface
detail. Add attachment anchors, animated control surfaces, throttle-responsive
exhaust, removable stores, damage responses and two original liveries per
initial aircraft. Bring material slots from 12 toward 3–4 and validate reduced
LODs during actual combat, not just in a turntable.

Build a functional hangar: choose aircraft, inspect role and control scheme,
select permitted equipment/livery, preview the actual mesh and launch. Save
selection locally with schema/version and a safe fallback for removed items.
Do not display clickable upgrades or currencies whose effects do not exist.

Acceptance: the three profiles feel distinct in measured acceleration,
turning and endurance, while each can win the same combat scenario. Choice,
visual loadout and gameplay loadout agree in local and network matches.

## Milestone 6 — Expand world and mission variety

Add Azure Basin only after elevated-water queries work, Redwake after terrain
route/occlusion quality is established, and Frostline after snow/sky contrast
and visibility are tested. Sable Caldera and the three additional aircraft
are later content, not prerequisites for the first playable combat release.

Convoy Intercept follows Relay Strike: ships move on a valid navigable route,
convoy damage affects objective state, teams receive attack/defend roles and
round swapping produces fair results. A carrier can begin as a moving visual
objective; takeoff, landing, arrestor wires and deck collision are separate
large features and must not be implied by the presence of a carrier mesh.

Weather and cloud concealment follow reliable targeting. Each effect needs
explicit rules for acquisition, existing missiles, guns, radar and AI. Map
atmosphere cannot secretly alter only one participant's visibility.

## Effort and release gates

Provisional engineering/art effort in focused person-weeks, excluding engine
breakage, external procurement and prolonged multiplayer debugging:

| Work package | Rough effort | Main uncertainty |
|---|---:|---|
| Combat training slice | 2–4 | Missile/AI feel and collision correctness |
| First production terrain/water slice | 3–6 | Terrain tools, shore quality and collision |
| Local team match | 1–3 | Bot decisions and spawn fairness |
| Four-player LAN completion | 3–6 | Prediction, lifecycle and network defects |
| Three production aircraft/hangar | 4–8 art + 1–3 engineering | UV/material/animation quality |
| Each additional finished map | 2–5 | Landmark art and route iteration |

These are planning ranges for scoping, not dates or a measured throughput
forecast. Parallel work only helps after interfaces and acceptance examples
are clear. Keep the first release to three aircraft, one polished map, one
combat mode and reliable LAN; broaden content after that loop is enjoyable.

Every milestone closes with an executable path, a rendered evidence capture,
meaningful engine tests, source/license records, frame-time measurements and
an accurate README. A feature marked planned must not be counted as complete
because its menu label, mesh or document exists.
