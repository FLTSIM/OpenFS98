#!/usr/bin/env python3
"""What the WORLD native break runners share (Story 1.8 slice 6b0): the input snapshot, the DLL and harness compile argvs
(the DLL's units come from src/world/sources.txt, as the retail build does), the Case builder, and the loop of the twelve
near-identical `world_<area>_native_breaks.py` runners: build the mutant DLL and a harness, run `wine harness dll N`,
count only `<AREA> UI DIFFER` with exit 1 as caught.  Faults and timeouts never count."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import break_build_only as bo
import module_flags
from pathlib import Path
import hashlib
import json
import subprocess
import sys

FLAGS = ['i686-w64-mingw32-gcc', *module_flags.FLAGS, '-std=c99', '-Itools/difftest', '-O2', '-Wall', '-Wextra', '-Werror']


_SOURCES_TXT = None


def sources_txt():
    """src/world/sources.txt, read once per run: the DLL's unit list and its hash in inputs.json come from the same bytes."""
    global _SOURCES_TXT
    if _SOURCES_TXT is None:
        _SOURCES_TXT = Path('src/world/sources.txt').read_bytes()
    return _SOURCES_TXT


def units():
    return sources_txt().decode().split()


def sources(harnesses, baseline_dll=False):
    """src/world's sources, the harness sources (their include path made local) and the prebuilt resource inputs."""
    data = {path.name: path.read_bytes() for path in Path('src/world').iterdir() if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in harnesses:
        data[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        data[name] = (Path('bin/world-diff') / name).read_bytes()
    data['sources.txt'] = sources_txt()
    if baseline_dll:
        data['baseline.dll'] = Path('bin/world-diff/WORLD_area.dll').read_bytes()
    return data


def write_inputs_json(output_dir, data):
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(blob).hexdigest() for name, blob in data.items()}, indent=2) + '\n')


def dll_command(work, dll, flags=FLAGS):
    return [*flags, '-Iinclude', '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
            *(str(work / Path(unit).name) for unit in units()),
            str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll']


def ui_command(work, exe, harness, flags=FLAGS):
    return [*flags, '-o', str(exe), str(work / harness), '-lgdi32', '-lntdll']


def mutate(data, name, filename, before, after, posts=None):
    """The one-anchor replacement every runner makes: `data` is filename's bytes."""
    if data.count(before.encode()) != 1:
        raise ValueError(f'{name}: mutation anchor is not unique')
    data = data.replace(before.encode(), after.encode(), 1)
    for old, new in (posts or {}).get(name, ()):
        if data.count(old) != 1:
            raise ValueError(f'{name}: follow-up anchor is not unique')
        data = data.replace(old, new)
    return data


def case(case_id, data, filename, replace, builds, outputs):
    """A Case over the snapshot `data`: only filename's mutated bytes are the overlay."""
    return bo.Case(case_id, lambda: {filename: replace(data[filename])}, builds, outputs)


def ui_main(label, directory, harness, marker, mutations, wine_n, timeout=180, baseline_dll=False, artifacts=False, posts=None, unknown=None):
    """A `world_<area>_native_breaks.py`: mutations are (name, file, before, after); wine_n(name) the harness's N."""
    exe_name = harness[:-2] + '.exe'
    args = sys.argv[1:]

    def cases_for(data):
        return [case(name, data, filename, lambda blob, n=name, f=filename, b=before, a=after: mutate(blob, n, f, b, a, posts),
                     lambda work: [dll_command(work, work / 'WORLD_area.dll'), ui_command(work, work / exe_name, harness)],
                     ['WORLD_area.dll', exe_name]) for name, filename, before, after in mutations]
    if '--build-only' in args:
        def factory():
            data = sources((harness, 'worlduidiff.c'), baseline_dll)
            return cases_for(data), data
        return bo.build_only(args[args.index('--build-only') + 1:], factory)
    output_dir = Path(directory)
    output_dir.mkdir(parents=True, exist_ok=True)
    data = sources((harness, 'worlduidiff.c'), baseline_dll)
    write_inputs_json(output_dir, data)
    requested = set(args)
    if requested - {item[0] for item in mutations}:
        raise ValueError(unknown or f'unknown {label} native mutation')
    selected = [c for c in cases_for(data) if not requested or c.id in requested]
    caught = 0
    for case_ in selected:
        name = case_.id
        work = output_dir / name
        work.mkdir(exist_ok=True)
        bo.write_case(work, data, case_.overlay())
        dll, exe = work / 'WORLD_area.dll', work / exe_name
        commands = case_.builds(work)
        for command in commands:
            build_result = subprocess.run(command, capture_output=True, text=True)
            if build_result.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build_result.stderr}', flush=True)
                return 1
        if artifacts:
            (work / 'artifacts.json').write_text(json.dumps({path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in (dll, exe)}, indent=2) + '\n')
        try:
            result = subprocess.run(['wine', str(exe), str(dll), wine_n(name)], capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = result.stdout + result.stderr
        (work / 'result.log').write_text(output)
        if result.returncode == 1 and marker in output:
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{output}', flush=True)
    print(f'{label}: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1
