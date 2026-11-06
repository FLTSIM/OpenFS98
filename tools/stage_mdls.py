#!/usr/bin/env python3
"""Copy a generated normal/crash MDL set into an explicitly selected game tree.

All inputs are checked before any copy. Private frame comparisons restore
the companion set to retail; --all installs or restores both families together.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

from build_mdls import NAMES
from gen_mdl_crash import CRASH_NAMES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--retail', action='store_true')
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('--manifest', type=Path)
    selection.add_argument('--all', action='store_true', help='install or restore all eleven MDLs')
    args = parser.parse_args()
    manifest = args.manifest or Path('bin/mdl-diff/modules.json')
    records = json.loads(manifest.read_text())
    names=tuple(Path(record['installed']).stem for record in records)
    if names not in (NAMES,CRASH_NAMES):
        raise ValueError('expected a complete normal or crash reference model set')
    family='normal' if names==NAMES else 'crash'
    selected=[(record,args.retail) for record in records]
    companion=Path('bin/mdl-crash/modules.json' if family=='normal' else 'bin/mdl-diff/modules.json')
    if args.all or companion.is_file():
        others=json.loads(companion.read_text())
        expected=CRASH_NAMES if family=='normal' else NAMES
        if tuple(Path(record['installed']).stem for record in others)!=expected:
            raise ValueError('companion model manifest changed')
        selected += [(record,args.retail if args.all else True) for record in others]
    plan = []
    for record,retail in selected:
        relative = Path(record['installed'])
        if relative.is_absolute() or '..' in relative.parts or relative.parts[0] != 'Aircraft':
            raise ValueError('invalid model install path')
        original = Path('orig') / relative
        source = original if retail else Path(record['rebuilt'])
        target = args.game / relative
        if not source.is_file() or not target.is_file():
            raise ValueError(f'missing model input or destination: {source}, {target}')
        if hashlib.sha256(original.read_bytes()).hexdigest() != record['sha256']:
            raise ValueError(f'reference model changed: {original}')
        plan.append((source, target))
    for source, target in plan:
        temporary = target.with_name(target.name + '.openfs98-tmp')
        shutil.copy2(source, temporary)
        temporary.replace(target)
        if source.read_bytes() != target.read_bytes():
            raise ValueError(f'copied model differs: {target}')
    if args.all:
        print(f'{len(plan)} {"retail" if args.retail else "rebuilt"} MDLs staged in {args.game}')
    else:
        print(f'{len(records)} {"retail" if args.retail else "rebuilt"} {family} MDLs staged in {args.game}; {len(plan)-len(records)} retail companions')


if __name__ == '__main__':
    main()
