#!/usr/bin/env python3
"""Actual owner acceptance in disposable game trees and Wine prefixes."""
import argparse
import fcntl
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'bin/mpdialog-diff'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def windows_path(path):
    return 'Z:' + str(Path(path).resolve()).replace('/', '\\')


def retail_tree(seed, destination):
    shutil.copytree(seed, destination)
    copied = {}
    for directory in ('Modules', 'English', ''):
        existing = {p.name.lower(): p for p in (destination / directory).iterdir() if p.is_file()}
        for original in sorted((ROOT / 'orig' / directory).iterdir()):
            if not original.is_file():
                continue
            name = original.name.lower()
            if directory == 'Modules':
                name = {'facil.dll': 'facilities.dll', 'multip.dll': 'multiplayer.dll'}.get(name, name)
            if name in existing:
                shutil.copy2(original, existing[name])
                copied[str(existing[name].relative_to(destination))] = digest(original)
    return copied


def build_probes(audit):
    flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror']
    sources = {}
    for source, target, extra in [
        ('mpdialoghosthook.c', 'mpdialoghosthook.dll', ['-shared', '-Wl,--kill-at', '-luser32']),
        ('mpdialoghostsession.c', 'mpdialog_hostsession.exe', ['-luser32']),
        ('mpdialoghostpeer.c', 'mpdialog_hostpeer.exe', ['-ldxguid', '-lole32', '-luuid']),
    ]:
        path = ROOT / 'tools/difftest' / source
        shutil.copy2(path, audit / source)
        subprocess.run([*flags, str(path), '-o', str(audit / target), *extra], cwd=ROOT, check=True)
        sources[source] = digest(path)
        sources[target] = digest(audit / target)
    return sources


def run_side(audit, side, seed, peer, skip_intro, initial_post_only, installed_owners=False):
    run = audit / side
    run.mkdir()
    game, prefix = run / 'game', run / 'prefix'
    record = {'side': side, 'installed_owners': installed_owners}
    if installed_owners:
        shutil.copytree(seed, game)
        record['installed_inputs'] = {str(p.relative_to(game)): digest(p)
                                     for directory in ('Modules', 'English')
                                     for p in sorted((game / directory).iterdir()) if p.is_file()}
    else:
        record['retail_inputs'] = retail_tree(seed, game)
    tested = ROOT / 'orig/English/mpdialog.dll' if side == 'retail' else OUT / 'MP_DIALOG_ours.dll'
    shutil.copy2(tested, game / 'English/MP_DIALOG.DLL')
    config = game / 'FLTSIM98.CFG'
    data = config.read_bytes()
    # Match the startup smoke's private profile: no welcome dialog can own
    # the main window underneath the multiplayer modal workflow.
    for key, value in ((b'SPLASH_SCREEN_OFF', b'1'), (b'PROMPT_ON_EXIT', b'0')):
        data = re.sub(rb'(?m)^' + key + rb'=.*$', key + b'=' + value + b'\r', data)
    if skip_intro:
        data, count = re.subn(rb'(?m)^ALWAYS_SHOW_INITIAL_DIALOG=1\r?$', b'ALWAYS_SHOW_INITIAL_DIALOG=0\r', data)
        if count != 1:
            raise ValueError('expected one initial-dialog profile key in the private seed')
    config.write_bytes(data)
    record['skip_intro'] = skip_intro
    record['initial_post_only'] = initial_post_only
    record['dll_before'] = digest(game / 'English/MP_DIALOG.DLL')
    record['config_before'] = digest(game / 'FLTSIM98.CFG')
    environment = dict(os.environ, WINEPREFIX=str(prefix), WINEARCH='win32', WINEDEBUG='-all')
    try:
        with (run / 'wineboot.log').open('w') as log:
            subprocess.run(['wineboot', '-u'], env=environment, stdout=log,
                           stderr=subprocess.STDOUT, timeout=120, check=True)
        (prefix / 'drive_c/FS98').symlink_to(game, target_is_directory=True)
        record['native_runtime'] = {}
        for name in ('dplayx.dll', 'dpwsockx.dll', 'dplaysvr.exe'):
            target = prefix / 'drive_c/windows/system32' / name
            if target.is_symlink():
                target.unlink()
            shutil.copy2(ROOT / 'cd/directx' / name, target)
            record['native_runtime'][name] = digest(target)
        key = r'HKLM\Software\Microsoft\DirectPlay\Service Providers\Internet TCP/IP Connection For DirectPlay'
        subprocess.run(['wine', 'reg', 'add', key, '/v', 'Path', '/t', 'REG_SZ', '/d',
                        r'C:\windows\system32\dpwsockx.dll', '/f'], env=environment,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30, check=True)
        environment.update(WINEDLLOVERRIDES='dplayx,dpwsockx,dplaysvr.exe=n',
                           MPD_OWNER_HOOK=windows_path(audit / 'mpdialoghosthook.dll'),
                           MPD_OWNER_LOG=windows_path(run / 'hook.log'),
                           MPD_OWNER_CONTROLLER_LOG=windows_path(run / 'controller.log'),
                           MPD_OWNER_ACCEPT='1')
        if initial_post_only:
            environment['MPD_OWNER_INITIAL_POST_ONLY'] = '1'
        if peer:
            environment.update(MPD_OWNER_PEER=windows_path(audit / 'mpdialog_hostpeer.exe'),
                               MPD_OWNER_PEER_LOG=windows_path(run / 'peer.log'))
        record['environment'] = {k: v for k, v in environment.items()
                                 if k.startswith('MPD_OWNER_') or k.startswith('WINE')}
        command = ['wine', 'explorer', '/desktop=MPDialogAcceptance,1024x768',
                   windows_path(audit / 'mpdialog_hostsession.exe'), '90']
        record['command'] = command
        start = time.monotonic()
        with (run / 'wine.log').open('w') as log:
            process = subprocess.Popen(command, cwd=game, env=environment, stdout=log, stderr=subprocess.STDOUT)
            while time.monotonic() - start < 105:
                controller = run / 'controller.log'
                if controller.exists() and 'LOCAL_HOST_END' in controller.read_text(errors='replace'):
                    record['controller_completed'] = True
                    break
                if process.poll() is not None:
                    record['launcher_exit'] = process.returncode
                    break
                time.sleep(.25)
        record['elapsed'] = time.monotonic() - start
    except (OSError, subprocess.SubprocessError) as error:
        record['error'] = str(error)
    finally:
        subprocess.run(['wineserver', '-k'], env=environment, timeout=20, check=False)
        subprocess.run(['wineserver', '-w'], env=environment, timeout=20, check=False)
        record['dll_after'] = digest(game / 'English/MP_DIALOG.DLL')
        record['config_after'] = digest(game / 'FLTSIM98.CFG')
    return record


def check_side(run, record, peer):
    controller_path, hook_path = run / 'controller.log', run / 'hook.log'
    controller = controller_path.read_text(errors='replace') if controller_path.exists() else ''
    hook = hook_path.read_text(errors='replace') if hook_path.exists() else ''
    required = ['HOST_FLOW_RETURNED main_enabled=1', 'SETTINGS_ACTION HostSettings',
                'DETAILS_ACTION capacities=4/2', 'OWNER_CAPACITY pilot=4 observer=2',
                'SETTINGS_ACTION OK', 'LOCAL_HOST_END result=0 flow_returned=1 details=1 returned_to_game=1']
    missing = [value for value in required if value not in controller]
    for value in ['session host Open return HRESULT=00000000',
                  'session group create first return HRESULT=00000000',
                  'session group create second return HRESULT=00000000',
                  'entry=0x110c validation', 'entry=0x110c accept', 'entry=0x110c refresh', 'entry=0x110b OK']:
        if value not in hook:
            missing.append(value)
    if record.get('initial_post_only'):
        required = ['FLOW_ACTION AutoNext.Cancel', 'HOST_FLOW_RETURNED after-cancel main_enabled=1',
                    'LOCAL_HOST_END result=0 flow_returned=1 details=0 returned_to_game=1']
        missing = [value for value in required if value not in controller]
    if peer:
        peer_log = (run / 'peer.log').read_text(errors='replace') if (run / 'peer.log').exists() else ''
        for value in ['PEER join=00000000', 'PEER player=00000000', 'PEER RECEIVED_1016']:
            if value not in peer_log:
                missing.append(value)
        if 'entry=0x110c remove-confirm' not in hook:
            missing.append('actual Remove callback')
        if 'REMOVE_DELIVERY wait=0' not in controller:
            missing.append('confirmed peer delivery')
        if 'REMOVE_CONFIRMATION accepted' not in controller:
            missing.append('actual confirmation')
    if record.get('skip_intro'):
        if 'FLOW_ACTION Factory.AutoNext' not in controller or '+1c=00000000' not in hook:
            missing.append('actual factory initial-post branch')
    if record['dll_before'] != record['dll_after']:
        missing.append('unchanged tested DLL')
    if 'layout unresolved' in hook or ('unsupported owner layout' in hook and not record.get('installed_owners')):
        missing.append('resolved module layouts')
    return missing


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seed', type=Path, default=ROOT / 'game')
    parser.add_argument('--side', choices=('retail', 'replacement', 'both'), default='both')
    parser.add_argument('--no-peer', action='store_true')
    parser.add_argument('--skip-intro', action='store_true')
    parser.add_argument('--initial-post-only', action='store_true',
                        help='use profile 0 and cancel Choice after the factory-posted Next')
    parser.add_argument('--installed-connect-cancel', action='store_true',
                        help='keep installed owner modules and verify automatic Choice/Cancel')
    args = parser.parse_args()
    if args.installed_connect_cancel:
        args.initial_post_only, args.side = True, 'replacement'
    if args.initial_post_only:
        args.skip_intro, args.no_peer = True, True
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / 'wine-run.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        audit = Path(tempfile.mkdtemp(prefix='owner-acceptance-', dir=OUT))
        shutil.copy2(__file__, audit / 'runner.py')
        manifest = {'replacement': digest(OUT / 'MP_DIALOG_ours.dll'),
                    'source_game_dll_before': digest(ROOT / 'game/English/MP_DIALOG.DLL'),
                    'seed': str(args.seed.resolve()), 'peer': not args.no_peer,
                    'probes': build_probes(audit), 'runs': []}
        print(audit, flush=True)
        sides = ('retail', 'replacement') if args.side == 'both' else (args.side,)
        for side in sides:
            record = run_side(audit, side, args.seed, not args.no_peer, args.skip_intro, args.initial_post_only, args.installed_connect_cancel)
            record['missing'] = check_side(audit / side, record, not args.no_peer)
            if digest(ROOT / 'game/English/MP_DIALOG.DLL') != manifest['source_game_dll_before']:
                record['missing'].append('unchanged source game DLL')
            manifest['runs'].append(record)
            manifest['source_game_dll_after'] = digest(ROOT / 'game/English/MP_DIALOG.DLL')
            (audit / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
            print(side, 'PASS' if not record['missing'] else record['missing'], flush=True)
        if any(run['missing'] for run in manifest['runs']):
            raise SystemExit(1)


if __name__ == '__main__':
    main()
