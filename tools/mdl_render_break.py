#!/usr/bin/env python3
"""Prove exterior-view frame acceptance sees the rebuilt aircraft geometry.

Rotate the RG model's rendered heading by 90 degrees, then require a
deterministic retail baseline and a rendered-frame mismatch, with clean exits.
The normal build and the user's game remain untouched.
"""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from build_mdls import build


def main():
    root = Path.cwd()
    output = root / 'bin/mdl-render-break'
    directory = output / 'mutation'
    directory.mkdir(parents=True, exist_ok=True)
    records = json.loads(Path('bin/mdl-diff/modules.json').read_text())
    record = records[2]
    original = root / 'bin/mdl-diff' / Path(record['path']).stem
    for file in original.iterdir():
        if file.name != 'build.log': shutil.copy2(file, directory / file.name)
    assembly = (directory / 'model_data.S').read_text().replace(str(original), str(directory))
    (directory / 'model_data.S').write_text(assembly)
    source = Path('src/mdl/mdl_normal.c').read_text()
    anchor = 'copy(state + 0x28, global + 0x5b0, 0x18);'
    if source.count(anchor) != 1:
        raise ValueError('model pose-copy anchor changed')
    # The copied pose's heading is at state +0x24 (GLOBAL +0x580).
    source = source.replace(anchor, anchor + '\n    S32(0x24, G32(0x580) + 0x40000000u);')
    modified = directory / 'mdl_normal.c'
    modified.write_text(source)
    record['rebuilt'] = str(directory / Path(record['rebuilt']).name)
    Path(record['rebuilt']).unlink(missing_ok=True)
    build(record, directory, update_source=modified)
    manifest = output / 'modules.json'
    manifest.write_text(json.dumps(records, indent=2) + '\n')
    subprocess.run(['python3', '-B', 'tools/fly_scripts.py', 'bin/mdl-diff/scripts'], check=True)
    env = dict(os.environ, HG2D_FRAMES_MDLS='1', HG2D_FRAMES_NOSOUND='1',
               HG2D_FRAMES_OUT=str(output / 'frames'), HG2D_FRAMES_MDL_MANIFEST=str(manifest),
               HG2D_FRAMES_DISPLAY=os.environ.get('MDL_GAME_DISPLAY', ':76'))
    result = subprocess.run(['tools/hg2d_frames.sh', 'Situatio\\STGEORGE.STN', '600',
                             'bin/mdl-diff/scripts/models.txt'], env=env, text=True, capture_output=True)
    transcript = result.stdout + result.stderr
    (output / 'result.txt').write_text(transcript)
    print(transcript, end='', flush=True)
    if (result.returncode != 1 or 'retail against retail: identical' not in transcript or
            not re.search(r'retail against ours:\s+line ', transcript)):
        raise RuntimeError('heading mutation did not cause the expected deterministic frame mismatch')
    for side in ('retail-a', 'retail-b', 'ours'):
        if not re.search(side + r': exit 0, .*end 00000258', transcript):
            raise RuntimeError(f'{side}: expected clean completion missing')
        log = (output / 'frames' / side / 'game.log').read_text(errors='replace')
        if not re.search(r'Loaded L"[^"\n]*CESSNA182RRG_N\.MDL"', log, re.IGNORECASE):
            raise RuntimeError(f'{side}: RG model not loaded')
    print('Rendered-heading mutation caught by frames; all three games completed cleanly.')


if __name__ == '__main__':
    main()
