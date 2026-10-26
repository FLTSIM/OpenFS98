#!/usr/bin/env python3
"""Require initializer transcript/state differences, including CString cleanup."""
from pathlib import Path
import subprocess


def main():
    pOutput = Path('bin/world-diff/init-breaks')
    pOutput.mkdir(parents=True, exist_ok=True)
    dSources = {pPath.name: pPath.read_bytes() for pPath in Path('src/world').iterdir()
                if pPath.suffix in ('.c', '.h', '.inc')}
    for pcFile in ('worldinitdiff.c', 'worlddiff.c'):
        dSources[pcFile] = (Path('tools/difftest') / pcFile).read_bytes().replace(b'../../src/world/', b'')
    aMutations = [
        ('cstring-unwind', 's_pInitServices->pfnStringDestroy(pState->pString);', '(void)pState;'),
        ('cstring-ownership', 'stState.iLive = 1;', 'stState.iLive = 0;'),
        ('cstring-release', 'pfnStringRelease(&uiString, -1)', 'pfnStringRelease(&uiString, 0)'),
        ('resource-id', '0xf8d5, pcBuffer', '0xf8d6, pcBuffer'),
        ('empty-label-control', 'pfnSetText(AT(pObject, 0x12c0)', 'pfnSetText(AT(pObject, 0x13a8)'),
        ('edit-color', '0x1258), pFont, 0xffffff', '0x1258), pFont, 0x00ff00'),
        ('position-limit', '0x19c0), 0xc5, 4, 0', '0x19c0), 0xc5, 3, 0'),
        ('reload-enabled', 's_auiEnable[uiIndex]), iEnabled)', 's_auiEnable[uiIndex]), iEnabled && WORLD_U32(pObject, 0x10e0) != 0)'),
        ('skin-result', 'if (!((int (FS98_WORLD_THISCALL', 'if (((int (FS98_WORLD_THISCALL'),
        ('init-return', '    return 1;\n}', '    return 0;\n}'),
    ]
    uiCaught = 0
    for pcName, pcBefore, pcAfter in aMutations:
        pWork = pOutput / pcName
        pWork.mkdir(exist_ok=True)
        for pcFile, byData in dSources.items():
            if pcFile == 'world_init.c':
                if byData.count(pcBefore.encode()) != 1:
                    raise ValueError(f'{pcName}: anchor is not unique')
                byData = byData.replace(pcBefore.encode(), pcAfter.encode(), 1)
            (pWork / pcFile).write_bytes(byData)
        pExe = pWork / 'worldinitdiff.exe'
        oBuild = subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
            '-fno-strict-aliasing', '-o', str(pExe), str(pWork / 'worldinitdiff.c'),
            str(pWork / 'world_init.c'), str(pWork / 'world_area.c'), '-lntdll'], capture_output=True, text=True)
        if oBuild.returncode:
            raise RuntimeError(f'{pcName}: build failed, not counted\n{oBuild.stderr}')
        oResult = subprocess.run(['wine', str(pExe), '32', 'unwind'], capture_output=True, text=True, timeout=120)
        (pWork / 'result.log').write_text(oResult.stdout + oResult.stderr)
        if oResult.returncode != 1 or 'WORLD INIT DIFFER' not in oResult.stderr:
            raise RuntimeError(f'{pcName}: no actual difference (exit {oResult.returncode})\n{oResult.stderr}')
        uiCaught += 1
        print(f'{pcName}: detected', flush=True)
    print(f'World initializer mutations: {uiCaught}/{len(aMutations)} detected')


if __name__ == '__main__':
    main()
