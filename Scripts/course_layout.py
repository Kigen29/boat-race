from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parents[1] / "games/BoatRace/Scripts/course_layout.py"), run_name="__main__")
