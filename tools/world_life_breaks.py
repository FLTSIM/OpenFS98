#!/usr/bin/env python3
"""Require actual differences for area lifetime, factory and unwind mutations."""
from pathlib import Path
import os
import subprocess

def main():
    pRoot = Path(__file__).resolve().parent.parent
    pOutput = pRoot / 'bin/world-diff/life-breaks'
    pOutput.mkdir(parents=True, exist_ok=True)
    pcSource = (pRoot / 'src/world/world_objects.c').read_text()
    aMutations = (
        ('base-template', 'pfnBaseConstruct(pObject, 0x6b3, pParent)',
         'pfnBaseConstruct(pObject, 0x6b4, pParent)', 'normal'),
        ('constructor-state', 'WORLD_U32(pObject, 0x10e0) = 1;',
         'WORLD_U32(pObject, 0x10e0) = 0;', 'normal'),
        ('missing-last-constructor', 'stState.uiConstructed < sizeof(s_astAreaControls) / sizeof(s_astAreaControls[0])',
         'stState.uiConstructed + 1 < sizeof(s_astAreaControls) / sizeof(s_astAreaControls[0])', 'normal'),
        ('missing-base-destructor', 's_pObjectServices->pfnDestroy(pState->pObject, WORLD_AREA_BASE_DTOR);',
         'if (0) s_pObjectServices->pfnDestroy(pState->pObject, WORLD_AREA_BASE_DTOR);', 'normal'),
        ('delete-flag', 'if (uiFlags & 1)', 'if (uiFlags & 2)', 'normal'),
        ('skip-unwind-control', 'world_destroy_remaining((struct world_object_unwind *)pFrame);',
         '((struct world_object_unwind *)pFrame)->uiConstructed = 0; world_destroy_remaining((struct world_object_unwind *)pFrame);', 'unwind'),
        ('wrong-unwind-delete', 's_pObjectServices->pfnDialogDelete(pState->pObject);',
         's_pObjectServices->pfnUiDelete(pState->pObject);', 'unwind'),
        ('lost-allocation-ownership', 'stState.pObject = pObject; stState.uiLive = 1;',
         'stState.pObject = pObject; stState.uiLive = 0;', 'unwind'),
        ('late-ownership-release', 'stState.uiLive = 0;\n    if (!s_pObjectServices->pfnCreate',
         'stState.uiLive = 1;\n    if (!s_pObjectServices->pfnCreate', 'unwind'),
        ('factory-parent', 'pfnCreate(pObject, 0x6b3, NULL)', 'pfnCreate(pObject, 0x6b3, (void *)uiParent)', 'normal'),
        ('create-failure-message', 'if (!s_pObjectServices->pfnCreate', 'if (s_pObjectServices->pfnCreate', 'normal'),
        ('show-command', 'pfnShow(pObject, 1)', 'pfnShow(pObject, 0)', 'normal'),
        ('missing-ddx', 'uiIndex < sizeof(s_aaunAreaDdx) / sizeof(s_aaunAreaDdx[0])',
         'uiIndex + 1 < sizeof(s_aaunAreaDdx) / sizeof(s_aaunAreaDdx[0])', 'normal'),
        ('help-command', '"fs98.hlp", 12, s_auiAreaHelp', '"fs98.hlp", 10, s_auiAreaHelp', 'normal'),
        ('context-result', '    return 1;\n}', '    return 0;\n}', 'normal'),
        ('typed-catch-cleanup', 'world_factory_cleanup(&pState->stFrame);',
         'pState->uiLive = 0; world_factory_cleanup(&pState->stFrame);', 'typed'),
        ('typed-catch-dispatch', 's_pObjectServices->pfnExitCleanly(pException);',
         's_pObjectServices->pfnExitCleanly((uint8 *)pException + 4);', 'typed'),
    )
    aHeaderMutations = (
        ('derived-pointer-adjustment', 'pbyException += (int)puiType[2];',
         'pbyException += 0;', 'typed', 'world_exceptions.h'),
        ('virtual-pointer-adjustment', 'puiType[3] + *(int *)(pbyVtable + puiType[4])',
         '*(int *)(pbyVtable + puiType[4])', 'typed', 'world_exceptions.h'),
        ('typed-catch-type', '".PAVCException@@"', '".PBVCException@@"', 'typed', 'world_exceptions.h'),
    )
    aAllMutations = [(*aMutation, 'world_objects.c') for aMutation in aMutations] + list(aHeaderMutations)
    for pcName, pcBefore, pcAfter, pcMode, pcFile in aAllMutations:
        pcOriginal = (pRoot / 'src/world' / pcFile).read_text()
        if pcOriginal.count(pcBefore) != 1:
            raise SystemExit(f'{pcName}: mutation anchor is not unique')
        pCase = pOutput / pcName
        pCase.mkdir(exist_ok=True)
        pSource = pCase / 'world_objects.c'
        pExecutable = pCase / 'mutant.exe'
        pcChanged = pcOriginal.replace(pcBefore, pcAfter)
        pSource.write_text(pcChanged if pcFile == 'world_objects.c' else pcSource)
        if pcFile != 'world_objects.c':
            (pCase / pcFile).write_text(pcChanged)
        subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/world',
                        '-o', str(pExecutable), 'tools/difftest/worldlifediff.c', str(pSource), '-lntdll'],
                       cwd=pRoot, check=True)
        oResult = subprocess.run(['wine', str(pExecutable), '32', '0x4a971b63', pcMode],
                                 cwd=pRoot, env=os.environ.copy(), capture_output=True,
                                 text=True, timeout=120)
        (pOutput / f'{pcName}.log').write_text(oResult.stdout + oResult.stderr)
        if oResult.returncode != 1 or 'DIFFER scenario=' not in oResult.stderr:
            raise SystemExit(f'{pcName}: no actual difference (exit {oResult.returncode})\n{oResult.stderr}')
        print(f'{pcName}: detected', flush=True)
    print(f'World area lifetime: {len(aAllMutations)} / {len(aAllMutations)} mutations detected')


if __name__ == '__main__':
    main()
