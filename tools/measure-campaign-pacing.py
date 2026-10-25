"""Repeatable Windows display-mode frame-time measurements (no player saves)."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import subprocess


def percentile(values, percent):
    ordered = sorted(values)
    at = (len(ordered) - 1) * percent / 100
    lo = int(at)
    hi = min(lo + 1, len(ordered) - 1)
    return ordered[lo] + (ordered[hi] - ordered[lo]) * (at - lo)


def summarize(path, warmup_frame, hz):
    rows = []
    for line in Path(path).read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        fields = line.split()
        if int(fields[0]) >= warmup_frame:
            rows.append([float(v) for v in fields])
    if len(rows) < 30:
        raise RuntimeError(f'Too few timed presents in {path}: {len(rows)}')
    # Legacy trace: frame, interpolation, CPU start us, swap-block us.
    # New trace adds actual CPU completion us after rendering and buffer swap.
    column = 4 if len(rows[0]) >= 5 else 2
    intervals = [(b[column] - a[column]) / 1000 for a, b in zip(rows, rows[1:])]
    tick = 1000 / hz
    return dict(samples=len(intervals), timestamp='completion' if column == 4 else 'start',
                mean_ms=statistics.mean(intervals), median_ms=statistics.median(intervals),
                p95_ms=percentile(intervals, 95), p99_ms=percentile(intervals, 99),
                max_ms=max(intervals), stddev_ms=statistics.pstdev(intervals),
                over_1_5_refresh=sum(v > tick * 1.5 for v in intervals),
                under_half_refresh=sum(v < tick * .5 for v in intervals),
                swap_mean_ms=statistics.mean(row[3] / 1000 for row in rows))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--iso', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--frames', type=int, default=900)
    parser.add_argument('--warmup-frame', type=int, default=300)
    parser.add_argument('--hz', type=float, default=60)
    parser.add_argument('--modes', nargs='+', default=['windowed', 'borderless', 'exclusive'])
    parser.add_argument('--vsync', nargs='+', type=int, choices=[0, 1], default=[1, 0])
    parser.add_argument('--repeat', type=int, default=1)
    args = parser.parse_args()
    exe, iso, output = args.executable.resolve(), args.iso.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for vsync in args.vsync:
        for mode in args.modes:
            if mode not in ('windowed', 'borderless', 'exclusive'):
                parser.error(f'Invalid display mode: {mode}')
            for repeat in range(args.repeat):
                folder = output / f'{mode}-vsync{vsync}-{repeat + 1}'
                folder.mkdir(parents=True, exist_ok=True)
                save = folder / 'save'
                save.mkdir(exist_ok=True)
                (save / 'recvx.ini').write_text(
                    f'show_fps = 0\ndifficulty_select = 0\nvsync = {vsync}\n'
                    'scale = 2\nfullscreen_w = 0\nfullscreen_h = 0\n', encoding='utf-8')
                env = {k: v for k, v in os.environ.items()
                       if not k.startswith(('RECVX_', 'ASHFORD_LAUNCHER_'))}
                env.update(RECVX_EXIT_FRAME=str(args.frames), RECVX_SKIP_MOVIES='1',
                           RECVX_TRACE_PRESENT=str(folder / 'presents.txt'), RECVX_TRACE_TASKS='1',
                           RECVX_INPUT='100:down,103:-,110:cross,113:-,160:cross,163:-,200:cross,203:-,240:cross,243:-,280:cross,283:-,320:start,323:-,360:circle,363:-,400:up,520:up+right,560:up,600:start,603:-,640:circle,643:-,700:up,800:up+circle')
                command = [str(exe), str(iso), '--skip-launcher', '--save-dir', str(save),
                           '--size', '1280x720', '--' + mode, '--skip-intro',
                           '--profile=' + str(folder / 'logic.csv')]
                print(f'Measuring {mode}, VSync {vsync}, run {repeat + 1}', flush=True)
                with (folder / 'runtime.log').open('w', encoding='utf-8') as log:
                    run = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                                         timeout=max(120, args.frames / 15), creationflags=0x08000000)
                runtime = (folder / 'runtime.log').read_text(encoding='utf-8', errors='replace')
                if run.returncode or 'test: exit at frame' not in runtime or 'tasks: Pad Game Event' not in runtime:
                    raise RuntimeError(f'Run failed or missed New Game: {folder}/runtime.log')
                result = dict(mode=mode, vsync=vsync, repeat=repeat + 1,
                              **summarize(folder / 'presents.txt', args.warmup_frame, args.hz))
                results.append(result)
                print(json.dumps(result), flush=True)
                (output / 'results.json').write_text(json.dumps(dict(
                    executable=str(exe), sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                    scene='Opening RM_0000, fixed cameras, deterministic controller script',
                    internal_scale=2, windowed_size='1280x720',
                    fullscreen_size='Desktop mode requested; startup selection recorded per runtime.log',
                    hz=args.hz, warmup_frame=args.warmup_frame, results=results), indent=2))
    with (output / 'results.csv').open('w', newline='') as target:
        writer = csv.DictWriter(target, fieldnames=results[0].keys())
        writer.writeheader()
        writer.writerows(results)


if __name__ == '__main__':
    main()
