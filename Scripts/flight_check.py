from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parents[1] / "games/JetFighter/Scripts/flight_check.py"), run_name="__main__")
