# Boat Race: combat and visual improvement plan

Updated 16 November 2026 after inspecting the working files and the supplied session history.

## Current position

The project has four-player racing, computer rivals, a garage, weather,
weapons, wreck recovery and LAN networking. The working tree contains a large
unfinished set of changes; preserve these while completing the work.

Already present in source: a deck-mounted machine gun with heat, server-side
damage and local tracer effects; missile target acquisition and lock cues;
incoming-missile warnings and flares; brighter scenery, extra paints, sharper
render settings and larger generated liveries. These are implementation
findings, not confirmation that the current game builds or plays correctly.

The supplied session reports earlier successful combat builds and screenshots,
but ends immediately after further visual edits. Rebuild and verify the final
combined version before calling those changes complete.

## 1. Finish and verify the interrupted work

- Review the existing changes for incomplete code and mismatched model names.
- Compile with `Scripts/build.sh`.
- Regenerate changed Blender models and liveries, then import them. Check that
  blossom/autumn trees, bunting and deck guns actually appear in the game.
- Run `Scripts/smoke_test.sh` and inspect its screenshots, including combat,
  the garage, bright weather and dark weather.

Done when: the current source builds, the changed art loads without fallback
  shapes, the HUD fits on screen and the tour completes without crashes.

## 2. Make boat combat satisfying

Keep the patrol-boat inspiration: a visible forward deck gun, continuous fire
while holding CTRL, muzzle flashes, tracers, distinct impact sounds and clear
hit feedback. Hull damage must lead to a visible wreck and timed recovery.

- Tune damage, aim assistance, range, heat and cooling through configuration.
  Measure time to wreck each boat class before choosing final numbers.
- Check that terrain and obstacles block shots, shield and respawn protection
  work, and remote players see firing stop when the trigger is released.
- Tune rival firing bursts and opening-race delays so the start stays playable.
- Confirm keyboard/gamepad controls and document them in the README.

Done when: a player can deliberately wreck an opponent, understand incoming
damage and recover without repeated unavoidable destruction.

## 3. Finish targeted missiles

- Hold the sights over a valid opponent to acquire a lock; display acquisition
  progress, the selected target and a clear locked state.
- Verify target switching, range limits and obstruction checks on the server.
- Check that a locked missile follows a turning boat, flares divert it, and
  destroyed or disconnected targets are handled safely.
- Check incoming warnings, impact damage and missile replication on a client.

Done when: host and client agree on the target and damage; firing without a
lock, losing a target and using flares all have predictable outcomes.

## 4. Add colour and detail while preserving clarity

- Finish the new paints and larger liveries; inspect every boat in the garage.
- Check flower fields, warmer cliffs, blossom trees and race bunting in motion.
- Use a consistent colour palette for boats, pickups, gates and danger cues.
- Adjust saturation, contrast and sharpening from side-by-side screenshots;
  avoid clipped colours, bright halos and unreadable night scenes.
- Measure native-resolution rendering on the Intel laptop with four boats and
  combat effects. Keep an affordable graphics preset available.

Done when: targets and gates remain easy to read across weather conditions,
new assets appear correctly, and performance is measured against the previous
build on the same machine. Aim for 60 fps; record dips and frame-time spikes.

## 5. Tune the racing through full races

Play at least three complete races: a normal forward race, a reverse race and
a combat-heavy race. Record steering response, wave handling, camera comfort,
rival difficulty, wreck frequency and whether weapons overwhelm racing.

Adjust one group of settings at a time. If physics or course geometry changes,
run `Scripts/physics_check.py` and the course generator/checker; verify garage
handling extremes as described in `CLAUDE.md`.

Done when: full races are enjoyable and finish reliably, and tuning changes
have written observations rather than screenshot-only evidence.

## 6. Validate multiplayer and progression

- Run the local host/client check with `Scripts/net_test.sh`.
- Race on two physical machines over Wi-Fi, checking gun hits, missile locks,
  flares, wreck recovery, joining, leaving and LAN discovery.
- Track earnings over several races against boat and upgrade prices. Set a
  desired purchase pace, then adjust rewards and costs to match it.
- Update README status, controls and known limitations to reflect verification.

Done when: a full cross-device race completes with consistent combat results
and the upgrade pace has been measured.

## Later content

After the above passes: add a genuinely different second circuit, then crowd
ambience and spectators. Validate Windows packaging on Windows. More expensive
effects and public-server anti-cheat belong to a later scope.

## Verification progress — 16 November 2026

- Current C++ build: passed on Unreal 5.9 preview-1.
- Regenerated 45 GLB models, including the missing blossom/autumn trees and
  updated gantry and boats; imported assets into Unreal. All four generated
  livery PNGs are 2048 × 512.
- Physics and course checks: both ended with `FAILURES: 0`.
- Visual smoke test: passed, 14 screenshots; tour-stop readings 44–64 fps at
  1280 × 720. This is not a native-resolution benchmark or a sustained minimum.
- Local host/client network test: passed; four boats started, the client
  scored gates on the server, and its deck-gun trigger reached the server.
  This check does not verify missile targeting or combat damage consistency.
- Inspected sunny, stormy, night and combat screenshots: new trees, deck gun,
  muzzle flash, tracers, incoming-missile cues and heat HUD are visible.
- Updated README status, paint count and combat controls. Fixed model-script
  error propagation; verified that a failed Blender command returns failure.
- Importer warnings remain: the commandlet cannot obtain the editor subsystem
  used to disable Nanite. Some generated meshes also report tangent warnings.
  These did not fail the tour; investigate if mesh or shading defects appear.
- Full manual races, cross-device combat scenarios and native-resolution
  performance remain unverified.

## Combat correctness pass — 16 November 2026

- Gun target selection and missile acquisition now share a cover check:
  visibility collision queries for solid obstacles, plus terrain sampling for
  coast sections without collision. Cover is checked on the server as well as
  by the owner's sights.
- Missile proximity checks cover the entire movement segment, so a fast
  missile cannot step across its target without triggering. The blast occurs
  at the closest point on that segment rather than beyond the target.
- Wrecked or destroyed targets are cleared from the missile, ending obsolete
  incoming-lock warnings.
- Five engine automation tests pass via `bash Scripts/combat_test.sh`: solid
  cover, between-frame missile impact, flares and lost targets, moving-target
  homing versus unlocked flight, and shield/wreck/respawn protection. These
  tests exercise actual actors in isolated worlds; they do not establish
  multiplayer timing, client lock-acquisition duration or gameplay balance.
- Post-fix visual tour: passed with all 14 screenshots; tour-stop readings
  43–63 fps at 1280 × 720. Inspected the combat screenshot with the gun firing.
- The combat runner disables config writes and captures a separate console
  log. One runner validation exited before completing the queue, without a
  logged assertion; the complete rerun passed all five tests. The cause of
  that interrupted run is not established.

## Controlled network combat pass — 16 November 2026

- Added `bash Scripts/net_combat_test.sh`, which launches two real game
  processes and uses the owning client's ordinary weapon RPCs.
- Passed: both processes agree on gun damage and trigger release; the client
  acquires a missile lock, fires, spends ammunition and sees the same damage
  as the server; client-fired flares remove an incoming missile's target and
  leave the hull undamaged; a wreck is observed by the client and the normal
  game mode revives it with replicated respawn protection.
- Both processes exited cleanly without a crash or assertion. The scenario
  runs only with the explicit `-NetCombatTest` host flag. Its scripts disable
  config/progress writes.
- Scope: this test holds boats stationary above the scenery, uses slower
  missiles to allow time to inspect replication, and creates the wreck with
  server damage. It verifies message flow and state agreement, rather than
  racing balance, default-speed projectile behaviour or real Wi-Fi conditions.

Recommended next action: full forward, reverse and combat-heavy races to
tune handling, difficulty and weapon balance, followed by a cross-device
Wi-Fi race. Default-speed missile movement already has isolated engine tests.

## Tracks and menu improvements — 19 November 2026

- Fixed Escape in Garage and Leaderboard by sharing the same Back function
  used by Settings, Controls, the gamepad and the on-screen Back rows. Address
  entry also supports gamepad Back. Menu lists adapt to the available height.
- Added Harbour Sprint (2.48 km) and Canyon Run (3.28 km) alongside the original
  3.0 km Bay Circuit. All three have their own racing line, shoreline profile,
  obstacles and barge routes, and can be raced forwards or reversed.
- Added a route preview, course profile and personal-best lap to race setup.
  Circuit and direction are separate settings. The selected setup survives
  opening a host session; the circuit is replicated to joining clients.
- Updated coin routes, minimap bounds/land, showroom placement, cached obstacle
  data and ghosts when changing course. Each course/direction has separate
  records and ghost files; old Bay Circuit save keys remain compatible.
- Geometry generator checks all three circuits before atomically replacing
  the generated header. All checks pass and generation is deterministic.
- Final build and frontend engine tests pass. Harbour's visual tour captured
  14 screenshots at 31–77 fps at its stops; Canyon's captured 14 at 57–74 fps.
  These are readings at 1280 × 720, rather than sustained/native-resolution
  benchmarks. Inspected the sunny Canyon view and menu screenshots.
- Canyon host/client check passed: the client received the host's course,
  scored gates, and its gun trigger reached the server.
- Live menu testing caught the old RPC option clamp redirecting Direction to
  Circuit. Extended the accepted range and added a screenshot test that checks
  exact circuit/direction combinations, so this failure cannot pass silently.
- Final live-menu test passed: twelve fresh screenshots, including Harbour
  forward, Harbour reverse and Canyon reverse. Inspected the final course
  preview and direction setting after the fix.
- Research and the distinction between implemented additions and future
  championship/shortcut/split-screen ideas are in `RACING_RESEARCH.md`.

## Distinct environments — 19 November 2026

- Rebuilt Bay Circuit as a low tropical coast with palms, scrub, sand,
  limestone islets and turquoise water. Removed the shared flowering/alpine
  forest and lowered the mountain backdrop.
- Rebuilt Harbour Sprint as an industrial port: flat reclaimed concrete land,
  container stacks, warehouses, cargo cranes and navigation towers, with cool
  water/light and a grey sky. Removed forest, cottages and leisure traffic.
- Rebuilt Canyon Run with tall sandstone walls, terraced mesas, cacti,
  boulders and a natural arch; separate desert light, teal water and warm map
  colours. Shared cottages and leisure traffic are absent, and gull calls are
  suppressed in the desert.
- Added nine procedural GLB models and a focused generator/import script.
  Imported them through the full editor; Nanite was disabled successfully.
- Build, model-file validation and the frontend geometry/navigation tests
  pass. All three matching sunny captures passed, and the final harbour and
  canyon full weather/combat tours each captured fourteen images without a
  crash or assertion. Lowest tour-stop readings were 54 fps for harbour and
  61 fps for canyon at 1280 × 720; these are not sustained benchmarks.
- New comparison screenshots are in `Saved/Screenshots/EnvironmentComparison`;
  `Scripts/environment_test.sh` reproduces the same sunny racing viewpoints.
