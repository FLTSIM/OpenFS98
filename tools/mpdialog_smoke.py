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
import mpdialog_provenance as provenance


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

    # Optional Wine compatibility shim, copied only into the disposable tree.
    # Keep the same trace-error gate in both modes.
    use_sehfix = os.environ.get('MPD_SMOKE_SEHFIX', '0')
    if use_sehfix not in ('0', '1'):
        raise ValueError('MPD_SMOKE_SEHFIX must be 0 or 1')
    private_sehfix = copy / 'Modules/SEHFIX.DLL'
    sehfix_hash = None
    if use_sehfix == '1':
        sehfix = root / 'bin/SEHFIX.DLL'
        if not sehfix.is_file():
            raise ValueError('optional isolated smoke requires bin/SEHFIX.DLL')
        sehfix_hash = sha256(sehfix)
        shutil.copy2(sehfix, private_sehfix)
        if sha256(private_sehfix) != sehfix_hash:
            raise RuntimeError('private-copy SEHFIX differs from its input')
    elif private_sehfix.is_file():
        # The source game may already contain the shim; label and verify it.
        sehfix_hash = sha256(private_sehfix)

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
    wineboot_command = ['wineboot', '-u']
    game_command = ['wine', 'explorer', '/desktop=MPDialogSmoke,1024x768',
                    'C:\\FS98\\FLTSIM98.EXE']
    provenance.event('smoke-start', shimmed=bool(sehfix_hash), cwd=str(copy),
                     wineboot_command=wineboot_command, game_command=game_command,
                     timeout_seconds=seconds, configuration=provenance.configuration(environment),
                     game_executable=provenance.digest(copy / 'FLTSIM98.EXE'),
                     retail_install=provenance.digest(source_dll),
                     replacement=provenance.digest(installed),
                     shim=provenance.digest(private_sehfix) if sehfix_hash else None,
                     game_config=provenance.digest(config), trace=str(trace))
    print(f'Private game copy: {copy}', flush=True)
    process = None
    try:
        subprocess.run(wineboot_command, cwd=copy, env=environment,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=120, check=True)
        with trace.open('w') as log:
            process = subprocess.Popen(game_command, cwd=copy, env=environment,
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
    gauge_loads = re.findall(r'Loaded L"([^\"]*\.gau)"', trace_text, re.IGNORECASE)
    distinct_gauges = len({path.lower() for path in gauge_loads})
    replacement_loaded = bool(re.search(r'english.*mp_dialog\.dll', trace_text,
                                        re.IGNORECASE))
    sehfix_loaded = bool(re.search(r'modules.*sehfix\.dll', trace_text,
                                   re.IGNORECASE))
    sehfix_unchanged = not sehfix_hash or sha256(private_sehfix) == sehfix_hash
    installed_unchanged = sha256(installed) == installed_hash
    source_unchanged = sha256(source_dll) == source_hash
    errors = [line for line in trace_text.splitlines()
              if re.search(r'err:|unhandled exception', line, re.IGNORECASE)
              and not re.search(r'winediag|randr', line, re.IGNORECASE)]

    print(f'Modules loaded: {len(expected_modules) - len(missing)}/{len(expected_modules)}')
    print(f'Gauge trace mentions: {gauges}; load records: {len(gauge_loads)}; '
          f'distinct gauge paths: {distinct_gauges}; replacement loaded: {replacement_loaded}')
    print(f'Private replacement SHA-256: {installed_hash}')
    print(f'Private replacement unchanged: {installed_unchanged}; source game DLL unchanged: {source_unchanged}')
    if sehfix_hash:
        print(f'Private SEHFIX SHA-256: {sehfix_hash}; loaded: {sehfix_loaded}; unchanged: {sehfix_unchanged}')
    print(f'Load trace: {trace}')
    if missing:
        print('Missing modules:', ', '.join(missing))
    for line in errors[:8]:
        print(line)
    passed = (not missing and gauges > 0 and replacement_loaded and
              installed_unchanged and source_unchanged and not errors and
              (not sehfix_hash or (sehfix_loaded and sehfix_unchanged)))
    provenance.event('smoke-finish', shimmed=bool(sehfix_hash), passed=passed,
                     missing_modules=missing, gauge_mentions=gauges,
                     gauge_load_records=len(gauge_loads), distinct_gauge_paths=distinct_gauges,
                     replacement_loaded=replacement_loaded, shim_loaded=sehfix_loaded,
                     replacement_unchanged=installed_unchanged, source_unchanged=source_unchanged,
                     shim_unchanged=sehfix_unchanged, errors=errors,
                     trace=provenance.digest(trace), configuration=provenance.configuration(environment))
    print('MP_DIALOG isolated startup smoke: ' + ('PASS' if passed else 'FAIL'))
    return 0 if passed else 1


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(f'MP_DIALOG startup smoke failed: {error}', file=sys.stderr)
        sys.exit(1)
