#!/usr/bin/env python3
"""Prove the runtime harness rejects defects in four independent behaviors."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT=Path(__file__).resolve().parent.parent
DIRECTORY=ROOT/'bin/host-runtime'


def main():
    source=(DIRECTORY/'host_runtime.c').read_text()
    mutations={
        'timer-callback': ('198f',';'),
        'startup-import': ('7e72','mdl_write(f,(f->esi+4),0,32);'),
        'init-once': ('7cf8','mdl_write(f,(f->edx+4),0x1234,32);'),
        'shutdown-order': ('7d24','f->ecx=mdl_read(f,f->eax,32);'),
    }
    environment=os.environ.copy()
    environment.update(WINEPREFIX=str(DIRECTORY/'test-prefix'),WINEDEBUG='-all')
    baseline=subprocess.run(['wine','hostruntime.exe','200'],cwd=DIRECTORY,
                            env=environment,capture_output=True,text=True,timeout=60)
    if baseline.returncode or '600 timer/lifecycle/divide comparisons, 0 differ' not in baseline.stdout:
        raise RuntimeError('baseline failed: '+baseline.stdout+baseline.stderr)
    results={}
    for name,(address,replacement) in mutations.items():
        directory=DIRECTORY/'breaks'/name
        directory.mkdir(parents=True,exist_ok=True)
        pattern=r'(label_'+address+r':[^\n]*\n)[^\n]*'
        broken,count=re.subn(pattern,lambda match:match[1]+replacement,source)
        if count!=1:
            raise ValueError(f'{name}: expected one instruction, found {count}')
        path=directory/'runtime.c'
        path.write_text(broken)
        flags=['-std=c99','-O1','-fno-strict-aliasing','-Iinclude','-Isrc/host','-masm=intel']
        subprocess.run(['i686-w64-mingw32-gcc',*flags,'-c',str(path),'-o',str(directory/'runtime.o')],cwd=ROOT,check=True)
        subprocess.run(['i686-w64-mingw32-gcc',*flags,'-shared','-Wl,--disable-dynamicbase',
                        '-Wl,--image-base,0x43000000','-o',str(directory/'HOST_ours.dll'),
                        'src/host/host_main.c','src/host/host_runtime_native.S',str(directory/'runtime.o'),
                        str(DIRECTORY/'host_entries.S'),str(DIRECTORY/'host_services.o'),
                        'tools/host_runtime_test.def','-lmsvcrt','-lgdi32','-luser32','-lwinmm',
                        '-ladvapi32','-lmsacm32','-lddraw','-lkernel32'],cwd=ROOT,check=True)
        for filename in ('hostruntime.exe','FLTSIM98.EXE','mfc42.dll'):
            shutil.copyfile(DIRECTORY/filename,directory/filename)
        run=subprocess.run(['wine','hostruntime.exe','200'],cwd=directory,env=environment,
                           capture_output=True,text=True,timeout=60)
        (directory/'result.log').write_text(run.stdout+run.stderr)
        # A modeled-state mismatch or the harness's explicit crash exit catches
        # a mutation. Loader/build failures do not count as coverage.
        caught=(run.returncode==1 and 'differ' in run.stdout) or (
            run.returncode==5 and 'hostdiff: fault' in run.stdout)
        results[name]={'caught':caught,'returncode':run.returncode,'output':run.stdout.strip()}
        print(f'{name}: {"caught" if caught else "MISSED"}',flush=True)
        if not caught:
            raise RuntimeError(f'{name}: {run.stdout} {run.stderr}')
    (DIRECTORY/'breaks/results.json').write_text(json.dumps(results,indent=2)+'\n')


if __name__=='__main__':
    main()
