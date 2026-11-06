#!/usr/bin/env python3
"""Run isolated per-gauge comparisons and retain every result under bin/."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess

ROOT = Path('bin/gauges-all')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--count', type=int, default=20000)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--hangs', action='store_true')
    parser.add_argument('names', nargs='*')
    args = parser.parse_args()
    if args.hangs:
        failed = []
        cases = [(name, bits, side) for name in ('SCHCMP', 'SOPCOMP')
                 for bits in ('7ff8000000000000', '7ff0000000000000', 'fff0000000000000', '7e37e43c8800759c')
                 for side in range(2)]
        def hang(case):
            name, bits, side = case
            environment = dict(os.environ, GAUGES_HANG_SIDE=str(side), GAUGES_HANG_BITS=bits)
            path = ROOT / name / f'hang-{bits}-{side}.log'
            with path.open('w') as log:
                result = subprocess.run(['wine', str(ROOT / 'allgaugesdiff.exe'), '1', name],
                                        env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=30)
            return case, result.returncode, path.read_text()
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for case, code, output in pool.map(hang, cases):
                passed = code == 77 and 'HANG ' in output
                print(f'{case}: {"matched retail hang" if passed else "FAIL"}', flush=True)
                if not passed: failed.append(case)
        return bool(failed)
    gauges = json.loads((ROOT / 'manifest.json').read_text())
    if args.names:
        gauges = [gauge for gauge in gauges if gauge['name'] in args.names]
    def run(gauge):
        command = ['wine', str(ROOT / 'allgaugesdiff.exe'), str(args.count), gauge['name']]
        suffix = '-relocated' if os.environ.get('GAUGES_RELOCATE') else ''
        log_path = ROOT / gauge['name'] / f'diff{suffix}.log'
        try:
            with log_path.open('w') as log:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, text=True, timeout=1200)
            output, code = log_path.read_text(), result.returncode
        except subprocess.TimeoutExpired as error:
            output, code = str(error), 124
        log_path.write_text(output)
        return gauge['name'], code, output
    failed = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for name, code, output in pool.map(run, gauges):
            if code:
                failed.append(name)
            print(f'{name}: {"PASS" if not code else "FAIL"} ' + output.strip().split('\n')[0], flush=True)
    suffix = '-relocated' if os.environ.get('GAUGES_RELOCATE') else ''
    (ROOT / f'check-summary{suffix}.json').write_text(json.dumps({'count': args.count, 'gauges': len(gauges), 'failed': failed}, indent=2))
    print(f'{len(gauges) - len(failed)}/{len(gauges)} gauges pass; failed: {", ".join(failed) or "none"}')
    return bool(failed)


if __name__ == '__main__':
    raise SystemExit(main())
