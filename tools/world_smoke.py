#!/usr/bin/env python3
"""Boot an isolated game copy with rebuilt English/World.DLL.

Invoked by run_worlddiff.sh --smoke. Only the dedicated World Wine prefix
is stopped; the source game tree and normal Wine prefix are never changed.
The game copy and trace are kept in ignored bin/world-diff for inspection.
"""
from pathlib import Path
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time


def main():
    pRoot = Path.cwd()
    pOutput = pRoot / 'bin/world-diff'
    pPrefix = pOutput / 'wineprefix'
    if Path(os.environ.get('WINEPREFIX', '')).resolve() != pPrefix.resolve():
        raise ValueError('this smoke run requires the dedicated World prefix')
    pSource = pRoot / 'game'
    pDll = pOutput / 'WORLD_area.dll'
    if not (pSource / 'FLTSIM98.EXE').is_file() or not pDll.is_file():
        raise ValueError('game install or rebuilt partial World missing')
    pCopy = Path(tempfile.mkdtemp(prefix='smoke-game-', dir=pOutput))
    shutil.copytree(pSource, pCopy, dirs_exist_ok=True, copy_function=shutil.copy2)
    pInstalled = pCopy / 'English/World.DLL'
    shutil.copy2(pDll, pInstalled)
    byExpected = pInstalled.read_bytes()
    pConfig = pCopy / 'FLTSIM98.CFG'
    pcConfig = pConfig.read_bytes().decode('latin1')
    pcConfig = re.sub(r'(?im)^SPLASH_SCREEN_OFF=.*$', 'SPLASH_SCREEN_OFF=1', pcConfig)
    pcConfig = re.sub(r'(?im)^PROMPT_ON_EXIT=.*$', 'PROMPT_ON_EXIT=0', pcConfig)
    pConfig.write_bytes(pcConfig.encode('latin1'))
    pLink = pPrefix / 'drive_c/FS98'
    if pLink.is_symlink():
        if pLink.resolve().parent != pOutput.resolve():
            raise ValueError('existing FS98 symlink is not a World smoke copy')
        pLink.unlink()
    elif pLink.exists():
        raise ValueError('FS98 already exists in the dedicated prefix')
    pLink.symlink_to(pCopy, target_is_directory=True)
    pTrace = pCopy / 'world-smoke.log'
    iSeconds = int(sys.argv[1]) if len(sys.argv) > 1 else 75
    if not 10 <= iSeconds <= 300:
        raise ValueError('smoke duration must be between 10 and 300 seconds')
    dEnvironment = dict(os.environ, WINEDEBUG='+loaddll')
    print(f'Isolated game copy: {pCopy}', flush=True)
    with pTrace.open('w') as pLog:
        pProcess = subprocess.Popen([
            'wine', 'explorer', '/desktop=WorldSmoke,1024x768',
            'C:\\FS98\\FLTSIM98.EXE'], cwd=pCopy, env=dEnvironment,
            stdout=pLog, stderr=subprocess.STDOUT)
        try:
            fDeadline = time.monotonic() + iSeconds
            while time.monotonic() < fDeadline:
                time.sleep(0.5)
        finally:
            subprocess.run(['wineserver', '-k'], check=True)
            pProcess.wait(timeout=15)
    pcTrace = pTrace.read_text(errors='replace')
    aExpectedModules = [pPath.name for pPath in (pCopy / 'Modules').iterdir()
                 if pPath.suffix.lower() == '.dll' and pPath.name.upper() != 'SEHFIX.DLL']
    aMissing = [pcName for pcName in aExpectedModules
                if not re.search(r'modules.*' + re.escape(pcName), pcTrace, re.IGNORECASE)]
    iGauges = len(re.findall(r'\.gau', pcTrace, re.IGNORECASE))
    iWorld = bool(re.search(r'english.*world\.dll', pcTrace, re.IGNORECASE))
    iImage = pInstalled.read_bytes() == byExpected
    aErrors = [pcLine for pcLine in pcTrace.splitlines()
               if re.search(r'err:|unhandled exception', pcLine, re.IGNORECASE)
               and not re.search(r'winediag|randr', pcLine, re.IGNORECASE)]
    print(f'modules loaded: {len(aExpectedModules)-len(aMissing)}/{len(aExpectedModules)}')
    print(f'gauges loaded: {iGauges}; English World loaded: {iWorld}; replacement bytes match: {iImage}')
    print(f'World SHA-256: {hashlib.sha256(pInstalled.read_bytes()).hexdigest()}')
    if aMissing: print('Missing modules:', ', '.join(aMissing))
    for pcLine in aErrors[:8]: print(pcLine)
    iPassed = not aMissing and iGauges > 0 and iWorld and iImage and not aErrors
    print('World area isolated startup smoke: ' + ('PASS' if iPassed else 'FAIL'))
    return 0 if iPassed else 1


if __name__ == '__main__':
    sys.exit(main())
