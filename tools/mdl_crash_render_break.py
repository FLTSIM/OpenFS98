#!/usr/bin/env python3
"""Require a valid crash-pose mutation of the decompiled C182RG crash model to change
real-game rendered frames: a quarter turn of the state's heading and every piece's."""
import json
import os
from pathlib import Path
import re
import subprocess

from gen_mdl_crash import build_hand


def main():
    root=Path.cwd();output=root/'bin/mdl-crash-render-break';directory=output/'mutation'
    records=json.loads(Path('bin/mdl-crash/modules.json').read_text())
    record=dict(records[0]); directory.mkdir(parents=True,exist_ok=True)
    source=Path('src/mdl/mdl_crash.c').read_text()
    anchor='    crash_step(st, G32(0xd48));\n}'
    if source.count(anchor)!=1: raise ValueError('crash update boundary changed')
    source=source.replace(anchor,'    crash_step(st, G32(0xd48));\n'
        '    { int i; SET_S32(0x24, S32(0x24) + 0x40000000u);\n'
        '      for (i = 0; i < 13; i++) SET_S32(0x84 + i * 0x24 + 0x20, S32(0x84 + i * 0x24 + 0x20) + 0x40000000u); }\n}')
    (directory/'mdl_crash.c').write_text(source)
    record['rebuilt']=str((directory/'CESSNA182RRG_C.MDL').resolve())
    Path(record['rebuilt']).unlink(missing_ok=True)
    build_hand(record,Path('bin/mdl-crash')/'CESSNA182RRG_C',0,source=directory/'mdl_crash.c')
    records[0]=record
    manifest=output/'modules.json';manifest.write_text(json.dumps(records,indent=2)+'\n')
    subprocess.run(['python3','-B','tools/fly_scripts.py','bin/mdl-diff/scripts'],check=True)
    env=dict(os.environ,HG2D_FRAMES_MDLS='1',HG2D_FRAMES_CRASH_TEST='1',HG2D_FRAMES_NOSOUND='1',
             HG2D_FRAMES_EXPECT_MDL='CESSNA182RRG_C.MDL',HG2D_FRAMES_MDL_MANIFEST=str(manifest),
             HG2D_FRAMES_OUT=str(output/'frames'),HG2D_FRAMES_DISPLAY=os.environ.get('MDL_GAME_DISPLAY',':91'))
    result=subprocess.run(['tools/hg2d_frames.sh','Situatio\\ANTILLES.STN','1200','bin/mdl-diff/scripts/crash_models.txt'],
                          env=env,text=True,capture_output=True)
    transcript=result.stdout+result.stderr;(output/'result.txt').write_text(transcript)
    print(transcript,end='',flush=True)
    if result.returncode!=1 or 'retail against retail: identical' not in transcript or not re.search(r'retail against ours:\s+line ',transcript):
        raise RuntimeError('crash pose mutation did not produce the expected frame mismatch')
    for side in ('retail-a','retail-b','ours'):
        if not re.search(side+r': exit 0, .*end 000004b0',transcript):
            raise RuntimeError('crash pose test did not complete cleanly')
    print('Crash pose mutation caught by frames; all three games completed 1200 passes cleanly.')


if __name__=='__main__': main()
