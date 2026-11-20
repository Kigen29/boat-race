# JetFighter: combat and art production plan

Prepared 20 November 2026. Research reference: Wildlife Studios' Sky Warriors.
This is an original game direction informed by published developer material,
not a plan to copy its assets, branding, exact balancing or interface.

## Read in order

1. [Research and evidence](01_SKY_WARRIORS_RESEARCH.md): what the publisher
   actually describes, historical changes, screenshot observations and unknowns.
2. [Game design](02_GAME_DESIGN.md): flight, weapons, targeting, damage, bots,
   objectives, networking and complete player-facing feature definitions.
3. [Art bible](03_ART_BIBLE.md): original aircraft, five terrain families,
   lakes/channels, water architecture, HUD, VFX, audio and asset budgets.
4. [Production roadmap](04_PRODUCTION_ROADMAP.md): implementation order,
   proposed code boundaries, dependencies, estimates and acceptance gates.
5. [Asset manifest](05_ASSET_MANIFEST.csv): production inventory and status.
6. [Feature backlog](06_FEATURE_BACKLOG.csv): functional work with dependencies
   and observable acceptance criteria.

## Decisions

- Keep BoatRace and JetFighter separate, sharing only suitable engine code.
- Build an accessible PC chase-camera jet combat game with four-player LAN,
  bots and original aircraft; mobile is not the current delivery target.
- Complete the gun/missile/flare/drone training loop next, then one
  coherent mountain-channel map and a local team match, then LAN completion.
- Initial aircraft: Kite, Rook and Manta. Additional Bulwark, Specter and Heron
  are later production concepts, not six finished aircraft.
- First production map: Stonegate Strait. Azure Basin, Redwake Canyons,
  Frostline Fjord and Sable Caldera follow proven terrain/water tools.
- First match mode: our proposed Relay Strike. Convoy Intercept comes later.
- Rajdhani is our chosen aviation-style HUD font, supplied under OFL.
- No copied Sky Warriors assets, purchased-stat advantage or cosmetic-only
  claim that a nonfunctional feature is complete.

## What exists now

Local assisted/expert flight, mouse/direct/gamepad controls, terrain and sea
contact, pause/reset, navigation landmarks, simple islands, altitude-aware
water, three original selectable model prototypes and the new HUD typeface.
The three models share the existing flight profile. Their visual stores are
not working weapons. Current islands are not the five proposed finished maps.

A first stationary-drone gun encounter now adds hull damage, swept rounds,
heat, hit feedback, destruction and restart; see [combat training](../COMBAT_TRAINING.md).
Missiles, targeting, countermeasures, AI, team matches,
LAN sessions, production terrain, lake bodies, hangar and progression remain
planned. Read the roadmap before interpreting a roster or map table as a
shipping feature list. Older docs outside this folder are historical; this
folder supersedes their ring-course-first combat sequence.

## Reproduce the art

From the repository root:

```bash
Scripts/build.sh jet
Scripts/import_jet_art.sh
Scripts/play.sh jet
# V or D-pad left selects the next original airframe.
Scripts/jet_flight_test.sh -RequireJetArt
Scripts/jet_art_tour.sh
```

The committed GLBs can be imported without Blender. To regenerate them from
original source, run Scripts/make_jets.sh first. See [Art provenance](../../Art/README.md).
Generated Unreal assets and screenshots are ignored by repository policy.
