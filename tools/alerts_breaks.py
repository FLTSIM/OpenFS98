#!/usr/bin/env python3
"""Break Alerts event logic in isolated copies and require an actual mismatch.

Run via bash tools/run_alertsdiff.sh --breaks [N] to keep Wine state isolated.
A compile error, runtime fault or timeout is not accepted as a caught mutation.
The baseline source and executable are never edited or overwritten.
"""
from pathlib import Path
import subprocess
import sys


def main():
    iCount = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    if iCount < 100:
        raise ValueError('use at least 100 scenarios for mutations')
    pDirectory = Path('bin/alerts-diff/breaks')
    pDirectory.mkdir(parents=True, exist_ok=True)
    szSource = Path('src/alerts/alerts_events.c').read_text()
    aBreaks = [
        ('first-answer', 'fs98_alerts_modal_answer(pObject, 0x10e4);',
         'fs98_alerts_modal_answer(pObject, 0x10e8);'),
        ('second-answer', 'fs98_alerts_modal_answer(pObject, 0x10e8);',
         'fs98_alerts_modal_answer(pObject, 0x10ec);'),
        ('third-answer', 'fs98_alerts_modal_answer(pObject, 0x10ec);',
         'fs98_alerts_modal_answer(pObject, 0x10e4);'),
        ('suppression-field', 'D32(pDescriptor, 0x10) = D32(pObject, 0x1550);',
         'D32(pDescriptor, 0x10) = D32(pObject, 0x1550) ^ 1;'),
        ('advisory-quit', 'D32(pDescriptor, 0x0c) = 1;', 'D32(pDescriptor, 0x0c) = 0;'),
        ('advisory-help', '"fs98.hlp", 8, 0xbc2', '"fs98.hlp", 8, 0xbc3'),
        ('close-noop', '    (void)pObject;\n}', '    fs98_alerts_virtual(pObject, 0xec);\n}'),
        ('live-gate', 'if (D32(pDescriptor, 0x0c) == 0)',
         'if (D32(pDescriptor, 0x0c) != 0)'),
        ('progress-gate', 'if (D32(pDescriptor, 0x10) == 0)',
         'if (D32(pDescriptor, 0x10) != 0)'),
        ('saved-vtable', 'ppFunctions[0x60 / 4](pObject);',
         'fs98_alerts_virtual(pObject, 0x60);'),
        ('total-floor', '        iTotal = 1;', '        iTotal = 2;'),
        ('current-cap', '        iCurrent = iTotal;', '        iCurrent = iTotal - 1;'),
        ('limit-cap', 'D32(pDescriptor, 0x20) = 100;', 'D32(pDescriptor, 0x20) = 99;'),
        ('limit-floor', 'D32(pDescriptor, 0x20) = 1;', 'D32(pDescriptor, 0x20) = 2;'),
        ('start-clamp', 'D32(pDescriptor, 0x1c) >= D32(pDescriptor, 0x20)',
         'D32(pDescriptor, 0x1c) > D32(pDescriptor, 0x20)'),
        ('unsigned-progress', '(uint32)((uiLimit - uiStart) * (uint32)iCurrent) / (uint32)iTotal',
         '(int32)((uiLimit - uiStart) * (uint32)iCurrent) / iTotal'),
        ('widen-product', '(uint32)((uiLimit - uiStart) * (uint32)iCurrent) / (uint32)iTotal',
         '(uint32)(((uint64)(uiLimit - uiStart) * (uint32)iCurrent) / (uint32)iTotal)'),
        ('split-divisor', 'iSplit / 100', 'iSplit / 101'),
        ('right-edge', 'fs98_alerts_sub(iWidth, 1)', 'fs98_alerts_sub(iWidth, 0)'),
        ('bottom-edge', 'fs98_alerts_sub(iHeight, 1)', 'fs98_alerts_sub(iHeight, 0)'),
        ('clip-rectangle', 'iX, iY, 6, &stFilled, pszText',
         'iX, iY, 6, &stRemaining, pszText'),
        ('draw-options', 'iX, iY, 6, &stRemaining, pszText',
         'iX, iY, 4, &stRemaining, pszText'),
        ('foreground', 'pTextColor(abyDc, 0xffffff)', 'pTextColor(abyDc, 0xfffffe)'),
        ('text-metrics-dc', 'pTextExtent(D32(abyDc, 8)', 'pTextExtent(D32(abyDc, 4)'),
        ('modal-ddx-id', 'pDdxControl(pExchange, 0x7d1,', 'pDdxControl(pExchange, 0x7d2,'),
        ('modeless-ddx-offset', 'pDdxControl(pExchange, 0x7d6, (uint8 *)pObject + 0x10e4)',
         'pDdxControl(pExchange, 0x7d6, (uint8 *)pObject + 0x1124)'),
        ('advisory-ddx-id', 'pDdxControl(pExchange, 0x7d7,', 'pDdxControl(pExchange, 0x7d8,'),
        ('modeless-init-flags', 'pObject, 0x14f, s_uiResourceModule, 9',
         'pObject, 0x14f, s_uiResourceModule, 8'),
        ('pointer-as-resource', 'if ((int32)uiText <= 0xffff)', 'if (1)'),
        ('modeless-icon', 'uiTag == 1 ? 0x86', 'uiTag == 1 ? 0x87'),
        ('zero-tag-load', '    if (uiTag)\n', '    if (1)\n'),
        ('sprite-position', '+ 0xff4, 0x1e, 0x22,', '+ 0xff4, 0x1f, 0x22,'),
        ('modal-button-count', '&& uiCount < 3', '&& uiCount < 2'),
        ('modal-button-shift', 'if (D32(pObject, 0x10e8) == 0)', 'if (0)'),
        ('modal-default-answer', 'D32(pObject, 0x10f4) = uiIndex;',
         'D32(pObject, 0x10f4) = uiIndex + 1;'),
        ('modal-label', 's_auiButtonText[uiBit],', 's_auiButtonText[uiBit] ^ 1,'),
        ('advisory-check', 'pSetChecked(pCheck, (int32)D32(pDescriptor, 0x10))',
         'pSetChecked(pCheck, (int32)D32(pDescriptor, 0x0c))'),
        ('advisory-message', 'pLoadString(0xf6f0,', 'pLoadString(0xf6f1,'),
        ('advisory-button-order', 'uiIndex == 0 ? 0x1398', 'uiIndex == 0 ? 0x11e4'),
    ]
    iCaught = 0
    for szName, szBefore, szAfter in aBreaks:
        if szBefore not in szSource:
            raise ValueError(f'{szName}: mutation anchor missing')
        pSource = pDirectory / f'{szName}.c'
        pExecutable = pDirectory / f'{szName}.exe'
        pSource.write_text(szSource.replace(szBefore, szAfter, 1))
        # Always compile the copy directly to its own executable; never depend
        # on make timestamps and never replace the verified baseline output.
        pBuild = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra',
            '-fno-strict-aliasing', '-Isrc/alerts', '-o', str(pExecutable),
            'tools/difftest/alertseventsdiff.c', str(pSource)],
            text=True, capture_output=True)
        if pBuild.returncode:
            print(f'{szName}: BUILD FAILED\n{pBuild.stderr}')
            continue
        try:
            pRun = subprocess.run(['wine', str(pExecutable), str(iCount)],
                                  text=True, capture_output=True, timeout=45)
        except subprocess.TimeoutExpired:
            print(f'{szName}: TIMEOUT (not counted)')
            continue
        szOutput = pRun.stdout + pRun.stderr
        (pDirectory / f'{szName}.log').write_text(szOutput)
        if pRun.returncode == 1 and 'DIFFER scenario=' in szOutput:
            iCaught += 1
            print(f'{szName}: caught', flush=True)
        else:
            print(f'{szName}: NOT CAUGHT (exit {pRun.returncode})\n{szOutput}', flush=True)
    print(f'Alerts events mutations: {iCaught}/{len(aBreaks)} caught')
    return 0 if iCaught == len(aBreaks) else 1


if __name__ == '__main__':
    sys.exit(main())
