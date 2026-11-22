#!/usr/bin/env python3
"""Oracle check for FLTSIM98.EXE's ABI facts: tools/ofs host-rt-tables-check (docs/tools.md).

    python3 -B tools/gen_host_rt_tables.py [--self-test]

src/host/runtime/host_rt_tables.inc is committed source (Story 1.4, slice 16).  This script is what produced it: it
regenerates the rows in memory from orig/FLTSIM98.EXE (and the CD's MFC42.DLL, for how many argument bytes each MFC
ordinal pops), as tools/gen_simsui_tables.py does for SIMSUI, writes nothing, and exits 1 if

  - orig/FLTSIM98.EXE is not the reference executable (sha256 below);
  - host_rt_tables.inc differs from the regenerated rows outside its header comment (the first /* ... */, skipped on both sides);
  - the .rdata bytes it reads (the raw 0x200 bytes) differ from the image src/host/runtime/host_rt_rdata.inc compiles to.

It needs orig/FLTSIM98.EXE, cd/mfc42.dll, objdump, i686-w64-mingw32-nm with the mingw import libraries
(gen_simsui_tables.py) and a host C compiler (cc), so it does not run in a clean checkout alone.  The disassembly and MFC
analysis are kept because they are not recoverable from the file.  The rows:

  HOST_RT_IMPORT(index, ident, "dll", "name", slot_rva, pop_bytes, thiscall, arguments)
      every import slot, in the import directory's order; a caller-cleaned import's arguments
      are the fewest bytes its callers remove after it;
  HOST_RT_FUNCTION(rva, pop_bytes, thiscall)
      every routine the runtime reaches (tools/host_rt_survey.py: the entry,
      call targets, code addresses in .data and pushed code addresses), not the services;
      thiscall if it reads ecx before writing it;
  HOST_RT_SERVICE(rva, offset, pop_bytes)
      the HOST service table's routines (0x417ba0 + offset), which the runtime calls directly
      and the harness logs rather than runs; pop bytes are the first ret's, as SIMSUI's;
  HOST_RT_THUNK(rva, index)
      the import thunks (jmp [slot]) and the import each jumps to;
  HOST_RT_CODE_POINTER(site_rva, target_rva)
      every word of .rdata and .data that holds a code address (the application's vtable, the
      static constructors, the service table, SEH scopes), for the executable to point at ours;
  HOST_RT_DATA_POINTER(site_rva, target_rva)
      and every one that holds an address in .rdata, .data or .idata, for it to rebase (any
      word in that range is taken for a pointer);
  HOST_RT_DIRECT(ident, "symbol")
      every import but MFC's by the symbol of its slot in an import library's thunk table
      (__imp__name@bytes, or __imp__name for a cdecl one), for a build of the services that
      calls the shared routines (HOST_RT_STANDALONE in host_rt.h) through its own import table.

tools/check_data_image.py checks the two pointer kinds against retail on every build (rawptrs= in the HOST row of
tools/data_images.txt).
"""
from pathlib import Path
import hashlib
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_simsui_tables import mfc_pops, mfc_members, reads_ecx, first_ret, MEMBERS_SEEN  # noqa: E402
from dialogmap import disassemble  # noqa: E402
from host_rt_survey import HostSurvey  # noqa: E402

OUTPUT = 'src/host/runtime/host_rt_tables.inc'
RDATA_INC = 'src/host/runtime/host_rt_rdata.inc'
REFERENCE = 'orig/FLTSIM98.EXE'
# the host build depends on the reference EXE (hard-coded addresses here, in host.c and in the images); a different pressing must stop the check
REFERENCE_SHA256 = 'f47c124a1ad44475a8a3a5ecbce83f5dc5d6e7dad78cf120b20616738bd99f6a'
# caller-cleaned (wsprintfA, MCIWndCreateA: VFWAPIV) or argumentless by ordinal (COMCTL32 #17, InitCommonControls)
CDECL_WIN32 = {'wsprintfA', 'MCIWndCreateA', '#17'}
# MFC members the runtime reaches only by a tail jump, with ecx set by its caller: CWinApp's
# constructor (#561) and destructor (#815), and the destructor #818 of the global at 0x418798
HOST_MEMBERS = {561, 815, 818}
# caller-cleaned imports whose calls the cleanup scan cannot see (through a register loaded from the
# slot, or a thunk with other instructions before the add esp): their C prototypes' counts; for the
# variadic wsprintfA its two fixed ones (the harness logs the text it writes)
CALLER_CLEANED_ARGUMENTS = {'wsprintfA': 2, 'MCIWndCreateA': 4, '_makepath': 5, 'realloc': 2, 'malloc': 1, 'atof': 1,
                            'strspn': 2, 'strtoul': 3, 'strtod': 2, 'exit': 1}
WIN32_LIBRARIES = ['user32', 'gdi32', 'kernel32', 'winmm', 'advapi32', 'ddraw', 'comctl32', 'shell32',
                   'msvfw32', 'imm32', 'msacm32']


def win32_pops():
    import subprocess
    pops = {}
    for library in WIN32_LIBRARIES:
        text = subprocess.run(['i686-w64-mingw32-nm', f'/usr/i686-w64-mingw32/lib/lib{library}.a'], check=True,
                              capture_output=True, text=True).stdout
        for match in re.finditer(r' T _(\w+)@(\d+)$', text, re.M):
            pops[match[1]] = int(match[2])
    return pops


def ident(dll, name):
    if dll == 'MFC42.DLL':
        return 'MFC_' + name[1:]
    if name.startswith('??3'):
        return 'operator_delete'
    if name.startswith('??2'):
        return 'operator_new'
    return re.sub(r'\W', '_', name)


def build():
    """(the rows as the text of host_rt_tables.inc, retail's raw .rdata bytes, counts)."""
    runtime = HostSurvey()
    base = runtime.pe.uiBase
    instructions = {base + rva: value for rva, value in runtime.instructions.items()}
    addresses = sorted(instructions)
    imports = runtime.pe.imports()
    slots = {entry['iat_va']: index for index, entry in enumerate(imports)}
    thunks = {}
    for address in addresses:
        mnemonic, operands = instructions[address]
        match = re.match(r'^DWORD PTR ds:0x([0-9a-f]+)$', operands)
        if mnemonic == 'jmp' and match and int(match[1], 16) in slots:
            thunks[address] = slots[int(match[1], 16)]
    member, cleanup, previous = {}, {}, []
    for position, address in enumerate(addresses):
        mnemonic, operands = instructions[address]
        if mnemonic == 'call':
            match = re.match(r'^(?:0x([0-9a-f]+)|DWORD PTR ds:0x([0-9a-f]+))$', operands)
            target = None
            if match and match[1] and int(match[1], 16) in thunks:
                target = thunks[int(match[1], 16)]
            elif match and match[2] and int(match[2], 16) in slots:
                target = slots[int(match[2], 16)]
            if target is not None:
                loads = False
                for prior_mnemonic, prior_operands in reversed(previous):
                    if prior_mnemonic == 'push':
                        continue
                    loads = prior_mnemonic in ('mov', 'lea') and prior_operands.startswith('ecx,')
                    break
                member.setdefault(target, []).append(loads)
                if position + 1 < len(addresses):
                    next_mnemonic, next_operands = instructions[addresses[position + 1]]
                    match = re.match(r'^esp,(0x[0-9a-f]+)$', next_operands)
                    if next_mnemonic == 'add' and match:
                        cleanup.setdefault(target, []).append(int(match[1], 16))
                    elif next_mnemonic == 'pop' and next_operands in ('ecx', 'edx'):   # one argument popped away
                        cleanup.setdefault(target, []).append(4)
        previous = (previous + [(mnemonic, operands)])[-6:]
    win32, mfc, members = win32_pops(), mfc_pops(), mfc_members()
    rows, direct_rows = [], []
    for index, entry in enumerate(imports):
        dll, name = entry['dll'].upper(), entry['name']
        if dll == 'MFC42.DLL':
            pop = mfc.get(int(name[1:]), 0)
        elif dll == 'MSVCRT.DLL':
            pop = 0
        elif name in win32:
            pop = win32[name]
        elif name in CDECL_WIN32:
            pop = 0
        else:
            raise ValueError(f'{dll} {name}: no stdcall decoration and not a known cdecl import')
        votes = member.get(index, [])
        thiscall = int(dll == 'MFC42.DLL' and (int(name[1:]) in members or int(name[1:]) in MEMBERS_SEEN or int(name[1:]) in HOST_MEMBERS or
                                               (bool(votes) and sum(votes) * 2 > len(votes))))
        arguments = min(cleanup[index]) // 4 if pop == 0 and index in cleanup and not thiscall else pop // 4
        if pop == 0 and not thiscall and name in CALLER_CLEANED_ARGUMENTS:
            arguments = CALLER_CLEANED_ARGUMENTS[name]
        if dll != 'MFC42.DLL':
            stdcall = dll != 'MSVCRT.DLL' and name not in CDECL_WIN32
            direct_rows.append(f'HOST_RT_DIRECT({ident(dll, name)}, "__imp__{name}' + (f'@{pop}' if stdcall else '') + '")')
        rows.append(f'HOST_RT_IMPORT({index}, {ident(dll, name)}, "{dll}", "{name}", {entry["iat_va"] - base:#x}, '
                    f'{pop}, {thiscall}, {arguments})')
    functions = []
    for start in sorted(runtime.functions):
        if start in runtime.services or base + start in thunks:
            continue
        pop = runtime.signatures.get(start)
        if pop is None:
            pop = 0xffff        # no single ret (a tail into another routine, or none)
        functions.append(f'HOST_RT_FUNCTION({start:#x}, {pop}, {int(reads_ecx(instructions, addresses, base + start))})')
    linear = disassemble(runtime.pe)
    linear_addresses = sorted(linear)
    services = [f'HOST_RT_SERVICE({rva:#x}, {offset:#x}, {first_ret(linear, linear_addresses, base + rva)})'
                for rva, offset in sorted(runtime.services.items())]
    thunk_rows = [f'HOST_RT_THUNK({address - base:#x}, {index})' for address, index in sorted(thunks.items())]
    pointer_rows, data_rows, rdata = [], [], None
    for section in runtime.pe.aSections:
        if section['name'] == '.rdata':
            rdata = runtime.pe.read_bytes(section['raw_offset'], section['raw_size'])
        if section['name'] not in ('.rdata', '.data'):
            continue
        for site in range(section['rva'], section['rva'] + section['raw_size'] - 3, 4):
            value = runtime.pe.va_uint32(base + site)
            if runtime.pe.is_code(value):
                pointer_rows.append(f'HOST_RT_CODE_POINTER({site:#x}, {value - base:#x})')
            elif base + 0x16000 <= value < base + 0x1f000:
                data_rows.append(f'HOST_RT_DATA_POINTER({site:#x}, {value - base:#x})')
    lines = ['/* Generated by tools/gen_host_rt_tables.py. */',
             '#ifdef HOST_RT_IMPORT'] + rows + ['#endif', '#ifdef HOST_RT_FUNCTION'] + functions + \
            ['#endif', '#ifdef HOST_RT_SERVICE'] + services + ['#endif', '#ifdef HOST_RT_THUNK'] + thunk_rows + \
            ['#endif', '#ifdef HOST_RT_CODE_POINTER'] + pointer_rows + ['#endif', '#ifdef HOST_RT_DATA_POINTER'] + data_rows + \
            ['#endif', '#ifdef HOST_RT_DIRECT'] + direct_rows + ['#endif']
    return '\n'.join(lines) + '\n', rdata, (len(rows), len(functions), len(services))


def body(text):
    """Text after the leading header comment (the file must start with it; None if it does not): skipped on both sides."""
    return text[text.index('*/') + 2:] if text.startswith('/*') and '*/' in text else None


def problems_of(committed, regenerated, compiled, rdata):
    """What differs between the committed file and the regenerated rows, and between the compiled and retail .rdata; [] if nothing."""
    found = []
    if body(committed) is None or body(regenerated) is None:
        found.append(f'{OUTPUT} does not start with its header comment')
    elif body(committed) != body(regenerated):
        found.append(f'{OUTPUT} differs from the rows regenerated from {REFERENCE} (outside its header comment)')
    if compiled != rdata:
        found.append(f'{RDATA_INC} compiles to different bytes than retail\'s .rdata (0x{len(rdata):x} raw bytes)')
    return found


def self_test():
    """Negative checks on in-memory mutations of the committed text and the compiled .rdata, with no regeneration; writes nothing."""
    text, rdata = Path(OUTPUT).read_text(), compiled_rdata()
    row = text.index('HOST_RT_FUNCTION(', text.index('#ifdef HOST_RT_FUNCTION'))
    flipped = bytes([rdata[0] ^ 1]) + rdata[1:]
    cases = [('the committed file against itself', text, rdata, False),
             ('a header edit', text.replace('/*', '/* x', 1), rdata, False),
             ('a changed row', text[:row] + 'HOST_RT_FUNCTION(0x1, 0, 0)\n' + text[text.index('\n', row) + 1:], rdata, True),
             ('a prepended line', '#define HOST_RT_IMPORT(a, b, c, d, e, f, g, h)\n' + text, rdata, True),
             ('a changed .rdata byte', text, flipped, True)]
    failed = [name for name, committed, compiled, bad in cases if bool(problems_of(committed, text, compiled, rdata)) != bad]
    return failed, len(cases)


def compiled_rdata():
    """The bytes host_rt_rdata.inc compiles to, as a host C compiler lays them out."""
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory, 'rdata.c')
        source.write_text('#include <stdint.h>\n#include <stdio.h>\n'
                          'static const uint8_t rdata[] = {\n#include "%s"\n};\n'
                          'int main(void) { for (unsigned i = 0; i < sizeof rdata; i++) printf("%%02x", rdata[i]); return 0; }\n'
                          % Path(RDATA_INC).resolve())
        subprocess.run(['cc', '-std=c99', '-w', '-o', str(Path(directory, 'rdata')), str(source)], check=True)
        return bytes.fromhex(subprocess.run([str(Path(directory, 'rdata'))], check=True, capture_output=True, text=True).stdout)


def main():
    if sys.argv[1:] == ['--self-test']:
        failed, count = self_test()
        for name in failed:
            print('self-test FAILED: ' + name)
        print(f'host-rt-tables-check self-test: {count - len(failed)}/{count} cases' + (': FAILED' if failed else ''))
        return 1 if failed else 0
    if sys.argv[1:]:
        sys.exit(__doc__)
    if hashlib.sha256(Path(REFERENCE).read_bytes()).hexdigest() != REFERENCE_SHA256:
        print(f'{REFERENCE}: not the reference executable')
        return 1
    failed, count = self_test()
    if failed:
        print('self-test FAILED: ' + ', '.join(failed))
        print('host-rt-tables-check: FAILED')
        return 1
    text, rdata, (imports, functions, services) = build()
    problems = problems_of(Path(OUTPUT).read_text(), text, compiled_rdata(), rdata)
    for problem in problems:
        print(problem)
    if problems:
        print('host-rt-tables-check: FAILED')
        return 1
    print(f'host-rt-tables-check: {imports} imports, {functions} routines, {services} services match; '
          f'.rdata 0x{len(rdata):x} bytes match; self-test {count} cases')
    return 0


if __name__ == '__main__':
    sys.exit(main())
