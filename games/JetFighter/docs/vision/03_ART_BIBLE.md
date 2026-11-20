# JetFighter: aircraft, world and interface art bible

Production direction, 20 November 2026. All names and specifications below are
original proposals. Current aircraft GLBs are useful silhouette/material
prototypes; they are not being presented as final handcrafted, textured assets.

## 1. Visual target

Aim for grounded, readable aviation art with restrained cinematic lighting:
convincing airframe proportions, manufactured surfaces, clear silhouettes,
large geological forms, believable coastlines and a sparse technical HUD.
Aircraft should feel like machinery built for flight. Mountains should feel
like connected terrain shaped by geology and drainage, not smooth cones placed
on a blue plane. The world must remain understandable while moving quickly.

The target is neither a cockpit simulator nor a toy made from coloured boxes.
Detail is spent where the player can see it: the ownship's upper fuselage,
canopy, wings, tail and exhaust; nearby canyon walls and shorelines; weapon
launches, target feedback and hero landmarks. Distant towns become clusters of
roofs and light, rather than thousands of individually detailed windows.

The first production-quality scene is **one aircraft over one mountainous
channel with a functional combat encounter**. A dozen inconsistent asset packs
and five incomplete maps would be a weaker result.

### Shared visual rules

- Shape and material identify a faction/vehicle before paint colour does.
- Preserve a clean ownship and target silhouette against sky, rock and water.
- Use cool environmental distance and warmer close highlights, without a
  permanent orange filter that hides warning colours.
- A small accent palette identifies navigation, friendly units, selected
  targets and danger. Decorative emissive lines do not cover entire surfaces.
- Important structures read at 500 m and 2 km, not only in a close editor view.
- Atmosphere conceals the distant limit; it must not conceal nearby threats.
- Every collision-relevant visible structure has matching gameplay geometry.

## 2. Aircraft roster and role targets

The first three airframes now have original generated meshes, an import
pipeline and in-game selection. The next art pass improves them, rather than
throwing them away for unrelated purchased models. Roles become actual flight
and combat data in a later implementation milestone.

| Airframe | Intended role | Silhouette rules | Gameplay tradeoff to implement |
|---|---|---|---|
| AF-01 Kite | Light fighter | Compact single-engine fuselage, canards, delta-like wings, one fin | Quick roll and turning; modest durability and strike payload |
| AF-02 Rook | Interceptor | Longer pointed body, swept trapezoid wings, twin engine/tail arrangement | Strong speed/climb; wider turns and more energy penalty in close fighting |
| AF-03 Manta | Multirole | Broad cranked wing, substantial fuselage, twin exhausts | Balanced objective attack and endurance; less nimble than Kite |
| AF-04 Bulwark | Heavy strike, later | Broad roots, visible engine mass, large pylons and robust landing gear | More objective damage/durability; slow roll and a large target |
| AF-05 Specter | Recon/electronic, later | Narrow sensor nose, distinctive canted surfaces and sensor fairings | Information and lock-disruption utility; lighter weapons; no permanent invisibility |
| AF-06 Heron | Naval/escort, later | Wing-fold break, robust gear/hook silhouette and broad control surfaces | Low-speed handling and escort equipment; extra drag/weight |

The final roster does not need six aircraft before the first good dogfight.
Kite, Rook and Manta are enough to establish visual identity, then test genuine
role differences. Additional aircraft require both a distinct silhouette and a
useful playstyle; a different thumbnail or a flat stat increase is insufficient.

### Initial profile hypotheses

These are compressed **game speeds**, not real-world aircraft performance.
The existing numerical baseline remains the default until profiles are tested.

| Role | Cruise target | Full throttle | Boost | G/roll target | Cost of its strength |
|---|---:|---:|---:|---|---|
| Light fighter | 480–540 km/h | 680–730 | 900–970 | 8.5–9 G, 230–260°/s | Lower hull/payload |
| Interceptor | 540–600 | 760–820 | 1,000–1,080 | 7–8 G, 170–210°/s | Larger turn radius, weak slow-speed fight |
| Multirole | 480–540 | 680–750 | 900–990 | 7.5–8.5 G, 190–230°/s | No single top-tier manoeuvre advantage |
| Heavy strike | 420–490 | 600–680 | 800–900 | 6–7 G, 140–180°/s | Slow escape/large silhouette |

A role's final numbers are accepted only after timed pursuit, crossing-shot,
missile-evasion and objective-attack trials. If two profiles play alike, the
roster still lacks meaningful variety regardless of the model quality.

## 3. Aircraft asset construction

### Required visible anatomy

A production airframe includes a continuous fuselage, a credible nose/radome,
canopy and frame, wings with thickness and root transitions, tail surfaces,
intake lips and dark interior, engine/nozzle depth, control-surface seams,
pylons/attachment points, navigation lights, painted identification and a
consistent engine glow. Optional stores have distinct silhouettes and attach
to real hardpoints. Their visible presence must agree with the equipped loadout.

Model panel breaks deliberately: wing roots, maintenance panels, gear doors,
control surfaces and engine shrouds. Do not scatter uniform noisy scratches
across every surface. Put wear around access points, leading edges and hot
exhaust areas. Keep the aircraft's identifying shape readable at reduced LODs.

The ownship needs detail from rear three-quarter, above, side-on, nose-on and
hangar close-up. A jet that only looks good in a single promotional angle fails
the art gate. Canopy proportions, stabiliser thickness and exhaust alignment
are reviewed in orthographic views before materials hide mistakes.

### Geometry and texture targets

| Asset | Initial production budget | Notes |
|---|---:|---|
| Ownship external LOD0 | 18k–30k triangles | Provisional; prioritise silhouette/control surfaces |
| Aircraft LOD1 | 7k–12k | Preserve wing/tail shape, simplify interior detail |
| Aircraft LOD2 | 2k–4k | Distant silhouette and main material regions |
| Very distant aircraft | 500–1,000 plus HUD marker | Optional after readability tests |
| Canopy/interior visible externally | Included in hero budget | A full interactive cockpit is separate scope |
| Aircraft body atlas | 2k base colour + normal + packed masks | 1k low preset and shared detail where useful |
| Material slots | Target 3, occasional 4 | Body, canopy, emissive/stores; measure actual draw calls |
| Collision | Simple authored hull/sweep shapes | Visual triangles are not the primary aircraft physics body |

Current prototypes are about 3.8k–4.3k triangles and twelve material slots each.
Their source material regions make iteration easy but are not the shipping
batching target. Bake/merge them into atlases before increasing the roster.
A low triangle count does not cancel twelve material submissions per aircraft.

### Materials

Paint is a mostly dielectric coloured coating with restrained clear coat;
exposed metal has its own metallic response. Normal maps provide panel lines,
fasteners and subtle manufactured waviness without changing the silhouette.
Roughness varies by material and use. Exhaust interiors remain dark around a
controlled emissive core, rather than becoming featureless white circles.

Use an inexpensive opaque tinted canopy for the lowest preset. The higher
preset may use a glass material with a limited reflection solution. Avoid
making the entire aircraft translucent or requiring Lumen to read the model.
Liveries are masks over a common body atlas so three paint schemes do not mean
three independent 4k texture sets.

### Rig and animation requirements

Prepare named attachment/animation anchors now, even if animation arrives in
the next pass: nose/gun muzzles, missile hardpoints, exhausts, navigation lights,
camera, centre of mass and damage-effect sites. The production rig includes
ailerons/elevons, elevators/canards, rudders, airbrake and engine/nozzle visual
controls. Landing gear and a canopy mechanism are later, explicit features.

Control-surface motion should reflect actual commands and airbrake state.
Stores disappear when launched and return when rearmed. Engine glow and trail
respond to throttle/boost. Damage effects attach to the relevant airframe,
rather than following an unrelated invisible cube.

### Source and import contract

Author in metres, +X forward and +Z up, with the origin at the defined flight
reference point. Record length/span and validate imported bounds. Use stable
part/material names and explicit hardpoint names. Export GLB with applied
transforms, correct normals and a usable UV layout. Keep the procedural source
or source Blender file, exported source model, preview, and asset manifest.

Current sources live in `Art/Models`; `Scripts/make_jets.py` regenerates the
original prototypes. `Scripts/import_jet_art.py` creates JetFighter-owned
`/Game/Aircraft` assets, disables Nanite and creates three conventional LODs.
Generated Unreal assets remain excluded from Git. A clean checkout must be
able to recreate them with documented commands.

## 4. Environment families and map layouts

These are distinct terrain constructions and route designs, not one map with
five colour filters. The initial playable area remains approximately a 12 km
wide airspace, but combat focuses on a smaller central zone. Spawns should
produce first contact in roughly 8–15 seconds, not force pilots to search an
empty map for a minute. Layout dimensions below are starting proposals.

### A. Stonegate Strait — first production map

**Purpose:** prove the complete art/combat standard. A coastal mountain chain
is split by a navigable water channel. Two major ridges frame the central fight;
a lower pass offers an alternate approach, and open sea provides a safe wider
route. A headland radar/relay, a believable airstrip compound and a bridge or
viaduct form memorable silhouettes.

Main channel width: roughly 450–800 m; exposed turns open to 700–1,000 m.
Mountains rise about 900–1,700 m. Add one advanced side passage around
180–250 m wide, with a clear entrance, enough forward visibility and a vertical
escape. Do not spawn a beginner directly into that passage. Competitive
objectives are reachable without using the narrowest route.

The central relay has high, low and seaward approaches. An intercepting
fighter can climb above the ridge or cut through a pass; a strike aircraft can
use terrain to approach but has to expose itself to attack. The landscape
supports a decision before it receives foliage or buildings.

Art kit: layered coastal rock, weathered headlands, pale shore shelves, scrub,
small conifers, runway/service compound, viaduct modules, radar station,
communications equipment, docks and distant settlements. Keep buildings at
human scale so speed is perceptible.

### B. Azure Basin — mountain lake and dams

**Purpose:** a distinct inland arena with elevated water, shoreline routes,
cliff transitions and a central open-water fight. A lake sits around 500–800 m
above sea level, enclosed by mountain walls with three usable passes. A dam
and spillway provide an unmistakable reference; a valley service road and
small hydroelectric compound reinforce scale.

Peaks rise around 1,600–2,300 m. The lake spans roughly 2–3 km; two tributary
valleys make different routes rather than two mirrored slots. Objectives sit
on opposite shore structures and a reachable ridge site. There must be an
open route for a damaged or inexperienced pilot.

Art kit: granite/scree, mountain vegetation concentrated in appropriate belts,
waterline rock staining, dam segments, spillway, pylons, road/bridge sections,
small buildings and cloud banks above the passes. Deep lake colour differs
from shallow coastal water, and its surface is not automatically a stormy sea.

### C. Redwake Canyons — eroded channels

**Purpose:** close terrain breaks, open mesas above and multiple channels below.
Use broad eroded canyons with terraces and feeder valleys. A wider main route
supports normal combat; tighter optional branches reward mastery without
becoming the only safe missile defence.

Walls rise about 300–900 m above the channel floor. Main routes are
350–700 m wide, with widened junctions and visible pull-out routes. Put
objectives at junctions or plateau edges, not inside a blind one-way tunnel.
A dry floor, limited water channel or reservoir creates intentional geography;
water is not added just to make every map look like the archipelago.

Art kit: stratified sandstone wall modules, talus, isolated pillars, sparse
vegetation, maintenance tracks, radar outpost and a viaduct. Strong horizontal
strata provide scale; a few landmark rock profiles identify junctions.

### D. Frostline Fjord — long water corridors

**Purpose:** a coastal map with longer, colder approach routes, steep shoulders
and strong weather identity. One main fjord, a branch inlet and an elevated
saddle create a loop rather than a dead end. Snow belongs to high terrain and
sheltered ledges; exposed lower rock remains visible.

Mountains rise about 1,500–2,400 m, and the fjord is roughly 500–1,000 m wide.
Place a coastal monitoring station and a port/service facility at useful
objective sites. Weather must preserve the horizon and target contrast. A
white aircraft, white cliff and dense white cloud cannot all occupy the same
combat view without outline/marker support.

Art kit: angular rock, selective snow masks, cold water, small conifer patches,
industrial pier, monitoring station and ice only where geography justifies it.
Do not rely on expensive volumetric snowfall to identify the biome.

### E. Sable Caldera — later specialist arena

**Purpose:** dramatic verticality around a volcanic basin, with a central
waterbody and broken rim passages. The rim must have multiple accessible gaps;
it cannot be an attractive bowl that traps slow aircraft. An observatory or
communications array is the hero landmark. Basalt faces, ash slopes and
contrasting water provide identity; active lava and eruption are optional,
separately budgeted features rather than mandatory scenery noise.

This map follows the first three production maps. It is not a reason to delay
the first complete fight or to ship five unfinished terrain generators.

## 5. Terrain authoring and contact architecture

### Replace isolated domes with coherent landforms

Start with a route diagram and a height/blockout pass. Define ridge spines,
drainage paths, saddles, cliffs, shore shelves and flat building pads. Use
noise and erosion to modify those forms, not to decide the entire layout.
The same mountain chain should have directional structure and subordinate
ridges. Height variation must survive both the 30 m flight view and the 4 km
survey; tiny ground noise alone will not improve a featureless dome.

Author terrain through editable heightfields/curves and modular cliff meshes.
Keep a deterministic baked gameplay representation. Split visible terrain into
chunks with near/mid/far LODs so an entire 16 km grid does not need uniform
high detail. Use erosion/sculpted detail near flight routes and lower density
where terrain is only a distant silhouette.

### Collision parity

The existing map already samples the same triangle planes used for rendering.
Preserve that principle when introducing chunks. Within collision-critical
flight distance, render and contact surfaces must agree to a defined tolerance.
A distant visual LOD may be coarser because a jet is not interacting with it;
when approached, it must refine before the aircraft can touch it.

Overhangs, arches, caves and bridges cannot be represented by a single height
per XY. Give them explicit authored meshes with appropriate swept collision
and weapon occlusion. Do not let a heightfield pretend an arch opening is a
solid mountain, or let a visible bridge be permeable to aircraft and missiles.
Prop colliders use simple geometry, not every decorative bolt.

### Lakes and channels require a water system, not another blue rectangle

Introduce a map water-body definition with polygon/spline bounds, surface
height, wave scale, flow, material parameters and body ID. A unified surface
query returns the relevant solid/water surface at a position and time. Flight
impact, missile contact, effects and warnings must use it consistently.

A mountain lake at 650 m cannot use the ocean's zero-height collision plane.
The current water material also uses world Z for crest colour; putting it on
an elevated lake without a water-level parameter would turn the whole lake
into a false crest. Use water-relative height in shaders and matching body
levels in collision. Wave displacement can still derive from OceanWaves,
scaled and offset consistently rather than inventing another unrelated sine.

Rivers need width, banks, longitudinal height and flow direction. Shore masks
or body polygons stop water leaking across land and into dry basins. Lake,
river and ocean transitions must agree in both geometry and shading. Water
LOD must avoid overlapping coplanar sheets—the earlier aerial-water bug is a
regression case, not a technique to reintroduce.

Epic's [Water Body Actors](https://dev.epicgames.com/documentation/en-us/unreal-engine/water-body-actors-in-unreal-engine)
are a possible later authoring route. The current code-only, low-spec project
should first prove its own small polygon/spline water-body layer. A plugin
switch requires a working benchmark and contact-parity prototype, not a guess
that a feature-rich system is automatically cheaper.

## 6. HUD, typography and readable combat

Use Rajdhani for navigation, headings and instrument labels. Regular is for
small labels, SemiBold for alerts and headings. These are supplied under the
SIL Open Font License; preserve the license alongside the fonts. This is our
chosen aviation direction, not an identification of Sky Warriors' font.
[Google Fonts source](https://github.com/google/fonts/tree/main/ofl/rajdhani).

A readable HUD has an information hierarchy, rather than every possible
instrument sharing the same weight:

- Centre: flight-path marker, gun aim and lead solution; target brackets only
  when there is a target. Distinct shapes for acquiring, locked and lost.
- Upper centre: team score, time and current objective state. Brief point
  feedback must name the cause, such as RELAY DESTROYED +3.
- Lower sides: speed, altitude above mean sea level, terrain clearance,
  throttle/afterburner, hull and heat. Distinguish altitude from clearance.
- Radar: world-relative contacts and objective symbols with an explicit
  heading indicator. Terrain-hidden enemies must not silently become visible.
- Immediate threat: directional missile warning, an audible acquisition
  change and a countermeasure readiness symbol. Colour alone is insufficient.
- Edge: off-screen objective and threat arrows with distance, bounded so they
  do not overlap pause menus or score displays.

Use warm amber for player selection, cyan for flight guidance, white for
neutral instruments, green for friendly contacts and red plus shape for
hostile threats. Bright water, snow, sky and dark cliffs all need outlines
or a restrained translucent instrument backing. Avoid giant decorative
panels covering the approach corridor. Safe areas and UI scale must work at
1280×720 and larger displays; test long names and disconnected controllers.

The current Canvas HUD uses a runtime UFont containing the actual font faces.
A raw composite font alone does not satisfy Canvas' font-object requirement;
this was caught in rendered verification. Font loading logs are insufficient:
acceptance requires visible, legible labels in the actual game and pause menu.

## 7. Effects and sound are gameplay systems

| Event | Visual response | Audio response | Gameplay linkage |
|---|---|---|---|
| Afterburner | Throttle-driven emissive nozzle and short plume | Layered engine intensity | Actual engine state, no permanent plume |
| High-G manoeuvre | Brief wing condensation in suitable conditions | Airframe strain/breath cue | Measured load, optional comfort setting |
| Gunfire | Short muzzle flash and sparse tracers | Cadence/heat change | Authoritative shots and ammunition |
| Missile launch | Rail departure, smoke trail, ignition | Launch and target warning | Actual seeker and replicated launch |
| Countermeasure | Ejected bright flare and decaying smoke | Release/cooldown cue | Actual inventory and decoy simulation |
| Hull hit | Local spark/debris, brief directional indicator | Material-dependent impact | Confirmed hit location and damage |
| Engine damage | Intermittent smoke and reduced thrust response | Rough engine loop | Actual subsystem state |
| Terrain impact | Scaled debris, flash and smoke | Distance-filtered impact | One crash event, then respawn |
| Objective capture | Beacon colour/state transition | Brief team notification | Authoritative objective transition |

Do not spawn an explosion without a corresponding gameplay event. Pool short
lived effects; cap smoke particles and trail lifetime. No full-screen flash
or violent camera shake is needed to prove a missile hit. Offer camera shake
and flashing-effect reduction. A low graphics setting must preserve threat
information even when it reduces decorative particles.

Engine audio needs at least idle, cruise and afterburner layers, with pitch
and volume from measured throttle and speed; different airframe families can
share source layers initially. Doppler and distance falloff distinguish an
enemy crossing overhead from the player's engine. Lock, incoming missile,
stall and ground-clearance warnings need distinct sounds and priorities.
Reserve the highest priority channel for imminent collision or missile danger.
Use licensed recordings or original synthesis, with provenance per sound.

## 8. Source-to-game asset pipeline

1. Define the role, dimensions, silhouette and camera distance before detail.
2. Author metres, +X forward and +Z up, centred around the flight reference.
3. Keep editable source and a deterministic export script in Art; export GLB.
4. Preserve semantic material names and attachment sockets in metadata.
5. Import into the game's own /Game/Aircraft mount. Shared ocean assets stay
   in MaritimeCore; one game must never import the other's private content.
6. Generate and inspect LODs. Test silhouettes, fins and canopy at transitions.
7. Configure collision separately from render detail. High-poly canopy
   triangles should not define flight collision or damage authority.
8. Validate the actual packaged build: cooked meshes, fonts, materials and
   required sounds must exist on a fresh checkout after asset generation.

The current make_jets.py creates the three original prototypes and a gallery;
import_jet_art.py publishes stable mesh paths and three reduction LODs. GLBs
are committed source assets; generated Unreal assets are deliberately ignored
under repository policy and must be regenerated before cooking. The generated
.blend and preview are convenience outputs, not the sole editable source.

Production meshes need named engine, muzzle, missile rail, wingtip and camera
anchors. Stores need removable components so firing a missile changes the
visible rail. Damage masks must be driven by confirmed damage; avoid a generic
permanent black texture. Initial liveries should be original, with no real
military insignia required. Hangar previews must use the same mesh and livery
as flight, with honest locked/unlocked and installed-equipment states.

For terrain, store the height source, erosion/authoring parameters, splines,
material layers and landmark placements. Record seed and version so collision
and rendered terrain rebuild together. Source licenses, author and modification
history belong in the asset manifest. [Poly Haven](https://polyhaven.com/license)
and [ambientCG](https://docs.ambientcg.com/license/) offer CC0 asset sources;
that does not make every image found through a search reusable. No reference
screenshots or Sky Warriors assets are shipping content.

## 9. Performance budgets and gates

The following are starting budgets to measure, not achieved shipping claims.
Target the existing Linux machine at 1280×720 first, 60 fps sustained with four
active aircraft and combat effects. The earlier empty-world 109–117 fps survey
is useful headroom, not proof of a busy match or mobile performance.

| Area | Initial budget / rule |
|---|---|
| Frame time | 16.7 ms target; investigate 95th/99th percentile spikes |
| Aircraft | 18–30k hero triangles; screen-size LOD; 3–4 material slots |
| Current prototypes | About 3.8–4.3k triangles, 12 slots; consolidate before scaling |
| Textures | 2k main aircraft atlas; 1k low preset; shared masks where useful |
| Terrain | Chunked visible detail; distant ridges simplified; no uniform giant grid growth |
| Foliage | Instanced, culled, concentrated near playable approaches |
| Effects | Bounded concurrent trails, explosions and smoke; pooled allocations |
| Shadows | Limit dynamic casters and distance; landmarks must remain readable |
| Water | One authoritative body query; no coplanar overlapping surfaces |
| Networking | Measure packets, correction frequency and bandwidth with four peers |

Epic documents [automatic mesh LOD reduction](https://dev.epicgames.com/documentation/en-us/unreal-engine/static-mesh-automatic-lod-generation-in-unreal-engine)
and [instanced mesh components](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine).
Apply those tools to measured bottlenecks. Nanite, a water plugin or a larger
texture is not a substitute for a frame capture and a repeatable benchmark.

Art acceptance requires: a recognisable aircraft in bank and chase views;
no box silhouette; coherent materials under sun and shadow; no wing inversion
or detached stores; readable HUD against all map families; collision matching
visible ground; no water seams from low flight through 6 km; clear objective
landmarks; LOD transitions without major silhouette loss; and a packaged build
with all licensed source dependencies reproducible. Terrain screenshots alone
do not pass the gate: fly through every intended route at combat speed.
