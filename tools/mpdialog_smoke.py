#!/usr/bin/env python3
"""Start a disposable game copy with the isolated MP_DIALOG replacement."""
from pathlib import Path
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    root = Path.cwd()
    output = root / 'bin/mpdialog-diff'
    prefix = output / 'wineprefix-smoke'
    if Path(os.environ.get('WINEPREFIX', '')).resolve() != prefix.resolve():
        raise ValueError('this smoke run requires the dedicated MP_DIALOG smoke prefix')
    source = root / 'game'
    source_dll = source / 'English/MP_DIALOG.DLL'
    replacement = output / 'MP_DIALOG_ours.dll'
    if not (source / 'FLTSIM98.EXE').is_file() or not source_dll.is_file():
        raise ValueError('game install or English/MP_DIALOG.DLL missing')
    if not replacement.is_file():
        raise ValueError('isolated MP_DIALOG replacement missing; build it first')

    source_hash = sha256(source_dll)
    replacement_hash = sha256(replacement)
    copy = Path(tempfile.mkdtemp(prefix='smoke-game-', dir=output))
    shutil.copytree(source, copy, dirs_exist_ok=True, copy_function=shutil.copy2)
    installed = copy / 'English/MP_DIALOG.DLL'
    shutil.copy2(replacement, installed)
    installed_hash = sha256(installed)
    if installed_hash != replacement_hash:
        raise RuntimeError('private-copy MP_DIALOG differs from the standalone build')

    config = copy / 'FLTSIM98.CFG'
    config_text = config.read_bytes().decode('latin1')
    config_text = re.sub(r'(?im)^SPLASH_SCREEN_OFF=.*$',
                         'SPLASH_SCREEN_OFF=1', config_text)
    config_text = re.sub(r'(?im)^PROMPT_ON_EXIT=.*$',
                         'PROMPT_ON_EXIT=0', config_text)
    config.write_bytes(config_text.encode('latin1'))

    link = prefix / 'drive_c/FS98'
    link.parent.mkdir(parents=True, exist_ok=True)
    if link.is_symlink():
        if link.resolve().parent != output.resolve():
            raise ValueError('existing FS98 symlink is outside the MP_DIALOG smoke output')
        link.unlink()
    elif link.exists():
        raise ValueError('FS98 already exists in the dedicated MP_DIALOG smoke prefix')
    link.symlink_to(copy, target_is_directory=True)

    trace = copy / 'mpdialog-smoke.log'
    seconds = int(sys.argv[1]) if len(sys.argv) > 1 else 75
    if not 10 <= seconds <= 300:
        raise ValueError('smoke duration must be between 10 and 300 seconds')
    environment = dict(os.environ, WINEDEBUG='+loaddll')
    print(f'Private game copy: {copy}', flush=True)
    process = None
    try:
        subprocess.run(['wineboot', '-u'], cwd=copy, env=environment,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=120, check=True)
        with trace.open('w') as log:
            process = subprocess.Popen([
                'wine', 'explorer', '/desktop=MPDialogSmoke,1024x768',
                'C:\\FS98\\FLTSIM98.EXE'], cwd=copy, env=environment,
                stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                time.sleep(0.5)
    finally:
        subprocess.run(['wineserver', '-k'], cwd=copy, env=environment,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=20, check=False)
        if process is not None:
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

    trace_text = trace.read_text(errors='replace')
    expected_modules = [path.name for path in (copy / 'Modules').iterdir()
                         if path.suffix.lower() == '.dll' and
                         path.name.upper() != 'SEHFIX.DLL']
    missing = [name for name in expected_modules
               if not re.search(r'modules.*' + re.escape(name), trace_text,
                                re.IGNORECASE)]
    gauges = len(re.findall(r'\.gau', trace_text, re.IGNORECASE))
    replacement_loaded = bool(re.search(r'english.*mp_dialog\.dll', trace_text,
                                        re.IGNORECASE))
    installed_unchanged = sha256(installed) == installed_hash
    source_unchanged = sha256(source_dll) == source_hash
    errors = [line for line in trace_text.splitlines()
              if re.search(r'err:|unhandled exception', line, re.IGNORECASE)
              and not re.search(r'winediag|randr', line, re.IGNORECASE)]

    print(f'Modules loaded: {len(expected_modules) - len(missing)}/{len(expected_modules)}')
    print(f'Gauges loaded: {gauges}; replacement loaded: {replacement_loaded}')
    print(f'Private replacement SHA-256: {installed_hash}')
    print(f'Private replacement unchanged: {installed_unchanged}; source game DLL unchanged: {source_unchanged}')
    print(f'Load trace: {trace}')
    if missing:
        print('Missing modules:', ', '.join(missing))
    for line in errors[:8]:
        print(line)
    passed = (not missing and gauges > 0 and replacement_loaded and
              installed_unchanged and source_unchanged and not errors)
    print('MP_DIALOG isolated startup smoke: ' + ('PASS' if passed else 'FAIL'))
    return 0 if passed else 1


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(f'MP_DIALOG startup smoke failed: {error}', file=sys.stderr)
        sys.exit(1)
