# Boat Race: comparison and implemented improvements

Research checked 16 November 2026 against publisher/developer descriptions.

[Hydro Thunder Hurricane — Vector Unit](https://www.vectorunit.com/hydro-thunder-hurricane/)
emphasises themed environments, dynamic water, varied events, shortcuts,
collectibles and unlockable content.

[Riptide GP: Renegade — publisher description on Nintendo](https://www.nintendo.com/en-gb/Games/Nintendo-Switch-download-software/Riptide-GP-Renegade-1314620.html)
describes distinct environments and interactive obstacles, career progression,
vehicle upgrades, ghost challenges, online racing and local split-screen.

## What that suggests for this game

Boat Race already has moving water, hazards, boost, upgrades, ghosts, time
trial, elimination and LAN racing. Adding another copy of those systems would
do less for replay value than course variety and making existing options clear.
This prioritisation is a design judgement from the comparison, not a claim
that the reference games use this menu design.

Implemented in this pass:

- Harbour Sprint: a shorter 2.48 km circuit with lower, gentler banks and
  quicker harbour turns.
- Canyon Run: a longer 3.28 km circuit with more cliff-lined banks and sweeping
  corners. Both have their own checkpoint, obstacle and barge layouts.
- All three circuits support forward and reverse racing, for six layouts.
- A route silhouette, course profile and personal-best lap in race setup.
- Independent lap/race records and ghosts for each course and direction;
  existing Bay Circuit saves retain their original keys and filenames.
- One consistent Back path for Escape, gamepad Back and on-screen Back,
  including Garage and Leaderboard. Menu lists fit the available height.
- Course selection follows the host to clients and survives opening a host
  session from the title screen.

Environment rework, 19 November 2026:

- Bay Circuit now uses low palm-covered tropical banks, pale sand, small
  offshore islets and turquoise water. The mixed alpine/flowering forest is gone.
- Harbour Sprint uses reclaimed concrete ground, stacked containers, port
  warehouses, cargo cranes and steel navigation structures. It has cooler
  light and water, and no forest, cottages, lighthouse or leisure traffic.
- Canyon Run uses high terraced sandstone walls, rock mesas, sparse cacti,
  boulders and a natural rock arch. The lighting, water and minimap use a
  separate desert palette, with no shared cottages or leisure traffic.
- Nine new procedural models are generated and imported by
  `Scripts/build_environments.sh`. Matching screenshots come from actual
  gameplay via `Scripts/environment_test.sh`.

## Remaining gaps

A multi-race championship, optional branching shortcuts and local split-screen
would be worthwhile future work. They are not included in this change.
Cross-device Wi-Fi play and hands-on tuning remain necessary; automatic
geometry, navigation, screenshot and network tests cannot establish race feel.
