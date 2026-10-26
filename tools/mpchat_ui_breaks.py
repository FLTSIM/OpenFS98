#!/usr/bin/env python3
"""Prove real MPCHAT GUI comparisons detect rendering, protocol and cleanup errors.

Every build uses frozen MPCHAT sources and numeric ABI headers. Only a native
comparison mismatch with exit 1 counts; build failures, faults and timeouts fail.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys


def main():
    directory = Path('bin/mpchat-ui/breaks')
    directory.mkdir(parents=True, exist_ok=True)
    inputs = [*Path('src/mpchat').iterdir(), Path('include/fs98/linkage.h'),
              Path('include/fs98/host.h'), Path('tools/difftest/mpchatuidiff.c')]
    sources = {str(path): path.read_bytes() for path in inputs if path.is_file()}
    sources['mpchat.res'] = Path('bin/mpchat-ui/mpchat.res').read_bytes()
    (directory / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest()
        for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('sender-color', 'ADDR(line), 0xff0000, 2, 0, 0);', 'ADDR(line), 0x00ff00, 2, 0, 0);'),
        ('message-indent', 'append(M32(S_CHAT), text, 0, 1, 1, 0);', 'append(M32(S_CHAT), text, 0, 1, 0, 0);'),
        ('resized-transcript-width', '(int)(split - 3), (int)(bottom - 0x23),\n                     SWP_NOZORDER);',
         '(int)(split - 9), (int)(bottom - 0x23),\n                     SWP_NOZORDER);'),
        ('send-focus', '                SetFocus(WND(M32(S_MAIN_WND)));', '                SetFocus(WND(M32(S_ENTRY)));'),
        ('restore-selection',
         '    if ((uint32_t)sel.min != (uint32_t)sel.max || (uint32_t)sel.min != len)\n        SendMessageA(WND(hwnd), 0x437, 0, (LPARAM)&sel);',
         '    (void)sel;'),
        ('packet-size', 'MULTIP_SEND(0, S_MSG, n + 8, 0);', 'MULTIP_SEND(0, S_MSG, n + 7, 0);'),
        ('hidden-message-slot', 'WINMSG_SHOW(ADDR(line), 6, 0x5a, 0);', 'WINMSG_SHOW(ADDR(line), 5, 0x5a, 0);'),
        ('received-message-type', 'if (M32(M32(args + 4)) != MSG_CHAT)', 'if (M32(M32(args + 4)) != MSG_CHAT + 1)'),
        ('rich-focus-selection', 'EM_SETSEL, (WPARAM)-1, 0);', 'EM_SETSEL, 0, -1);'),
        ('entry-limit', 'buf, 0x3ff);', 'buf, 0x400);'),
        ('unregister-class', '    UnregisterClassA(STR(S_CLASS), (HINSTANCE)(uintptr_t)M32(S_INSTANCE));', '    (void)S_CLASS;'),
        ('free-unknown-string', '    return H_FREE(S_UNKNOWN);', '    return 0;'),
    ]
    requested = set(sys.argv[1:])
    if requested - {entry[0] for entry in mutations}:
        raise ValueError('unknown native MPCHAT mutation')
    jobs = [('baseline', None, None)] + [entry for entry in mutations if not requested or entry[0] in requested]
    caught = 0
    for name, before, after in jobs:
        work = directory / name
        work.mkdir(exist_ok=True)
        for source_name, data in sources.items():
            target = work / source_name
            target.parent.mkdir(parents=True, exist_ok=True)
            if source_name == 'src/mpchat/mpchat.c' and before:
                if data.count(before.encode()) != 1:
                    raise ValueError(f'{name}: mutation must match exactly once')
                data = data.replace(before.encode(), after.encode(), 1)
            target.write_bytes(data)
        dll = work / 'MPCHAT_ours.dll'
        harness = work / 'mpchatuidiff.exe'
        commands = [
            ['i686-w64-mingw32-gcc', '-std=c99', '-Os', '-Wall', '-Wextra', '-Werror',
             '-I' + str(work / 'include'), '-ffreestanding', '-fno-builtin',
             '-fno-asynchronous-unwind-tables', '-fomit-frame-pointer', '-fno-delete-null-pointer-checks',
             '-shared', '-nostdlib', '-Wl,--enable-stdcall-fixup', '-Wl,--entry=_DllMain@12',
             '-Wl,--disable-dynamicbase', '-Wl,--disable-nxcompat', '-Wl,--image-base=0x20840000',
             '-o', str(dll), str(work / 'src/mpchat/mpchat.c'), str(work / 'mpchat.res'),
             str(work / 'src/mpchat/mpchat.def'), '-lkernel32', '-luser32', '-lgdi32', '-lgcc'],
            ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
             '-fno-strict-aliasing', '-o', str(harness), str(work / 'tools/difftest/mpchatuidiff.c'), '-lgdi32'],
        ]
        for command in commands:
            build = subprocess.run(command, text=True, errors='replace', capture_output=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        try:
            run = subprocess.run(['wine', str(harness), str(dll), '2'],
                                 text=True, errors='replace', capture_output=True, timeout=120)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = run.stdout + run.stderr
        (work / 'result.log').write_text(output)
        if name == 'baseline':
            if run.returncode or '0 differ' not in output:
                print(f'baseline: FAILED\n{output}', flush=True)
                return 1
            print('baseline: passed', flush=True)
        elif run.returncode == 1 and 'MPCHAT UI DIFFER' in output:
            caught += 1
            for artifact in Path('bin/mpchat-ui').glob('differ-*'):
                target = work / artifact.name
                # A hidden-window or teardown mismatch has no current image.
                # Do not retain a BMP from an earlier visible checkpoint.
                if artifact.suffix == '.bmp' and 'pixels=0/0' in output:
                    target.unlink(missing_ok=True)
                    continue
                shutil.copy2(artifact, target)
            print(f'{name}: caught', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output}', flush=True)
    print(f'MPCHAT native GUI mutations: {caught}/{len(jobs)-1} caught', flush=True)
    return 0 if caught == len(jobs)-1 else 1


if __name__ == '__main__':
    sys.exit(main())
