#!/usr/bin/env python3
"""Emit explicit C transliterations and data/resource images for every gauge.

The disassembler supplies instruction boundaries, not executable payloads.
Every accepted operation has a checked C lowering below; unknown operations,
branches, imports, calling conventions or pointers fail generation.
Generated source and retail data stay in ignored bin/gauges-all/.
"""
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

from dialogmap import Fs98Pe, SurveyError

OUT = Path('bin/gauges-all')
REGS = {'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp'}
PARTS = {'ax': ('eax', 0, 16), 'cx': ('ecx', 0, 16), 'dx': ('edx', 0, 16), 'al': ('eax', 0, 8), 'ah': ('eax', 8, 8),
         'cl': ('ecx', 0, 8), 'bl': ('ebx', 0, 8), 'dl': ('edx', 0, 8)}
ARITIES = {0x40: 2, 0x44: 1, 0x48: 1, 0x4c: 2, 0x50: 1, 0x58: 1,
           0x60: 1, 0x64: 2, 0x68: 1, 0x6c: 1, 0x70: 2, 0x74: 1,
           0x7c: 1, 0x80: 3, 0x84: 2, 0x88: 2, 0x8c: 1, 0x90: 2,
           0x98: 1, 0xa0: 1}
CONDITIONS = {'je': '(f->flags & 0x40)', 'jne': '!(f->flags & 0x40)',
              'ja': '!(f->flags & 0x41)', 'jbe': '(f->flags & 0x41)',
              'jl': '((!!(f->flags & 0x80)) != (!!(f->flags & 0x800)))',
              'jge': '((!!(f->flags & 0x80)) == (!!(f->flags & 0x800)))',
              'jle': '((f->flags & 0x40) || ((!!(f->flags & 0x80)) != (!!(f->flags & 0x800))))',
              'jg': '(!(f->flags & 0x40) && ((!!(f->flags & 0x80)) == (!!(f->flags & 0x800))))'}


def relocations(pe):
    rva, size = pe.directory(5)
    cursor = pe.file_offset(rva, size)
    end = cursor + size
    result = []
    while cursor < end:
        page, length = struct.unpack('<II', pe.read_bytes(cursor, 8))
        if length < 8 or length % 2 or cursor + length > end:
            raise SurveyError('invalid relocation block')
        for offset in range(cursor + 8, cursor + length, 2):
            entry = pe.read_uint16(offset)
            if not entry >> 12:
                continue
            if entry >> 12 != 3:
                raise SurveyError('unexpected relocation type')
            address = page + (entry & 4095)
            result.append((address, pe.read_uint32(pe.file_offset(address, 4)) - pe.uiBase))
        cursor += length
    return result


class Gauge:
    def __init__(self, path):
        self.path = path
        self.pe = Fs98Pe(path)
        self.sections = {section['name']: section for section in self.pe.aSections}
        self.relocs = relocations(self.pe)
        self.imports = {entry['iat_va']: entry['name'] for entry in self.pe.imports()}
        if set(self.imports.values()) - {'DisableThreadLibraryCalls', '_ftol', 'wsprintfA', 'lstrcpyA'}:
            raise SurveyError(f'{path}: unsupported imports')
        self.instructions = {}
        listing = subprocess.run(['objdump', '-d', '--no-show-raw-insn', '-Mintel', str(path)],
                                 check=True, capture_output=True, text=True).stdout
        for line in listing.splitlines():
            match = re.fullmatch(r'([0-9a-f]+):\s+(\w+)\s*(.*)', line)
            if match:
                self.instructions[int(match[1], 16)] = (match[2], match[3])
        text = self.sections['.text']
        low = self.pe.uiBase + text['rva']
        high = low + text['virtual_size']
        starts = {low}
        for address, target in self.relocs:
            if not text['rva'] <= address < text['rva'] + text['virtual_size'] and low <= self.pe.uiBase + target < high:
                starts.add(self.pe.uiBase + target)
        for op, args in self.instructions.values():
            if op == 'call' and re.fullmatch('0x[0-9a-f]+', args):
                starts.add(int(args, 16))
        self.functions = {}
        self.thunks = set()
        ordered = sorted(starts)
        for index, start in enumerate(ordered):
            stop = ordered[index + 1] if index + 1 < len(ordered) else high
            body = {address: entry for address, entry in self.instructions.items() if start <= address < stop}
            first = body[start]
            if first[0] == 'jmp' and first[1].startswith('DWORD PTR ds:'):
                if self.imports[int(first[1].split(':')[1], 16)] != '_ftol':
                    raise SurveyError('unknown import thunk')
                self.thunks.add(start)
            else:
                self.functions[start] = body
        self.signatures = {}
        self.float_slots = set()
        self.print_slots = set()
        self.indirect_slots = set()

    def pointer_argument(self, body):
        origins = {}
        shift = 0
        pointer_argument = False
        data = self.sections['.data']
        for op, args in body.values():
            parts = args.split(',')
            # A global null slot used as an address is a runtime PANELS
            # surface/string pointer. Give it real storage in standalone tests.
            for register in re.findall(r'\b(e[a-z]{2})\b', ' '.join(re.findall(r'\[([^]]+)\]', args))):
                origin = origins.get(register)
                if origin == 'argument': pointer_argument = True
                elif isinstance(origin, int): self.indirect_slots.add(origin)
            if op == 'mov' and parts[0] in REGS:
                source = parts[1]
                match = re.fullmatch(r'(?:DWORD PTR )?ds:(0x[0-9a-f]+)', source)
                stack = re.fullmatch(r'DWORD PTR \[esp\+(0x[0-9a-f]+)\]', source)
                if match:
                    address = int(match[1], 16) - self.pe.uiBase
                    origins[parts[0]] = address - data['rva'] if data['rva'] <= address < data['rva'] + data['virtual_size'] and not self.pe.va_uint32(self.pe.uiBase + address) else None
                elif stack and int(stack[1], 16) + shift == 4:
                    origins[parts[0]] = 'argument'
                else:
                    origins[parts[0]] = origins.get(source)
            elif op in ('xor', 'and', 'or', 'lea', 'shl', 'shr', 'sar', 'pop'):
                origins[parts[0]] = None
            if op == 'push': shift -= 4
            elif op == 'pop': shift += 4
            elif op in ('add', 'sub') and parts[0] == 'esp':
                shift += int(parts[1], 0) * (1 if op == 'add' else -1)
            if op == 'call':
                origins['eax'] = None
                if args.startswith('0x') and int(args, 16) in self.thunks:
                    origins['edx'] = None
                else:
                    origins['ecx'] = origins['edx'] = None
        return pointer_argument

    def signature(self, start):
        if start in self.signatures:
            return self.signatures[start]
        body = self.functions[start]
        addresses = sorted(body)
        next_address = dict(zip(addresses, addresses[1:]))
        queue = deque([(start, 0)])
        visited, returns = set(), set()
        while queue:
            address, depth = queue.popleft()
            if (address, depth) in visited:
                continue
            if len(visited) > 10000 or not -1 <= depth <= 8:
                raise SurveyError(f'{self.path}: invalid FP flow at {address:x}/{depth}')
            visited.add((address, depth))
            op, args = body[address]
            if op in ('fld', 'fild'):
                depth += 1
            if op in ('fstp', 'fcomp', 'faddp', 'fsubp', 'fdivp'):
                depth -= 1
            if op == 'call' and args.startswith('0x'):
                target = int(args, 16)
                depth += -1 if target in self.thunks else self.signature(target)[1]
            if op == 'ret':
                returns.add((int(args or '0', 0), depth))
                continue
            if op == 'jmp' or op in CONDITIONS:
                queue.append((int(args, 0), depth))
                if op == 'jmp':
                    continue
            if address in next_address:
                queue.append((next_address[address], depth))
        if not returns or len({entry[0] for entry in returns}) != 1:
            raise SurveyError(f'{self.path}: inconsistent return signature {start:x}: {returns}')
        signature = (next(iter(returns))[0], max(entry[1] for entry in returns))
        if signature[0] not in (0, 4, 8, 12) or signature[1] not in (0, 1):
            raise SurveyError(f'{self.path}: unsupported return signature {signature}')
        self.signatures[start] = signature
        return signature

    def check_live_flags(self, body):
        # The lowering omits only flag bits no successor reads. In particular,
        # the gauges never consume shift/multiply carry or overflow flags.
        ordered = sorted(body)
        next_address = dict(zip(ordered, ordered[1:]))
        queue, visited = deque([(ordered[0], 0)]), set()
        requirements = {'je': 2, 'jne': 2, 'sete': 2, 'ja': 3, 'jbe': 3,
                        'jl': 12, 'jge': 12, 'jle': 14, 'jg': 14}
        while queue:
            address, flags = queue.popleft()
            if (address, flags) in visited: continue
            visited.add((address, flags))
            op, args = body[address]
            if op in requirements and requirements[op] & flags != requirements[op]:
                raise SurveyError(f'{self.path} {address:x}: reads unmodelled flag bits after {op}')
            if op in ('add', 'sub', 'cmp', 'test', 'and', 'or', 'xor', 'neg'): flags = 15
            elif op == 'dec': flags = (flags & 1) | 14
            elif op in ('shl', 'shr', 'sar'): flags = 6
            elif op in ('mul', 'div', 'idiv', 'call'): flags = 0
            if op == 'ret': continue
            if op == 'jmp' or op in CONDITIONS:
                queue.append((int(args, 0), flags))
                if op == 'jmp': continue
            if address in next_address: queue.append((next_address[address], flags))

    def address(self, operand):
        operand = re.sub(r'^(?:BYTE|WORD|DWORD|QWORD) PTR ', '', operand)
        if operand.startswith('ds:'):
            return f'gauge_address(0x{int(operand[3:], 16) - self.pe.uiBase:x})'
        if not operand.startswith('[') or not operand.endswith(']'):
            raise SurveyError(f'not a memory operand: {operand}')
        expression = operand[1:-1]
        expression = re.sub(r'\b(e[a-z]{2})\b', lambda m: 'f->' + m[1] if m[1] in REGS else m[1], expression)
        expression = re.sub(r'0x([0-9a-f]+)', r'0x\1u', expression)
        return '(' + expression + ')'

    def read(self, operand, floating=False):
        if operand in REGS:
            return 'f->' + operand
        if operand in PARTS:
            register, shift, bits = PARTS[operand]
            return f'((f->{register} >> {shift}) & 0x{(1 << bits) - 1:x}u)'
        if operand == 'st' or operand.startswith('st('):
            index = int(operand[3:-1]) if operand.startswith('st(') else 0
            return f'f->fp[f->depth - {index + 1}]'
        if operand.startswith(('ds:', '[', 'BYTE ', 'WORD ', 'DWORD ', 'QWORD ')):
            address = self.address(operand)
            if floating:
                if operand.startswith('QWORD') and 'ds:' in operand:
                    rva = int(operand.split(':')[1], 16) - self.pe.uiBase
                    data = self.sections['.data']
                    if data['rva'] <= rva < data['rva'] + data['virtual_size']:
                        self.float_slots.add(rva - data['rva'])
                return f'((long double)gauge_double({address}))'
            width = 8 if operand.startswith('BYTE') else 16 if operand.startswith('WORD') else 64 if operand.startswith('QWORD') else 32
            return f'gauge_u{width}({address})'
        value = int(operand, 0)
        if self.pe.uiBase <= value < self.pe.uiBase + max(s['rva'] + s['virtual_size'] for s in self.sections.values()):
            return f'gauge_address(0x{value - self.pe.uiBase:x})'
        return f'0x{value & 0xffffffff:x}u'

    def write(self, operand, value):
        if operand in REGS:
            return f'f->{operand} = (uint32_t)({value});'
        if operand in PARTS:
            register, shift, bits = PARTS[operand]
            mask = ((1 << bits) - 1) << shift
            return f'f->{register} = (f->{register} & 0x{~mask & 0xffffffff:x}u) | (((uint32_t)({value}) << {shift}) & 0x{mask:x}u);'
        width = 8 if operand.startswith('BYTE') else 16 if operand.startswith('WORD') else 32
        return f'gauge_w{width}({self.address(operand)}, (uint{width}_t)({value}));'

    def lower(self, address, op, args):
        operands = args.split(',')
        left = operands[0]
        right = operands[1] if len(operands) > 1 else None
        width = PARTS[left][2] if left in PARTS else 8 if left.startswith('BYTE') else 16 if left.startswith('WORD') else 32
        if op == 'nop': return ';'
        if op == 'mov': return self.write(left, self.read(right))
        if op == 'movsx': return self.write(left, f'(int32_t)(int8_t)({self.read(right)})')
        if op == 'lea': return self.write(left, self.address(right))
        if op == 'push': return f'gauge_push(f, {self.read(left)});'
        if op == 'pop': return self.write(left, 'gauge_pop(f)')
        if op in ('add', 'sub', 'cmp'):
            expression = f'gauge_{"sub" if op == "cmp" else op}(f, {self.read(left)}, {self.read(right)}, {width})'
            return expression + ';' if op == 'cmp' else self.write(left, expression)
        if op == 'dec':
            return '{ uint32_t carry = f->flags & 1; ' + self.write(left, f'gauge_sub(f, {self.read(left)}, 1, {width})') + ' f->flags = (f->flags & ~1u) | carry; }'
        if op == 'neg': return self.write(left, f'gauge_sub(f, 0, {self.read(left)}, {width})')
        if op in ('test', 'and', 'or', 'xor'):
            operator = {'test': '&', 'and': '&', 'or': '|', 'xor': '^'}[op]
            expression = f'({self.read(left)} {operator} {self.read(right)})'
            return '{ uint32_t value = ' + expression + f'; f->flags = gauge_flags(value, {width}); ' + (self.write(left, 'value') if op != 'test' else '') + ' }'
        if op in ('shl', 'shr', 'sar'):
            count = int(right, 0) & 31
            if not count: return ';'
            expression = f'({self.read(left)} << {count})' if op == 'shl' else f'({"(int32_t)" if op == "sar" else ""}{self.read(left)} >> {count})'
            return self.write(left, expression) + f' f->flags = gauge_flags({self.read(left)}, {width});'
        if op == 'cdq': return 'f->edx = (int32_t)f->eax < 0 ? UINT32_MAX : 0;'
        if op == 'mul': return '{ uint64_t product = (uint64_t)f->eax * ' + self.read(left) + '; f->eax = (uint32_t)product; f->edx = (uint32_t)(product >> 32); }'
        if op in ('div', 'idiv'): return f'gauge_divide(f, {self.read(left)}, {int(op == "idiv")});'
        if op == 'sete': return self.write(left, '!!(f->flags & 0x40)')
        if op in CONDITIONS: return f'if ({CONDITIONS[op]}) goto label_{int(left, 0) - self.pe.uiBase:x};'
        if op == 'jmp': return f'goto label_{int(left, 0) - self.pe.uiBase:x};'
        if op == 'ret': return f'f->esp += {4 + int(args or "0", 0)}; return;'
        if op == 'fld': return f'gauge_fp_push(f, {self.read(left, floating=True)});'
        if op == 'fild': return f'gauge_fp_push(f, (long double)(int{64 if left.startswith("QWORD") else 32}_t){self.read(left)});'
        if op in ('fst', 'fstp'):
            value = 'f->fp[f->depth - 1]'
            store = ';' if left == 'st(0)' else f'gauge_wdouble({self.address(left)}, {value});'
            return store + (' --f->depth;' if op == 'fstp' else '')
        if op in ('fmul', 'fadd', 'fsub', 'fsubr', 'fdiv', 'fidiv'):
            other = self.read(left, floating=op != 'fidiv')
            if op == 'fidiv': other = f'(long double)(int32_t)({other})'
            top = 'f->fp[f->depth - 1]'
            operator = {'fmul': '*', 'fadd': '+', 'fsub': '-', 'fsubr': '-', 'fdiv': '/', 'fidiv': '/'}[op]
            if right:
                other = self.read(right, floating=True)
            expression = f'{other} - {top}' if op == 'fsubr' else f'{top} {operator} {other}'
            return f'{top} = {expression};'
        if op in ('faddp', 'fsubp', 'fdivp'):
            operator = {'faddp': '+', 'fsubp': '-', 'fdivp': '/'}[op]
            return f'f->fp[f->depth - 2] = f->fp[f->depth - 2] {operator} f->fp[f->depth - 1]; --f->depth;'
        if op == 'fchs': return 'f->fp[f->depth - 1] = -f->fp[f->depth - 1];'
        if op == 'fcos': return 'f->fp[f->depth - 1] = gauge_cos(f->fp[f->depth - 1]);'
        if op == 'fxch':
            other = self.read(left, floating=True)
            return '{ long double value = ' + other + '; ' + other + ' = f->fp[f->depth - 1]; f->fp[f->depth - 1] = value; }'
        if op in ('fcom', 'fcomp'):
            return f'f->fp_status = gauge_compare(f->fp[f->depth - 1], {self.read(left, floating=True)});' + (' --f->depth;' if op == 'fcomp' else '')
        if op == 'fnstsw': return self.write(left, 'f->fp_status')
        if op == 'int3': return 'RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);'
        if op == 'call':
            if left.startswith('0x'):
                target = int(left, 16)
                if target in self.thunks:
                    return '{ int64_t value = gauge_ftol(gauge_fp_pop(f)); f->eax = (uint32_t)value; f->edx = (uint32_t)((uint64_t)value >> 32); }'
                return f'gauge_push(f, 0); execute_{target - self.pe.uiBase:x}(f);'
            if left.startswith('DWORD PTR ds:'):
                target = int(left.split(':')[1], 16)
                name = self.imports[target]
                if name == 'wsprintfA': return 'gauge_print(f);'
                if name == 'lstrcpyA': return 'f->eax = (uint32_t)(uintptr_t)lstrcpyA((char *)(uintptr_t)gauge_u32(f->esp), (char *)(uintptr_t)gauge_u32(f->esp + 4)); f->esp += 8;'
                raise SurveyError(f'unexpected direct import {name}')
            slot = int(re.fullmatch(r'DWORD PTR \[e[a-z]{2}\+(0x[0-9a-f]+)\]', left)[1], 16)
            return f'gauge_panel_call(f, {self.read(left)}, {ARITIES[slot]});'
        raise SurveyError(f'{self.path} {address:x}: unhandled {op} {args}')

    def emit(self):
        data = self.sections['.data']
        rdata = self.sections['.rdata']
        initial = bytearray(self.pe.read_bytes(data['raw_offset'], min(data['raw_size'], data['virtual_size'])))
        initial.extend(bytes(data['virtual_size'] - len(initial)))
        read_only = self.pe.read_bytes(rdata['raw_offset'], rdata['virtual_size'])
        exports = self.pe.exports()
        import_offset = exports['ImportTable'] - data['rva']
        linkage_offset = exports['Linkage'] - data['rva']
        if struct.unpack_from('<IIII', initial, import_offset) != (15, 0, 0, 0):
            raise SurveyError('expected a single PANELS import')
        bodies, bindings, code = [], [], []
        for start, body in self.functions.items():
            if start == self.pe.uiBase + 0x1000:
                continue  # same C DllMain as the first two verified gauges
            nargs, fp = self.signature(start)
            self.check_live_flags(body)
            rva = start - self.pe.uiBase
            statements = [f'static void execute_{rva:x}(struct gauge_frame *f) {{']
            if self.path.stem in ('SCHCMP', 'SOPCOMP') and rva == 0x1020:
                statements.append('/* RETAIL BUG (kept): GAUGE-COMPASS-WRAP-HANG -- subtraction does not progress for non-finite/huge headings. */')
            if self.path.stem in ('BELLTURN', 'CESSTURN', 'EXTURN') and rva == 0x1020:
                statements.append('/* RETAIL BUG (kept): GAUGE-TURN-EMPTY-RETURN -- selector values other than 0/1 supply no x87 return value. */')
            for address, (op, args) in body.items():
                statements.append(f'label_{address - self.pe.uiBase:x}: /* {op} {args} */ ' + self.lower(address, op, args))
            statements.append('}')
            bodies += statements
            parameters = ', '.join(f'uint32_t arg{index}' for index in range(nargs // 4)) or 'void'
            code += [f'static uint32_t __stdcall code_{rva:x}({parameters}) {{',
                     'struct gauge_frame frame, *f = &frame;',
                     'memset(f, 0, sizeof *f); f->esp = (uint32_t)(uintptr_t)(f->stack + 192);']
            for index in range(nargs // 4):
                code.append(f'gauge_w32(f->esp + {4 + 4 * index}, arg{index});')
            code += [f'execute_{rva:x}(f);']
            if fp:
                code += ['if (f->depth) {',
                         '/* Win32 callback ABI: publish ST0 without adding a value on a retail empty-return path. */',
                         '__asm__ volatile("fldt %0" : : "m"(f->fp[f->depth - 1]) : "st");', '}']
            code += ['return f->eax;', '}']
            offsets = [address - data['rva'] for address, target in self.relocs
                       if target == rva and data['rva'] <= address < data['rva'] + data['virtual_size']]
            header_rva = self.pe.va_uint32(self.pe.uiBase + exports['Linkage'] + 0x18)
            header_offset = header_rva - self.pe.uiBase - data['rva']
            void_offsets = {linkage_offset + 4, linkage_offset + 8} | {
                header_offset + slot for slot in range(0xc, 0x28, 4)}
            bindings.append({'rva': rva, 'args': nargs, 'fp': fp, 'pointer': offsets[0] if offsets else -1,
                             'arg_pointer': self.pointer_argument(body),
                             'is_void': bool(offsets) and all(offset in void_offsets for offset in offsets)})
        code_pointers = {}
        for address, target in self.relocs:
            if data['rva'] <= address < data['rva'] + data['virtual_size']:
                offset = address - data['rva']
                if self.pe.uiBase + target in self.functions and target != 0x1000:
                    code_pointers[offset] = target
                struct.pack_into('<I', initial, offset, 0)
        # Text buffers are populated by PANELS at runtime; the standalone test
        # needs the same destinations even when it calls a text callback alone.
        ordered = sorted(self.instructions.items())
        for index, (address, (op, args)) in enumerate(ordered):
            if op != 'call' or not args.startswith('DWORD PTR ds:'):
                continue
            name = self.imports[int(args.split(':')[1], 16)]
            if name not in ('wsprintfA', 'lstrcpyA'):
                continue
            prior = ordered[max(0, index - 40):index]
            pushes = [(at, value) for at, (ins, value) in prior if ins == 'push']
            register = pushes[-1][1]
            definitions = [(at, value) for at, (ins, value) in prior
                           if ins == 'mov' and value.startswith(register + ',')]
            if not definitions:
                raise SurveyError(f'{self.path}: unresolved print destination at {address:x}')
            source = definitions[-1][1].split(',', 1)[1]
            match = re.fullmatch(r'(?:DWORD PTR )?ds:(0x[0-9a-f]+)', source)
            if not match:
                raise SurveyError(f'{self.path}: indirect print destination {source}')
            self.print_slots.add(int(match[1], 16) - self.pe.uiBase - data['rva'])
        def array(name, blob, const=''):
            return f'{const}uint8_t {name}[{len(blob)}] = {{\n' + '\n'.join(
                ','.join(f'0x{value:02x}' for value in blob[offset:offset + 24]) + ','
                for offset in range(0, len(blob), 24)) + '\n};'
        source = ['/* Generated by tools/gen_gauges.py; explicit C transliteration, no retail text bytes. */',
                  '#include "gauge_lift.h"', array('gauge_data', initial), array('gauge_rdata', read_only, 'static const '),
                  'static uint32_t gauge_address(uint32_t rva);']
        source += [f'static void execute_{start - self.pe.uiBase:x}(struct gauge_frame *f);'
                   for start in self.functions if start != self.pe.uiBase + 0x1000]
        source += bodies + code
        source += ['static uint32_t gauge_address(uint32_t rva) {',
                   f'if (rva >= 0x{data["rva"]:x} && rva < 0x{data["rva"] + data["virtual_size"]:x}) return (uint32_t)(uintptr_t)(gauge_data + rva - 0x{data["rva"]:x});',
                   f'if (rva >= 0x{rdata["rva"]:x} && rva < 0x{rdata["rva"] + rdata["virtual_size"]:x}) return (uint32_t)(uintptr_t)(gauge_rdata + rva - 0x{rdata["rva"]:x});',
                   'switch (rva) {']
        source += [f'case 0x{binding["rva"]:x}: return (uint32_t)(uintptr_t)code_{binding["rva"]:x};' for binding in bindings]
        source += ['default: RaiseException(EXCEPTION_ILLEGAL_INSTRUCTION, 0, 0, NULL); return 0; }}',
                   f'__asm__(".globl _ImportTable\\n.set _ImportTable, _gauge_data + 0x{import_offset:x}\\n.globl _Linkage\\n.set _Linkage, _gauge_data + 0x{linkage_offset:x}\\n");',
                   'BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) { (void)reserved; if (reason == DLL_PROCESS_ATTACH) {']
        for address, target in self.relocs:
            if data['rva'] <= address < data['rva'] + data['virtual_size']:
                source.append(f'gauge_w32((uint32_t)(uintptr_t)(gauge_data + 0x{address - data["rva"]:x}), gauge_address(0x{target:x}));')
        source += ['DisableThreadLibraryCalls(instance); } return TRUE; }']
        name = self.path.stem
        directory = OUT / name
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'gauge.c').write_text('\n'.join(source) + '\n')
        (directory / 'gauge.def').write_text('EXPORTS\n    ImportTable DATA\n    Linkage DATA\n')
        resources = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0))
        bitmaps = []
        for resource in self.pe.resources():
            if resource['type'] == 16:
                continue
            if resource['type'] != 2 or not isinstance(resource['id'], int):
                raise SurveyError('unexpected gauge resource type')
            resources += struct.pack('<IIHHHHIHHII', resource['size'], 32, 0xffff, 2, 0xffff,
                                     resource['id'], 0, 0x1030, resource['language'], 0, 0)
            resources += self.pe.read_bytes(self.pe.file_offset(resource['rva'], resource['size']), resource['size'])
            resources += bytes(-len(resources) % 4)
            bitmaps.append([resource['id'], resource['language'], resource['size']])
        (directory / 'gauge.res').write_bytes(resources)
        # The installed filename is retained in the gauge's own header name.
        header = self.pe.va_uint32(self.pe.uiBase + exports['Linkage'] + 0x18) - self.pe.uiBase
        pointer_words = {address - data['rva'] for address, target in self.relocs
                         if data['rva'] <= address < data['rva'] + data['virtual_size']}
        pointer_words |= {import_offset + 4, header - data['rva'] + 0x28}
        root_slot = self.pe.va_uint32(self.pe.uiBase + header + 8)
        root = self.pe.va_uint32(root_slot)
        pending, elements = [root], set()
        while pending:
            element = pending.pop()
            if not element or element in elements: continue
            offset = element - self.pe.uiBase - data['rva']
            if not 0 <= offset < data['virtual_size'] - 0x2c or self.pe.va_uint32(element) > 7:
                raise SurveyError(f'{self.path}: invalid initial element tree')
            elements.add(element)
            pointer_words.add(offset + 0x24)  # tree_install writes each child's parent
            children = self.pe.va_uint32(element + 0x28)
            if children:
                for index in range(1024):
                    child = self.pe.va_uint32(children + 4 * index)
                    if not child: break
                    pending.append(child)
                else: raise SurveyError('unterminated gauge element children')
        installed = None
        for line in Path('docs/setup_map.tsv').read_text().splitlines():
            if line.startswith('#') or '\t' not in line:
                continue
            source_path, destination = line.split('\t', 1)
            if source_path.upper() == 'GAUGES\\' + self.path.name.upper():
                installed = destination.split('\\')[-1].upper()
        if installed is None:
            raise SurveyError(f'{self.path}: missing setup filename mapping')
        manifest = {'name': name, 'installed': installed, 'base': self.pe.uiBase,
                    'data_rva': data['rva'], 'data_size': data['virtual_size'], 'imports': import_offset,
                    'linkage': linkage_offset, 'header': header - data['rva'], 'functions': bindings,
                    'code_pointers': code_pointers, 'relocs': [[address - data['rva'], target] for address, target in self.relocs if data['rva'] <= address < data['rva'] + data['virtual_size']],
                    'float_slots': sorted(self.float_slots), 'print_slots': sorted(self.print_slots), 'bitmaps': bitmaps,
                    'indirect_slots': sorted(self.indirect_slots - self.print_slots - set(range(import_offset, import_offset + 16, 4))),
                    'pointer_words': sorted(pointer_words),
                    'sha256': hashlib.sha256(self.pe.byData).hexdigest()}
        (directory / 'manifest.json').write_text(json.dumps(manifest, indent=2))
        return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('names', nargs='*')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    manifests = []
    for path in sorted(Path('orig/Gauges').glob('*.GAU')):
        if args.names and path.stem not in args.names:
            continue
        manifest = Gauge(path).emit()
        manifests.append(manifest)
        print(f'{path.stem}: {len(manifest["functions"])} C routines, {manifest["data_size"]} data bytes')
    if not args.names:
        (OUT / 'manifest.json').write_text(json.dumps(manifests, indent=2))


if __name__ == '__main__':
    main()
