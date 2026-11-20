"""Validate project/plugin boundaries without needing Unreal installed."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
failures = []
ids = []
for name,mode in (("BoatRace", "BoatRaceGameMode"), ("JetFighter", "JetGameMode")):
    home = ROOT / "games" / name
    project = json.loads((home / f"{name}.uproject").read_text())
    if [m["Name"] for m in project["Modules"]] != [name]:
        failures.append(f"{name}: project must own only its game module")
    plugin_paths = [(home / p).resolve() for p in project.get("AdditionalPluginDirectories", [])]
    if ROOT / "packages" not in plugin_paths:
        failures.append(f"{name}: shared plugin directory missing")
    if not any(p["Name"] == "MaritimeCore" and p["Enabled"] for p in project["Plugins"]):
        failures.append(f"{name}: shared plugin must be enabled")
    for target in (name, name + "Editor"):
        if not (home / "Source" / f"{target}.Target.cs").is_file():
            failures.append(f"{name}: missing target {target}")
    config = (home / "Config/DefaultEngine.ini").read_text()
    if f"GlobalDefaultGameMode=/Script/{name}.{mode}" not in config:
        failures.append(f"{name}: wrong default game mode")
    ids.append(re.search(r"^ProjectID=(.+)$", (home / "Config/DefaultGame.ini").read_text(), re.M).group(1))
    other = "JetFighter" if name == "BoatRace" else "BoatRace"
    build = (home / "Source" / name / f"{name}.Build.cs").read_text()
    if f'"{other}"' in build or '"MaritimeCore"' not in build:
        failures.append(f"{name}: invalid runtime dependency")
    for p in (home / "Source").rglob("*"):
        if p.suffix in (".cpp", ".h"):
            text = p.read_text()
            if re.search(r'#include\s+["<](?:.*[/\\])?' + other + r'[^/\\"<>]*[">]', text):
                failures.append(f"{p.relative_to(ROOT)}: imports the other game")
if len(set(ids)) != 2:
    failures.append("Projects must have separate save/config identities")
for p in (ROOT / "packages/MaritimeCore/Source").rglob("*"):
    if p.suffix in (".cpp", ".h"):
        for include in re.findall(r'#include\s+"([^"]+)"', p.read_text()):
            if include in {"BoatRace.h", "BoatRaceGameState.h", "BayTerrain.h", "JetGame.h", "JetPawn.h"}:
                failures.append(f"{p.relative_to(ROOT)}: game dependency {include}")
for failure in failures:
    print("FAIL", failure)
print(f"MONOREPO CHECK: {len(failures)} failures")
raise SystemExit(bool(failures))
