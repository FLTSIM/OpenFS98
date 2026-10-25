#!/usr/bin/env python3
"""An actual reported difference is required; faults/timeouts do not count."""
from pathlib import Path
import subprocess
import sys


def main():
    iCount = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    if iCount < 100:
        raise ValueError('use at least 100 scenarios')
    pDirectory = Path('bin/help-diff/breaks')
    pDirectory.mkdir(parents=True, exist_ok=True)
    aMutations = [
        ('about-answer', '0x10dc, 0x1978, 0)', '0x10dc, 0x1978, 1)'),
        ('credits-answer', '0x1174, 0x1498, 1)', '0x1174, 0x1498, 0)'),
        ('lessons-state', '0x10dc, 0x187c, 0)', '0x10dc, 0x187c, 1)'),
        ('completion-answer', '0x10dc, 0, 1)', '0x10dc, 0, 0)'),
        ('help-command', '"fs98.hlp", 0xc,', '"fs98.hlp", 0xd,'),
        ('ddx-control-id', 's_aunDdx_##szName[uiIndex][1],', 's_aunDdx_##szName[uiIndex][1] ^ 1,'),
        ('ddx-base', 'if (iBase)', 'if (0)'),
        ('post-destroy-flags', '))(pObject, 1);', '))(pObject, 0);'),
        ('disable-window', 'pEnableWindow(D32(pObject, 0x20), 0)', 'pEnableWindow(D32(pObject, 0x20), 1)'),
        ('close-timer-order', '    D32(pObject, 0x10e8) = 0;', '    D32(pObject, 0x10e8) = 1;'),
        ('close-rich-edit', 'if (uiHandle) s_pServices->pDestroyWindow(uiHandle);', '(void)uiHandle;'),
        ('close-resource', '        D32(pObject, 0x110c) = 0;', '        D32(pObject, 0x110c) = 1;'),
        ('stream-read-failure', '*pRead = 0;', '*pRead = 1;'),
        ('stream-return', '    return 0;\n}\n\n/* 0x22332010', '    return 1;\n}\n\n/* 0x22332010'),
        ('stream-descriptor-order', '    PTR(pObject, 0x1174) = AT(pObject, 0x1110);\n    s_pServices->pCopyText(pName, \"fltcred.rtf\");', '    s_pServices->pCopyText(pName, \"fltcred.rtf\");\n    PTR(pObject, 0x1174) = AT(pObject, 0x1110);'),
        ('stream-limit', '0x435, 0, 0x7d00)', '0x435, 0, 0x7d01)'),
        ('stream-format', '0x449, 2,', '0x449, 1,'),
        ('stream-close-file', '    s_pServices->pCloseHandle((uint32)iFile);', '    (void)iFile;'),
        ('stream-scroll-limit', ' - 6;', ' - 5;'),
        ('timer-first-id', 'uiTimer = 3;', 'uiTimer = 4;'),
        ('timer-last-id', 'uiTimer < 15;', 'uiTimer < 14;'),
        ('timer-period', 'uiTimer, 500, 0)', 'uiTimer, 501, 0)'),
        ('timer-signed-bound', 'iLine > (int32)D32', '(uint32)iLine > D32'),
        ('timer-equal-bound', 'iLine > (int32)D32', 'iLine >= (int32)D32'),
        ('timer-reset-command', '0x115, 6, 0)', '0x115, 5, 0)'),
        ('toggle-bit', 'D32(pObject, 0x1108) ^= 1;', 'D32(pObject, 0x1108) ^= 2;'),
        ('lesson-invalid-selection', '0x188, 0, 0) == -1)', '0x188, 0, 0) == -2)'),
        ('lesson-radio-value', 'uiWindow == (uint32)iChecked;', 'uiWindow != (uint32)iChecked;'),
        ('lesson-radio-handle-reload', 'uiWindow = D32(pObject, 0x17a8);', 'uiWindow = D32(pObject, 0x17a8) ^ 1;'),
        ('lesson-record-stride', 'uiIndex * (uint32)0xa08', 'uiIndex * (uint32)0'),
        ('lesson-description', '(char *)AT(pRecord, 0x100)', '(char *)AT(pRecord, 0)'),
        ('lesson-extension', 'pExists(acPath, 0x1d)', 'pExists(acPath, 0x1e)'),
        ('lesson-availability', 'pExists(acPath, 0x1d) == 0', 'pExists(acPath, 0x1d) != 0'),
        ('lesson-double-click-order', '    fs98_help_lessons_select(pObject);\n    ((void (TC *)(void *))PTR(PTR(pObject, 0), 0xcc))(pObject);', '    ((void (TC *)(void *))PTR(PTR(pObject, 0), 0xcc))(pObject);\n    fs98_help_lessons_select(pObject);'),
        ('graph-tick-operation', 'fs98_help_graph_call(pObject, 1);', 'fs98_help_graph_call(pObject, 0);'),
        ('graph-accept-operation', 'fs98_help_graph_call(pObject, 2);', 'fs98_help_graph_call(pObject, 1);'),
        ('graph-notify-operation', 'fs98_help_graph_call(pObject, 0);', 'fs98_help_graph_call(pObject, 1);'),
        ('graph-state-write', 'D32(pObject, 0x10e4) = uiOperation;', 'D32(pObject, 0x10e4) = uiOperation ^ 1;'),
        ('graph-debug-pointer', '&& (uint32)pDescriptor != 0xcdcdcdcd', '&& (uint32)pDescriptor != 0xcccccccc'),
    ]
    aMutations += [
        ('skin-flags', 's_uiResourceModule, 9)', 's_uiResourceModule, 8)'),
        ('skin-resource', 'pObject, 0x14f,', 'pObject, 0x14e,'),
        ('button-bitmap', '0x132, AT(pObject, 0xff4)', '0x133, AT(pObject, 0xff4)'),
        ('completion-font', 'pInitText(AT(pObject, 0x10e0), AT(pObject, 0x220))', 'pInitText(AT(pObject, 0x10e0), AT(pObject, 0x370))'),
        ('lessons-font-color', 'D32(pObject, 0x228) = 0;', 'D32(pObject, 0x228) = 1;'),
        ('lessons-edit-limit', '0xc5, 0x3ff, 0)', '0xc5, 0x3fe, 0)'),
        ('lessons-radio-bitmap', 'pRadio, AT(pObject, 0x4c0), 0x134,', 'pRadio, AT(pObject, 0x4c0), 0x135,'),
        ('lessons-edit-error', 'pFont, 0xffffff, 0, 0xff0000,', 'pFont, 0xffffff, 0, 0xfe0000,'),
        ('credits-window-style', '"RICHEDIT", "", 0x503000c4,', '"RICHEDIT", "", 0x503000c5,'),
        ('credits-window-id', 'D32(pObject, 0x20), 0xede,', 'D32(pObject, 0x20), 0xedf,'),
    ]
    aMutations += [
        ('registry-name-capacity', '        *pCapacity = 0x63;', '        *pCapacity = 0x62;'),
        ('registry-product-capacity', '*pCapacity = 0x31;', '*pCapacity = 0x30;'),
        ('registry-key-access', '\\6.1", 0, 0x20019, pKey)', '\\6.1", 0, 0x20018, pKey)'),
        ('registry-name-pointer', '(char *)pScratch + 0xa8', '(char *)pScratch + 0xac'),
        ('registry-fallback', '"Registry Error"', '"Registry failure"'),
        ('version-minor', '6, 10, pBuild, 0x1a2', '6, 11, pBuild, 0x1a2'),
        ('version-build', '6, 10, pBuild, 0x1a2', '6, 10, pBuild, 0x1a3'),
        ('about-font-selector', 'pBaseFont(11)', 'pBaseFont(10)'),
        ('about-font-color', 'D32(s_pAboutState->abyFont, 8) = 0xffffff;', 'D32(s_pAboutState->abyFont, 8) = 0xfffffe;'),
        ('about-font-background', 'D32(s_pAboutState->abyFont, 0xc) = 0;', 'D32(s_pAboutState->abyFont, 0xc) = 1;'),
        ('about-font-init-bit', 's_pAboutState->byInitialized |= 1;', 's_pAboutState->byInitialized |= 2;'),
    ]
    aMutations = [(szName, 'events', 'normal', szBefore, szAfter)
                  for szName, szBefore, szAfter in aMutations]
    aMutations += [
        ('constructor-parent', 'objects', 'normal', 'pClass->unTemplate, pParent)', 'pClass->unTemplate, (void *)((uint32)pParent & 0))'),
        ('credits-initial-pause', 'objects', 'normal', 'D32(pObject, 0x1108) = 1;', 'D32(pObject, 0x1108) = 0;'),
        ('delete-flags', 'objects', 'normal', 'if (uiFlags & 1) s_pServices->pUiDelete', 'if (uiFlags & 2) s_pServices->pUiDelete'),
        ('placeholder-delete-allocator', 'objects', 'normal', 'if (uiFlags & 1) s_pServices->pMfcDelete', 'if (uiFlags & 1) s_pServices->pUiDelete'),
        ('factory-module', 'objects', 'normal', 'D32(pObject, 0x80) = s_uiModule;', 'D32(pObject, 0x80) = s_uiModule ^ 1;'),
        ('factory-show', 'objects', 'normal', 'pShow(pObject, 1)', 'pShow(pObject, 0)'),
        ('factory-size', 'objects', 'normal', 'pAllocate(pClass->unSize)', 'pAllocate(pClass->unSize - 4)'),
        ('factory-parent', 'objects', 'normal', 'fs98_help_construct(pObject, 0, uiKind)', 'fs98_help_construct(pObject, (void *)uiParent, uiKind)'),
        ('constructor-unwind-base', 'objects', 'unwind', '    oState.uiBase = 1;', '    oState.uiBase = 0;'),
        ('factory-unwind-allocator', 'objects', 'unwind', 'pDialogDelete(pState->pObject)', 'pUiDelete(pState->pObject)'),
    ]
    aMutations += [
        ('about-string-unwind', 'events', 'unwind', 'if (pState->uiLive) {', 'if (0) {'),
    ]
    iCaught = 0
    for szName, szUnit, szMode, szBefore, szAfter in aMutations:
        szOriginal = Path(f'src/help/help_{szUnit}.c').read_text()
        if szBefore not in szOriginal:
            raise ValueError(f'{szName}: missing mutation anchor')
        pSource = pDirectory / f'{szName}.c'
        pExecutable = pDirectory / f'{szName}.exe'
        pSource.write_text(szOriginal.replace(szBefore, szAfter, 1))
        pExecutable.unlink(missing_ok=True)
        oBuild = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
            '-fno-strict-aliasing', '-Isrc/help', '-o', str(pExecutable),
            'tools/difftest/helpeventsdiff.c', str(pSource),
            f'src/help/help_{"objects" if szUnit == "events" else "events"}.c', 'src/help/help_stack.S', '-lntdll'], text=True, capture_output=True)
        if oBuild.returncode:
            print(f'{szName}: BUILD FAILED (not counted)\n{oBuild.stderr}', flush=True)
            continue
        try:
            oRun = subprocess.run(['wine', str(pExecutable), str(iCount), '0x19a847e3', szMode],
                                  text=True, capture_output=True, timeout=60)
        except subprocess.TimeoutExpired:
            print(f'{szName}: TIMEOUT (not counted)', flush=True)
            continue
        szOutput = oRun.stdout + oRun.stderr
        (pDirectory / f'{szName}.log').write_text(szOutput)
        if oRun.returncode == 1 and 'DIFFER scenario=' in szOutput:
            iCaught += 1
            print(f'{szName}: caught', flush=True)
        else:
            print(f'{szName}: NOT CAUGHT (exit {oRun.returncode})\n{szOutput}', flush=True)
    print(f'Help mutations: {iCaught}/{len(aMutations)} caught', flush=True)
    return 0 if iCaught == len(aMutations) else 1


if __name__ == '__main__':
    sys.exit(main())
