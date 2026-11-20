# BoatRace + JetFighter

Two independently buildable Unreal Engine 5.9 games in one repository. They
share the `MaritimeCore` plugin, without depending on each other's gameplay
modules. BoatRace retains its armed boat races; JetFighter boots straight into
its own airborne flight prototype.

```text
games/
  BoatRace/                  BoatRace.uproject, Source, Config, Art, Scripts
  JetFighter/                JetFighter.uproject, Source, Config, Scripts, docs
packages/
  MaritimeCore/              Shared Unreal plugin: ocean, weather, water,
                             material helpers and generated Content
Scripts/                     Root build/play/test/package entry points
tools/                       Shared Unreal discovery and workspace checks
```

| Command from this folder | Result |
|---|---|
| `Scripts/build.sh boat` | Build BoatRaceEditor |
| `Scripts/build.sh jet` | Build JetFighterEditor |
| `Scripts/build.sh all` | Build both projects, in order |
| `Scripts/play.sh boat` | Open BoatRace's title screen |
| `Scripts/play.sh boat host` | Host a boat race |
| `Scripts/play.sh jet` | Start standalone JetFighter |
| `Scripts/test.sh all` | Numerical and engine tests for both games |
| `Scripts/package.sh boat` | `games/BoatRace/dist/BoatRace-Linux.tar.gz` |
| `Scripts/package.sh jet` | `games/JetFighter/dist/JetFighter-Linux.tar.gz` |

Set `UE_ROOT` to your Unreal installation if it is not found automatically.
Both projects discover the plugin through `AdditionalPluginDirectories`;
keep the repository structure intact when opening either `.uproject`.
Build each editor target before opening a project for the first time.
Builds, saves, logs, screenshots and packages belong to their respective game.
Generated project/plugin build output stays out of Git; shared generated
content belongs to the plugin directory.

Existing root boat commands such as `Scripts/setup.sh`, `Scripts/share.sh`,
`Scripts/net_test.sh` and `Scripts/play.sh host` remain compatibility wrappers.
`Scripts/play.sh jets` also launches the independent JetFighter project.
The BoatRace title screen no longer enters JetFighter in the same process.
JetFighter's **Esc / Start** opens pause; **Enter / A** resumes, **Q / B** quits.

## Shared assets

`MaritimeCore` owns `/MaritimeCore/Models` and `/MaritimeCore/Higgsfield`.
BoatRace's committed Blender sources remain in `games/BoatRace/Art`.
Run `Scripts/import_assets.sh` to regenerate the shared Unreal assets from
those sources. Both games have fallback shapes/materials when optional art is
missing. Imported `.uasset` files, downloads, caches and packaged builds stay
out of Git. Old `/Game/Models` and `/Game/Higgsfield` references are redirected
to the shared plugin for existing generated assets.

The shared water renderer accepts shoreline and wave-clock callbacks.
BoatRace supplies its coast and race clock; JetFighter uses open sea. Shared
code never includes either game's rules or terrain implementation.

## Current progress and next work

BoatRace has three circuits, LAN racing, computer rivals, combat, garage and
weather. JetFighter has local flight, mouse aiming, gamepad controls, a flight
HUD, island terrain, navigation landmarks, clouds, swept collisions, crash
recovery and airspace boundaries. Three selectable original aircraft prototypes
and a licensed Rajdhani HUD font replace the earlier box airframe. Aircraft
currently share one flight profile; controls need hands-on playtesting.

JetFighter now also has a local nose-gun drill: swept rounds, heat, real drone
hull damage, destruction and restart. LMB / gamepad A fires; T restarts.
Next for JetFighter: targeting, missiles, flares and a manoeuvring drone,
then an authored mountain-channel map, team objectives and four-player LAN.
The extensive research/art plan specifies original jets, lakes, canyons,
HUD, effects, sound and production gates. These further combat features remain planned.
Run `Scripts/import_jet_art.sh` after building to generate jet Unreal assets;
`Scripts/jet_art_tour.sh` checks all three. See games/JetFighter/Art for source.

- [BoatRace guide](games/BoatRace/README.md)
- [BoatRace multiplayer onboarding](games/BoatRace/ONBOARDING.md)
- [JetFighter controls and checks](games/JetFighter/README.md)
- [Jet research and production plan](games/JetFighter/docs/vision/00_README.md)
- [Shared plugin](packages/MaritimeCore/README.md)

Static workspace checks: `python3 tools/check_monorepo.py`.

## Verification

Verified on Linux with Unreal 5.9 preview-1, 20 November 2026: both editor
builds, BoatRace's seven and JetFighter's twelve engine tests, numerical
boat/jet checks, all three
course checks, BoatRace's 14-shot rendered tour (lowest sampled stop 48 fps at
1280 × 720), and JetFighter's flight/impact/respawn and pause/resume tours.
The new jet island-world survey measured 110–119 fps across 30 m, 300 m,
1.5 km and 4 km views at 1280 × 720.
Standalone packaging commands are provided; packaged releases and Windows
builds have not been validated for this layout yet.
