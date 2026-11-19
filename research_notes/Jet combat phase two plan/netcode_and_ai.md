# Netcode for fast aircraft combat and dogfighting AI (UE5, LAN, 2–4 players plus AI)

Context for the report writer: Boat Race already uses "the owner simulates its own boat; everyone else holds a kinematic copy; the server counts gates and decides all damage" (CLAUDE.md). Most findings below are measured against extending that model to jets. Research ran 19 Nov 2026 with ~18 tool calls; several primary sources (Valve wiki, ModDB devblog) returned HTTP 403 and are listed as gaps.

## 1. Client- vs server-authoritative aircraft movement, extrapolation, interpolation delay

### Takeaway
Shipping flight titles keep **hits and damage server-authoritative** while letting clients present predicted visuals. For a 2–4 player LAN game, owner-simulated aircraft (client-authoritative movement) plus a server that decides damage is the cheapest workable model. Remote aircraft are shown with snapshot interpolation (Hermite on position, slerp on rotation) plus a short, bounded extrapolation. Unbounded extrapolation at high G is what produces the "warping" seen in DCS.

### Cited Findings
- **Gaffer on Games, snapshot interpolation numbers:**
  - Buffer delay rule of thumb: about 3× the packet interval, so you can lose two packets in a row and still have something to interpolate toward. That gives 300 ms at 10 pps, plus 1–2 frames for jitter, about 350 ms in total.
  - 30 pps gives about 150 ms of delay and 60 pps about 85 ms.
  - Source: [Gaffer On Games – Snapshot Interpolation](https://gafferongames.com/post/snapshot_interpolation/)
- **Gaffer, interpolation method:**
  - Linear interpolation at low rates jitters at the sample points.
  - Hermite splines that use the sent linear velocity remove this, at a small bandwidth cost.
  - Slerp on orientation was good enough that angular velocity did not need to be sent.
  - Source: [Gaffer On Games](https://gafferongames.com/post/snapshot_interpolation/)
- **Gaffer, extrapolation:**
  - Extrapolating 200 ms cuts apparent delay from 350 to 150 ms, but it ignores collisions and forces and mispredicts rotation.
  - It works best for mostly linear motion over short windows of about 50–250 ms.
  - Send over UDP and skip lost snapshots rather than resending them.
  - Source: [Gaffer On Games](https://gafferongames.com/post/snapshot_interpolation/)
- **Gaffer, bandwidth example:** about 225 bits per rigid body per snapshot (position, orientation, flags), before compression. [Gaffer On Games](https://gafferongames.com/post/snapshot_interpolation/)
- **DCS World:**
  - Players have reported aircraft "warping" in multiplayer from 2013 to 2021.
  - Specific complaints cover extreme high-G manoeuvres warping the plane for everyone else, and high-AoA warping.
  - A 2017 netcode change was said to reduce jumpiness in beyond-visual-range (BVR) fights but hurt close-in gun combat.
  - These are anecdotal player reports, with no official Eagle Dynamics technical write-up.
  - Sources: [DCS forum – warping/desync](https://forum.dcs.world/topic/103424-open-warping-desync-in-multiplayer/), [DCS forum – high G warping](https://forum.dcs.world/topic/257345-extreme-high-g-maneuvers-result-in-the-plane-warping-for-everyone-else-in-the-game), [DCS forum – fix netcode or revert](https://forum.dcs.world/topic/163941-fix-netcode-or-revert-to-old-netcode)
- **DCS, missile hits:** players report a missile hit registering on the shooter's copy of the target but not on other clients, and desynced units after hits. This suggests per-client hit evaluation in at least some cases (unconfirmed). [DCS forum – inconsistency/desync of unit positions](https://forum.dcs.world/topic/235993-reportedinconsistencyde-sync-with-unit-positions-in-mp)
- **General trade-off (Unity Netcode docs):**
  - Server authority is more secure and less reactive, with no sync issues.
  - Client authority is more reactive and less secure, with possible sync issues.
  - Source: [Unity Netcode – Dealing with latency](https://mp-docs.dl.it.unity3d.com/netcode/2.0.0/learn/dealing-with-latency/)

### Inferences
- **Remote aircraft display.** A jet at 250 m/s moves 25 m in 100 ms, so a 350 ms interpolation buffer at 10 pps would draw a remote jet about 90 m behind its true position. That is unusable for guns. On LAN, 30–60 Hz owner updates with a ~2-packet buffer (about 35–70 ms) puts the drawn plane about 9–18 m behind. Short extrapolation can close the rest, provided it is clamped to roughly 100–150 ms. It should use velocity plus angular velocity (constant turn rate), not velocity alone. Jets fly arcs, and a straight-line extrapolation of a 9 G turn drifts outward.
- **Reusing the boat pattern.** The existing pattern (owner simulates, others kinematic) carries straight over. On LAN with friends, trusting the owner's position (known gap 5 in CLAUDE.md) stays acceptable. For internet play against strangers you would need server-side sanity checks (speed and turn-rate limits) or full server authority with prediction.
- **Warping.** DCS's high-G warping is consistent with naive dead reckoning of a manoeuvring aircraft. Blending smoothly toward the latest snapshot (error-decay over ~100–200 ms) rather than snapping is the standard mitigation.

### Gaps
- No primary netcode write-up was found for Nuclear Option (Steam says Early Access runs into 2027 and promises "improved multiplayer stability": [Steam](https://store.steampowered.com/app/2168680/Nuclear_Option)). It is not confirmed whether it uses Mirage or client authority.
- Nothing was found on Ace Combat 7's P2P netcode. One review says AC7 multiplayer has no AI at all, and a Steam post (secondhand, unverified) claims AI in multiplayer was tried and dropped. Sources: [GameRevolution review](https://www.gamerevolution.com/review/489767-ace-combat-7-review), [Steam discussion](https://steamcommunity.com/app/502500/discussions/0/2291716608426838436/?l=schinese)
- The Overwatch and Rocket League GDC talks were not fetched this round.

## 2. Gun hit registration and lag compensation; missile replication

### Takeaway
War Thunder simulates every bullet and shell on both server and client from the same start time and model, and the server alone decides hits. The client's hit effects are cosmetic and can be overruled. Shooter-side hit detection (the "favour the shooter" approach) feels best at high closing speeds but causes "hit me after I dodged" complaints. Server rewind (lag compensation) is the middle path. On LAN, round-trip time is a few ms, so server-decided hits without rewind are close to fine.

### Cited Findings
- **War Thunder, how shells are simulated:**
  - Shell and bullet trajectories start at the same time on server and client, using the same physical model and synchronised time sampling at the same rate.
  - Client trajectories match the server's in about 99% of cases and no longer depend on client FPS.
  - Every shell is simulated, with "hundreds of thousands" of bullets in flight at once.
  - Source: [War Thunder devblog "Shell and bullet synchronization" (6 Oct 2017)](https://warthunder.com/en/news/5005-development-shell-and-bullet-synchronization-en)
- **War Thunder, remaining problems:** the change reduces, but does not remove, hits shown on the client that do not count on the server. Unstable ping and packet loss still cause problems because the server must know a shot was fired. [War Thunder devblog](https://warthunder.com/en/news/5005-development-shell-and-bullet-synchronization-en)
- **War Thunder, who decides hits:**
  - Per a Gaijin developer blog (summarised by search, not fetched directly), all physics and hit decisions are server-side and a client cannot turn a miss into a hit.
  - Hit effects play on the client regardless, and the lag can be up to half a second.
  - The sight shows separate "server hit" markers.
  - Source: [warthunder.com devblog 4393](https://warthunder.com/en/news/4393/current)
- **War Thunder, player desync reports:** players report the server registering a kill before the shell visually arrives, and tracers that look like misses but register. A moderator pointed them to server replays. [WT forum](https://forum.warthunder.com/t/server-registers-kill-before-shell-impact-confirmed-with-replay-screenshot/246698), [WT forum – visual discrepancies](https://forum.warthunder.com/t/visual-discrepancies/116870)
- **IL-2 Sturmovik:** a 2016 community test found evidence for server-side hit detection. [IL-2 forum](https://forum.il2sturmovik.com/topic/24777-hit-detection-implementation-and-udp-cummunications)
- **DCS:** a missile hit can register on the shooter's copy but not on others' copies (anecdotal). [DCS forum](https://forum.dcs.world/topic/235993-reportedinconsistencyde-sync-with-unit-positions-in-mp)

### Inferences
- **Recommended LAN design (fits the existing deck-gun code).**
  - **Guns:** the server decides hits, as `FindGunTarget` already does. Only "trigger held" is replicated, and each machine draws its own cosmetic tracers. With LAN RTT under 10 ms, the server's view of a target is at most about 1–2 snapshots stale. That is roughly 4–8 m at 250 m/s, so a slightly generous hit volume or aim assist absorbs the error. Rewind-based lag compensation is optional on LAN.
  - **Rewind for internet play:** keep about 1 s of per-aircraft position history on the server at the physics tick. Rewind targets by (shooter RTT/2 + the shooter's interpolation delay) before testing the shot. Cap the rewind at about 200–250 ms to limit "shot after I broke".
  - **Missiles:**
    - Simulate on the server; replicate spawn (seed, launcher, target, launch time) plus a low-rate position for correction.
    - Clients run the same guidance law locally from the shared clock, the same idea as the "anything derivable from the shared clock is never replicated" rule.
    - Flares and countermeasure outcomes use a server-side random seed, so every machine agrees on which flare a missile chased.
    - Detonation and damage are server events.
- **Head-on passes.** At closing speeds of about 500 m/s, gun hits in head-on passes are very sensitive to stale positions: 20 ms equals 10 m. That argues for a generous hit radius, or for hits checked on the owner's own simulation of the target.

### Gaps
- Valve's "Latency Compensating Methods" paper (the canonical rewind reference) returned 403 and could not be quoted: [Valve dev wiki](https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization)
- No source found for missile replication specifics in War Thunder or DCS (deterministic seeds etc.). The missile design above is inference.
- No measured bandwidth per aircraft from any flight title.

## 3. UE5 specifics: replicated movement, physics replication modes, Network Prediction plugin, Mover, Iris (status late 2026)

### Takeaway
UE 5.9 offers three physics replication modes: Default, Predictive Interpolation and Resimulation. Mover/ChaosMover are still Experimental, with Epic aiming for Beta in 2026. The Network Prediction Plugin is essentially in maintenance and has had Iris incompatibilities. Iris is compiled in but off by default as of 5.7. For a code-only LAN jet game, the safest path is plain actor replication: the owner sends compressed state through an unreliable server RPC, the server re-replicates it to the others, and they interpolate. Do not adopt Mover/NPP/Resimulation for aircraft yet.

### Cited Findings
- **UE 5.9 networked physics, Default (legacy) mode:**
  - The client matches the server's velocity and forward-predicts received state by half the RTT.
  - Position error is turned into velocity over a correction time.
  - Local changes are overwritten, so objects snap back.
  - Source: [Epic – Networked Physics Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/networked-physics-overview)
- **Predictive Interpolation:**
  - For server-authoritative Actors; blends client local velocity with server velocity.
  - Corrections account for RTT and the send interval.
  - Quality degrades as latency and velocity rise.
  - Source: [Epic – Networked Physics Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/networked-physics-overview)
- **Resimulation:**
  - The client runs about half an RTT ahead of the server and caches each physics tick's state for at least one RTT.
  - On divergence it rewinds and resimulates; rendering smooths the snap.
  - Recommended for physics Pawns, but it costs more CPU, memory and network.
  - The Network Physics Component is a low-level system that must be implemented in C++.
  - Tuning is through `np2.PredictiveInterpolation`, `np2.Resim` and `p.Resim`.
  - Source: [Epic – Networked Physics Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/networked-physics-overview)
- **Status labels:** the API enum still labels Resimulation "Work In Progress". A community reply says Predictive Interpolation "is not viable for physics-based pawns" and that Resimulation "doesn't scale well". Physics Replication LOD (experimental) switches objects between the two modes. Sources: [EPhysicsReplicationMode API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/EPhysicsReplicationMode), [UE forum – NPP vs NetPhysics](https://forums.unrealengine.com/t/should-we-be-using-the-network-prediction-plugin-or-netphysics-predicted-physics/2535018/2), [UE forum – Physics Replication LOD](https://forums.unrealengine.com/t/follow-ue-fest-stockholm-on-physics-replication-lod/2704896)
- **Mover / ChaosMover:**
  - Epic staff are "shooting for Mover/ChaosMover to be in shape for Beta this year" (2026), aiming for after 5.8.
  - Remaining work: separating logic from data in layered moves, and a rollback-aware blackboard.
  - Rest-of-2026 focus: converging the Mover and ChaosMover simulations, Iris delta compression, and networked physics performance.
  - ChaosMover is already in beta in UEFN.
  - Sources: [UE forum – next steps for Chaos Mover](https://forums.unrealengine.com/t/question-about-the-next-steps-for-chaos-mover/2743117), [Epic – Mover docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/mover-in-unreal-engine)
- **Network Prediction Plugin:**
  - Per Epic staff on the forums, it is "not currently under active development beyond general support and bug fixes".
  - A forum reply says development stalled in favour of the Chaos backend, which Fortnite uses.
  - NPP was reported as not supported by Iris, leaving Mover non-functional under Iris.
  - A 5.8.2 thread still asks for an ETA; another post says Iris support was added for NPP/Mover in UE5/Main.
  - Sources: [UE forum – NPP status](https://forums.unrealengine.com/t/status-of-network-prediction-plugin-looking-for-epic-response/502509), [UE forum – Iris & NPP incompatibilities](https://forums.unrealengine.com/t/iris-network-prediction-plugin-incompatibilities/2651670), [UE forum – ETA on Iris/Mover/NPP (5.8.2)](https://forums.unrealengine.com/t/requesting-an-eta-on-iris-mover-network-prediction-plugin-compatibility-ue-5-8-2/2841863)
- **Iris:**
  - In 5.7 Iris moved from experimental to beta. It is compiled in by default but disabled (`net.Iris.UseIrisReplication`).
  - Epic docs say the legacy replication system is still the default.
  - Third-party bug reports exist (a GAS tag map wiped under Iris on 5.7–5.8; a bitstream overflow).
  - Sources: [UE forum – 5.7 Iris strategy](https://forums.unrealengine.com/t/5-7-iris-strategy/2699685), [Epic – Introduction to Iris](https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-iris-in-unreal-engine), [UE forum – Iris bitstream overflow](https://forums.unrealengine.com/t/iris-readobjectbatch-bitstream-overflow-received-corrupted-packet-data-iris-cannot-recover/2743137)

### Inferences
- **Recommended UE setup for jets.**
  - Keep the boats' architecture.
  - The owning client runs flight physics, either as a kinematic flight model it integrates itself or as a Chaos rigid body.
  - At 30–60 Hz it sends a quantised state through an unreliable Server RPC: position, quantised quaternion, velocity, angular velocity and a timestamp from the shared clock.
  - The server stores it (history for rewind, gate/kill logic) and forwards it to the others through a replicated property or multicast.
  - Simulated proxies run a Hermite-plus-slerp interpolation buffer with clamped extrapolation.
  - Set `NetUpdateFrequency`/`MinNetUpdateFrequency` on the jet actors to match the send rate. The engine's default `bReplicateMovement` path is tuned for walking characters and lower rates.
  - Do not use Resimulation for 4 aircraft. Its benefit is physical contact between objects, and jets rarely touch.
- **Bandwidth (back-of-envelope, not sourced):**
  - Per update: position (3×24-bit quantised over a ~16 km cube, about 1 m/1000 precision) plus smallest-three quaternion (about 32 bits) plus velocity (3×16) plus angular velocity (3×12) plus flags and timestamp (about 32) comes to about 230 bits, roughly 30 bytes plus UE headers.
  - At 60 Hz that is about 2–4 KB/s per aircraft per receiver, or about 50–100 KB/s total at the server for 4 players plus 4–8 AI.
  - This is trivial on LAN Wi-Fi. It also fits internet upload, but Wi-Fi jitter argues for a slightly larger buffer (about 3 packets) at 60 Hz.
- **Iris.** Leave it off: it is off by default, and its interaction with prediction plugins is unsettled.

### Gaps
- Could not confirm Iris's default state in 5.8/5.9, or Mover's exact label in 5.9 preview. Check `Engine/Plugins/Experimental/Mover` and release notes locally.
- No UE5 sample of a networked aircraft (e.g. on Mover) was found.

## 4. Dogfighting AI architecture: BT vs FSM vs utility; manoeuvres, energy, missiles, target choice, formations, terrain, difficulty

### Takeaway
Shipped and indie flight games mostly use **hierarchical state machines or decision trees on top of low-level steering behaviours**, with named manoeuvres as reusable tasks. The academic baseline is NASA's rule-based Adaptive Maneuvering Logic, which uses a hierarchy of ground avoidance, pointing algorithm, lead/lag logic and missile evasion. The DARPA AlphaDogfight winner was pure deep reinforcement learning (self-play, about 4 billion samples, guns only, 1v1). That is impressive but not practical for a small game. The best fit here is a utility- or priority-ordered layer (survive, then evade, then attack) choosing manoeuvres, with a PD-controller "fly to point" core shared by attack, formation and patrol.

### Cited Findings
- **Squadron: Mercenaries (indie space-combat devlog), architecture:**
  - Hierarchical state machines: a Mission (Patrol, Destroy Target, Wingman) selects an Operation, which is made of reusable Tasks.
  - Wander, then Approach Target, then Engage, each triggered by range.
  - Engage switches to a data-driven decision tree.
  - Source: [Squadron: Mercenaries devlog](https://ibgoge.itch.io/wing-mercenaries/devlog/718838/recapturing-the-golden-era-of-dogfighting-behind-the-scenes-of-squadron-mercenaries-space-combat-ai)
- **Squadron: Mercenaries, manoeuvres:**
  - Named manoeuvres from the Wing Commander manual's "Taggart's Tactics": Kickstop, Fish Hook, Tight Loop, Hard Brake, Sit-'n'-Spin, Tail.
  - Damaged pilots favour defensive manoeuvres or flee, and pilots switch between offence and defence as health and position change.
  - Exposed flanks trigger strafing runs; a positional advantage triggers Tail.
  - Difficulty works by restricting the available manoeuvres per pilot skill or ship type, and personalities are data.
  - The first demo AI produced "space jousting" (head-on passes), which is the classic failure mode.
  - Source: [Squadron: Mercenaries devlog](https://ibgoge.itch.io/wing-mercenaries/devlog/718838/recapturing-the-golden-era-of-dogfighting-behind-the-scenes-of-squadron-mercenaries-space-combat-ai)
- **Indie plane AI, steering and firing:**
  - The AI tests a small rotation each way and keeps whichever raises the dot product with the direction to the target (or lowers it, to disengage).
  - It banks into turns, clamps pitch to ±70°, and pitches up when within 80 units of the terrain.
  - It cycles through engage, break off, disengage and return on a timer.
  - It fires only when the dot product is above 0.8, the target is in range and a cooldown has expired. There is no lead calculation.
  - Source: [jli69 – Flight and fight plane AI (2024)](https://jli69.github.io/blog/2024/09/05/flight-and-fight-plane-ai.html)
- **Tiny Combat (indie):**
  - AI fights with guns and missiles at per-plane skill levels ("beware the aces").
  - Missiles use IR seekers whose apparent target temperature depends on distance and aspect.
  - Both AI and player use flares, whose effect depends on shot geometry.
  - Player and AI sensors are modelled separately.
  - Source: [Tiny Combat 0.3.0 devlog](https://why485.itch.io/tiny-combat-legacy/devlog/108375/030-dogfighting-foundations-update)
- **NASA CR-4160 (rule-based air combat / Adaptive Maneuvering Logic):** the decision hierarchy includes ground avoidance, a pointing algorithm and lead/lag manoeuvre logic, with versions for air-to-air combat and missile evasion. [NASA NTRS CR-4160](https://ntrs.nasa.gov/api/citations/19890018022/downloads/19890018022.pdf)
- **DCS AI basic fighter manoeuvres:** each aircraft type favours one-circle (nose-to-nose, minimise turning room) or two-circle (nose-to-tail) fights, or a combination. A two-circle fighter creates an offset to lead-turn across the tail. [DCS newsletter](https://www.digitalcombatsimulator.com/en/news/newsletters/d48111b832efd4f57df12df8f46e7ca1)
- **Falcon BMS:** community discussion of BVR AI "cranking" (turning off-axis after a missile shot to slow closure while still guiding). [Falcon BMS forum](https://forum.falcon-bms.com/post/20907)
- **DARPA AlphaDogfight Trials (Aug 2020):**
  - Simulated F-16, 1v1, nose guns only.
  - Heron Systems' agent ("Falco") was trained purely by deep RL with self-play over at least 4 billion samples.
  - It beat Lockheed's AI, then the F-16 instructor "Banger" 5–0.
  - It was credited with fast reactions and unusual tactics: very close range and extreme-angle gunshots.
  - It could not learn during the tournament, and its makers doubted it would transfer to the real world.
  - Sources: [The Batch – AI versus Ace](https://www.deeplearning.ai/the-batch/ai-versus-ace), [Aviation Week](https://aviationweek.com/aerospace/emerging-technologies/darpa-ais-dogfighting-win-shows-potential-teaming-humans), [Breaking Defense](https://breakingdefense.com/2020/08/ai-slays-top-f-16-pilot-in-darpa-dogfight-simulation)
- **Ace Combat:**
  - AC7 multiplayer has no AI forces (review).
  - The Ace Combat 8 preview (Gamescom 2026) says the team is expanding multiplayer.
  - No AI design write-up was found.
  - Sources: [GameRevolution](https://www.gamerevolution.com/review/489767-ace-combat-7-review), [Game Informer – AC8 multiplayer preview](https://gameinformer.com/preview/2026/09/01/ace-combat-8-wings-of-theve-multiplayer-hopes-to-bolster-the-series-long-term)

### Inferences (engineering recommendations; not from a single source)
- **Layering**, mirroring AML's hierarchy and the Squadron design:
  1. **Safety:** ground and terrain avoidance always wins.
  2. **Threat response:** incoming missile, or an enemy on the tail.
  3. **Tactical choice:** pick a target and a manoeuvre with utility scores.
  4. **Manoeuvre execution:** produce a desired direction, speed and roll.
  5. **Flight controller:** PD loops turning the desired direction into pitch, roll and yaw inputs, using the same input interface as a player (as `BoatRivalController` does).
- **Pursuit geometry:** aim the velocity vector at
  - the target's predicted position (lead: closes fastest, used for the gun solution);
  - the target itself (pure);
  - a point behind the target (lag: conserves energy and avoids overshoot).
  
  Switch on range and closure: lead inside gun range, lag when closing too fast or overshooting. The gun lead point is target position + target velocity × time-of-flight, iterated 2–3 times.
- **Energy and turn circles:** the AI stores corner speed per airframe. Below corner speed it unloads (eases off the G) or dives to regain speed; above it, it turns hard. A one-circle versus two-circle choice per aircraft class (from the DCS approach) gives the classes different personalities cheaply.
- **Missile evasion:**
  - On a launch warning, turn to put the missile at the 3 or 9 o'clock position ("beam" it) and dump flares on a timer.
  - At short time-to-impact, make a hard break turn into the missile's plane.
  - Skill sets reaction delay and flare timing.
- **Target selection:** score each candidate as w1·(1/distance) + w2·(how far its nose is from pointing at the AI) + w3·(how well the AI's nose points at it) + w4·(it targeted me) + w5·(it is low on health). Use hysteresis so the AI does not flip-flop between targets.
- **Wingmen and formation:**
  - The slot is an offset in the leader's frame, e.g. 30 m back and 30 m out.
  - Steer to the slot's predicted position, match the leader's speed with a P-controller on the slot error, and roll with the leader.
  - Break formation on commands: "attack my target", "cover me", "rejoin".
- **Terrain avoidance at speed:**
  - Probe along the velocity vector with a look-ahead time of about 3–5 s (750–1250 m at 250 m/s): one ray straight ahead and two angled down and to each side.
  - Also enforce a minimum height over the height function. The game already has a single land height function (`BayTerrain`), which is cheaper than raycasts.
  - Pull up proportionally to time-to-impact.
- **Difficulty and rubber-banding:**
  - Scale reaction time (0.2–1.0 s), G limit, gun aim error, flare discipline and missile-evasion probability.
  - Restrict manoeuvre sets per skill, as Squadron does.
  - Rubber-band gently through AI aggression or aim error based on score gap, not through physics cheats players can see.
- **Avoid the "jousting" failure:** use a timer or a break-off rule after a head-on pass, as jli69's break-off state and Squadron's lesson both point to.

### Gaps
- The ModDB "Devblog 10: making a dogfight behavior tree" returned 403: [ModDB](https://www.moddb.com/news/devblog-10-making-a-dogfight-behavior-tree)
- No Game AI Pro chapter specifically on flight or dogfight AI was found in the indexes seen ([gameaipro.com](https://www.gameaipro.com/)). Chapters on utility AI (dual-utility reasoning) exist there but were not fetched.
- No developer source found for Ace Combat, Project Wingman, Nuclear Option or War Thunder AI design.
- No published, sourced numbers for rubber-banding in flight games.
