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
    iCaught = 0
    szOriginal = Path('src/help/help_events.c').read_text()
    for szName, szBefore, szAfter in aMutations:
        if szBefore not in szOriginal:
            raise ValueError(f'{szName}: missing mutation anchor')
        pSource = pDirectory / f'{szName}.c'
        pExecutable = pDirectory / f'{szName}.exe'
        pSource.write_text(szOriginal.replace(szBefore, szAfter, 1))
        pExecutable.unlink(missing_ok=True)
        oBuild = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
            '-fno-strict-aliasing', '-Isrc/help', '-o', str(pExecutable),
            'tools/difftest/helpeventsdiff.c', str(pSource), '-lntdll'], text=True, capture_output=True)
        if oBuild.returncode:
            print(f'{szName}: BUILD FAILED (not counted)\n{oBuild.stderr}', flush=True)
            continue
        try:
            oRun = subprocess.run(['wine', str(pExecutable), str(iCount), '0x19a847e3'],
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
