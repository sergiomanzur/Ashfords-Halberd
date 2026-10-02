"""Run shipped executable checks; optionally validate/import a real local USA disc."""
import argparse
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import io
import pycdlib

parser = argparse.ArgumentParser()
parser.add_argument('exe', type=Path)
parser.add_argument('root', type=Path)
parser.add_argument('--iso', type=Path)
parser.add_argument('--reuse-import', action='store_true')
args = parser.parse_args()
exe = args.exe.resolve()
root = args.root.resolve()
root.mkdir(parents=True, exist_ok=True)
package = root / 'standalone-package'
package.mkdir(exist_ok=True)
shutil.copyfile(exe, package / exe.name)
if (exe.parent / 'licenses').exists():
    shutil.copytree(exe.parent / 'licenses', package / 'licenses', dirs_exist_ok=True)
exe = package / exe.name
environment = os.environ.copy()
system_directory = ctypes.create_unicode_buffer(32768)
assert ctypes.windll.kernel32.GetSystemDirectoryW(system_directory, len(system_directory))
environment['PATH'] = system_directory.value
environment['SDL_VIDEODRIVER'] = 'dummy'
environment['SDL_RENDER_DRIVER'] = 'software'
fixtures = Path(__file__).resolve().parents[1] / 'local/test-fixtures'


def run(options, data, expected=0):
    result = subprocess.run([str(exe), *map(str, options), '--data-dir', str(data)],
                            cwd=root, env=environment, capture_output=True, timeout=180)
    assert result.returncode == expected, (options, result.returncode, result.stderr.decode(errors='replace'))
    return result


for name in ['wrong_hash', 'wrong_region', 'wrong_boot', 'truncated', 'unsafe', 'extent', 'cycle']:
    folder = root / name
    run(['--inspect-iso', fixtures / f'{name}.iso'], folder, 1)
    assert (folder / 'last-error.txt').exists()

unicode_iso = root / '日本語 disc.iso'
shutil.copyfile(fixtures / 'wrong_hash.iso', unicode_iso)
run(['--inspect-iso', unicode_iso], root / 'unicode', 1)
assert 'executable does not match' in (root / 'unicode/last-error.txt').read_text()
smoke = root / 'ユーザー profile'
run(['--smoke-test'], smoke)
assert (smoke / 'smoke.txt').exists()
assert '[Display]' in (smoke / 'AshfordsHalberd.ini').read_text()
run(['--check-cache'], root / 'empty', 1)

if args.iso:
    iso = args.iso.resolve()
    game = root / 'real-game'
    run(['--inspect-iso', iso], game)
    inventory = (game / 'inspection.txt').read_text().splitlines()
    assert inventory[1] == 'f6c26ed664c2de72627fea5323b0d6a931601e13'
    assert inventory[2] == '51 files'
    if not args.reuse_import:
        run(['--import-iso', iso], game)
    run(['--check-cache'], game)
    incomplete = pycdlib.PyCdlib()
    incomplete.new(interchange_level=3)
    for name in ['SLUS_201.84', 'SYSTEM.CNF', 'DISCID.BIN']:
        data = (game / 'assets' / name).read_bytes()
        incomplete.add_fp(io.BytesIO(data), len(data), iso_path=f'/{name};1')
    incomplete_path = root / 'incomplete-retail.iso'
    incomplete.write(str(incomplete_path))
    incomplete.close()
    run(['--inspect-iso', incomplete_path], root / 'incomplete-retail', 1)
    ini = game / 'AshfordsHalberd.ini'
    before = ini.read_bytes()
    run(['--import-iso', fixtures / 'wrong_hash.iso'], game, 1)
    assert ini.read_bytes() == before, 'failed replacement changed working settings'
    run(['--check-cache'], game)
    ini.write_text('\n'.join('iso_path=Z:/Missing/CVX.iso' if line.startswith('iso_path=') else line
                            for line in ini.read_text().splitlines()) + '\n', encoding='utf-8')
    assert 'iso_path=Z:/Missing/CVX.iso' in ini.read_text()
    run(['--check-cache'], game)
    run(['--smoke-test'], game)
    # Cache validation must catch an incomplete imported file.
    asset = game / 'assets/DISCID.BIN'
    backup = asset.read_bytes()
    asset.write_bytes(backup[:1])
    run(['--check-cache'], game, 1)
    asset.write_bytes(backup)
    run(['--check-cache'], game)
    manifest = game / 'assets/manifest.txt'
    manifest_backup = manifest.read_bytes()
    manifest.write_bytes(b'\n'.join(line for line in manifest_backup.splitlines() if not line.startswith(b'BGM1.AFS\t')) + b'\n')
    run(['--check-cache'], game, 1)
    manifest.write_bytes(manifest_backup)
    # Reproduce a process interruption after the old cache has been moved aside.
    (game / 'assets').rename(game / 'assets.previous')
    run(['--check-cache'], game)
    assert (game / 'assets').is_dir()
    assert not (game / 'assets.previous').exists()
    print('Real USA disc inspection, import, failed-replacement preservation, cache reuse, and incomplete-cache rejection passed.')

print('Packaged executable checks passed (including Unicode paths and a different working directory).')
