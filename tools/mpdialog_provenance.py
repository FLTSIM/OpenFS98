#!/usr/bin/env python3
"""Record isolated MP_DIALOG runs without collecting unrelated environment data."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'bin/mpdialog-diff'
ENV_KEYS = ('WINEPREFIX', 'WINEARCH', 'WINEDEBUG', 'WINEDLLOVERRIDES',
            'DISPLAY', 'WAYLAND_DISPLAY', 'MPD_SMOKE_SEHFIX')


def digest(path):
    path = Path(path)
    if not path.is_file():
        return {'path': str(path), 'sha256': None}
    sha = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            sha.update(block)
    return {'path': str(path.resolve()), 'sha256': sha.hexdigest()}


def timestamp():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def configuration(env=None):
    env = os.environ if env is None else env
    prefix = Path(env['WINEPREFIX']) if env.get('WINEPREFIX') else None
    return {'environment': {key: env.get(key) for key in ENV_KEYS},
            'registry': [digest(prefix / name) for name in ('system.reg', 'user.reg', 'userdef.reg')] if prefix else []}


def event(kind, **fields):
    directory = os.environ.get('MPD_PROVENANCE_DIR')
    if not directory:
        return
    record = {'timestamp_utc': timestamp(), 'kind': kind, **fields}
    with (Path(directory) / 'events.jsonl').open('a', encoding='utf-8') as stream:
        stream.write(json.dumps(record, sort_keys=True) + '\n')


def begin(args):
    smoke = bool(args and args[0] == '--smoke')
    shimmed = (os.environ.get('MPD_SMOKE_SEHFIX') == '1' or
               (ROOT / 'game/Modules/SEHFIX.DLL').is_file())
    mode = ('smoke-sehfix' if shimmed else 'smoke-unshimmed') if smoke else 'isolated'
    directory = Path(tempfile.mkdtemp(prefix=mode + '-', dir=OUT / 'runs'))
    try:
        wine = subprocess.run(['wine', '--version'], text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=10, check=False).stdout.strip()
    except (OSError, subprocess.SubprocessError) as error:
        wine = f'unavailable: {error}'
    record = {'timestamp_utc': timestamp(), 'mode': mode,
              'invocation': ['bash', 'tools/run_mpdialogdiff.sh', *args],
              'cwd': str(ROOT), 'wine_executable': shutil.which('wine'), 'wine_version': wine,
              'configuration': configuration(),
              'retail': digest(ROOT / 'orig/English/mpdialog.dll'),
              'replacement': digest(OUT / 'MP_DIALOG_ours.dll'),
              'test_executable': digest(OUT / 'mpdialoguidiff.exe')}
    (directory / 'manifest.json').write_text(json.dumps(record, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    return directory


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == 'begin':
        (OUT / 'runs').mkdir(parents=True, exist_ok=True)
        print(begin(sys.argv[2:]))
    elif len(sys.argv) == 3 and sys.argv[1] == 'finish':
        event('runner-finish', exit_code=int(sys.argv[2]), configuration=configuration())
    elif len(sys.argv) >= 3 and sys.argv[1] == 'record-ui':
        replacement = Path(sys.argv[2])
        if not replacement.is_absolute() and not replacement.is_file():
            replacement = OUT / replacement
        event('comparison-start', command=['timeout', '300s', 'wine',
              'bin/mpdialog-diff/mpdialoguidiff.exe', *sys.argv[2:]],
              replacement=digest(replacement), configuration=configuration())
    else:
        raise SystemExit('usage: mpdialog_provenance.py begin [args...] | finish EXIT_CODE')


if __name__ == '__main__':
    main()
