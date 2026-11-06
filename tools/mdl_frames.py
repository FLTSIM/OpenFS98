#!/usr/bin/env python3
"""Frame-compare all eight normal MDLs in private, otherwise-retail games.

Uses a view/control script. Each flight must load its intended
model on both retail runs and the rebuilt run, and all frames must match.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import re
import subprocess

from build_mdls import NAMES

FLIGHTS = ('HONGKONG', 'BELLWTC', 'STGEORGE', 'PARIS', 'EXTRARIO', 'LEARLON', 'CHELAN', 'WAIKIKI')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('bin/mdl-frames'))
    parser.add_argument('--passes', type=int, default=600)
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--manifest', type=Path, default=Path('bin/mdl-diff/modules.json'))
    parser.add_argument('--flight', choices=FLIGHTS, action='append')
    args = parser.parse_args()
    if args.passes < 600:
        parser.error('at least 600 passes are needed for the complete models script')
    if not 1 <= args.jobs <= 4:
        parser.error('jobs must be between 1 and 4')
    display = os.environ.get('MDL_GAME_DISPLAY', ':76')
    if args.jobs > 1 and not re.fullmatch(r':\d+', display):
        parser.error('parallel jobs need a display base such as MDL_GAME_DISPLAY=:76')
    records = json.loads(args.manifest.read_text())
    if tuple(Path(r['installed']).stem for r in records) != NAMES:
        raise ValueError('expected the eight reference normal models')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / 'summary.txt').write_text('')
    subprocess.run(['python3', '-B', 'tools/fly_scripts.py', 'bin/mdl-diff/scripts'], check=True)
    summaries = []
    def run_flight(index, flight, record):
        flight_display = display if args.jobs == 1 else ':' + str(int(display[1:]) + index)
        env = dict(os.environ, HG2D_FRAMES_MDLS='1', HG2D_FRAMES_NOSOUND='1',
                   HG2D_FRAMES_OUT=str(output / flight),
                   HG2D_FRAMES_MDL_MANIFEST=str(args.manifest.resolve()),
                   HG2D_FRAMES_DISPLAY=flight_display)
        result = subprocess.run(['tools/hg2d_frames.sh', f'Situatio\\{flight}.STN',
                                 str(args.passes), 'bin/mdl-diff/scripts/models.txt'],
                                env=env, text=True, capture_output=True)
        transcript = result.stdout + result.stderr
        (output / (flight + '.result.txt')).write_text(transcript)
        if result.returncode:
            raise RuntimeError(f'{flight}: frame comparison failed ({result.returncode})\n{transcript}')
        wanted = Path(record['installed']).name
        pattern = re.compile(r'Loaded L"[^"\n]*' + re.escape(wanted) + r'"', re.IGNORECASE)
        for side in ('retail-a', 'retail-b', 'ours'):
            log = (output / flight / side / 'game.log').read_text(errors='replace')
            if not pattern.search(log):
                raise RuntimeError(f'{flight}/{side}: intended model {wanted} never loaded')
        return transcript, f'{flight}: {wanted}, {args.passes} passes, intended model loaded on all sides, identical frames'

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_flight, index, flight, record)
                   for index, (flight, record) in enumerate(zip(FLIGHTS, records))
                   if not args.flight or flight in args.flight]
        for future in as_completed(futures):
            transcript, summary = future.result()
            summaries.append(summary)
            (output / 'summary.txt').write_text('\n'.join(sorted(summaries)) + '\n')
            print(transcript + summary, flush=True)


if __name__ == '__main__':
    main()
