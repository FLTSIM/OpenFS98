#!/usr/bin/env python3
"""SIMSUI.DLL's ABI facts for our transliteration and its differential harness.

    python3 -B tools/gen_simsui_tables.py

Writes src/simsui/simsui_tables.inc: no machine code, only metadata read from
orig/SIMSUI.DLL (and the CD's MFC42.DLL, for how many argument bytes each MFC ordinal
pops):

  SIMSUI_IMPORT(index, ident, "dll", "name", slot_rva, pop_bytes, thiscall, arguments)
      every import slot, in the import directory's order; a caller-cleaned import's arguments
      are the fewest bytes its callers remove after it;
  SIMSUI_FUNCTION(rva, pop_bytes, convention, "export name")
      every routine in .text: the exports, call targets and code addresses held in
      .rdata (vtables, message maps); convention 0 thiscall, 1 stdcall, 2 cdecl;
  SIMSUI_VTABLE(rva, slots, "export name")
      the exported vtables, and how many slots each has (up to the next datum).

A routine's pop bytes are its first `ret` in address order, which for MSVC's single-exit
code is its own. Whether an MFC ordinal is a member (reads ECX) comes from its call sites
in SIMSUI, which load ECX last.
"""
from pathlib import Path
import re
import struct
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dialogmap import Fs98Pe, disassemble  # noqa: E402

IMAGE = 'orig/SIMSUI.DLL'
MFC = 'cd/mfc42.dll'
WIN32_LIBRARIES = ['user32', 'gdi32', 'kernel32', 'imm32']
OUTPUT = 'src/simsui/simsui_tables.inc'
RET = re.compile(r'^(0x[0-9a-f]+)$')


def win32_pops():
    """Argument bytes of each stdcall Win32 import, from mingw's decorated import libraries."""
    import subprocess
    pops = {}
    for library in WIN32_LIBRARIES:
        path = f'/usr/i686-w64-mingw32/lib/lib{library}.a'
        text = subprocess.run(['i686-w64-mingw32-nm', path], check=True, capture_output=True,
                              text=True).stdout
        for match in re.finditer(r' T _(\w+)@(\d+)$', text, re.M):
            pops[match[1]] = int(match[2])
    return pops


def first_ret(instructions, addresses, start, tails=None):
    """The pop bytes of the first ret at or after start, in address order. A jump to a known
    routine or import thunk before it is a tail call: that routine's or import's pops.
    tails maps such targets to their pops."""
    import bisect
    at = bisect.bisect_left(addresses, start)
    for address in addresses[at:at + 4000]:
        mnemonic, operands = instructions[address]
        if mnemonic == 'ret':
            return int(operands, 16) if RET.match(operands) else 0
        if mnemonic == 'jmp' and tails is not None and re.match(r'^0x[0-9a-f]+$', operands):
            target = int(operands, 16)
            if target != start and target in tails:
                return tails[target]()
    raise ValueError(f'no ret after {start:#x}')


def mfc_pops():
    pe = Fs98Pe(MFC)
    rva, _ = pe.directory(0)
    offset = pe.file_offset(rva, 40)
    ordinal_base = pe.read_uint32(offset + 16)
    count = pe.read_uint32(offset + 20)
    functions = pe.read_uint32(offset + 28)
    instructions = disassemble(pe)
    addresses = sorted(instructions)
    pops = {}
    for index in range(count):
        target = pe.uiBase + pe.read_uint32(pe.file_offset(functions + index * 4, 4))
        if target in instructions:
            # a bare jmp to another routine: that routine's
            mnemonic, operands = instructions[target]
            if mnemonic == 'jmp' and re.match(r'^0x[0-9a-f]+$', operands) and int(operands, 16) in instructions:
                target = int(operands, 16)
            pops[index + ordinal_base] = first_ret(instructions, addresses, target)
    return pops


# members whose code opens with a call (so ecx is not seen read) and whose callers set ecx
# further back than the vote looks: ~CPaintDC
MEMBERS_SEEN = {755}


def reads_ecx(instructions, addresses, target):
    """Whether a routine reads ecx before it writes it (or calls out): MFC's members, which
    take this in ecx, do; its static and global functions do not."""
    address, seen = target, 0
    positions = {value: index for index, value in enumerate(addresses)}
    while address in instructions and seen < 60:
        mnemonic, operands = instructions[address]
        seen += 1
        destination, _, source = operands.partition(',')
        if mnemonic in ('call', 'ret', 'int3'):
            return False
        if mnemonic == 'jmp':
            if re.match(r'^0x[0-9a-f]+$', operands) and int(operands, 16) in instructions:
                address = int(operands, 16)
                continue
            return 'ecx' in operands
        if mnemonic == 'pop' and operands == 'ecx':
            return False
        if mnemonic == 'xor' and operands == 'ecx,ecx':
            return False
        if mnemonic in ('mov', 'lea', 'movzx', 'movsx') and destination == 'ecx':
            return 'ecx' in source
        if 'ecx' in operands:
            return True
        index = positions.get(address)
        if index is None or index + 1 >= len(addresses):
            return False
        address = addresses[index + 1]
    return False


def mfc_members():
    pe = Fs98Pe(MFC)
    rva, _ = pe.directory(0)
    offset = pe.file_offset(rva, 40)
    ordinal_base = pe.read_uint32(offset + 16)
    count = pe.read_uint32(offset + 20)
    functions = pe.read_uint32(offset + 28)
    instructions = disassemble(pe)
    addresses = sorted(instructions)
    members = set()
    for index in range(count):
        target = pe.uiBase + pe.read_uint32(pe.file_offset(functions + index * 4, 4))
        if target in instructions and reads_ecx(instructions, addresses, target):
            members.add(index + ordinal_base)
    return members


def ident(dll, name):
    if dll == 'MFC42.DLL':
        return 'MFC_' + name[1:]
    if name.startswith('?terminate'):
        return 'terminate'
    return name


def convention(name):
    """From a mangled export: 0 thiscall, 1 stdcall, 2 cdecl."""
    match = re.search(r'@@([A-Z])([A-Z])([A-Z])', name)
    if not match or not name.startswith('?'):
        return None
    access = match[1]
    call = match[2] if access in 'YSKC' else match[3]
    return {'E': 0, 'G': 1, 'A': 2}.get(call)


def main():
    pe = Fs98Pe(IMAGE)
    base = pe.uiBase
    instructions = disassemble(pe)
    addresses = sorted(instructions)
    text = next(section for section in pe.aSections if section['name'] == '.text')
    rdata = next(section for section in pe.aSections if section['name'] == '.rdata')
    text_start, text_end = base + text['rva'], base + text['rva'] + text['virtual_size']

    def in_text(value):
        return text_start <= value < text_end

    def resync(start):
        """a linear pass lost after a jump table's data: disassembled again from a code
        pointer in .rdata (a vtable slot) to the routine's first ret"""
        result = subprocess.run(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn', f'--start-address={start:#x}',
                                 f'--stop-address={min(start + 0x1000, text_end):#x}', IMAGE],
                                check=True, capture_output=True, text=True)
        found = False
        for line in result.stdout.splitlines():
            match = re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)', line)
            if not match:
                continue
            address = int(match[1], 16)
            if address in instructions and found:
                break
            instructions[address] = (match[2], match[3].strip())
            found = True
            if match[2] == 'ret':
                break
        return found

    imports = pe.imports()
    slots = {entry['iat_va']: index for index, entry in enumerate(imports)}
    thunks = {}
    for address in addresses:
        mnemonic, operands = instructions[address]
        match = re.match(r'^DWORD PTR ds:0x([0-9a-f]+)$', operands)
        if mnemonic == 'jmp' and match and int(match[1], 16) in slots:
            thunks[address] = slots[int(match[1], 16)]

    # MFC members: SIMSUI loads ECX just before calling them
    member = {}
    cleanup = {}
    previous = []
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
        previous = (previous + [(mnemonic, operands)])[-6:]

    import_pops = {}
    win32 = win32_pops()
    mfc = mfc_pops()
    members = mfc_members()
    rows = []
    for index, entry in enumerate(imports):
        dll, name = entry['dll'].upper(), entry['name']
        if dll == 'MFC42.DLL':
            pop = mfc.get(int(name[1:]), 0)
        elif dll == 'MSVCRT.DLL':
            pop = 0
        else:
            pop = win32[name]
        import_pops[index] = pop
        votes = member.get(index, [])
        # a member: its code reads ecx first, or its callers load ecx just before (for those
        # that open with __EH_prolog, a call that keeps ecx)
        thiscall = int(dll == 'MFC42.DLL' and (int(name[1:]) in members or int(name[1:]) in MEMBERS_SEEN or
                                               (bool(votes) and sum(votes) * 2 > len(votes))))
        # a caller-cleaned import's arguments: the least its callers remove after it
        arguments = min(cleanup[index]) // 4 if pop == 0 and index in cleanup and not thiscall else pop // 4
        rows.append(f'SIMSUI_IMPORT({index}, {ident(dll, name)}, "{dll}", "{name}", {entry["iat_va"] - base:#x}, '
                    f'{pop}, {thiscall}, {arguments})')

    exports = pe.exports()
    names = {}
    for name, rva in exports.items():
        names.setdefault(rva, []).append(name)
    functions = set()
    for rva in exports.values():
        if in_text(base + rva) and base + rva not in thunks:
            functions.add(base + rva)
    for address in addresses:
        mnemonic, operands = instructions[address]
        if mnemonic == 'call' and re.match(r'^0x[0-9a-f]+$', operands):
            target = int(operands, 16)
            if in_text(target) and target not in thunks:
                functions.add(target)
    data = next(section for section in pe.aSections if section['name'] == '.data')
    for section in (rdata, data):
        raw = pe.read_bytes(section['raw_offset'], min(section['virtual_size'], section['raw_size']))
        for at in range(0, len(raw) - 3, 4):
            value = struct.unpack_from('<I', raw, at)[0]
            if in_text(value) and value not in thunks and value not in instructions and section is rdata \
                    and pe.read_bytes(pe.file_offset(value - base, 1), 1) != b'\xcc' and resync(value):
                addresses = sorted(instructions)
            if in_text(value) and value not in thunks and value in instructions:
                functions.add(value)
    # the entry point, and code whose address is pushed (atexit handlers, destructors given
    # to the vector iterators)
    entry = pe.read_uint32(pe.read_uint32(0x3c) + 0x28)
    functions.add(base + entry)
    for address in addresses:
        mnemonic, operands = instructions[address]
        if mnemonic in ('push', 'mov') and re.search(r'0x([0-9a-f]{8})$', operands):
            value = int(re.search(r'0x([0-9a-f]{8})$', operands)[1], 16)
            if in_text(value) and value not in thunks and value in instructions:
                functions.add(value)
    tails = {}
    for thunk, index in thunks.items():
        tails[thunk] = (lambda index=index: import_pops[index])
    for address in functions:
        tails[address] = (lambda address=address: first_ret(instructions, addresses, address, tails))
    function_rows = []
    for address in sorted(functions):
        try:
            first_ret(instructions, addresses, address, tails)
        except (ValueError, RecursionError):
            continue
        exported = names.get(address - base, [])
        conventions = {convention(name) for name in exported} - {None}
        call = conventions.pop() if len(conventions) == 1 else (0 if not exported else 2)
        function_rows.append(f'SIMSUI_FUNCTION({address - base:#x}, {first_ret(instructions, addresses, address, tails)}, '
                             f'{call}, "{exported[0] if exported else ""}")')

    data_exports = sorted((rva, name) for name, rva in exports.items() if not in_text(base + rva))
    vtable_rows = []
    for position, (rva, name) in enumerate(data_exports):
        if not name.startswith('??_7'):
            continue
        end = data_exports[position + 1][0] if position + 1 < len(data_exports) else rva + 0x400
        slots_count = 0
        while rva + slots_count * 4 < end:
            value = pe.read_uint32(pe.file_offset(rva + slots_count * 4, 4))
            if not in_text(value):
                break
            slots_count += 1
        vtable_rows.append(f'SIMSUI_VTABLE({rva:#x}, {slots_count}, "{name}")')

    lines = ['/* Generated by tools/gen_simsui_tables.py from orig/SIMSUI.DLL and cd/mfc42.dll: do not edit. */',
             '#ifdef SIMSUI_IMPORT'] + rows + ['#endif', '#ifdef SIMSUI_FUNCTION'] + function_rows + \
            ['#endif', '#ifdef SIMSUI_VTABLE'] + vtable_rows + ['#endif']
    Path(OUTPUT).parent.mkdir(parents=True, exist_ok=True)
    Path(OUTPUT).write_text('\n'.join(lines) + '\n')
    print(f'{OUTPUT}: {len(rows)} imports, {len(function_rows)} routines, {len(vtable_rows)} vtables')


if __name__ == '__main__':
    main()
