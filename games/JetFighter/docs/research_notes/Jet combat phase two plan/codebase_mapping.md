# Codebase mapping: how Boat Race's systems carry over to a jet fighter combat phase

All sources are local files in `/home/emmanuel/Games/BoatRace` (paths relative to the repository root, `file:line`). Nothing was built or run; everything comes from reading the code at commit `1f6b35e` plus an uncommitted change to `Config/DefaultEngine.ini` (the low-spec block, 15 lines).

## 1. Pawn and physics: rigid body, forces, frame-rate independence, networking, damage, wreck recovery

### Takeaway
`ABoatPawn` is one 2,405-line class (`BoatPawn.cpp`) plus a 680-line header. It mixes four separable concerns: (a) the water-specific force model; (b) a generic owner-simulates/kinematic-copy network scheme; (c) generic server-authoritative health, wreck, shield, ammunition and gun; (d) cosmetics, audio and the garage loadout. Parts (b) and (c) can be lifted almost unchanged into a shared `AArmedVehiclePawn` base. Part (a) has to be replaced with an aerodynamic model. Part (d) is roughly half reusable.

### Cited Findings
**Class shape and rigid body**
- `ABoatPawn : public APawn`, `UCLASS(Config = Game)`. The root component is a `UBoxComponent` "Hull" that simulates physics. Engine linear damping is 0 and angular damping is 1.1, because "our own drag and buoyancy damping do the work" — [BoatPawn.h:70-71](Source/BoatRace/BoatPawn.h), [BoatPawn.cpp:74-88](Source/BoatRace/BoatPawn.cpp)
- Visual parts hang off a `VisualRoot` with no collision. The placeholder is built from engine cubes and cones until `SM_*` loads (`TryLoadCustomMesh`), and the camera is a `USpringArmComponent` that does not inherit pitch or roll (`bInheritPitch=false`, `bInheritRoll=false`) — [BoatPawn.cpp:90-163](Source/BoatRace/BoatPawn.cpp). Jets need the opposite: they must inherit pitch and roll, or use a chase camera that follows the velocity vector.
- `BeginPlay` derives the physics constants from config. Mass is overridden by `SetMassOverrideInKg`. The centre of mass is set low and aft. Four buoyancy probes sit at the hull corners. Heave damping comes from a spring model. The quadratic drag coefficient is solved so that full thrust tops out at `ReferenceTopSpeed`. Yaw inertia is computed as a box — [BoatPawn.cpp:166-235](Source/BoatRace/BoatPawn.cpp)
- `SimulatePhysics` runs in this order: NaN guard (reset to grid) → `ApplyBuoyancy` → `ApplyHydrodynamics` → `ApplyEngineAndRudder` → `ApplyWhirlpools` → `KeepInsideBay` → `ApplyWindAndShore` → self-righting torque above `SelfRightAngle` → "capsize recovery" teleport upright after 2.5 s beyond 100° — [BoatPawn.cpp:530-590](Source/BoatRace/BoatPawn.cpp)
- Buoyancy and damping read `OceanWaves::GetHeightAndSlope` and `GetVerticalSpeed` at each probe — [BoatPawn.cpp:592-635](Source/BoatRace/BoatPawn.cpp). Drag acts horizontally only: `ForwardFlat` has Z zeroed, and the drag is scaled by `WaterContact` and is zero when airborne ("Airborne: nothing to push against") — [BoatPawn.cpp:637-697](Source/BoatRace/BoatPawn.cpp)
- Steering is a yaw-rate control loop, not raw physics. The torque is `(DesiredYawRate − CurrentYawRate) · YawInertia · TurnResponse`, and heel is steered towards a target roll angle. Both are gated on water flow and propeller bite — [BoatPawn.cpp:699-781](Source/BoatRace/BoatPawn.cpp). This "target a rate, let the rigid body follow" pattern is exactly how arcade flight models usually work (pitch, roll and yaw rate targets). The pattern carries over even though the code does not.
- `KeepInsideBay` is a soft spring force at `BoatTrack::BoundsX/Y` with a 20 m margin — [BoatPawn.cpp:814-845](Source/BoatRace/BoatPawn.cpp)
- `MaxSpinDegreesPerSecond = 190` is applied through `SetPhysicsMaxAngularVelocityInDegrees` — [BoatPawn.cpp:42](Source/BoatRace/BoatPawn.cpp), [BoatPawn.cpp:283](Source/BoatRace/BoatPawn.cpp). Fighter roll rates commonly exceed this, so it would need to be configurable per vehicle.

**Frame-rate independence**
- There is no custom substep callback or async physics tick. A grep for `Substep|AsyncPhysics|CCD` in `Source/` finds only the torpedo sweep. Forces are added once per game `Tick` — [BoatPawn.cpp:311-361](Source/BoatRace/BoatPawn.cpp)
- Engine-level substepping is on: `bSubstepping=True`, `MaxSubstepDeltaTime=0.016667`, `MaxSubsteps=6`. The comment says this "keeps the buoyancy forces stable when the frame rate dips" — [Config/DefaultEngine.ini](Config/DefaultEngine.ini) (`[/Script/Engine.PhysicsSettings]`)
- Frame-rate independence is verified offline: `physics_check.py` re-implements the force model in Python and checks "behaves the same from 120 fps down to 20 fps" — [Scripts/physics_check.py:1-6](Scripts/physics_check.py), [Scripts/physics_check.py:56](Scripts/physics_check.py) (`sim(dt, …)`), [CLAUDE.md](CLAUDE.md) "Verification"

**Networking (owner simulates; everyone else holds a kinematic copy)**
- `bReplicates=true`, `bAlwaysRelevant=true`, `SetReplicatingMovement(false)`: "Movement travels in FBoatNetState instead" — [BoatPawn.cpp:62-64](Source/BoatRace/BoatPawn.cpp)
- `FBoatNetState` holds Location, Rotation, Velocity, Steer, Throttle, `TeleportCounter` (uint8) and `bBoosting` — [BoatPawn.h:22-53](Source/BoatRace/BoatPawn.h). It is replicated `COND_SkipOwner` — [BoatPawn.cpp:241-243](Source/BoatRace/BoatPawn.cpp)
- `UpdatePhysicsMode`: only `IsLocallyControlled()` simulates; every other copy is kinematic but "still blocks and shoves" — [BoatPawn.cpp:265-284](Source/BoatRace/BoatPawn.cpp)
- The owner sends `ServerSendMove` (Unreliable) every `NetSendInterval = 1/30` s, and the host publishes directly — [BoatPawn.cpp:39](Source/BoatRace/BoatPawn.cpp), [BoatPawn.cpp:849-878](Source/BoatRace/BoatPawn.cpp)
- The server discards stale teleport sequences and NaNs, then clamps the reported position to the bay bounds, **Z to −20 m…+200 m** and **velocity to 2.5 × `ReferenceTopSpeed`** (about 72 m/s for the default boat) — [BoatPawn.cpp:880-913](Source/BoatRace/BoatPawn.cpp). Both clamps would break a jet as written.
- Remote copies use dead reckoning at the last velocity, capped at `MaxExtrapolationTime = 0.3` s, then `VInterpTo(…, 12)` and `RInterpTo(…, 10)` — [BoatPawn.cpp:40](Source/BoatRace/BoatPawn.cpp), [BoatPawn.cpp:927-945](Source/BoatRace/BoatPawn.cpp)
- Server teleports go through `TeleportBoat` (bumps `TeleportCounter`) and the reliable RPC `ClientTeleport` — [BoatPawn.cpp:951-996](Source/BoatRace/BoatPawn.cpp)
- `GetRaceLocation()` returns the owner's actor location, otherwise `NetState.Location`. The server uses it for gates and damage — [BoatPawn.cpp:1538-1541](Source/BoatRace/BoatPawn.cpp)
- `NetServerMaxTickRate=60` and `MaxClientRate=100000` — [Config/DefaultEngine.ini](Config/DefaultEngine.ini) (`[/Script/OnlineSubsystemUtils.IpNetDriver]`)

**Damage, wreck and recovery**
- `ApplyDamage(Amount, Attacker, bSmallArms)` is server-only and respects `InvulnerableUntil`. It sends `ClientHitFeedback` (rate-limited to roughly 3 per second for small arms). At 0 HP it sets `bWrecked` and increments `Victim->Wrecks` and `Attacker->Hits` — [BoatPawn.cpp:1006-1044](Source/BoatRace/BoatPawn.cpp)
- `ApplyExplosionImpulse` always adds 150 cm of upward lift to the direction. It is applied locally when owned, otherwise through the reliable RPC `ClientImpulse` — [BoatPawn.cpp:1046-1073](Source/BoatRace/BoatPawn.cpp)
- `Revive(InvulnerableSeconds)` restores health, clears the gun jam and sets `InvulnerableUntil` on the server clock — [BoatPawn.cpp:1091-1107](Source/BoatRace/BoatPawn.cpp). The game mode's `UpdateWrecks` waits `WreckSeconds` (4) and then teleports to `GetGateResetTransform(NextGate-1)` or the grid — [BoatRaceGameMode.cpp:378-409](Source/BoatRace/BoatRaceGameMode.cpp), [Config/DefaultGame.ini](Config/DefaultGame.ini) (`WreckSeconds=4.0`, `RespawnInvulnerableSeconds=3.0`)
- `CanDrive()` gates on wrecked, countdown and finished — [BoatPawn.cpp:286-309](Source/BoatRace/BoatPawn.cpp)

**Tuning structure (Config/DefaultGame.ini)**
- Every handling and weapon number is a `UPROPERTY(Config)` on the class, grouped per class section (`[/Script/BoatRace.BoatPawn]`, `[/Script/BoatRace.BoatMissile]`, `[/Script/BoatRace.BoatTorpedo]`, `[/Script/BoatRace.BoatMine]`, `[/Script/BoatRace.WaterSurface]`, `[/Script/BoatRace.BoatRaceGameMode]`, …). The file is 180 lines with comments explaining each value. Its header says "all four players must use the same values" — [Config/DefaultGame.ini:11-14](Config/DefaultGame.ini), [BoatPawn.h:182-344](Source/BoatRace/BoatPawn.h)
- Garage classes apply *multipliers* on top of the config defaults through `ApplyStats`, which reads the CDO, so "Config/DefaultGame.ini still tunes every boat at once" — [BoatPawn.cpp:1672-1691](Source/BoatRace/BoatPawn.cpp)

### Inferences
- **Proposed shared base (`AArmedVehiclePawn`, or keep the `ABoatPawn` name and add a sibling):** `FBoatNetState`, plus `SendMoveToServer`/`ServerSendMove`/`OnRep_NetState`/`FollowReportedState`, `TeleportBoat`/`ClientTeleport`, `UpdatePhysicsMode`, `GetRaceLocation`, `ApplyDamage`/`ApplyExplosionImpulse`/`Revive`/`IsInvulnerable`/`GrantShield`/`Repair`, ammunition fields and their replication conditions, the gun's heat, rate-of-fire and trigger logic (`UpdateGun`, `ServerSetGun`, `HoldGun`), missile lock state (`LockTarget`, `LockProgress`), `ServerFireMissile`/`ServerDropFlares`, `ClientHitFeedback`/`Rumble`/camera shake, the `EmitPuff` pool and loadout plumbing. The server-side clamps in `ServerSendMove` need to become virtual or per-class limits (bounds, Z range, maximum speed).
- **Jet-specific:** the force model (lift/drag/thrust on a velocity-aligned frame, pitch/roll/yaw rate targets, stall, gravity), a ground and sea crash test, takeoff and landing or air-start, a camera that inherits roll, configurable angular-velocity limits, and a missing equivalent of "self-righting" (aircraft simply crash).
- The torpedo, mine, grenade, whirlpool, buoyancy and planing code is boat-only and stays in `ABoatPawn`.
- `physics_check.py` is a good template: a Python mirror of a flight model with tests for "reaches top speed / stall speed / sustained turn rate / same at 20–120 fps / recovers from inverted" would match project practice.

### Gaps
- How much of `UpdateEffects` (lines 2124–2337) and `BuildEffects` (1954–2095) is generic smoke and fire versus water spray was not read in detail.
- No measurements exist of how the Chaos solver behaves with large aerodynamic forces at 60 Hz substeps.

## 2. Weapons: how much transfers to air-to-air missiles, guns and flares in 3D

### Takeaway
Most of the weapon *architecture* transfers. That includes server-authoritative hits, the owner-side lock with server re-check, only `bGunFiring` replicated with cosmetic tracers drawn locally, flares as a server-checked decoy, and a pooled explosion actor. Nearly every geometric test, however, is deliberately 2D (`Size2D`, `GetSafeNormal2D`, `Dist2D`), clamped to sea level, and tuned to boat speeds and ranges of 150–300 m. The missile is slower than a jet would be.

### Cited Findings
**Missile lock**
- `UpdateMissileLock` (owner side) sorts candidate boats by 2D dot product with the flattened forward vector. It is "sticky" on the current target, and pressing T cycles. Progress fills over `LockSeconds` and plays the `MissileLock` tone on completion — [BoatPawn.cpp:1140-1185](Source/BoatRace/BoatPawn.cpp)
- `ABoatMissile::CanLock` requires 2D distance < `LockRange × Slack` **and** a 2D dot product > 0.7/Slack (roughly a ±45° cone in yaw only) **and** `HasClearShot`. The server re-checks with Slack 1.3 at `Launch` — [BoatWeapons.cpp:332-369](Source/BoatRace/BoatWeapons.cpp)
- Config values: `LockSeconds=1.0`, `LockRange=30000` (300 m), `Speed=6500` (65 m/s), `TurnRate=85` °/s, `Damage=45`, `BlastRadius=800` — [Config/DefaultGame.ini](Config/DefaultGame.ini) `[/Script/BoatRace.BoatMissile]`, [BoatWeapons.h](Source/BoatRace/BoatWeapons.h) `ABoatMissile` config properties

**Missile flight**
- The server flies the missile and replicates movement (`SetReplicatingMovement(true)`, replicated `Target`), with `InitialLifeSpan=9` — [BoatWeapons.cpp:249-284](Source/BoatRace/BoatWeapons.cpp)
- Guidance is pure pursuit: it turns toward `target + 120 cm` at a maximum `TurnRate·dt` using a lerp of the forward vectors. It is **clamped to ≥ 2 m above `OceanWaves`** ("Sea-skimming") — [BoatWeapons.cpp:419-441](Source/BoatRace/BoatWeapons.cpp)
- The hit test checks the closest point on the frame's movement segment within 450 cm, decoy within 600 cm, terrain via `BayTerrain::GetHeight`, or age > 7 s → `Explode` → `BoatBlast::Detonate` — [BoatWeapons.cpp:443-469](Source/BoatRace/BoatWeapons.cpp). That segment test already handles fast closure, which matters for air-to-air.
- Flares: when a target within 250 m is `IsFlaring()`, the missile is decoyed to a point 15 m behind and 6 m above the target and its `Target` is cleared, which also clears the victim's warning — [BoatWeapons.cpp:405-417](Source/BoatRace/BoatWeapons.cpp). `ServerDropFlares` sets `FlaringUntil = now + 3` and spawns `AFlareBurst` — [BoatPawn.cpp:1216-1231](Source/BoatRace/BoatPawn.cpp)

**Gun**
- The server fires at a fixed rate by the clock (`while Now >= NextGunRound`). Heat fills over `GunOverheatSeconds`, the gun jams at 1.0 and stays jammed until heat drops below 0.4. Only `bGunFiring` (all clients) and `GunHeatNet` as a uint8 (owner only) are replicated — [BoatPawn.cpp:1308-1333](Source/BoatRace/BoatPawn.cpp), [BoatPawn.cpp:249-252](Source/BoatRace/BoatPawn.cpp)
- `FindGunTarget` is entirely 2D: nearest boat within `GunRange` (150 m) whose 2D bearing is within `GunAimAssistDegrees` (6°) + atan(160 cm / d), plus `HasClearShot` — [BoatPawn.cpp:1271-1306](Source/BoatRace/BoatPawn.cpp)
- Hits are probabilistic, not ballistic: `Chance = GunAccuracy · DriverAccuracy · lerp(1, 0.5, d/range)` and `FRand() > Chance` → miss — [BoatPawn.cpp:1335-1362](Source/BoatRace/BoatPawn.cpp)
- Tracers are cosmetic and per machine: misses land on `OceanWaves::GetHeight`, and `FDeckGunEffects` pools the meshes. `TracerSpeed = 42000` cm/s — [BoatPawn.cpp:1364-1416](Source/BoatRace/BoatPawn.cpp), [BoatWeapons.cpp:70-71](Source/BoatRace/BoatWeapons.cpp), [BoatWeapons.h](Source/BoatRace/BoatWeapons.h) (`FDeckGunEffects`)
- `BoatCombat::HasClearShot` does a visibility line trace **plus** samples `BayTerrain::GetHeight` every 5 m along the line, because "the procedural coast also has distant sections without collision" — [BoatWeapons.cpp:32-62](Source/BoatRace/BoatWeapons.cpp)

**Projectiles and blasts**
- `BoatBlast::Detonate` spawns a replicated `ABoatExplosion`. For every `ABoatPawn` within the radius it applies a linear-falloff impulse and damage (no self-damage), calls `ReportWreck` for the kill feed, and pushes barrels — [BoatProjectile.cpp:66-130](Source/BoatRace/BoatProjectile.cpp). The distance check is already 3D (`FVector::Dist`).
- The torpedo is deterministic from launch on every machine (`LaunchLocation + dir·speed·elapsed`, Z on the wave). Only the server sweeps (a 45 cm sphere, `ECC_Pawn`) — [BoatProjectile.cpp:483-536](Source/BoatRace/BoatProjectile.cpp). "Anything derivable from the shared clock is never replicated" — [CLAUDE.md](CLAUDE.md)
- Mines and `AWeaponCrate` are sea-surface objects — [BoatProjectile.h:164-276](Source/BoatRace/BoatProjectile.h)
- Combat tests in isolated worlds: `SolidCover`, `MissileCrossesTarget`, `FlaresAndLostTarget`, `DamageAndProtection`, `HomingAndUnmatchedLaunch` — [Tests/BoatCombatTests.cpp:52-140](Source/BoatRace/Tests/BoatCombatTests.cpp). There is also a two-process `net_combat_test.sh` that "holds the boats stationary above the scenery, uses slower missiles" — [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md) "Controlled network combat pass"

### Inferences
- **Reuse as is:** the `BoatBlast::Detonate` damage, impulse and kill-feed path (already 3D); the `ABoatExplosion` effect; the `AFlareBurst` actor and the decoy idea; the gun heat, jam and clock-based rate of fire; the replication split (`bGunFiring` only, cosmetics local); the segment-closest-point proximity fuse; the owner lock with server `Slack` re-check; and the `HasClearShot` structure (trace plus analytic terrain).
- **Generalise:** make `CanLock`, `FindGunTarget` and the lock sort use 3D cones (`FVector` dot product against the full forward vector, not `GetSafeNormal2D`). Make the missile's sea-skim clamp a per-type option. Change the missile from pure pursuit to proportional navigation with a speed above the target's (65 m/s against a jet doing roughly 200+ m/s will never catch it). Add config for lock cone angle, range and seeker type. Flares could become probabilistic (aspect- or seeker-dependent).
- **New:** probably ballistic or lead-computed gun rounds (the probabilistic 2D cone will not feel right in a dogfight), a lead indicator on the HUD, radar/IR seeker distinctions, chaff, a missile that can be outturned (limit on g, not degrees per second), and "missile warning" bearings in 3D.
- `HasClearShot`'s 5 m terrain sampling scales linearly with distance: at a 2–3 km gun or lock range it is 400–600 analytic `GetHeight` calls per check. Combined with 10 rounds per second per shooter and per-frame lock checks, a coarser step or a heightfield ray-march would be needed.
- The torpedo, mine, grenade and crate actors are boat-only. A jet phase might keep crates as airborne pickups (rings or balloons), but the crate actor floats on `OceanWaves`.

### Gaps
- `ABoatExplosion` effect sizing and its performance with many air bursts were not measured.
- No information was found on how the effect pools (`FPuff`, 16 trail puffs per missile) would cope with longer, faster missile trails.

## 3. World: terrain, sea, environment, weather and course scale compared with what jets need

### Takeaway
The playable world is about **1.44 km × 1.28 km** of water (`BoundsX/Y = ±720 m/±640 m`). Land meshes extend to about 5.2 km square and a flat water plane reaches 12 km. The default boat's top speed is 29 m/s (104 km/h). A jet at even 200–250 m/s would cross the whole playable area in 6–7 s. The three circuits could serve as **low-level set pieces** (canyon run, harbour strafing, bay), but a jet phase needs either a much larger analytic terrain or an outer "airspace" ring around the existing bays. The analytic, deterministic world design (one height function, shared clock, nothing replicated) is the most valuable thing to keep.

### Cited Findings
- `BoatTrack::BoundsX = 72000.f`, `BoundsY = 64000.f` (cm), `WaterZ = 0`, `BoatRideHeight = 45` — [BoatTrack.h:81-88](Source/BoatRace/BoatTrack.h)
- Terrain: an inner grid of ±840 m at 10 m cells with collision, and an outer grid of ±2,580 m at 60 m cells **without collision** (`BuildSection(1, Outer, false)`) — [BayTerrain.cpp:22-25](Source/BoatRace/BayTerrain.cpp), [BayTerrain.cpp:262-275](Source/BoatRace/BayTerrain.cpp)
- One height function per circuit. Bay: beaches, cliffs, hills and "a ring of mountains" up to roughly 240 m × 0.18 weighting at R 950–1,900 m. Harbour: flat reclaimed ground. Canyon: sandstone walls of 65–87 m with benches and a rim — [BayTerrain.cpp:102-146](Source/BoatRace/BayTerrain.cpp). The `BayTerrain::GetHeight/GetHeightMetres/GetClearanceMetres/GetSeaward` API is pure functions — [BayTerrain.h:20-30](Source/BoatRace/BayTerrain.h)
- Lap lengths: Bay 3.0 km, Harbour 2.48 km, Canyon 3.28 km. Gates are 90 m wide (`GateHalfWidthMetres = 45.0`) — [README.md](README.md) "The circuits", [BoatTrackData.h:14](Source/BoatRace/BoatTrackData.h)
- Gates are 2D line segments (`FBoatGate` holds `FVector2D` Center, Inner, Outer and Forward); `CrossesGate(From2D, To2D)` — [BoatTrack.h:1-15](Source/BoatRace/BoatTrack.h), [BoatRaceGameMode.cpp:451-490](Source/BoatRace/BoatRaceGameMode.cpp)
- `BoatTrackData.h` is generated by `Scripts/course_layout.py`. It checks that gate lines never cross, that no obstacle blocks a gate, that the course is water and that driving the line scores gates in order — [CLAUDE.md](CLAUDE.md) "Verification"
- `OceanWaves`: six sine components, vertical displacement only, a pure function of position and time — [CLAUDE.md](CLAUDE.md) "Architecture"; wave table mirrored in [Scripts/physics_check.py:28-36](Scripts/physics_check.py)
- `WaterSurface`: a procedural grid that follows the camera's XY in whole cells (`GetPlayerViewPoint` → snapped centre), with `GridCells=40`, `CellSize=700` (280 m across in the low preset, 420 m at 60 cells), plus a flat "FarWater" plane at 12 km scale — [WaterSurface.cpp:31-36](Source/BoatRace/WaterSurface.cpp), [WaterSurface.cpp:140-160](Source/BoatRace/WaterSurface.cpp), [Config/DefaultGame.ini:47-53](Config/DefaultGame.ini)
- `RaceEnvironment` owns Sun, Moon, SkyLight, SkyAtmosphere, VolumetricCloud and ExponentialHeightFog components, plus pylons, ramps, whirlpools, barges and the start gantry — [RaceEnvironment.h:117-137](Source/BoatRace/RaceEnvironment.h). `FWeatherLook` holds sun angle and intensity, fog density, wave multiplier, wind, gustiness, overcast, cumulus and snow; `GetWind` and `GetLightningAge` are derived from the server clock — [RaceWeather.h:30-76](Source/BoatRace/RaceWeather.h)
- The low preset turns off Lumen (`r.DynamicGlobalIlluminationMethod=0`), uses SSR, turns off distance fields, virtual shadow maps, motion blur, `r.VolumetricCloud` and `r.VolumetricFog`, sets `r.ScreenPercentage=70` and `r.ViewDistanceScale=0.7`, and the water grid becomes 40 cells updated at 30 Hz — [Scripts/low_spec.sh](Scripts/low_spec.sh), [Config/DefaultEngine.ini](Config/DefaultEngine.ini) "BOAT RACE LOW SPEC"
- Target hardware: Intel Meteor Lake integrated graphics, 14 GB shared memory, 60–80 fps for v3. Recent tour-stop readings were 43–77 fps at 1280 × 720 — [CLAUDE.md](CLAUDE.md), [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md) "Verification progress"
- Scenery identity is a design rule: "Do not make the courses visually identical" — [CLAUDE.md](CLAUDE.md)

### Inferences
- **Scale arithmetic** (derived from the figures above): a 250 m/s jet covers the 1,440 m playable width in about 5.8 s and the 5,160 m terrain mesh in about 21 s. A 3g turn at 250 m/s has a radius of about 2.1 km (v²/(n·g)), which is larger than the entire boat bay. Jets either need much slower arcade speeds (around 80–120 m/s gives a turn radius of roughly 200–500 m at 3–6g), a far larger world, or both.
- The analytic height function makes larger terrain cheap to *query* (missiles, AI, ground collision) at any size. The *rendered* mesh is the cost. A 20–40 km world would need a LOD or clipmap terrain mesh (the existing WaterSurface "grid follows the camera in whole cells" technique could be reused for land), plus fog or `ViewDistanceScale` to hide the edge.
- The 12 km far-water plane and the camera-following water grid (280–420 m) will look flat or tiled from altitude. A jet camera at 1–3 km altitude would see the grid's edge. The grid would need to scale its cell size with camera height.
- Volumetric clouds are **disabled in the low preset**, so cloud-based gameplay (hiding in cloud) would not work on the target laptop. A cheaper cloud representation would be new work.
- Canyon Run (walls of 65–87 m plus benches and a rim) is the natural jet set piece (a "trench run"). Harbour's cranes and container stacks suit strafing targets. The 90 m gate width is a plausible air-race gate width if gates gain a Z band.
- `KeepInsideBay`, `ServerSendMove`'s bounds clamp and the torpedo's out-of-bounds check all key off `BoatTrack::BoundsX/Y`. Jets need their own airspace bounds (including a ceiling) with a "return to combat area" behaviour rather than a spring.

### Gaps
- No measured frame-rate data exists for a view from altitude, where far more terrain and trees are visible. Instanced tree counts in `ScatterPlants` were not counted.
- No sky or horizon testing has been done from above roughly 200 m (the server clamps Z to 200 m, so nothing has ever been flown there).

## 4. Game flow, HUD, menus, LAN, progression, AI, audio, models and tests

### Takeaway
The game flow is race-shaped (Lobby → Countdown → Racing → Results, gates, laps, positions) and mostly reusable. A jet *air race* would fit almost unchanged; *dogfight/deathmatch* scoring needs a new mode branch. HUD, menus, LAN discovery, prefs, the coin economy, the audio synthesiser and the Blender pipeline are all code-driven and extensible by adding data. The rival AI is a 2D waypoint follower and would need rewriting for 3D air combat.

### Cited Findings
**Game mode and state**
- `ABoatRaceGameMode` sets `DefaultPawnClass = ABoatPawn`, plus the controller, player state, game state and HUD classes — [BoatRaceGameMode.cpp:18-27](Source/BoatRace/BoatRaceGameMode.cpp)
- Config: `TotalLaps`, `CountdownSeconds`, `ResultsSeconds`, `FinishGraceSeconds`, `WreckSeconds`, `RespawnInvulnerableSeconds`, `bFillWithRivals`, `RivalSkill` — [BoatRaceGameMode.h:46-79](Source/BoatRace/BoatRaceGameMode.h)
- Functions: `SpawnRivals`/`RemoveRivals`, `StartCountdown`/`StartRacing`/`UpdateRacing`/`UpdateWrecks`/`EndRace` and `UpdateElimination` (an elimination mode already exists) — [BoatRaceGameMode.h:82-98](Source/BoatRace/BoatRaceGameMode.h), [BoatRaceGameMode.cpp:378-596](Source/BoatRace/BoatRaceGameMode.cpp)
- `ERaceState { Lobby, Countdown, Racing, Results }`. The replicated game state holds `RaceStartTime`, `TotalLaps`, `bElimination`, `bTimeTrial`, `bReverse`, `CircuitIndex` and the weather byte. The kill feed is a multicast (`MulticastWreck`) — [BoatRaceGameState.h:9-121](Source/BoatRace/BoatRaceGameState.h)
- `GetServerNow()` returns `static_cast<float>(GetServerWorldTimeSeconds())` — [BoatRaceGameState.cpp:96-99](Source/BoatRace/BoatRaceGameState.cpp)
- Player state fields: `SlotIndex`, loadout, `LapsCompleted`, `NextGate`, `GatesPassed`, `bFinished`, `bEliminated`, `FinishTime`, `FinishPosition`, `BestLapTime`, `Hits`, `Wrecks` — [BoatRacePlayerState.h:21-71](Source/BoatRace/BoatRacePlayerState.h). `Hits` and `Wrecks` are already a kill/death tally.

**HUD**
- Canvas-only, with functions `DrawRaceHud`, `DrawSpeed`, `DrawStatus`, `DrawMiniMap`, `DrawKillFeed`, `DrawGateMarker` (world-space waypoint), `DrawHeadingStrip` (compass tape), `DrawNameTags`, `DrawDamageEdge`, menus, garage and score card — [BoatRaceHUD.h:52-103](Source/BoatRace/BoatRaceHUD.h)
- The missile-lock brackets project the target to the screen, shrink with `LockProgress`, turn red when locked and show a 2D distance in metres — [BoatRaceHUD.cpp:1170-1200](Source/BoatRace/BoatRaceHUD.cpp)

**Controller, input and menus**
- Menus are data: `BuildMenuItems` uses an `Add(Label, EMenuAction, Hint, Value, …)` lambda per `EMenuScreen` (Main lists Solo, Time trial, Garage, Leaderboard, Host, Join found, Join by address, Weather, Mode, Circuit, Direction, Laps, Rivals, Settings, Controls, Quit) — [BoatRacePlayerController.cpp:359-430](Source/BoatRace/BoatRacePlayerController.cpp), [BoatRacePlayerController.h:9-130](Source/BoatRace/BoatRacePlayerController.h)
- Input is raw polling (`IsInputKeyDown(EKeys::W)`, `GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis)`, …) inside `ABoatPawn::ReadInput`. There is no Enhanced Input or `BindAxis` anywhere in the source — [BoatPawn.cpp:364-470](Source/BoatRace/BoatPawn.cpp) (grep for `EnhancedInput|BindAxis|BindAction` returned nothing)

**LAN**
- `ULanDiscovery` is a game instance subsystem that broadcasts on UDP 7778 with a versioned signature, port, player count, `MaxPlayers` and a racing flag; it defaults the join port to 7777 — [LanDiscovery.cpp:20](Source/BoatRace/LanDiscovery.cpp), [LanDiscovery.cpp:137-189](Source/BoatRace/LanDiscovery.cpp)

**Garage and progression**
- `FBoatLoadout { Boat, Paint, Flame, Levels[4] }` is replicated on the player state. `FBoatClass` holds name, model, blurb, length, speed, thrust, turn, armour, price and gun position. `EUpgrade { Engine, Handling, Armour, Nitro }`. The `BoatGarage` namespace has prices, `StartingCoins = 500`, `CoinValue = 2`, `CoinsForPlace` and `RivalLoadout` — [BoatGarage.h](Source/BoatRace/BoatGarage.h). Four classes: Interceptor, Bullet, Hornet, Bulldog — [BoatGarage.cpp:11-17](Source/BoatRace/BoatGarage.cpp)
- `UBoatPrefs` (Config = Game, the user's Game.ini): best laps, `Coins`, `OwnedBoats/Paints/Flames` bitmasks, a `Leaderboard` string array, `RecordRace`, `Persist`; "test runs never write" — [BoatPrefs.h:22-143](Source/BoatRace/BoatPrefs.h), [CLAUDE.md](CLAUDE.md)

**AI**
- `ABoatRivalController : AAIController`. `RacingTarget(World, Boat, NextGate, Speed, LaneMetres)` returns a `FVector2D`; avoidance is 2D circles around rocks, whirlpools, barges and logs; shallows come from `BayTerrain::GetHeight`; weapons are `RequestSpecial` and `RequestFire`; driving is `SetDriverInput(Throttle, Steer)` — [BoatRivalController.cpp:23-234](Source/BoatRace/BoatRivalController.cpp), [BoatRivalController.h:20-40](Source/BoatRace/BoatRivalController.h)

**Audio**
- Every sound is synthesised at startup into `USoundWaveProcedural` (`UBoatSynthWave`). `EBoatSound` has 31 entries (Engine "outboard at 60 Hz firing", WaterRush, Explosion, MissileLaunch, MissileLock, Flare, GunFire, GunHit, music, weather beds, …). There are `PlayAt`/`Play2D`/`MakeLoop` APIs and a shared `FarAttenuation` — [BoatAudio.h:13-165](Source/BoatRace/BoatAudio.h), [BoatAudio.cpp:927-931](Source/BoatRace/BoatAudio.cpp)

**Models**
- `Scripts/make_models.py` (scenery) and `Scripts/make_boats.py` (boats) build GLBs in Blender: subdivision hulls via `smooth_hull`, `join_and_export`, procedural liveries at 2048 × 512, and helpers such as `deck_gun`, `outboard`, `person` and `windscreen`. The rules are metres, +X forward, origin at the waterline and materials named `Part_RRGGBB` — [Scripts/make_boats.py:19-871](Scripts/make_boats.py), [Scripts/make_models.sh](Scripts/make_models.sh), [CLAUDE.md](CLAUDE.md). `BoatArt` maps slot names to master materials (`M_Paint`, `M_Livery`, `M_Surface`, `M_Glass`, `M_Foam`) — [CLAUDE.md](CLAUDE.md) "Architecture"

**Tests and scripts**
- `physics_check.py` (a Python mirror of the rigid body, `FAILURES: 0`), `course_layout.py`, `smoke_test.sh` (photo tour, fps floor), `net_test.sh` (headless host plus client scoring gates), `combat_test.sh` (5 automation tests), `net_combat_test.sh`, `frontend_test.sh` (2 automation tests), `menu_test.sh` and `environment_test.sh` — [CLAUDE.md](CLAUDE.md) "Verification", [Tests/BoatFrontendTests.cpp:10-43](Source/BoatRace/Tests/BoatFrontendTests.cpp)
- Module dependencies: Core, CoreUObject, Engine, InputCore, Slate, SlateCore, AIModule, ProceduralMeshComponent, Sockets, Networking — [Source/BoatRace/BoatRace.Build.cs](Source/BoatRace/BoatRace.Build.cs). "No new engine plugins without good reason" — [CLAUDE.md](CLAUDE.md)

### Inferences
- **Reuse as is:** LAN discovery (bump the signature version to stop cross-joining), `BoatPrefs` storage (add jet bitmasks and records), the coin economy, the menu framework (add a top-level "Boats / Jets" or "Phase" choice and a "Mode: Dogfight"), the kill feed, `Hits` and `Wrecks`, the countdown and start lights, the results screen and score card, weather selection, `AFinishReplay`/ghost (`BoatGhost` records transforms at 15 Hz and 10 Hz), the audio synth framework and photo mode.
- **Generalise:** gates need a 3D form (ring or box with a Z band) for air races; `CrossesGate` is 2D. HUD functions take an `ABoatPawn*`, so they should take the base class. Speed, heading and minimap are reusable; jets add altitude, pitch ladder, horizon, g, AoA/stall warning, a lead reticle and 3D missile warning. `FBoatClass` and `FBoatStats` need a jet equivalent (turn → roll/pitch rate, armour, missile capacity). `ReadInput` should probably move to Enhanced Input or at least a per-pawn virtual, since jets need pitch, roll, yaw and throttle axes with mouse or stick.
- **New:** a dogfight/deathmatch game-mode branch (score limit or time limit, respawn in the air, no gates), an air-combat AI (3D pursuit, energy, evasion, terrain avoidance using `BayTerrain::GetHeight`), synthesised jet-engine, afterburner, wind-rush and stall-warning sounds, aircraft models in a new `make_jets.py` (the hull lofting helpers do not apply, but the material naming and export do), and a flight-model Python check.

### Gaps
- `BoatShowroom`, `BoatCoins` and `BoatGhost` internals were not read beyond their headers and CLAUDE.md descriptions.
- Whether `menu_test.sh` and `frontend_test.sh` hard-code counts of menu items, which adding a phase selector would break, was not checked.

## 5. Existing mentions of aircraft, planes, flying or "phase 2"

### Takeaway
None. A case-insensitive search of source, scripts, configs and docs found no aircraft, plane, fighter or "phase 2" references. The only "jet" hits are a jet-ski model and water-jet effects, and "fly" appears only in photo mode and comments.

### Cited Findings
- `jet_ski()` builds a JetSki boat model used as leisure traffic — [Scripts/make_boats.py:765-783](Scripts/make_boats.py), [RaceEnvironment.cpp:1471-1473](Source/BoatRace/RaceEnvironment.cpp)
- "Water jet" explosion piece kinds — [BoatProjectile.cpp:203](Source/BoatRace/BoatProjectile.cpp), [BoatProjectile.cpp:363](Source/BoatRace/BoatProjectile.cpp)
- Photo mode "WASD fly" is a free camera that already handles 3D movement and the mouse — [BoatRaceHUD.cpp:288](Source/BoatRace/BoatRaceHUD.cpp), [README.md](README.md) controls table
- The future-work lists name a second circuit, crowd, spectators, Windows packaging and anti-cheat; flight is never mentioned — [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md) "Later content", [CLAUDE.md](CLAUDE.md) "Known gaps"
- The `research_notes/Jet combat phase two plan/` directory existed only as an empty folder before these notes.

### Inferences
- Phase two is greenfield in design terms. Nothing in the docs constrains it, but the CLAUDE.md design rules (one analytic world function, nothing derivable replicated, owner simulates, models from Blender scripts, tuning in config) should be treated as binding.

### Gaps
- `RACING_RESEARCH.md` and `ONBOARDING.md` were only covered by the grep and not read in full.

## 6. Risks: float precision, high-speed collision, network update rates, naming

### Takeaway
The main technical risks are hard-coded boat-scale clamps in the network path, a float server clock, tunnelling through thin terrain at jet speeds (no CCD, and the outer terrain has no collision at all), 30 Hz updates interpolated with a lagging `VInterpTo`, and pervasive `Boat*`/`ABoatPawn` coupling (228 references to `ABoatPawn` in the source, 8 `TActorIterator<ABoatPawn>` loops).

### Cited Findings
- **Server clamps:** Z is limited to [−20 m, 200 m] and speed to 2.5 × `ReferenceTopSpeed` in `ServerSendMove` — [BoatPawn.cpp:893-900](Source/BoatRace/BoatPawn.cpp)
- **Clock:** `GetServerNow()` is a `float` cast of a double server time — [BoatRaceGameState.cpp:96-99](Source/BoatRace/BoatRaceGameState.cpp). Waves, barges, wind, lightning, torpedoes and invulnerability windows all use it — [BoatPawn.cpp:257-263](Source/BoatRace/BoatPawn.cpp), [BoatProjectile.cpp:494-505](Source/BoatRace/BoatProjectile.cpp), [RaceWeather.h:70-76](Source/BoatRace/RaceWeather.h)
- **Positions:** the gameplay code mixes `FVector` (double in UE5) with many `static_cast<float>` distance casts, for example `FVector::Dist2D` in the gun and missile code — [BoatPawn.cpp:1283-1287](Source/BoatRace/BoatPawn.cpp), [BoatWeapons.cpp:339](Source/BoatRace/BoatWeapons.cpp)
- **Collision:** the hull is a single box with `QueryAndPhysics`. No CCD is enabled anywhere (grep for `CCD|bUseCCD` found nothing). Engine substeps are 16.7 ms. The outer terrain section is built with `bCollision=false` — [BoatPawn.cpp:74-88](Source/BoatRace/BoatPawn.cpp), [BayTerrain.cpp:269-275](Source/BoatRace/BayTerrain.cpp), [Config/DefaultEngine.ini](Config/DefaultEngine.ini)
- **Net rates:** owner send rate 30 Hz, extrapolation cap 0.3 s, `VInterpTo` speed 12, server tick ≤ 60 Hz. Missiles use the engine's replicated movement — [BoatPawn.cpp:39-40](Source/BoatRace/BoatPawn.cpp), [BoatPawn.cpp:927-945](Source/BoatRace/BoatPawn.cpp), [BoatWeapons.cpp:252-253](Source/BoatRace/BoatWeapons.cpp), [Config/DefaultEngine.ini](Config/DefaultEngine.ini)
- **Trust:** "Each player's own boat position is trusted — fine for friends, not for strangers" — [CLAUDE.md](CLAUDE.md) "Known gaps" #5
- **Naming:** `ABoatPawn` appears in 15 source files (97 times in `BoatPawn.cpp`; 6–11 times each in HUD, game mode, projectiles, weapons and rival). Every module is `Boat*`, and the module itself is named `BoatRace`. CLAUDE.md notes a past unity-build name clash fix — [Source/BoatRace](Source/BoatRace) (grep counts), commit `1f6b35e` message "rename unity-build clashes"
- **Multiplayer maturity:** "Multiplayer checked on one machine only (`net_test.sh`); not yet across two machines on Wi-Fi" — [CLAUDE.md](CLAUDE.md) "Known gaps" #2

### Inferences
- **Float clock:** a float holds about 7 significant digits. After about 4.6 hours of server uptime (16,384 s) the step is about 2 ms, and after about 18 hours about 8 ms. That is harmless for waves but matters for anything integrating jet positions from the clock, such as deterministic projectiles at 300–1,000 m/s, where 8 ms is 2.4–8 m of position error. Switching `GetServerNow` to `double` (or race-relative time) is cheap and should come first.
- **World size:** UE5 large-world coordinates make positions doubles, so a world of 20–50 km is fine for transforms. Chaos and render precision are comfortable well inside ±20 km. The float casts are fine for distances but should not be used to rebuild positions.
- **Tunnelling:** at 250 m/s a body moves about 4.2 m per 16.7 ms substep. That is less than a 7 m box but more than thin features (crane booms, rock spires, the canyon arch). The outer terrain has no collision at all. Recommended approach: an analytic ground test against `BayTerrain::GetHeight` and `OceanWaves` every substep (as the missile does with `bIntoLand`), a swept test for obstacles, and CCD on the jet body.
- **Interpolation:** at 30 Hz a 250 m/s jet moves about 8.3 m between updates. The 0.3 s cap allows 75 m of extrapolation. `VInterpTo` at speed 12 lags visibly at those speeds. A Hermite or velocity-matched interpolation (or a short fixed delay with buffered states) is advisable, and the send rate may need to rise to 45–60 Hz (still small: about 4 players × ~60 B × 60 Hz).
- **Lag compensation:** server-side hit decisions use `GetRaceLocation()`, which is the last reported position (up to about 33 ms plus latency old). For boats that is centimetres to metres; for a crossing jet it is tens of metres. Lock and gun checks need generous slack or rewind.
- **Naming strategy:** a full rename (`Boat*` → `Vehicle*` or `Race*`) touches every file and the uproject module name. A cheaper path is to introduce `AArmedVehiclePawn` (base) with `ABoatPawn` and `AJetPawn` deriving from it, switch iterators, HUD and blast code to the base type, and leave file names alone. Unity-build clashes are a known hazard, so new helpers need unique names.

### Gaps
- No measurements of network bandwidth or latency exist in the repository. `net_test.sh` checks only that a client joins and scores gates.
- Engine-level CCD performance on the Intel integrated GPU/CPU was not measured, and is not knowable without running it.
