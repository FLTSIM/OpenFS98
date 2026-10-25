#!/usr/bin/env python3
"""Boot an isolated game copy with rebuilt English/Views.DLL.

Invoked by run_viewsdiff.sh --smoke. Only the dedicated Views Wine prefix
is stopped; the source game tree and normal Wine prefix are never changed.
The game copy and trace are kept in ignored bin/views-diff for inspection.
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
    root = Path.cwd()
    output = root / 'bin/views-diff'
    prefix = output / 'wineprefix'
    if Path(os.environ.get('WINEPREFIX', '')).resolve() != prefix.resolve():
        raise ValueError('this smoke run requires the dedicated Views prefix')
    source = root / 'game'
    dll = output / 'VIEWS_ours.dll'
    if not (source / 'FLTSIM98.EXE').is_file() or not dll.is_file():
        raise ValueError('game install or rebuilt Views missing')
    copy = Path(tempfile.mkdtemp(prefix='smoke-game-', dir=output))
    shutil.copytree(source, copy, dirs_exist_ok=True, copy_function=shutil.copy2)
    installed = copy / 'English/Views.DLL'
    shutil.copy2(dll, installed)
    expected_bytes = installed.read_bytes()
    config = copy / 'FLTSIM98.CFG'
    config_text = config.read_bytes().decode('latin1')
    config_text = re.sub(r'(?im)^SPLASH_SCREEN_OFF=.*$', 'SPLASH_SCREEN_OFF=1', config_text)
    config_text = re.sub(r'(?im)^PROMPT_ON_EXIT=.*$', 'PROMPT_ON_EXIT=0', config_text)
    config.write_bytes(config_text.encode('latin1'))
    link = prefix / 'drive_c/FS98'
    if link.is_symlink():
        if link.resolve().parent != output.resolve():
            raise ValueError('existing FS98 symlink is not an Views smoke copy')
        link.unlink()
    elif link.exists():
        raise ValueError('FS98 already exists in the dedicated prefix')
    link.symlink_to(copy, target_is_directory=True)
    trace = copy / 'views-smoke.log'
    seconds = int(sys.argv[1]) if len(sys.argv) > 1 else 75
    if not 10 <= seconds <= 300:
        raise ValueError('smoke duration must be between 10 and 300 seconds')
    environment = dict(os.environ, WINEDEBUG='+loaddll')
    print(f'Isolated game copy: {copy}', flush=True)
    with trace.open('w') as log:
        process = subprocess.Popen([
            'wine', 'explorer', '/desktop=ViewsSmoke,1024x768',
            'C:\\FS98\\FLTSIM98.EXE'], cwd=copy, env=environment,
            stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                time.sleep(0.5)
        finally:
            subprocess.run(['wineserver', '-k'], check=True)
            process.wait(timeout=15)
    trace_text = trace.read_text(errors='replace')
    expected_modules = [path.name for path in (copy / 'Modules').iterdir()
                 if path.suffix.lower() == '.dll' and path.name.upper() != 'SEHFIX.DLL']
    missing = [name for name in expected_modules
                if not re.search(r'modules.*' + re.escape(name), trace_text, re.IGNORECASE)]
    gauges = len(re.findall(r'\.gau', trace_text, re.IGNORECASE))
    views = bool(re.search(r'english.*views\.dll', trace_text, re.IGNORECASE))
    image = installed.read_bytes() == expected_bytes
    errors = [line for line in trace_text.splitlines()
               if re.search(r'err:|unhandled exception', line, re.IGNORECASE)
               and not re.search(r'winediag|randr', line, re.IGNORECASE)]
    print(f'modules loaded: {len(expected_modules)-len(missing)}/{len(expected_modules)}')
    print(f'gauges loaded: {gauges}; English Views loaded: {views}; replacement bytes match: {image}')
    print(f'Views SHA-256: {hashlib.sha256(installed.read_bytes()).hexdigest()}')
    if missing: print('Missing modules:', ', '.join(missing))
    for line in errors[:8]: print(line)
    passed = not missing and gauges > 0 and views and image and not errors
    print('Views isolated startup smoke: ' + ('PASS' if passed else 'FAIL'))
    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
