#!/usr/bin/env python3
"""SIM1.sim's ABI facts for its decompilation and the per-routine harness (sim1rdiff).

    python3 -B tools/gen_sim1_tables.py

Writes src/sim1/sim1_tables.inc: no machine code, only metadata read from orig/Sim/SIM1.sim
and the retail modules it calls:

  SIM1_ROUTINE(rva, pop, inputs, outputs, flags_out, cc_out)
      every routine: the Linkage entries, call targets and code addresses held in data.
      pop is its first ret's bytes. inputs is a mask of the registers it reads before writing
      them (bit 0 eax, 1 ecx, 2 edx, 3 ebx, 4 esp, 5 ebp, 6 esi, 7 edi); outputs is the
      registers its callers read after it returns before writing them, and flags_out whether
      a caller tests the flags it leaves. Many of SIM1's routines are old hand-written
      assembly that pass values in registers, so what a routine must leave is what its
      callers use, not the C ABI.
  SIM1_SERVICE(index, module_id, offset, pop, reads, flags)
      every module service SIM1 calls through its ImportTable (link at .data 0x16004 + 8 *
      import), with the bytes the retail service pops, and which of ecx/edx (the same mask)
      and the flags SIM1 reads after it before writing them.
  SIM1_IMPORT(index, "dll", "name", slot_rva)
      the PE imports (KERNEL32, MSVCRT).

The register analysis walks each path forward from an entry (or a return address) for at
most 64 instructions, following jumps, until every register has been read or written.
"""
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dialogmap import Fs98Pe, disassemble  # noqa: E402

IMAGE = 'orig/Sim/SIM1.sim'
OUTPUT = 'src/sim1/sim1_tables.inc'
BASE = 0x22000000
TEXT_END = 0x22014160
CRT_START = 0x22014160          # the MSVC runtime's import thunks at the end of .text (SIM1's own code runs to 0x2201415d)
REGS = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
ALIASES = {'eax': ('eax', 'ax', 'al', 'ah'), 'ecx': ('ecx', 'cx', 'cl', 'ch'),
           'edx': ('edx', 'dx', 'dl', 'dh'), 'ebx': ('ebx', 'bx', 'bl', 'bh'),
           'esp': ('esp', 'sp'), 'ebp': ('ebp', 'bp'), 'esi': ('esi', 'si'), 'edi': ('edi', 'di')}
MODULE_FILES = {1: 'orig/FLTSIM98.EXE', 3: 'orig/Modules/MAIN.DLL', 4: 'orig/Modules/CHAIN.DLL',
                10: 'orig/Modules/WINDOW.DLL', 16: 'orig/Modules/CONTROLS.DLL',
                20: 'orig/Modules/UTIL.DLL', 23: 'orig/Modules/ACONTAIN.DLL',
                32: 'orig/Modules/REALITY.DLL', 39: 'orig/Modules/SOUND.DLL'}
HOST_TABLE = 0x417ba0
FLAG_READERS = re.compile(r'^(j(?!mp)|set|adc|sbb|cmov|rc[lr]|pushf|lahf)')
WRITE_ONLY = {'mov', 'lea', 'movzx', 'movsx', 'pop', 'fnstsw', 'fstsw', 'setb', 'sete', 'setne', 'seta', 'setl', 'setg'}
FLAG_WRITERS = re.compile(r'^(add|sub|and|or|xor|cmp|test|inc|dec|neg|shl|shr|sar|sal|imul|mul|'
                          r'div|idiv|bt|bsf|bsr|sahf|popf|fcomi|fucomi)')


def mentions(operand, reg):
    return any(re.search(r'\b' + name + r'\b', operand) for name in ALIASES[reg])


def split(operands):
    parts, depth, current = [], 0, ''
    for char in operands:
        if char == ',' and depth == 0:
            parts.append(current.strip()); current = ''
            continue
        depth += char == '['
        depth -= char == ']'
        current += char
    if current.strip():
        parts.append(current.strip())
    return parts


def effects(mnemonic, operands):
    """(read, written) register sets of one instruction, coarsely but conservatively."""
    parts = split(re.sub(r'<[^>]*>', '', operands))
    read, written = set(), set()
    if mnemonic in ('xor', 'sub', 'sbb') and len(parts) == 2 and parts[0] == parts[1] and '[' not in parts[0]:
        for reg in REGS:        # xor r, r: a write only (sbb r, r reads the carry, not r)
            if mentions(parts[0], reg):
                written.add(reg if parts[0] == reg else reg + '.low')
        return read, written
    for reg in REGS:
        for index, part in enumerate(parts):
            if not mentions(part, reg):
                continue
            if '[' in part:
                read.add(reg)
            elif index == 0 and len(parts) >= 1 and mnemonic in WRITE_ONLY:
                # a 32-bit write kills the register; a partial one only its low part (low_dead)
                if part == reg or mnemonic in ('movzx', 'movsx', 'lea') or reg in ('esp',):
                    written.add(reg)
                else:
                    written.add(reg + '.low')
            elif index == 0 and mnemonic == 'xor' and len(parts) == 2 and parts[0] == parts[1]:
                written.add(reg)
            elif index == 0 and mnemonic == 'push':
                read.add(reg)
            else:
                read.add(reg)
                if index == 0 and mnemonic not in ('cmp', 'test', 'push'):
                    written.add(reg)
    implicit = {
        'cdq': ({'eax'}, {'edx'}), 'cwd': ({'eax'}, {'edx'}), 'cwde': ({'eax'}, {'eax'}),
        'cbw': ({'eax'}, {'eax'}), 'lahf': (set(), {'eax'}), 'sahf': ({'eax'}, set()),
        'fnstsw': (set(), {'eax'}) if 'ax' in operands else (set(), set()),
        'leave': ({'ebp'}, {'esp', 'ebp'}),
    }
    if mnemonic in implicit:
        r, w = implicit[mnemonic]
        read |= r; written |= w
    if mnemonic in ('mul', 'imul', 'div', 'idiv') and len(parts) == 1:
        read |= {'eax'} | ({'edx'} if mnemonic in ('div', 'idiv') else set())
        written |= {'eax', 'edx'}
    if mnemonic.startswith('rep') or (mnemonic.startswith(('movs', 'stos', 'lods', 'scas', 'cmps')) and
                                      mnemonic not in ('movsx', 'movsd' if '[' not in operands else '')):
        read |= {'ecx', 'esi', 'edi', 'eax'}
        written |= {'ecx', 'esi', 'edi'}
    if mnemonic.startswith(('shl', 'shr', 'sar', 'rol', 'ror', 'rcl', 'rcr', 'shld', 'shrd')) and 'cl' in parts[-1:]:
        read.add('ecx')
    if mnemonic == 'xchg' and len(parts) == 2:
        for reg in REGS:
            if any(mentions(p, reg) for p in parts):
                read.add(reg)
                if any(mentions(p, reg) and '[' not in p for p in parts):
                    written.add(reg)
    for reg in list(read):     # a read of the whole register, not only its low part
        if any(re.search(r'\b' + reg + r'\b', part) for part in parts) or mnemonic in ('cdq', 'push', 'pushad'):
            read.add(reg + '.full')
    return read - {'esp', 'esp.full'}, written - {'esp'}


def live_reads(read, dead):
    return {r for r in read if r in REGS and r not in dead and (r + '.low' not in dead or r + '.full' in read)}


class Sim1:
    def __init__(self):
        self.pe = Fs98Pe(IMAGE)
        self.code = {a: v for a, v in disassemble(self.pe).items() if BASE + 0x1000 <= a < TEXT_END}
        self.addresses = sorted(self.code)
        self.next = {a: b for a, b in zip(self.addresses, self.addresses[1:] + [TEXT_END])}
        linkage = self.pe.linkage()
        self.roots = set()
        for slot in range(1, 15):
            value = self.pe.va_uint32(linkage['va'] + slot * 4)
            if BASE + 0x1000 <= value < TEXT_END:
                self.roots.add(value)
        self.services_entries = set(self.roots)     # called by modules for their results
        self.targets = set()
        for address, (mnemonic, operands) in self.code.items():
            match = re.match(r'(0x[0-9a-f]+)', operands)
            if mnemonic == 'call' and match:
                self.targets.add(int(match[1], 16))
        # code addresses pushed or stored as data (callbacks)
        for address, (mnemonic, operands) in self.code.items():
            for value in re.findall(r'0x(220[01][0-9a-f]{4})\b', operands):
                value = int(value, 16)
                if value in self.code and mnemonic in ('push', 'mov') and '[' not in operands.split(',')[-1]:
                    self.roots.add(value)
        # code addresses held in .rdata/.data (relocated words): callbacks and tables of them
        from gen_gauges import relocations
        for site, target in relocations(self.pe):
            if site >= 0x15000 and BASE + 0x1000 <= BASE + target < TEXT_END and BASE + target in self.code:
                self.roots.add(BASE + target)
        self.routines = sorted(a for a in self.roots | self.targets if a < CRT_START)

    def cases(self, table):
        result = []
        for index in range(256):
            value = self.pe.va_uint32(table + index * 4)
            if value not in self.code:
                break
            result.append(value)
        return result

    def first_ret(self, start):
        import bisect
        at = bisect.bisect_left(self.addresses, start)
        for address in self.addresses[at:at + 6000]:
            mnemonic, operands = self.code[address]
            if mnemonic == 'ret':
                return int(operands, 16) if operands else 0
        return -1

    CC_WRITERS = re.compile(r'^(fcom|fucom|ftst|fxam|fprem|fsin|fcos|fsincos|fptan|fpatan|fninit|finit|frstor|fldenv)')

    def walk(self, start, want_flags, want_cc=False):
        """Registers read before written on any path from start, and whether flags are (and the
        x87 condition codes, in self.cc_read)."""
        read, flags = set(), False
        self.cc_read = False
        self.returns = []           # the dead set on each path that reaches a ret
        stack = [(start, frozenset(), want_flags, 0, want_cc)]
        seen = set()
        while stack:
            address, dead, flags_live, steps, cc_live = stack.pop()
            while address in self.code and steps < 400:
                if (address, dead, flags_live, cc_live) in seen:
                    break
                seen.add((address, dead, flags_live, cc_live))
                mnemonic, operands = self.code[address]
                if cc_live and mnemonic in ('fnstsw', 'fstsw', 'fnsave', 'fsave', 'fnstenv', 'fstenv'):
                    self.cc_read = True
                if cc_live and self.CC_WRITERS.match(mnemonic):
                    cc_live = False
                r, w = effects(mnemonic, operands)
                read |= live_reads(r, dead)
                dead = dead | w
                if flags_live and FLAG_READERS.match(mnemonic):
                    flags = True
                if flags_live and FLAG_WRITERS.match(mnemonic):
                    flags_live = False
                steps += 1
                if mnemonic == 'ret':
                    # back in the caller's caller: what it uses of this routine's result
                    self.returns.append(dead)
                    if cc_live:
                        self.cc_returns = True
                    break
                if mnemonic == 'call':
                    target = re.match(r'(0x[0-9a-f]+)', operands)
                    # a callee's own inputs are read here; it writes eax/ecx/edx as C does
                    cc_live = False      # a callee's own compares come first, or it leaves them
                    if target and int(target[1], 16) in self.inputs_cache:
                        read |= live_reads(self.inputs_cache[int(target[1], 16)] | {x + '.full' for x in self.inputs_cache[int(target[1], 16)]}, dead)
                    dead = dead | {'eax', 'ecx', 'edx'}
                    flags_live = False
                    address = self.next[address]
                    continue
                if mnemonic == 'jmp':
                    target = re.match(r'(0x[0-9a-f]+)$', operands.split()[0]) if operands else None
                    table = re.match(r'DWORD PTR \[e[a-z]x\*4\+0x([0-9a-f]+)\]$', operands)
                    if table:           # a switch: every case
                        for case in self.cases(int(table[1], 16)):
                            stack.append((case, dead, flags_live, steps, cc_live))
                        break
                    if not target:
                        read |= {'eax', 'ecx', 'edx', 'ebx', 'ebp', 'esi', 'edi'} - dead
                        break
                    address = int(target[1], 16)
                    continue
                if mnemonic.startswith('j') or mnemonic.startswith('loop'):
                    target = re.match(r'(0x[0-9a-f]+)', operands)
                    if target:
                        stack.append((int(target[1], 16), dead, flags_live, steps, cc_live))
                address = self.next[address]
        return read, flags

    def analyse(self):
        self.inputs_cache = {}
        inputs = {}
        for routine in self.routines:
            inputs[routine], _ = self.walk(routine, False)
        # callers' consumption of each routine's results
        import bisect
        outputs = {r: set() for r in self.routines}
        flags_out = {r: False for r in self.routines}
        cc_out = {r: False for r in self.routines}
        for root in self.roots:
            if root in outputs:
                # Linkage entries: the C ABI's eax and callee-saved registers (edx is scratch for
                # these void and 32-bit entries). Callbacks (CHAIN's): only the callee-saved;
                # CHAIN sets eax before each and ignores what it returns. It does pass edx on to
                # the next callback and return ecx/edx to its caller (docs/modules/chain.md):
                # whether any reader cares is left to the game-level register check.
                outputs[root] |= {'ebx', 'ebp', 'esi', 'edi'} | ({'eax'} if root in self.services_entries else set())
        sites = []
        for address, (mnemonic, operands) in self.code.items():
            match = re.match(r'(0x[0-9a-f]+)', operands)
            if mnemonic != 'call' or not match:
                continue
            target = int(match[1], 16)
            if target not in outputs:
                continue
            caller = self.routines[bisect.bisect_right(self.routines, address) - 1]
            self.cc_returns = False
            read, flags = self.walk(self.next[address], True, True)
            sites.append((target, caller, read, list(self.returns), self.cc_returns))
            outputs[target] |= read
            flags_out[target] |= flags
            cc_out[target] |= self.cc_read
        changed = True
        while changed:              # what reaches a ret is what the caller's callers use
            changed = False
            for target, caller, read, returns, cc_returns in sites:
                for dead in returns:
                    more = outputs[caller] - dead - outputs[target]
                    if more:
                        outputs[target] |= more
                        changed = True
                if cc_returns and cc_out[caller] and not cc_out[target]:
                    cc_out[target] = True
                    changed = True
        self.cc_out = cc_out
        return inputs, outputs, flags_out

    def services(self):
        """(import index, offset) of every module call, by a forward scan per routine."""
        found, unresolved = set(), []
        self.service_reads = {}
        for address, (mnemonic, operands) in self.code.items():
            if mnemonic != 'call' or not operands.startswith('DWORD PTR ['):
                continue
            match = re.match(r'DWORD PTR \[(e[a-z]{2})\+(0x[0-9a-f]+)\]$', operands)
            if not match:
                unresolved.append((address, operands))
                continue
            reg, offset = match[1], int(match[2], 16)
            # walk back to the load of reg
            import bisect
            at = bisect.bisect_left(self.addresses, address)
            link = None
            for back in reversed(self.addresses[max(0, at - 40):at]):
                m, o = self.code[back]
                load = re.match(rf'{reg},(?:DWORD PTR )?ds:0x(2201600[0-9a-f]|220160[1-4][0-9a-f])$', o)
                if m == 'mov' and load:
                    link = int(load[1], 16) - BASE
                    break
                if re.match(rf'{reg}\b', o) and m in WRITE_ONLY | {'add', 'xor', 'sub'}:
                    break
            if link is None or (link - 0x16004) % 8:
                unresolved.append((address, operands))
                continue
            found.add(((link - 0x16004) // 8, offset))
            read, flags = self.walk(self.next[address], True)
            key = ((link - 0x16004) // 8, offset)
            self.service_reads[key] = self.service_reads.get(key, set()) | (read & {'ecx', 'edx'})
            if flags:
                self.service_reads[key].add('flags')
        return sorted(found), unresolved

    def import_id(self, index):
        return self.pe.va_uint32(BASE + 0x16000 + index * 8)


def module_pop(module_id, offset):
    pe = Fs98Pe(MODULE_FILES[module_id])
    if module_id == 1:
        target = pe.va_uint32(HOST_TABLE + offset)
    else:
        target = pe.va_uint32(pe.linkage()['va'] + offset)
    code = disassemble(pe) if module_id not in module_pop.cache else module_pop.cache[module_id]
    module_pop.cache[module_id] = code
    addresses = sorted(code)
    import bisect
    at = bisect.bisect_left(addresses, target)
    for address in addresses[at:at + 6000]:
        mnemonic, operands = code[address]
        if mnemonic == 'jmp' and address == target and re.match(r'0x[0-9a-f]+$', operands):
            at = bisect.bisect_left(addresses, int(operands, 16))
            continue
        if mnemonic == 'ret':
            return int(operands, 16) if operands else 0
    raise SystemExit(f'no ret for module {module_id} +{offset:#x}')


module_pop.cache = {}


def mask(regs):
    return sum(1 << REGS.index(r) for r in regs)


def main():
    sim1 = Sim1()
    inputs, outputs, flags_out = sim1.analyse()
    services, unresolved = sim1.services()
    lines = ['/* Generated by tools/gen_sim1_tables.py from orig/Sim/SIM1.sim: metadata only. */',
             '#ifndef SIM1_ROUTINE', '#define SIM1_ROUTINE(rva, pop, inputs, outputs, flags_out, cc_out)', '#endif',
             '#ifndef SIM1_SERVICE', '#define SIM1_SERVICE(index, module_id, offset, pop, reads, flags)', '#endif',
             '#ifndef SIM1_IMPORT', '#define SIM1_IMPORT(index, dll, name, slot)', '#endif']
    for routine in sim1.routines:
        lines.append(f'SIM1_ROUTINE(0x{routine - BASE:x}, {sim1.first_ret(routine)}, 0x{mask(inputs[routine]):02x}, '
                     f'0x{mask(outputs[routine]):02x}, {int(flags_out[routine])}, {int(sim1.cc_out[routine])})')
    for index, (link, offset) in enumerate(services):
        module_id = sim1.import_id(link)
        reads = sim1.service_reads.get((link, offset), set())
        lines.append(f'SIM1_SERVICE({index}, {module_id}, 0x{offset:x}, {module_pop(module_id, offset)}, '
                     f'0x{mask(reads - {"flags"}):02x}, {int("flags" in reads)})')
    for index, (dll, name, slot) in enumerate((i['dll'], i['name'], i['slot']) for i in imports(sim1.pe)):
        lines.append(f'SIM1_IMPORT({index}, "{dll}", "{name}", 0x{slot:x})')
    lines += ['#undef SIM1_ROUTINE', '#undef SIM1_SERVICE', '#undef SIM1_IMPORT', '']
    Path(OUTPUT).write_text('\n'.join(lines))
    print(f'{OUTPUT}: {len(sim1.routines)} routines, {len(services)} services, '
          f'{len(unresolved)} unresolved indirect calls')
    for address, operands in unresolved:
        print(f'  unresolved {address:#x}: call {operands}')


def imports(pe):
    result = []
    for entry in pe.imports():
        if isinstance(entry, dict):
            result.append({'dll': entry['dll'], 'name': entry['name'], 'slot': entry['iat_va'] - BASE})
        else:
            dll, name, slot = entry[:3]
            result.append({'dll': dll, 'name': name, 'slot': slot})
    return result


if __name__ == '__main__':
    main()
