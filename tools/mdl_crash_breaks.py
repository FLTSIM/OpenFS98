#!/usr/bin/env python3
"""Build isolated crash-model defects and require a behavioral mismatch."""
import json
import os
from pathlib import Path
import subprocess

from gen_mdl_crash import CrashLift,build_crash


def replace_one(text,before,after):
    if text.count(before)!=1: raise ValueError(f'mutation anchor not unique: {before}')
    return text.replace(before,after)


def main():
    root=Path.cwd(); output=root/'bin/mdl-crash'
    original=json.loads((output/'modules.json').read_text())[0]
    lift=CrashLift(Path(original['path']))
    cases=['shared-tail-entry','angle-pointer','signed-multiply','subtract-carry',
           'skip-initialization','unsigned-cleanup','draw-phase','bank-boundary']
    env=dict(os.environ,WINEPREFIX=str(root/'bin/mdl-diff/wineprefix'),WINEARCH='win32',WINEDEBUG='-all')
    for name in cases:
        directory=output/'breaks'/name
        record=lift.emit(directory)
        record['rebuilt']=str(directory/'CESSNA182RRG_C.MDL')
        source=(directory/'crash.c').read_text()
        header=Path('src/mdl/mdl_crash_lift.h').read_text()
        if name=='shared-tail-entry':
            source=replace_one(source,'++mdl_coverage[12];\ngoto label_27dd;','++mdl_coverage[12];\ngoto label_1f0e;')
        elif name=='angle-pointer':
            before='(f->eax+0x2cu), 32), 3, 6);'
            if source.count(before)!=5: raise ValueError('sincos call sites changed')
            source=source.replace(before,'(f->eax+0x2cu), 32), 3, 7);')
        elif name=='signed-multiply':
            header=replace_one(header,'uint64_t product = is_signed ?','uint64_t product = 0 ?')
        elif name=='subtract-carry':
            header=replace_one(header,'(uint64_t)a - b - carry','(uint64_t)a - b')
        elif name=='skip-initialization':
            source=replace_one(source,'if (!(f->flags & 0x40)) goto label_1103;','goto label_1103;')
        elif name=='unsigned-cleanup':
            source=replace_one(source,'if ((!!(f->flags & 0x80) != !!(f->flags & 0x800))) goto label_114c;',
                               'if (f->flags & 1) goto label_114c;')
        elif name=='draw-phase':
            source=replace_one(source,'mdl_arith(f, mdl_read(f, (f->esi+0x80u), 16), 0x2u, 16, 1, 0);',
                               'mdl_arith(f, mdl_read(f, (f->esi+0x80u), 16), 0x3u, 16, 1, 0);')
        elif name=='bank-boundary':
            source=source.replace('0x15550000u, 32, 1, 0);','0x1554ffffu, 32, 1, 0);')
            if source==(directory/'crash.c').read_text(): raise ValueError('bank boundary moved')
        (directory/'crash.c').write_text(source)
        (directory/'mdl_crash_lift.h').write_text(header)
        Path(record['rebuilt']).unlink(missing_ok=True)
        build_crash(record,directory,'-O1')
        result=subprocess.run(['wine',str(output/'mdlcrashdiff.exe'),'128','relocate',record['rebuilt']],
                              env=env,text=True,capture_output=True,timeout=120)
        transcript=result.stdout+result.stderr
        (directory/'test.log').write_text(transcript)
        markers=('crash state','crash data','crash GLOBAL','crash calls','event count')
        if result.returncode!=1 or not any(marker in transcript for marker in markers):
            raise RuntimeError(f'{name}: expected behavioral mismatch missing\n{transcript}')
        print(name+': caught ('+transcript.splitlines()[-1]+')',flush=True)


if __name__=='__main__': main()
