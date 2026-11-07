#!/usr/bin/env python3
"""Rebuild SIM1 defects and require value differences or clean game divergence."""
import fcntl
import json
import os
from pathlib import Path
import re
import subprocess

from gen_sim1_lift import SimLift

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'bin/sim1-diff/lift-breaks'
CC = 'i686-w64-mingw32-gcc'


def run(command, directory, name, env=None, timeout=300):
    result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    (directory / name).write_text(result.stdout)
    return result


def build(directory, source, entries, native, reuse=False):
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'sim1_lift.c').write_text(source)
    (directory / 'entries.S').write_text(entries)
    (directory / 'native.S').write_text(native)
    obj = ROOT / 'bin/sim1-diff/lift/sim1_lift.o' if reuse else directory / 'lift.o'
    if not reuse:
        result = run([CC, '-std=c99', '-O1', '-Wall', '-Wextra', '-Wno-unused-function',
                      '-Wno-unused-label', '-fno-strict-aliasing', '-masm=intel', '-Isrc/sim1',
                      '-c', str(directory / 'sim1_lift.c'), '-o', str(obj)], directory, 'compile.log')
        if result.returncode:
            raise RuntimeError('mutation compile failed: ' + result.stdout)
    dll = directory / 'SIM1_COMPLETE_TEST.DLL'
    dll.unlink(missing_ok=True)
    result = run([CC, '-std=c99', '-O2', '-DSIM1_COMPLETE', '-Iinclude', '-Isrc/sim1',
                  '-Ibin/sim1-diff', '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12',
                  '-Wl,--disable-dynamicbase', '-Wl,--disable-nxcompat', '-Wl,--image-base,0x22000000',
                  '-o', str(dll), 'src/sim1/sim1.c', 'src/sim1/sim1_stack.S',
                  'tools/difftest/sim1nativeprobe.c',
                  'src/sim1/sim1_state_stack.S', 'src/sim1/sim1_velocity.S', str(obj),
                  str(directory / 'entries.S'), str(directory / 'native.S'),
                  'bin/sim1-diff/lift/resource.o', 'tools/sim1_complete_test.def',
                  '-lmsvcrt', '-lkernel32', '-lgcc'], directory, 'build.log')
    if result.returncode:
        raise RuntimeError('mutation link failed: ' + result.stdout)
    return dll


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'bin/sim1-diff/lift/sim1_lift.c').read_text()
    entries = (ROOT / 'bin/sim1-diff/lift/sim1_entries.S').read_text()
    native = (ROOT / 'src/sim1/sim1_lift_native.S').read_text()
    lift = SimLift(ROOT / 'orig/Sim/SIM1.sim')
    env = dict(os.environ, WINEPREFIX=str(ROOT / 'bin/sim1-diff/wineprefix'), WINEARCH='win32', WINEDEBUG='-all')
    results = []
    with (ROOT / 'bin/sim1-diff/wine.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        baseline = run(['wine', 'bin/sim1-diff/sim1decodediff.exe', 'relocate'], OUTPUT, 'baseline.log', env)
        if baseline.returncode or 'PASS:' not in baseline.stdout:
            raise RuntimeError('decoder baseline failed: ' + baseline.stdout)
        baseline = run(['wine', 'bin/sim1-diff/sim1decodediff.exe', 'relocate',
                        'bin/sim1-diff/SIM1_COMPLETE_TEST.DLL', 'setup'], OUTPUT, 'baseline-setup.log', env)
        if baseline.returncode or 'PASS:' not in baseline.stdout:
            raise RuntimeError('setup baseline failed: ' + baseline.stdout)
        cases = [
            ('decoder-return', 'source', 'f->eax = (uint32_t)(0x1u);\ngoto label_c5b2;', 'f->eax = 0;\ngoto label_c5b2;'),
            ('decoder-copy-destination', 'source', 'label_b5c3: /* push 0x22018c04 */\n' + lift.lower(0xb5c3), 'label_b5c3: /* deliberate defect */\n' + lift.lower(0xb5c3).replace('0x18c04', '0x18c05')),
            ('angle-scale', 'source', 'label_4106: /* fmul QWORD PTR ds:0x22016690 */\n' + lift.lower(0x4106),
             'label_4106: /* deliberate defect */\n' + lift.lower(0x4106).replace('"fmul ', '"fadd ')),
            ('unsigned-angle', 'source', 'label_40e8: /* xor ecx,ecx */\n' + lift.lower(0x40e8),
             'label_40e8: /* deliberate defect */\nf->ecx = 0xffffffffu;'),
            ('native-edx', 'native', 'movl 20(%ebx), %edx\n    movl %edx, 8(%eax)',
             'movl 24(%ebx), %edx\n    movl %edx, 8(%eax)'),
            ('native-random-result', 'native', 'movl 28(%ebx), %edx\n    movl %edx, 0(%eax)',
             'movl 28(%ebx), %edx\n    xorl $0x80000000, %edx\n    movl %edx, 0(%eax)'),
            ('native-flags', 'native', 'movl 32(%ebx), %edx\n    movl %edx, 32(%eax)',
             'xorl %edx, %edx\n    movl %edx, 32(%eax)'),
        ]
        for name, kind, old, new in cases:
            text = native if kind == 'native' else source
            if old == new or text.count(old) != 1:
                raise ValueError(f'{name}: mutation anchor changed')
            directory = OUTPUT / name
            dll = build(directory, source.replace(old, new, 1) if kind == 'source' else source,
                        entries, native.replace(old, new, 1) if kind == 'native' else native, kind == 'native')
            result = run(['wine', 'bin/sim1-diff/sim1decodediff.exe', 'relocate', str(dll)], directory, 'result.log', env)
            caught = result.returncode == 1 and 'side=1' in result.stdout and bool(re.search(
                r'decoded image|decoded GLOBAL|decoder registers|decoder imports|native probe|x87', result.stdout))
            results.append(dict(name=name, caught=caught))
            print(f'{name}: {"caught" if caught else "FAILED"}', flush=True)
            if not caught:
                raise RuntimeError(result.stdout)
        name='aircraft-setup'
        anchor='_sim1_entry_fc40:\npushfl'
        if entries.count(anchor)!=1: raise ValueError('setup entry anchor changed')
        directory=OUTPUT/name
        dll=build(directory,source,entries.replace(anchor,'_sim1_entry_fc40:\nret\npushfl',1),native,reuse=True)
        result=run(['wine','bin/sim1-diff/sim1decodediff.exe','relocate',str(dll),'setup'],directory,'result.log',env)
        caught=result.returncode==1 and 'side=1' in result.stdout and bool(re.search(
            r'decoded image|decoded GLOBAL|decoder registers|decoder imports|x87',result.stdout))
        results.append(dict(name=name,caught=caught))
        print(f'{name}: {"caught by direct setup comparison" if caught else "FAILED"}',flush=True)
        if not caught: raise RuntimeError(result.stdout)
    for index, (name, root) in enumerate((('tick', 0x1020), ('frame', 0x1040))):
        anchor = f'_sim1_entry_{root:x}:\npushfl'
        if entries.count(anchor) != 1:
            raise ValueError('entry anchor changed')
        replacement = f'_sim1_entry_{root:x}:\nret ${lift.signatures[root]}\npushfl'
        directory = OUTPUT / name
        dll = build(directory, source, entries.replace(anchor, replacement, 1), native, reuse=True)
        game_env = dict(os.environ, FS98_DIR=str(ROOT / 'game'), HG2D_FRAMES_ONLY='SIM1',
                        HG2D_FRAMES_SIM1=str(dll), HG2D_FRAMES_NOSOUND='1',
                        HG2D_FRAMES_OUT=str(directory / 'frames'), HG2D_FRAMES_DISPLAY=f':{94+index}')
        result = run(['tools/hg2d_frames.sh', 'Situatio\\STGEORGE.STN', '600',
                      'bin/sim1-diff/scripts/controls.txt'], directory, 'result.log', game_env, timeout=900)
        caught = (result.returncode == 1 and 'retail against retail: identical' in result.stdout and
                  bool(re.search(r'retail against ours:\s+line ', result.stdout)))
        for side in ('retail-a', 'retail-b', 'ours'):
            caught = caught and bool(re.search(side + r': exit 0, .*end 00000258', result.stdout))
            if (directory / 'frames' / side / 'frames.log').read_text().count('fault '):
                caught = False
        results.append(dict(name=name, caught=caught))
        print(f'{name}: {"caught by game state/frames" if caught else "FAILED"}', flush=True)
        if not caught:
            raise RuntimeError(result.stdout)
    (OUTPUT / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(f'PASS: all {len(results)} rebuilt SIM1 defects caught')


if __name__ == '__main__':
    main()
