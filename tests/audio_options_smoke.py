"""Exercise the actual Options action and restart with a fresh headless profile."""
import configparser
import os
from pathlib import Path
import subprocess
import sys
import uuid

exe, scratch = map(lambda value: Path(value).resolve(), sys.argv[1:])
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_RENDER_DRIVER='software',
           SDL_AUDIODRIVER='dummy')


def run(profile, flags):
    result = subprocess.run([str(exe), '--smoke-test', *flags, '--data-dir', str(profile)],
                            env=env, capture_output=True, timeout=180)
    assert result.returncode == 0, (result.stdout, result.stderr,
        (profile/'last-error.txt').read_text() if (profile/'last-error.txt').exists() else '')
    config = configparser.ConfigParser()
    config.read(profile/'AshfordsHalberd.ini')
    return config.getint('Audio', 'effects_volume')


for initial, expected_steps in [(None, [0]), (95, [100, 0]), (99, [100, 0]),
                                (1, [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 0])]:
    profile = scratch / str(uuid.uuid4())
    if initial is not None:
        profile.mkdir(parents=True)
        (profile/'AshfordsHalberd.ini').write_text(
            f'[Audio]\neffects_volume={initial}\n', encoding='utf-8')
    for expected in expected_steps:
        assert run(profile, ('--audio-options-smoke-test',)) == expected
        assert (profile/'audio-options-smoke.txt').read_text().strip() == (
            f'effects_volume={expected}\nmenu_return=1')
        assert run(profile, ()) == expected
print('Headless main-menu effects volume range, mute and process restart passed')
