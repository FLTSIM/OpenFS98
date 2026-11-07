#!/usr/bin/env python3
"""Recover SIM1 control flow and emit explicit C (never executable payloads).

Reference bytes are read from the verified original. Generated sources and
evidence belong in ignored bin/sim1-diff/lift/.
"""
from collections import Counter, deque
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct

from dialogmap import Fs98Pe
from gen_gauges import relocations
from gen_mdl_crash import CrashLift, CONDITIONS
from gen_sim1_data import SHA256

CONDITIONS.update(jge='(!!(f->flags & 0x80) == !!(f->flags & 0x800))',
                  jo='f->flags & 0x800', jno='!(f->flags & 0x800)',
                  jp='f->flags & 4', jnp='!(f->flags & 4)')


class SimLift(CrashLift):
    def __init__(self, path):
        self.path = Path(path)
        if hashlib.sha256(self.path.read_bytes()).hexdigest() != SHA256:
            raise ValueError('SIM1 reference hash differs')
        self.pe = Fs98Pe(path)
        self.instructions = {}
        self.next = {}
        self.relocs = dict(relocations(self.pe))
        self.relocated_instructions = set()
        self.tables = {}
        self.islands = set()
        self.decode(0x1000)
        self.roots = {0x1020, 0x1040, 0xaf90, 0xfc40, 0x40d0}
        # Every relocated code pointer outside text is a callable entry.
        self.roots.update(target for site, target in self.relocs.items()
                          if site >= 0x15000 and 0x1000 <= target < 0x14160)
        self.functions = {}
        pending = deque(sorted(self.roots))
        while pending:
            start = pending.popleft()
            if start in self.functions:
                continue
            body = self.discover(start)
            self.functions[start] = body
            for address in body:
                op, args = self.instructions[address]
                if op == 'call' and args.startswith('0x'):
                    pending.append(int(args.split()[0], 16) - self.pe.uiBase)
                end = self.next[address]
                for site in range(address, end):
                    target = self.relocs.get(site, 0)
                    if 0x1000 <= target < 0x14160:
                        # Instruction immediates taking an address are callbacks;
                        # an indirect switch operand points to its data table.
                        if op in ('push', 'mov') and '[' not in args and 'PTR' not in args:
                            self.roots.add(target)
                            pending.append(target)
        self.signatures = {}
        for start, body in self.functions.items():
            returns = {int(self.instructions[a][1] or '0', 0)
                       for a in body if self.instructions[a][0] == 'ret'}
            if len(returns) == 1:
                self.signatures[start] = returns.pop()
            elif start in self.roots:
                raise ValueError(f'entry {start:x} returns {returns}')
        reached = set().union(*self.functions.values())
        occupied = {}
        for address in reached:
            for byte in range(address, self.next[address]):
                if byte in occupied and occupied[byte] != address:
                    raise ValueError(f'overlapping instructions {address:x}/{occupied[byte]:x}')
                occupied[byte] = address
        if set(occupied) & self.islands:
            raise ValueError(f'switch data overlaps reachable instructions: {sorted(set(occupied) & self.islands)[:20]}')

    def decode(self, start):
        output = subprocess.check_output(
            ['objdump', '-d', '-M', 'intel', '--insn-width=16',
             f'--start-address={self.pe.uiBase + start}', str(self.path)], text=True)
        for line in output.splitlines():
            match = re.fullmatch(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s+(\S+)\s*(.*)', line)
            if match:
                address = int(match[1], 16) - self.pe.uiBase
                size = len(match[2].split())
                op, args = match[3], match[4].strip()
                # Objdump prints the nearest symbol only on direct branches.
                args = re.sub(r' <[^>]+>$', '', args)
                self.instructions[address] = op, args
                self.next[address] = address + size
                if any(site in self.relocs for site in range(address, address + size)):
                    self.relocated_instructions.add(address)

    def jump_targets(self, address, args):
        match = re.fullmatch(r'DWORD PTR \[[a-z]+\*4\+(0x[0-9a-f]+)\]', args)
        if not match:
            raise ValueError(f'indirect jump {address:x}: {args}')
        table = int(match[1], 16) - self.pe.uiBase
        targets, cursor = [], table
        while cursor in self.relocs and 0x1000 <= self.relocs[cursor] < 0x14160:
            targets.append(self.relocs[cursor])
            self.islands.update(range(cursor, cursor + 4))
            cursor += 4
        if not targets:
            raise ValueError(f'empty switch table {table:x}')
        self.tables[address] = table, targets
        return targets

    def discover(self, start):
        queue, body = deque([start]), set()
        while queue:
            address = queue.popleft()
            if address in body:
                continue
            if address not in self.instructions:
                self.decode(address)
            if address not in self.instructions:
                raise ValueError(f'missing instruction {address:x}')
            body.add(address)
            op, args = self.instructions[address]
            if op == 'ret':
                continue
            if op == 'jmp' or op in CONDITIONS or op in ('jcxz', 'jecxz', 'loop'):
                if args.startswith('0x'):
                    queue.append(int(args, 16) - self.pe.uiBase)
                else:
                    # CRT import thunks return through the native bridge.
                    if args.startswith('DWORD PTR ds:'):
                        continue
                    queue.extend(self.jump_targets(address, args))
                if op == 'jmp':
                    continue
            queue.append(self.next[address])
        return body

    def report(self):
        reached = set().union(*self.functions.values())
        return dict(functions=len(self.functions), instructions=len(reached),
                    roots={hex(root): self.signatures.get(root) for root in sorted(self.roots)},
                    operations=dict(sorted(Counter(self.instructions[a][0] for a in reached).items())),
                    switches={hex(a): [hex(t), len(targets)] for a, (t, targets) in self.tables.items()})

    def address(self, operand):
        return super().address(re.sub(r'^(QWORD|TBYTE) PTR ', '', operand))

    def lower(self, address):
        self.current_address = address
        op, args = self.instructions[address]
        parts = args.split(',')
        if op.startswith('f'):
            if op in ('fnstsw', 'fstsw') and args == 'ax':
                return ('{ uint16_t status; __asm__ volatile("' + op + ' %0" : "=a"(status)); '
                        'f->eax = (f->eax & 0xffff0000u) | status; }')
            if 'PTR' in args:
                match = re.fullmatch(r'(DWORD|QWORD|WORD|TBYTE) PTR (.*)', args)
                if not match:
                    raise ValueError(f'x87 memory {address:x}: {args}')
                asm = f'{op} {match[1]} PTR [%0]'
                operands = f': : "r"({self.address(match[2])})'
            else:
                asm, operands = op + (' ' + args if args else ''), ': :'
            return ('__asm__ volatile("' + asm + '" ' + operands +
                    ' : "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)", "memory");')
        if op == 'test':
            width = self.width(parts[0], self.width(parts[1]))
            return f'mdl_logic(f, {self.read(parts[0], width)} & {self.read(parts[1], width)}, {width});'
        if op == 'sahf':
            return 'f->flags = (f->flags & ~0xd5u) | ((f->eax >> 8) & 0xd5u);'
        if op == 'setne':
            return self.write(args, '!(f->flags & 0x40)', 8)
        if op == 'not':
            return self.write(args, '~(' + self.read(args) + ')', self.width(args))
        if op in ('jcxz', 'jecxz'):
            mask = '0xffff' if op == 'jcxz' else '0xffffffff'
            return f'if (!(f->ecx & {mask}u)) goto label_{int(args, 16)-self.pe.uiBase:x};'
        if op in ('pusha', 'popa'):
            if op == 'pusha':
                return '{ uint32_t sp = f->esp; ' + ' '.join(
                    f'mdl_push(f, {"sp" if reg == "esp" else "f->" + reg}, 32);'
                    for reg in ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')) + ' }'
            return ' '.join('f->esp += 4;' if reg == 'esp' else f'f->{reg} = mdl_pop(f, 32);'
                            for reg in ('edi', 'esi', 'ebp', 'esp', 'ebx', 'edx', 'ecx', 'eax'))
        if op in ('div', 'idiv'):
            width = self.width(args)
            if width not in (16, 32):
                raise ValueError(f'division width {address:x}')
            return ('{ uint' + str(width) + '_t a = f->eax, d = f->edx, operand = ' + self.read(args) + '; '
                    '__asm__ volatile("' + op + ' %2" : "+a"(a), "+d"(d) : "r"(operand) : "cc"); '
                    + self.write('ax' if width == 16 else 'eax', 'a') + self.write('dx' if width == 16 else 'edx', 'd') + ' }')
        if op == 'imul' and len(parts) == 2:
            return ('{ int64_t product = mdl_signed(' + self.read(parts[0]) + ',32) * mdl_signed(' +
                    self.read(parts[1]) + ',32); ' + self.write(parts[0], '(uint32_t)product') +
                    ' f->flags = (f->flags & ~0x801u) | (product == (int32_t)product ? 0 : 0x801u); }')
        if op == 'call':
            if args.startswith('0x'):
                target = int(args, 16) - self.pe.uiBase
                return f'mdl_push(f, mdl_address(f, 0x{self.next[address]:x}), 32); execute_{target:x}(f);'
            return f'sim1_lift_native(f, {self.read(args)});'
        if op == 'jmp' and not args.startswith('0x'):
            if args.startswith('DWORD PTR ds:'):
                return f'f->esp += 4; sim1_lift_native(f, {self.read(args)}); return;'
            _, targets = self.tables[address]
            result = [f'switch ({self.read(args)}) {{']
            for target in sorted(set(targets)):
                result.append(f'case 0x{self.pe.uiBase+target:x}u: goto label_{target:x};')
            result.append('default: sim1_unsupported(); }')
            return '\n'.join(result)
        return super().lower(address)

    def emit(self, directory):
        # Copy only the proven switch tables and their one byte-index map.
        # Every other byte of the text backing remains zero.
        self.islands.update(range(0xd654, 0xd688))
        data = bytearray(0x15000)
        for rva in self.islands:
            data[rva] = self.pe.byData[self.pe.file_offset(rva)]
        rdata = next(s for s in self.pe.aSections if s['name'] == '.rdata')
        data += self.pe.read_bytes(rdata['raw_offset'], rdata['virtual_size'])
        code = ['/* Generated explicit operations; no runtime instruction decoder. */',
                '#include "sim1_lift.h"', 'extern uint8_t sim1_image[];',
                'extern void sim1_unsupported(void);', 'DWORD sim1_lift_tls;',
                'static uint8_t constants[] = {']
        for offset in range(0, len(data), 32):
            code.append(','.join(str(b) for b in data[offset:offset+32]) + ',')
        code += ['};', 'static uint32_t crt[4];']
        code += [f'extern void sim1_entry_{root:x}(void);' for root in sorted(self.roots)]
        code += ['uint32_t sim1_lift_address(struct mdl_frame *f, uint32_t rva) { (void)f; switch (rva) {']
        code += [f'case 0x{root:x}: return (uint32_t)(uintptr_t)sim1_entry_{root:x};' for root in sorted(self.roots)]
        code += [f'case 0x{rva:x}: return (uint32_t)(uintptr_t)&crt[{i}];' for i, rva in enumerate((0x1b060, 0x1b064, 0x1b068, 0x1b06c))]
        code += ['default: if(rva >= 0x16000 && rva < 0x1a3f8) return (uint32_t)(uintptr_t)(sim1_image+rva-0x16000);',
                 'if(rva < sizeof(constants)) return (uint32_t)(uintptr_t)(constants+rva);',
                 'sim1_unsupported(); return 0; } }']
        code += [f'static void execute_{start:x}(struct mdl_frame *f);' for start in sorted(self.functions)]
        for start, body in sorted(self.functions.items()):
            code += [f'static void execute_{start:x}(struct mdl_frame *f) {{', f'goto label_{start:x};']
            for address in sorted(body):
                op, args = self.instructions[address]
                if address == 0x11692:
                    code += ['/* RETAIL BUG (kept): SIM1-AIR-UNCHECKED-TABLE: unchecked count-1 table access. */']
                code += [f'label_{address:x}: /* {op} {args} */', self.lower(address)]
                if op not in ('jmp', 'ret'):
                    code += [f'goto label_{self.next[address]:x};']
            code += ['}']
        code += ['void sim1_lift_enter(uint32_t entry, uint32_t *saved) {',
                 'struct sim1_frame context; struct mdl_frame *f=&context.cpu;',
                 'uint32_t *stack_top=(uint32_t *)(uintptr_t)sim1_lift_stack();',
                 'uint32_t i, argc=0, expected; void *previous;',
                 'for(i=0;i<8192+64;i++) stack_top[(int32_t)i-8192]=0xa5a5a5a5u;',
                 'f->eax=saved[7]; f->ecx=saved[6]; f->edx=saved[5]; f->ebx=saved[4];',
                 'f->ebp=saved[2]; f->esi=saved[1]; f->edi=saved[0]; f->flags=saved[8];',
                 'f->stack_low=f->stack_high=0; f->image=sim1_image;',
                 'f->esp=(uint32_t)(uintptr_t)stack_top; switch(entry) {']
        code += [f'case 0x{root:x}: argc={self.signatures[root]//4}; break;' for root in sorted(self.roots)]
        code += ['default: sim1_unsupported(); }', 'expected=f->esp+4+argc*4;',
                 'mdl_write(f,f->esp,*(uint32_t *)(uintptr_t)(saved[3]+4),32);',
                 'for(i=0;i<argc;i++) mdl_write(f,f->esp+4+i*4,*(uint32_t *)(uintptr_t)(saved[3]+8+i*4),32);',
                 'previous=TlsGetValue(sim1_lift_tls); TlsSetValue(sim1_lift_tls,f);', 'switch(entry) {']
        code += [f'case 0x{root:x}: execute_{root:x}(f); break;' for root in sorted(self.roots)]
        code += ['}', 'if(f->esp != expected) sim1_unsupported();',
                 'TlsSetValue(sim1_lift_tls,previous);',
                 'saved[7]=f->eax; saved[6]=f->ecx; saved[5]=f->edx; saved[4]=f->ebx;',
                 'saved[2]=f->ebp; saved[1]=f->esi; saved[0]=f->edi; saved[8]=f->flags;', '}',
                 'BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {',
                 '(void)reserved; if(reason==DLL_PROCESS_ATTACH) { HMODULE module;',
                 'DisableThreadLibraryCalls(instance); sim1_lift_tls=TlsAlloc();',
                 'if(sim1_lift_tls==TLS_OUT_OF_INDEXES) return FALSE;',
                 'module=GetModuleHandleA("msvcrt.dll");']
        # Confirmed below against the original PE import directory.
        for i, name in enumerate(('_CIacos', '_CIasin', '_CIpow', '_ftol')):
            code += [f'crt[{i}]=(uint32_t)(uintptr_t)GetProcAddress(module,"{name}"); if(!crt[{i}]) return FALSE;']
        for site, target in sorted(self.relocs.items()):
            if 0x16094 <= site < 0x1a3f8 or 0x15000 <= site < 0x15759:
                code += [f'mdl_write(NULL,mdl_address(NULL,0x{site:x}),mdl_address(NULL,0x{target:x}),32);']
        code += ['} else if(reason==DLL_PROCESS_DETACH) TlsFree(sim1_lift_tls); return TRUE; }']
        (directory / 'sim1_lift.c').write_text('\n'.join(code) + '\n')
        asm = ['.text']
        for root in sorted(self.roots):
            asm += [f'.globl _sim1_entry_{root:x}', f'_sim1_entry_{root:x}:',
                    'pushfl', 'pushal', 'pushl %esp', f'pushl $0x{root:x}',
                    'call _sim1_lift_enter', 'addl $8,%esp', 'popal', 'popfl',
                    f'ret ${self.signatures[root]}' if self.signatures[root] else 'ret']
        for name, root in (('sim1_chain_tick', 0x1020), ('sim1_chain_frame', 0x1040), ('sim1_aircraft_setup', 0xfc40)):
            asm += [f'.globl _{name}', f'.set _{name}, _sim1_entry_{root:x}']
        (directory / 'sim1_entries.S').write_text('\n'.join(asm) + '\n')
        resources = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0))
        for resource in self.pe.resources():
            if not all(isinstance(resource[key], int) and 0 <= resource[key] <= 65535
                       for key in ('type', 'id', 'language')):
                raise ValueError('unsupported named SIM1 resource')
            resources += struct.pack('<IIHHHHIHHII', resource['size'], 32, 0xffff,
                                     resource['type'], 0xffff, resource['id'], 0,
                                     0x1030, resource['language'], 0, 0)
            resources += self.pe.read_bytes(self.pe.file_offset(resource['rva'], resource['size']), resource['size'])
            resources += bytes(-len(resources) % 4)
        (directory / 'sim1.res').write_bytes(resources)


def main():
    directory = Path('bin/sim1-diff/lift')
    directory.mkdir(parents=True, exist_ok=True)
    lift = SimLift('orig/Sim/SIM1.sim')
    report = lift.report()
    lift.emit(directory)
    (directory / 'survey.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
