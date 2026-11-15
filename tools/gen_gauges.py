#!/usr/bin/env python3
"""Data/resource images and harness metadata for every gauge.

For each CD gauge: image.inc (retail's .data and .rdata at their RVAs, the exports, and a
DllMain rebinding every relocated pointer, the code ones to the hand-written gauge's routines,
src/gauges/<name>.c), gauge.def, gauge.res, and a manifest of what the harness needs: each
routine's arguments and x87 return, the data words read as floats or integers, the print and
indirect slots, the pointers. The disassembler supplies instruction boundaries and operands
only; no code is emitted. Everything generated stays in ignored bin/gauges-all/.
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
CONDITIONS = ('je', 'jne', 'ja', 'jbe', 'jl', 'jge', 'jle', 'jg')
# The operands each mnemonic reads as values: (operand index, read as a float).
INTEGER_READS = {'mov': (1,), 'movsx': (1,), 'push': (0,), 'add': (0, 1), 'sub': (0, 1), 'cmp': (0, 1),
                 'dec': (0,), 'neg': (0,), 'test': (0, 1), 'and': (0, 1), 'or': (0, 1), 'xor': (0, 1),
                 'shl': (0,), 'shr': (0,), 'sar': (0,), 'mul': (0,), 'div': (0,), 'idiv': (0,), 'fild': (0,),
                 'fidiv': (0,)}
FLOAT_READS = {'fld': (0,), 'fmul': (0, 1), 'fadd': (0, 1), 'fsub': (0, 1), 'fsubr': (0, 1), 'fdiv': (0, 1),
               'fidiv': (1,), 'fxch': (0,), 'fcom': (0,), 'fcomp': (0,)}


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
        self.int_slots = set()
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

    def scan(self, op, args):
        """Note the .data words an instruction reads: as a float (a QWORD), or as an integer."""
        operands = args.split(',')
        data = self.sections['.data']
        if op in ('shl', 'shr', 'sar') and not int(operands[1], 0) & 31:
            return   # a shift by nothing reads nothing
        for index, floating in [(index, False) for index in INTEGER_READS.get(op, ())] + \
                               [(index, True) for index in FLOAT_READS.get(op, ())]:
            if index >= len(operands):
                continue
            operand = operands[index]
            if floating:
                match = re.fullmatch(r'QWORD PTR ds:(0x[0-9a-f]+)', operand)
                if match:
                    rva = int(match[1], 16) - self.pe.uiBase
                    if data['rva'] <= rva < data['rva'] + data['virtual_size']:
                        self.float_slots.add(rva - data['rva'])
                continue
            width = 8 if operand.startswith('BYTE') else 16 if operand.startswith('WORD') else 64 if operand.startswith('QWORD') else 32
            match = re.fullmatch(r'(?:(?:BYTE|WORD|DWORD) PTR )?ds:(0x[0-9a-f]+)', operand)
            if match and width < 64:   # an integer the code reads from its data (the harness varies it)
                rva = int(match[1], 16) - self.pe.uiBase
                if data['rva'] <= rva < data['rva'] + data['virtual_size']:
                    self.int_slots.add((rva - data['rva'], width))

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
        bindings = []
        for start, body in self.functions.items():
            if start == self.pe.uiBase + 0x1000:
                continue  # DllMain: image.inc's own
            nargs, fp = self.signature(start)
            rva = start - self.pe.uiBase
            for op, args in body.values():
                self.scan(op, args)
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
        name = self.path.stem
        directory = OUT / name
        directory.mkdir(parents=True, exist_ok=True)
        # The image for the hand-written gauge (src/gauges/<name>.c): retail's
        # data and constants at their RVAs, the exports, and DllMain rebinding every relocated
        # pointer, the code ones through the hand file's gauge_routine(rva).
        image = ['/* Generated by tools/gen_gauges.py: retail\'s data for a hand-written gauge; no code. */',
                 f'#define GAUGE_DATA_RVA 0x{data["rva"]:x}u', f'#define GAUGE_DATA_SIZE 0x{data["virtual_size"]:x}u',
                 f'#define GAUGE_RDATA_RVA 0x{rdata["rva"]:x}u', f'#define GAUGE_RDATA_SIZE 0x{rdata["virtual_size"]:x}u',
                 f'#define GAUGE_BASE 0x{self.pe.uiBase:x}u',
                 array('gauge_data', initial), array('gauge_rdata', read_only, 'static const '),
                 'static void *gauge_routine(uint32_t rva);',
                 'static uint32_t gauge_rebind(uint32_t rva) {',
                 f'if (rva >= 0x{data["rva"]:x} && rva < 0x{data["rva"] + data["virtual_size"]:x}) return (uint32_t)(uintptr_t)(gauge_data + rva - 0x{data["rva"]:x});',
                 f'if (rva >= 0x{rdata["rva"]:x} && rva < 0x{rdata["rva"] + rdata["virtual_size"]:x}) return (uint32_t)(uintptr_t)(gauge_rdata + rva - 0x{rdata["rva"]:x});',
                 '{ void *routine = gauge_routine(rva); if (!routine) RaiseException(EXCEPTION_ILLEGAL_INSTRUCTION, 0, 0, NULL); return (uint32_t)(uintptr_t)routine; }}',
                 f'__asm__(".globl _ImportTable\\n.set _ImportTable, _gauge_data + 0x{import_offset:x}\\n.globl _Linkage\\n.set _Linkage, _gauge_data + 0x{linkage_offset:x}\\n");',
                 'BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) { (void)reserved; if (reason == DLL_PROCESS_ATTACH) {']
        for address, target in self.relocs:
            if data['rva'] <= address < data['rva'] + data['virtual_size']:
                image.append(f'{{ uint32_t bound = gauge_rebind(0x{target:x}); memcpy(gauge_data + 0x{address - data["rva"]:x}, &bound, 4); }}')
        image += ['DisableThreadLibraryCalls(instance); } return TRUE; }']
        (directory / 'image.inc').write_text('\n'.join(image) + '\n')
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
                    # integers the code reads, less pointers (relocated words, the import, print and indirect slots)
                    'int_slots': sorted([offset, width] for offset, width in self.int_slots
                                        if not any(offset <= word < offset + width // 8 or word <= offset < word + 4
                                                   for word in pointer_words | set(self.print_slots) | self.indirect_slots
                                                   | set(range(import_offset, import_offset + 16, 4)))),
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
        print(f"{path.stem}: {len(manifest['functions'])} routines, {manifest['data_size']} data bytes")
    if not args.names:
        (OUT / 'manifest.json').write_text(json.dumps(manifests, indent=2))


if __name__ == '__main__':
    main()
