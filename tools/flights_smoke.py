#!/usr/bin/env python3
"""Boot isolated game copies with retail's and with our English/Flights.DLL.

    python3 -B tools/flights_smoke.py [seconds]

The game opens Flights' welcome dialog (0x1109) at start, so a boot reaches our code. Each
boot runs in its own copy of the game, under its own prefix, bin/flights-diff/smoke-prefix
(a copy of the harness's, made on first use), so the harness's prefix and its wineserver
are left alone; the source game tree and the normal prefix are never changed. Retail's
boot is the control: this wine logs a few errors on any boot, so ours passes when it loads
everything retail does and logs no error retail does not. The copies and traces stay in
ignored bin/flights-diff for inspection.
"""
from collections import Counter
from pathlib import Path
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path.cwd()
OUTPUT = ROOT / 'bin/flights-diff'
PREFIX = OUTPUT / 'smoke-prefix'


def boot(dll, seconds):
    """One boot of a fresh game copy with `dll` as English/Flights.DLL."""
    source = ROOT / 'game'
    copy = Path(tempfile.mkdtemp(prefix='smoke-game-', dir=OUTPUT))
    shutil.copytree(source, copy, dirs_exist_ok=True, copy_function=shutil.copy2)
    installed = copy / 'English/Flights.DLL'
    shutil.copy2(dll, installed)
    expected_bytes = installed.read_bytes()
    config = copy / 'FLTSIM98.CFG'
    config_text = config.read_bytes().decode('latin1')
    config_text = re.sub(r'(?im)^SPLASH_SCREEN_OFF=.*$', 'SPLASH_SCREEN_OFF=1', config_text)
    config_text = re.sub(r'(?im)^PROMPT_ON_EXIT=.*$', 'PROMPT_ON_EXIT=0', config_text)
    config.write_bytes(config_text.encode('latin1'))
    link = PREFIX / 'drive_c/FS98'
    if link.is_symlink():
        if link.resolve().parent != OUTPUT.resolve():
            raise ValueError('existing FS98 symlink is not a Flights smoke copy')
        link.unlink()
    elif link.exists():
        raise ValueError('FS98 already exists in the dedicated prefix')
    link.symlink_to(copy, target_is_directory=True)
    trace = copy / 'flights-smoke.log'
    environment = dict(os.environ, WINEPREFIX=str(PREFIX), WINEDEBUG='+loaddll')
    print(f'Isolated game copy: {copy}', flush=True)
    with trace.open('w') as log:
        process = subprocess.Popen([
            'wine', 'explorer', '/desktop=FlightsSmoke,1024x768',
            'C:\\FS98\\FLTSIM98.EXE'], cwd=copy, env=environment,
            stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                time.sleep(0.5)
        finally:
            subprocess.run(['wineserver', '-k'], check=True, env=environment)
            process.wait(timeout=15)
    trace_text = trace.read_text(errors='replace')
    expected_modules = [path.name for path in (copy / 'Modules').iterdir()
                        if path.suffix.lower() == '.dll' and path.name.upper() != 'SEHFIX.DLL']
    missing = [name for name in expected_modules
               if not re.search(r'modules.*' + re.escape(name), trace_text, re.IGNORECASE)]
    errors = Counter(re.sub(r'^[0-9a-f]{4}:', '', line) for line in trace_text.splitlines()
                     if re.search(r'err:|unhandled exception', line, re.IGNORECASE)
                     and not re.search(r'winediag|randr', line, re.IGNORECASE))
    result = {
        'modules': len(expected_modules) - len(missing), 'expected': len(expected_modules),
        'missing': missing, 'gauges': len(re.findall(r'\.gau', trace_text, re.IGNORECASE)),
        'flights': bool(re.search(r'english.*flights\.dll', trace_text, re.IGNORECASE)),
        'image': installed.read_bytes() == expected_bytes, 'errors': errors,
        'sha': hashlib.sha256(installed.read_bytes()).hexdigest(),
    }
    print(f"  modules loaded: {result['modules']}/{result['expected']}; gauges: {result['gauges']}; "
          f"English Flights loaded: {result['flights']}; replacement bytes match: {result['image']}")
    print(f"  Flights SHA-256: {result['sha']}")
    for line, count in errors.items():
        print(f'  {count} x {line}')
    return result


def main():
    if not PREFIX.exists():
        shutil.copytree(OUTPUT / 'wineprefix', PREFIX, symlinks=True,
                        ignore=lambda directory, names: ['FS98'] if Path(directory).name == 'drive_c' else [])
    ours_dll = OUTPUT / 'FLIGHTS_ours.dll'
    if not (ROOT / 'game/FLTSIM98.EXE').is_file() or not ours_dll.is_file():
        raise ValueError('game install or rebuilt Flights missing')
    seconds = int(sys.argv[1]) if len(sys.argv) > 1 else 75
    if not 10 <= seconds <= 300:
        raise ValueError('smoke duration must be between 10 and 300 seconds')
    print('Retail Flights (the control):', flush=True)
    retail = boot(ROOT / 'orig/English/Flights.DLL', seconds)
    print('Ours:', flush=True)
    ours = boot(ours_dll, seconds)
    extra = ours['errors'] - retail['errors']
    for line, count in extra.items():
        print(f'  not in the control: {count} x {line}')
    passed = (not ours['missing'] and ours['gauges'] >= retail['gauges'] > 0 and ours['flights']
              and ours['image'] and not extra)
    print('Flights isolated startup smoke: ' + ('PASS' if passed else 'FAIL'))
    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
