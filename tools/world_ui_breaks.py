#!/usr/bin/env python3
"""Snapshot and mutate native area DLLs; faults/timeouts never count as caught."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    pOutput = Path('bin/world-diff/ui-breaks')
    pOutput.mkdir(parents=True, exist_ok=True)
    dSources = {pPath.name: pPath.read_bytes() for pPath in Path('src/world').iterdir()
                if pPath.suffix in ('.c', '.h', '.inc', '.def')}
    dSources['worlduidiff.c'] = Path('tools/difftest/worlduidiff.c').read_bytes().replace(b'../../src/world/', b'')
    for pcFile in ('world-res.o', 'libworld-simsui.a'):
        dSources[pcFile] = (Path('bin/world-diff') / pcFile).read_bytes()
    (pOutput / 'inputs.json').write_text(json.dumps({pcName: hashlib.sha256(byData).hexdigest()
        for pcName, byData in dSources.items()}, indent=2) + '\n')
    aMutations = [
        ('edit-color', 'world_init.c', '0x1258), pFont, 0xffffff', '0x1258), pFont, 0x00ff00'),
        ('position-limit', 'world_init.c', '0x19c0), 0xc5, 4, 0', '0x19c0), 0xc5, 3, 0'),
        ('empty-path-enable', 'world_init.c', 'WORLD_U32(pObject, 0x10e0) = 0;', 'WORLD_U32(pObject, 0x10e0) = 1;'),
        ('layer-selection', 'world_area.c', 'uiLayer / 10 - 1, 0', 'uiLayer / 10, 0'),
        ('path-dispatch', 'world_runtime_tables.inc', 'fs98_world_area_path_changed(pObject);', '(void)pObject;'),
        ('cancel-answer', 'world_area.c', 'WORLD_U32(pDescriptor, 0) = 0;', 'WORLD_U32(pDescriptor, 0) = 1;'),
        ('accept-answer', 'world_area.c', 'WORLD_U32(WORLD_PTR(pObject, 0x10e4), 0) = 1;', 'WORLD_U32(WORLD_PTR(pObject, 0x10e4), 0) = 0;'),
        ('path-error-focus', 'world_area.c', 'pfnFocusControl((uint8 *)pObject + 0x1258);', 'pfnFocusControl((uint8 *)pObject + 0x1938);'),
        ('module-unwind-delete', 'world_runtime.c', '((void (*)(void *))world_mfc(825))(pState->pDynamic);', '(void)pState;'),
        ('module-deinit-clear', 'world_runtime.c', 's_uiModule = s_uiResourceModule = 0;', 's_uiResourceModule = 0;'),
    ]
    aRequested = set(sys.argv[1:])
    if aRequested - {tMutation[0] for tMutation in aMutations}:
        raise ValueError('unknown World native mutation')
    aJobs = [('baseline', None, None, None)] + [tMutation for tMutation in aMutations if not aRequested or tMutation[0] in aRequested]
    uiCaught = 0
    for pcName, pcFile, pcBefore, pcAfter in aJobs:
        pWork = pOutput / pcName
        pWork.mkdir(exist_ok=True)
        for pcSource, byData in dSources.items():
            if pcSource == pcFile:
                if byData.count(pcBefore.encode()) != 1:
                    raise ValueError(f'{pcName}: mutation anchor is not unique')
                byData = byData.replace(pcBefore.encode(), pcAfter.encode(), 1)
            (pWork / pcSource).write_bytes(byData)
        pDll, pHarness = pWork / 'WORLD_area.dll', pWork / 'worlduidiff.exe'
        aCommands = [
            ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing',
             '-fno-delete-null-pointer-checks', '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12',
             '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(pDll),
             *(str(pWork / f'world_{pcUnit}.c') for pcUnit in ('area', 'objects', 'init', 'runtime')),
             str(pWork / 'world.def'), str(pWork / 'world-res.o'), str(pWork / 'libworld-simsui.a'),
             '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
            ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing',
             '-o', str(pHarness), str(pWork / 'worlduidiff.c'), '-lgdi32', '-lntdll'],
        ]
        for aCommand in aCommands:
            oBuild = subprocess.run(aCommand, capture_output=True, text=True)
            if oBuild.returncode:
                print(f'{pcName}: BUILD FAILED (not counted)\n{oBuild.stderr}', flush=True)
                return 1
        try:
            oResult = subprocess.run(['wine', str(pHarness), str(pDll), '8'], capture_output=True, text=True, timeout=120)
        except subprocess.TimeoutExpired as oError:
            (pWork / 'result.log').write_bytes((oError.stdout or b'') + (oError.stderr or b''))
            print(f'{pcName}: TIMEOUT (not counted)', flush=True)
            return 1
        pcOutput = oResult.stdout + oResult.stderr
        (pWork / 'result.log').write_text(pcOutput)
        if pcName == 'baseline':
            if oResult.returncode or '0 differ' not in pcOutput:
                print(f'World native baseline FAILED\n{pcOutput}', flush=True)
                return 1
            print('World native baseline: passed', flush=True)
        elif oResult.returncode == 1 and ('WORLD UI DIFFER' in pcOutput or 'WORLD INIT DIFFER' in pcOutput):
            uiCaught += 1
            print(f'{pcName}: detected', flush=True)
        else:
            print(f'{pcName}: NOT DETECTED (exit {oResult.returncode})\n{pcOutput}', flush=True)
    print(f'World native mutations: {uiCaught}/{len(aJobs) - 1} detected', flush=True)
    return 0 if uiCaught == len(aJobs) - 1 else 1


if __name__ == '__main__':
    sys.exit(main())
