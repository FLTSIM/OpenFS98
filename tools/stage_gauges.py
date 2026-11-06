#!/usr/bin/env python3
"""Install or restore the complete gauge collection using setup's filenames.

All inputs are checked before copying. Used by private frame comparisons and
the normal explicit install/revert targets; never changes other module files.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--retail', action='store_true')
    parser.add_argument('--manifest', type=Path, default=Path('bin/gauges-all/manifest.json'))
    parser.add_argument('--rebuilt', type=Path, default=Path('bin/gauges-all/Gauges'))
    args = parser.parse_args()
    gauges = json.loads(args.manifest.read_text())
    if len(gauges) != 120 or len({gauge['installed'] for gauge in gauges}) != 120:
        raise ValueError('expected the complete reference collection of 120 distinct gauges')
    destination = args.game / 'Gauges'
    if not destination.is_dir():
        raise ValueError('game/Gauges directory missing')
    plan = []
    for gauge in gauges:
        source = Path('orig/Gauges') / (gauge['name'] + '.GAU') if args.retail else args.rebuilt / gauge['installed']
        if not source.is_file():
            raise ValueError(f'gauge input missing: {source}')
        if args.retail and hashlib.sha256(source.read_bytes()).hexdigest() != gauge['sha256']:
            raise ValueError(f'reference gauge changed: {source}')
        plan.append((source, destination / gauge['installed']))
    for source, target in plan:
        temporary = target.with_name(target.name + '.openfs98-tmp')
        shutil.copy2(source, temporary)
        temporary.replace(target)
        if source.read_bytes() != target.read_bytes():
            raise ValueError(f'installed gauge differs: {target}')
    print(f'{len(plan)} {"retail" if args.retail else "rebuilt"} gauges staged in {destination}')


if __name__ == '__main__':
    main()
