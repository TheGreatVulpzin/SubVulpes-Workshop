"""Use the SDK runner and check the demo trace order twice."""
from pathlib import Path
import math
import os
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
SDK = Path(os.environ['SUBVULPES_SDK']).resolve()
BUILD = Path(sys.argv[1]).resolve()
plugin = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else BUILD / 'plugins/sv.jukebox'
tools = SDK / 'tools'
server = os.environ.get('SERVER_EXE')
assert server and Path(server).exists(), 'Set SERVER_EXE to subvulpes_server.exe (the Action does).'
command = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(tools / 'run_plugin_test.ps1'),
           '-Plugin', str(plugin), '-Scenario', str(ROOT / 'tests/jukebox-demo.svtest'),
           '-ServerExe', server, '-Runner', str(tools / 'subvulpes_plugin_test.exe'), '-Probe', str(tools / 'probe')]
expected = [(0, 0, 1, 60, 39, 1.0), (0, 0, 10, 36, 21, 0.25),
            (0, 15, 1, 64, 39, 1.25992105), (0, 30, 1, 67, 39, 1.49830708),
            (0, 30, 10, 42, 21, 2.0), (0, 45, 1, 72, 39, 2.0)]
pattern = re.compile(r'^\[sv\.jukebox\] JUKEBOX_SOUND tick=(\d+) at=(\d+) ch=(\d+) note=(\d+) sound=(\d+) pitch=([0-9.]+)', re.M)
# Windows PowerShell 5.1 fails to find its own commands when it starts inside PowerShell 7 and inherits its module path.
env = {k: v for k, v in os.environ.items() if k.upper() != 'PSMODULEPATH'}
previous = None
for _ in range(2):
    result = subprocess.run(command, capture_output=True, text=True, timeout=120, env=env)
    assert result.returncode == 0 and 'PASS jukebox-demo.svtest' in result.stdout, result.stdout + result.stderr
    actual = [tuple(map(int, row[:5])) + (float(row[5]),) for row in pattern.findall(result.stdout)]
    assert len(actual) == len(expected), (actual, result.stdout)
    for got, want in zip(actual, expected):
        assert got[:5] == want[:5] and math.isclose(got[5], want[5], rel_tol=0, abs_tol=0.000002), (got, want, result.stdout)
    if previous is not None:
        assert actual == previous, 'The demo gave different sound calls.'
    previous = actual
print('JUKEBOX_SCENARIO_PASS: SDK runner, exact sound order, tick, pitch and repeated trace')
