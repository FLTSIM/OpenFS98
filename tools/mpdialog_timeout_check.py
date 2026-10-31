#!/usr/bin/env python3
"""Exercise mutation-runner timeout cleanup against an independent Wine process."""
import os
import select
import subprocess
import tempfile
from pathlib import Path

import mpdialog_ui_breaks as runner


def main():
    runner.OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='timeout-check-', dir=runner.OUT) as work:
        work = Path(work)
        source = work / 'sentinel.c'
        executable = work / 'sentinel.exe'
        source.write_text('#include <windows.h>\n#include <stdio.h>\n'
                          'int main(void) { puts("SENTINEL_READY"); fflush(stdout); '
                          'Sleep(60000); return 0; }\n')
        subprocess.run([runner.CC, '-O2', '-o', str(executable), str(source)], check=True)
        sentinel_env = os.environ.copy()
        sentinel_env['WINEPREFIX'] = str(work / 'sentinel-prefix')
        sentinel = subprocess.Popen(['wine', str(executable)], cwd=runner.ROOT,
                                    env=sentinel_env, stdout=subprocess.PIPE,
                                    stderr=subprocess.DEVNULL, text=True,
                                    start_new_session=True)
        try:
            ready, _, _ = select.select([sentinel.stdout], [], [], 45)
            if not ready or sentinel.stdout.readline().strip() != 'SENTINEL_READY':
                raise RuntimeError('independent Wine sentinel did not start')
            with runner.isolated_wine_prefix() as prefix:
                init = runner.run_wine(['wine', 'cmd', '/c', 'exit'], 'private prefix init', 60)
                if init.returncode:
                    raise RuntimeError('private Wine prefix initialization failed')
                os.environ['MPD_TEST_STALL_DESTROY'] = 'case3-page0'
                try:
                    command = ['wine', str(runner.OUT / 'mpdialoguidiff.exe'),
                               str(runner.OUT / 'MP_DIALOG_ours.dll'), '12',
                               '--flow-mutation-case', '3']
                    try:
                        runner.run_wine(command, 'controlled-destroy-stall', 6)
                    except RuntimeError as error:
                        expected = ('controlled-destroy-stall timed out; last checkpoint: '
                                    'FLOW LIFETIME PHASE side=0 case=203 page=0 phase=destroy-enter')
                        if expected not in str(error):
                            raise
                        print(str(error).splitlines()[0], flush=True)
                    else:
                        raise RuntimeError('controlled stall did not time out')
                finally:
                    os.environ.pop('MPD_TEST_STALL_DESTROY', None)
            if Path(prefix).exists() or sentinel.poll() is not None:
                raise RuntimeError('timeout cleanup leaked a prefix or stopped the sentinel')
            print('PRIVATE PREFIX REMOVED; INDEPENDENT WINE SENTINEL STILL RUNNING', flush=True)
        finally:
            subprocess.run(['wineserver', '-k'], cwd=runner.ROOT, env=sentinel_env,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=20, check=False)
            sentinel.wait(timeout=20)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
