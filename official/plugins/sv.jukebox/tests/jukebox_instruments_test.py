"""Run the instruments driver on the default instruments.ini of the plugin."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
driver = Path(sys.argv[1]).resolve().parent / 'sv.jukebox/tests/jukebox-instruments-test.exe'
result = subprocess.run([str(driver), str(ROOT / 'instruments.ini')], capture_output=True, text=True, timeout=30)
print(result.stdout, end='')
assert result.returncode == 0, result.stdout + result.stderr
