#!/usr/bin/env python3
"""FLTSIM98.EXE's ABI facts for the decompiled host runtime and its differential harness.

    python3 -B tools/gen_host_rt_tables.py

Writes src/host/runtime/host_rt_tables.inc: no machine code, only metadata read from
orig/FLTSIM98.EXE (and the CD's MFC42.DLL, for how many argument bytes each MFC ordinal
pops), as tools/gen_simsui_tables.py does for SIMSUI:

  HOST_RT_IMPORT(index, ident, "dll", "name", slot_rva, pop_bytes, thiscall, arguments)
      every import slot, in the import directory's order; a caller-cleaned import's arguments
      are the fewest bytes its callers remove after it;
  HOST_RT_FUNCTION(rva, pop_bytes, thiscall)
      every routine the runtime reaches (tools/gen_host_runtime.py's discovery: the entry,
      call targets, code addresses in .data and pushed code addresses), not the services;
      thiscall if it reads ecx before writing it;
  HOST_RT_SERVICE(rva, offset, pop_bytes)
      the HOST service table's routines (0x417ba0 + offset), which the runtime calls directly
      and the harness logs rather than runs; pop bytes are the first ret's, as SIMSUI's.
"""
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_simsui_tables import mfc_pops, mfc_members, reads_ecx, first_ret, MEMBERS_SEEN  # noqa: E402
from dialogmap import disassemble  # noqa: E402
from gen_host_runtime import HostRuntime  # noqa: E402

OUTPUT = 'src/host/runtime/host_rt_tables.inc'
# caller-cleaned (wsprintfA, MCIWndCreateA: VFWAPIV) or argumentless by ordinal (COMCTL32 #17, InitCommonControls)
CDECL_WIN32 = {'wsprintfA', 'MCIWndCreateA', '#17'}
# MFC members the runtime reaches only by a tail jump, with ecx set by its caller: CWinApp's
# constructor (#561) and destructor (#815), and the destructor #818 of the global at 0x418798
HOST_MEMBERS = {561, 815, 818}
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


def main():
    runtime = HostRuntime()
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
    rows = []
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
    lines = ['/* Generated by tools/gen_host_rt_tables.py from orig/FLTSIM98.EXE and cd/mfc42.dll: do not edit. */',
             '#ifdef HOST_RT_IMPORT'] + rows + ['#endif', '#ifdef HOST_RT_FUNCTION'] + functions + \
            ['#endif', '#ifdef HOST_RT_SERVICE'] + services + ['#endif']
    Path(OUTPUT).parent.mkdir(parents=True, exist_ok=True)
    Path(OUTPUT).write_text('\n'.join(lines) + '\n')
    print(f'{OUTPUT}: {len(rows)} imports, {len(functions)} routines, {len(services)} services')


if __name__ == '__main__':
    main()
