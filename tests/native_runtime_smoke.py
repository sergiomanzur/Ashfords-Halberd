"""Headless application verification against the user's imported retail assets."""
import os
from pathlib import Path
import subprocess
import sys

exe, rooms, system, profile = map(lambda value: Path(value).resolve(), sys.argv[1:])
if profile.exists():
    raise SystemExit('Use a fresh diagnostic profile directory')
profile.mkdir(parents=True)
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_RENDER_DRIVER='software')
result = subprocess.run([str(exe), '--smoke-test', '--runtime-smoke-test',
                         '--archive', str(rooms), '--system', str(system),
                         '--data-dir', str(profile)], env=env,
                        capture_output=True, timeout=180)
assert result.returncode == 0, (result.stdout, result.stderr,
    (profile/'last-error.txt').read_text() if (profile/'last-error.txt').exists() else '')
report = dict(line.split('=', 1) for line in (profile/'runtime-smoke.txt').read_text().splitlines())
assert int(report['ticks']) >= 240
assert int(report['rendered_frames']) == 240
assert int(report['movement_ticks']) > 0
assert int(report['focus_blocked_ticks']) == 10
assert report['menu_return'] == '1'
assert (profile/'AshfordsHalberd.ini').exists()
print('Headless native application runtime passed:', report)
