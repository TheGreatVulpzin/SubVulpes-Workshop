# MIDI Jukebox (sv.jukebox)

Plays MIDI music with any game sound. Use it from the console of the server.

The source and the tests are in this folder. Build and test it with `tools\run_plugin_tests.ps1 -Only sv.jukebox` at the root of the repository.
The scenario test needs `SERVER_EXE`, the path of `subvulpes_server.exe`. The Action sets it.

`tests/jukebox_removal.py` and `tests/jukebox_v2_removal.py` are manual tools. They remove one bound check at a time, rebuild, and require the matching test to fail.
Run them by hand in an x86 build prompt with no other build running: `python -I tests\jukebox_v2_removal.py build\stage`.
