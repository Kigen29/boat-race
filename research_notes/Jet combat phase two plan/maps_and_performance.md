# Jet combat maps and large-world rendering on integrated graphics (UE5)

Scope: level/world design for 2–4 player jet combat, and how to draw a big air-combat world at 60 fps on an Intel Meteor Lake iGPU (Vulkan, Linux, UE5 low preset: Lumen, VSM, volumetric cloud off). Research budget was about 15 searches and fetches, so several questions only have forum-level or community-wiki evidence. Each finding says which kind of source it comes from.

## 1. Map scale, action density, boundaries, terrain and spawns for jet combat

### Takeaway
Shipped air-combat maps cover a wide range. War Thunder's arcade-only maps are 32 km square or smaller, and its general maps run 64–128 km. A small arena-style aerial game (Sky Combat 2) uses only about 25–30 km² (about 5 km square) with a fixed 3 km standard altitude. For 2–4 players the evidence points to the small end: a central landmark that pulls fights together, spawns at equal distances from it, terrain features (cliffs, bridges, towers, tunnels) used as cover and as ways to break contact, and room to climb or dive away from a threat at any time.

### Cited Findings
**Map sizes**
- War Thunder arcade-only air maps "tend to be small in size compared to other maps (32 km x 32 km or less)" and often have exaggerated terrain. General air maps, also used in arcade, are generally 64×64 km to 131×131 km. — [War Thunder wiki: Arcade battles](https://wiki.warthunder.com/arcade_battles)
- A War Thunder developer Q&A says air maps are "usually 64x64 km and some locations have a size of 128x128 km". — [War Thunder dev Q&A](https://warthunder.com/en/news/6771-qa-answers-from-the-developers-en?page=6)
- The War Thunder Britain air map is 65×65 km and appears in Arcade, which conflicts with the "32 km or less" generalisation. The later Dover Strait version is 128×128 km. — [War Thunder wiki: Britain](https://wiki.warthunder.com/Britain)
- Sky Combat 2, a smaller arena aerial combat game, targets about 25–30 km² per map with a fixed standard altitude of 3 km. Map size is set by how long pilots take to reach key locations, retreat and rejoin after a respawn, averaged across aircraft types. — [LL Games: interview with a level designer](https://ll-games.com/en/news/interview-with-a-level-designer/)
- Nuclear Option's Heartland map is "~100 km wide" according to a third-party server-hosting guide (weak source, unconfirmed). Its later map, Ignus Archipelago, has "longer distances", and Heartland is about 80% land in the areas players use most. — [xgamingserver guide](https://xgamingserver.com/docs/nuclear-option/game-modes); [Skyward FM](https://www.skywardfm.com/post/nuclear-option-open-ocean-sams)
- VTOL VR's map editor maximum comes from a 2770×2770 heightmap. A fan wiki reports it as "196 sq km", which would be only about 14×14 km. This is community-reported and the unit is doubtful; it may mean 196 km per side. Treat it as unverified. — [VTOL VR fandom wiki: Map Editor](https://vtol-vr.fandom.com/wiki/Map_Editor?oldid=1178)
- Project Wingman's terrain was made in World Machine and "scaled up to what we needed", then imported into UE4 as heightmaps. — [PCGamesN](https://pcgamesn.com/project-wingman/unreal-engine-4-flight-simulator)

**Layout principles (Sky Combat 2 designer interview, the most flight-specific source found)** — [LL Games](https://ll-games.com/en/news/interview-with-a-level-designer/)
- A central "tower" landmark draws the main fighting, with spawn points around it at equal distances. Several landmarks per map make different fights feel different.
- Players should almost always be able to climb or dive to escape. Large distances against high speed give pilots time to assess threats.
- Avoid tight corridors, narrow choke points and camping spots, because aircraft cannot hide the way infantry can. Use cliffs, bridges and skyscrapers as cover and for manoeuvring, and make every placed object useful to both sides.
- Provide tunnel-like routes that let players break contact and recover. Tight turns and narrow passages need the most tuning in playtests.
- At speed, players navigate by shape and colour. Obstacles should stand out by form, texture or contrast, and visually similar structures such as bridges should each get a distinct look.
- Fully enclosed maps can work if nothing is overly tight. Fights drift towards spawns when one side pushes forward, and layouts get reworked when that happens.
- Put refill and power-up points near key areas so players can stay in a fight or leave it.
- The usual failure mode is "creating the same open-field map over and over again". Vary theme, verticality and lighting zones.

**Boundaries and mission structure (Ace Combat)**
- Ace Combat 7 marks a "Return Line" on the map. Crossing it to resupply or repair is allowed but penalised; for example, a medal requires 50 ground kills without using it. — [Ace Combat wiki: Long Day](https://acecombat.wiki.gg/wiki/Long_Day)
- Ace Combat 7 moves missions forward by combat-area state, such as all allies leaving the combat area. It also routes some missions through set-piece terrain, such as the karst Yinshi Valley. — [Ace Combat wiki: First Contact](https://acecombat.wiki.gg/wiki/First_Contact)
- Pre-mission briefings show objectives, expected opposition, terrain and weather on a 3D map display. — [Wikipedia: Ace Combat 7](https://en.wikipedia.com/wiki/Ace_Combat_7)

**Clouds and weather as level design (Ace Combat 7)** — [Skyward FM: clouds and weather in AC7](https://www.skywardfm.com/post/the-form-and-function-of-clouds-and-weather-in-ace-combat-7)
- Clouds hide aircraft visually, but the aircraft stay on radar and HUD. Entering a cloud can briefly break a lock. In cloud, lock-on takes longer, lock range is shorter and homing is worse.
- Staying in cloud too long builds ice, which can lock the controls and force a stall. Lasers lose power and range in cloud.
- High wind pushes aircraft off course and changes missile paths. A lightning strike causes a nosedive, a flash and about 30 s of flickering systems. It appears in only one single-player mission and one multiplayer map.
- The weather effects were deliberately exaggerated to suit fast combat. The team consulted Japan Air Self-Defense Force pilots and air traffic controllers. The cloud system used Simul's trueSKY middleware in UE4, and the work was presented by Masato Kanno at CEDEC 2019, not GDC. — [Skyward FM](https://www.skywardfm.com/post/the-form-and-function-of-clouds-and-weather-in-ace-combat-7); [80.lv: trueSKY](https://80.lv/articles/truesky-making-skies-alive/); [Unreal Engine interview](https://www.unrealengine.com/en-US/developer-interviews/ace-combat-7-soars-high-with-ue4-to-become-franchise-s-best-installment)
- Ace Combat 7 was the first Ace Combat game on Unreal Engine (UE4). — [Unreal Engine interview](https://www.unrealengine.com/en-US/developer-interviews/ace-combat-7-soars-high-with-ue4-to-become-franchise-s-best-installment)

**General level-design support**
- Verticality creates vantage points, landmarks, occlusion and focal points, and whoever is higher has the advantage. — General level-design sources surfaced in search, e.g. [Game Developer: engaging level design for combat](https://www.gamedeveloper.com/design/engaging-level-design-for-combat-pt-2)

### Inferences
These are my own judgements, not sourced facts.
- **Map size for 2–4 players.** War Thunder's 64–128 km maps hold 16–32 aircraft. With 2–4 players, a play area of roughly **8–20 km square** is plausible: bigger than Sky Combat 2's ~5 km square arena, about half to two-thirds of War Thunder's arcade-only maps. At 250–300 m/s game speed (assumed, arcade-style), crossing the area takes 30–80 s. A visible or rendered horizon of 20–40 km beyond it can be faked; see section 2.
- **Fit with the existing BoatRace bay.** The bay could be the "central landmark" region: canyon (Canyon Run circuit 2), harbour cranes (circuit 1) and lighthouse/sea stacks (circuit 0) give the low-altitude cover and contact-breaking routes the Sky Combat 2 designer recommends. Open sea around the bay is the cheap outer area.
- **Boundaries.** A soft Return Line-style boundary fits arcade play: a warning, then a forced turn-back or a timer before the player is counted as out. It is friendlier than a hard wall.
- **Cloud gameplay.** Ace Combat-style cloud effects (lock break, slower lock) can sit on the existing `UpdateMissileLock` and `CanLock` path. They need no volumetric rendering, only a cloud volume test.
- **Spawns.** Spawns equidistant from a central landmark, with spawn protection and altitude separation, address the drift-to-spawn problem the Sky Combat 2 designer describes.
- **Ground and sea targets.** These can reuse BoatRace's barges and boats as moving targets that ride the shared clock, so they need no extra replication.

### Gaps
- No primary source gave mission-area dimensions for Ace Combat 7 or Project Wingman. I also found no official size for Nuclear Option's Heartland or for VTOL VR maps; the VTOL VR unit (km² vs km) is ambiguous.
- No published design analysis of canyon runs specifically, beyond Sky Combat 2's "tight passages need most tuning".
- No sourced guidance on how dense the action should be per player count (engagement frequency, time-to-contact targets).
- No GDC talk on Ace Combat 7 terrain rendering was found. The Kanno CEDEC 2019 talk exists but its content was not retrieved.

## 2. Rendering huge view distances cheaply in UE5 on integrated graphics

### Takeaway
On an iGPU, avoid UE5's expensive defaults: World Partition plus Landscape HLOD (fiddly and forum-buggy), Nanite (already off in this project), and volumetric clouds (not supported on mobile and costly on low-end PCs). Use a camera-following procedural terrain or clipmap mesh, the Sky Atmosphere's LUT-based aerial perspective with exponential height fog to hide distance, and billboard or impostor clouds, the pre-volumetric technique used in Microsoft Flight Simulator 2004. Since UE 5.4, Large World Coordinates rendering (DoubleFloat, camera-relative) has removed the tile-boundary jitter. Materials should still use camera-relative world position.

### Cited Findings
**Large World Coordinates (LWC)**
- UE5 adds double-precision types. In UE4, 32-bit floats limited world size. The default world bounds stayed at the UE4 size, and a much larger WORLD_MAX applies only when the bounds check is turned off. — [Epic: LWC in UE5](https://dev.epicgames.com/documentation/unreal-engine/large-world-coordinates-in-unreal-engine-5)
- Rendering uses the DoubleFloat library: emulated doubles from a pair of floats. Shaders should convert to camera-relative ("translated world") space as early as possible. Before UE 5.4, the fixed 256k tile size in the old tile/offset types caused oscillating precision, visible as jitter and vibration. DoubleFloat removes the fixed tile. — [Epic: LWC rendering](https://dev.epicgames.com/documentation/unreal-engine/large-world-coordinates-rendering-in-unreal-engine-5)
- DoubleFloat operations carry "a substantial performance cost". `DFFast*` variants are cheaper but less precise. In materials, set **World Position Origin Type = Camera Relative World Position** for the best precision and performance. Absolute world space works at LWC scale but costs more. — [Epic: LWC rendering](https://dev.epicgames.com/documentation/unreal-engine/large-world-coordinates-rendering-in-unreal-engine-5)
- Niagara particle data stays single precision, and large worlds are split into grid cells. Projects using doubles should audit for precision errors. — [Epic: LWC](https://dev.epicgames.com/documentation/unreal-engine/large-world-coordinates-in-unreal-engine-5); [Epic: LWC conversion guidelines](https://dev.epicgames.com/documentation/unreal-engine/large-world-coordinates-project-conversion-guidelines-in-unreal-engine-5)
- A forum report from UE 5.0.3 describes visible jitter about 10 km from the origin and suggests rebasing. This predates the 5.4 fix. — [Unreal forums](https://forums.unrealengine.com/t/lwc-large-world-coordinates-do-not-work-for-first-person-shooters-driving-games-wobbly-rendering-artefacts-ue5-only-ue4-works-fine/652748)

**Sky Atmosphere and aerial perspective (distance hiding)** — [Epic: Sky Atmosphere (5.9 docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine)
- Instead of per-pixel ray marching, the sky is evaluated into small low-resolution lookup tables: FastSkyViewLUT, AerialPerspectiveLUT (a 3D volume texture; enlarging it "can significantly increase memory"), MultipleScatteringLUT, TransmittanceLUT and DistanceSkyLightLUT.
- Keep the fast paths on: `r.SkyAtmosphere.FastSkyLUT=1` and `r.SkyAtmosphere.AerialPerspectiveLUT.FastApplyOnOpaque=1`. Setting them to 0 is the "cinematic", slower mode.
- Quality and cost settings: `r.SkyAtmosphere.SampleCountMax`, `r.SkyAtmosphere.DistanceToSampleCountMax`, `r.SkyAtmosphere.FastSkyLUT.SampleCountMax`, `.FastSkyLUT.Width/Height`, `.AerialPerspectiveLUT.Width`, `.AerialPerspectiveLUT.DepthResolution`, and the component's Trace Sample Count Scale.
- Turning on `r.PostProcessing.PropagateAlpha` with fog, cloud or atmosphere holdout triggers an expensive path; avoid it.
- **Aerial Perspective View Distance Scale** makes surfaces look thicker or hazier when seen from altitude. It is an artistic control and fits jets well, since it hides the far terrain edge. The page lists Sky Atmosphere as supported on mobile and low-end PC; mobile needs a skydome mesh with an "Is Sky" material. The page gives no millisecond costs.

**Volumetric clouds (for reference, and why to avoid them)**
- Not supported on mobile. On XB1/PS4, shadow maps are recommended for acceptable performance. — [Epic: Volumetric Cloud component](https://dev.epicgames.com/documentation/unreal-engine/volumetric-cloud-component-in-unreal-engine)
- `r.VolumetricRenderTarget.Mode 0` traces at quarter resolution, reconstructs at half and upsamples to full. Epic recommends it for fast-paced and flying games. Other cost cuts: one multiple-scattering octave (high Contribution and low Occlusion fakes more at no cost), Beer Shadow Maps instead of ray-marched shadows, lower sample scales (0.25–8), no Ground Contribution, and Conservative Density. The settings are `r.VolumetricCloud.ViewRaySampleMaxCount`, `.SampleMinCount`, `.DistanceToSampleMaxCount` and `.ReflectionRaySampleMaxCount`. The page's only millisecond figures are for sky light real-time capture: Sky and Cloud 0.6 ms and Specular Convolution 0.8 ms per frame, time-sliced over 9 frames. — [Epic: Volumetric Cloud component](https://dev.epicgames.com/documentation/unreal-engine/volumetric-cloud-component-in-unreal-engine)
- A custom mobile ray-marched cloud went from 300 ms per frame (3 fps) to 60 fps with 64 density steps and 2 light steps, rendered into a **320×180** render target and upscaled to 1080p with a 4-sample box blur. The catch is that clouds lose depth and draw over everything. — [80.lv: volumetric clouds for mobile in UE5](https://80.lv/articles/creating-volumetric-clouds-for-mobile-in-unreal-engine-5)
- An old UE4-era forum anecdote: simple volumetric clouds took 50–60% of frame time, complex ones 98–99%. This is a rough signal only. — [Unreal forums](https://forums.unrealengine.com/t/how-scalable-the-upcoming-volumetric-clouds-feature-would-be-across-hardwares/148794)

**Cheap cloud alternatives (billboard and impostor clouds)**
- Harris (GDC 2002), "Real-Time Cloud Rendering for Games": constant-shape clouds built from particles. Multiple forward scattering is computed in a preprocess, with anisotropic single scattering at runtime. Clouds are rendered into **impostors** (billboards refreshed only when the view changes enough) to exploit frame-to-frame coherence in a flight simulator. The paper notes that earlier games used planar textures or semi-transparent objects with fog. — [Harris GDC 2002 PDF](https://aurora.cs.uaf.edu/~olawlor/academic/thesis/ref/RTCloudsForGames_HarrisGDC2002.pdf); [gamedev.net tutorial](https://gamedev.net/tutorials/programming/general-and-gameplay-programming/real-time-realistic-cloud-rendering-and-lighting-r2273)
- Microsoft Flight Simulator 2004 used sprite (billboard) clouds with a simplified shadow model. The technique is in Niniane Wang's "Fast, Realistic Cloud-Rendering in Microsoft Flight Simulator 2004" (SIGGRAPH 2003 talk). — [SIGGRAPH history: Wang](https://history.siggraph.org/learning/realistic-and-fast-cloud-rendering-in-computer-games)

**Terrain LOD, World Partition and HLOD**
- The UE 5.0 docs said Landscape and Water components were not supported by HLOD actors, yet users report that landscape HLODs are generated. Practical fixes: raise Landscape "LOD 0 Screen Size" so HLOD bakes from a lower LOD, set Landscape Max LOD to 5, and match World Partition Load Range to the HLOD distance. Users report HLOD bugs with Landscape Nanite in 5.4.1 and black distant HLODs caused by shader errors. — [Unreal forums: Landscape HLODs in World Partition](https://forums.unrealengine.com/t/how-do-i-build-correct-landscape-hlods-in-world-partition/527124)
- A UE4-era forum note: a standard-scale UE4 landscape map is about 5×5 km, and World Composition made "nearly infinite" terrain possible, limited by performance. — [Unreal forums](https://forums.unrealengine.com/t/what-is-the-max-map-size/110476/5)

**Intel iGPU optimisation (Intel's UE5 Optimization Guide, chapter 2)** — [Intel](https://www.intel.com/content/www/us/en/developer/articles/technical/unreal-engine-optimization-chapter-2.html)
- Default scalability `PerfIndexThresholds` (`GPU 18 42 115`) are too low for current iGPUs. Intel suggests starting near `GPU 150 260 550` and profiling. `synthbenchmark` scores up to 30% higher on Test builds than on Development builds.
- Example scalability values (illustrative): `r.ViewDistanceScale` 0.6/0.8, `foliage.DensityScale` and `grass.DensityScale` 0.4/1.0, and `r.Nanite.Streaming.StreamingPoolSize` 512→256.
- On low presets, avoid programmable raster (World Position Offset, Pixel Depth Offset, tessellation), turn off WPO at a distance (WPO Disable Distance), cut textures and samplers, and use real geometry instead of masked materials.
- Fewer materials means fewer draw calls (Nanite shading bins).
- Thermal throttling and power plans affect sustained frame times, so measure warmed-up.
- The tests ran on a Core Ultra X9 388H with Arc B390 (newer than Meteor Lake). The article gives no frame-time results.
- Meteor Lake iGPU: 4 Xe cores (64 vector engines), 4 ray-tracing units. — [Notebookcheck](https://www.notebookcheck.net/Arc-140V-vs-Graphics-4-Core_12693_12259.247598.0.html)
- A user report from a Core Ultra 7 155H handheld: a UE5 game was playable at about 30 fps on Low. — [Intel community](https://community.intel.com/t5/Intel-ARC-Graphics/Bad-performance-amp-Low-wattage-on-Arc-B580/td-p/1714747); anecdotal, and the thread is mainly about other hardware.
- Arc drivers have caused fps capping in UE 5.6; disabling VSync in the driver profile was the suggested fix. This report is about Windows. — [Unreal forums](https://forums.unrealengine.com/t/weird-fps-capping-ue5-6-arc-a770/2633917)

### Inferences
These are my own judgements, not sourced facts.
- **Terrain.** This project already follows the camera with a procedural water grid in whole cells (`WaterSurface`) and builds land from one height function (`BayTerrain`). The natural next step is a **geometry clipmap**: nested rings of procedural mesh, each double the cell size of the one inside, centred on the camera and snapped to whole cells, with heights from the same function. This avoids World Partition, Landscape and HLOD entirely; per the forums they are fiddly, and they assume an editor-built `.umap`, which this code-only project does not have. Ring updates can be time-sliced. Distant rings can use a flat or vertex-coloured material with no textures.
- **Far plane.** Aerial perspective from Sky Atmosphere plus exponential height fog with a distance start lets the outer ring end at perhaps 20–40 km. Fog, not geometry, hides the edge. Raising Aerial Perspective View Distance Scale makes this stronger at altitude, when far ground is most visible.
- **Clouds.** Use about 50–200 billboard or impostor cloud puffs. Each cloud is a cluster of soft camera-facing sprites lit with a baked gradient, which matches the existing "mesh puff" effects system. Add a 2D high-altitude cloud layer as a scrolling texture in the sky material. Clouds should be placed from the shared clock and a seed so nothing is replicated, in line with the project's rule. Gameplay cloud volumes (lock break, hiding) are simple spheres or boxes tested on the CPU.
- **LWC.** On 5.9 a world of ±20–30 km from the origin should not jitter, provided all materials use Camera Relative World Position. `OceanWaves` maths is in float; at 30 km from the origin, float resolution is about 2 mm, which is acceptable. Check the shader side (world-position sine waves) for precision at large coordinates, and consider evaluating the waves relative to a tile origin.
- **Budget guess** (unmeasured): at 1920×1200 and 70% screen percentage on Meteor Lake, the existing boat game runs at 60–80 fps. Adding altitude raises view distance and visible triangles, so the clipmap, fog and impostor approach is needed to stay near 60.

### Gaps
- No published millisecond costs for Sky Atmosphere, height fog or clouds on Intel iGPUs. No Meteor Lake plus UE5 Linux/Vulkan benchmark with Lumen off was found. The project's own measurements will have to be the source.
- No source for `r.LandscapeLODBias` or Landscape LOD settings on low-end hardware. No official guidance comparing ProceduralMeshComponent with Landscape for performance.
- The English text of Wang's FS2004 cloud article was not retrieved, so the details of its sprite method are unconfirmed here.

## 3. Speed sensation and HUD markers for distant targets

### Takeaway
Players and designers agree that perceived speed comes from field of view, detail near the ground (objects of known size), and screen-space motion cues: speed lines or stretched particles, shake, and FOV kick on boost. A designer's textbook notes that in flight games raw speed matters less than in driving games. Low-altitude passes through canyons, harbours and over the sea are where speed reads best.

### Cited Findings
- A wider FOV is widely reported to increase perceived speed (IL-2, MSFS players). Some dispute this from experience with high-FOV VR headsets. — [DCS forum](https://forum.dcs.world/topic/171868-perception-of-speed-is-not-good); [CombatAce forum](https://combatace.com/forums/topic/27262-the-sense-of-speed-inside-and-outside-the-cockpit)
- Nearby ground detail on objects of known size (grass, buildings, runway markings) gives the sense of closeness and speed. One developer noted that matching real perception would need about 1 m terrain detail. — [AVSIM forum](https://www.avsim.com/forums/topic/499872-what-helps-with-the-sensation-of-speed-on-landing/); [DCS forum](https://forum.dcs.world/topic/234213-feeling-of-speed-needs-to-be-improved)
- Speed lines and screen shake that grow stronger during boost, together with a wide FOV or fisheye effect, are described as speed cues in a Game Developer thesis article. — [Game Developer: maneuverability and speed](https://gamedeveloper.com/design/maneuverability-in-games-the-importance-of-speed-and-control)
- Rollings and Adams write that in flight sims there is "little need for a sense of speed" compared with driving games. — [O'Reilly: Rollings & Adams](https://www.oreilly.com/library/view/andrew-rollings-and/1592730019/1592730019_ch13lev1sec3.html)
- Ace Combat 7 keeps aircraft visible on radar and HUD while they are hidden in cloud, so the HUD marker remains the main way to find a distant target. — [Skyward FM](https://www.skywardfm.com/post/the-form-and-function-of-clouds-and-weather-in-ace-combat-7)

### Inferences
These are my own judgements, not sourced facts.
- **FOV.** Map FOV to speed and boost, for example 80° rising to 95–100°, smoothed over time. Add camera shake at low altitude and on afterburner.
- **Motion cues near the ground.** Use pooled stretched-sprite "air streaks" near the camera, wingtip vortices or contrails on high-g turns and at altitude, and spray or dust kicked up when skimming water or ground. These reuse BoatRace's pooled puff, wake and spray systems, which are all cheap on the iGPU.
- **Ground detail.** Keep ground detail dense only in the near clipmap ring (trees, rocks, buoys, boats from the existing scenery). Objects of known size are the cue, and that is where low passes happen.
- **HUD.** Reuse the existing canvas HUD, which already draws a world-space waypoint and a compass tape. Add target boxes with distance, an off-screen arrow at the screen edge, a lock-progress ring (tied to `UpdateMissileLock`) and a small radar. At 10+ km a jet is sub-pixel, so the HUD box is the target.

### Gaps
- No controlled study on FOV versus speed perception; the evidence is player anecdote. No sourced numbers for FOV ranges used by Ace Combat or Project Wingman.
- No source on how Ace Combat or Project Wingman lay out HUD target markers, beyond the radar and HUD persistence noted above.
