#!/usr/bin/env python3
"""Resources and the layout checks for every gauge; `--check` the committed tables against retail.

Default (the build step): for each CD gauge, bin/gauges-all/<NAME>/gauge.def and gauge.res (the
bitmaps, from orig/Gauges/<NAME>.GAU) and manifest.json (a copy of the gauge's line of the committed
src/gauges/gauges_manifest.json), rewritten only when they change. The gauge's image is committed C
(src/gauges/images/<NAME>/: layout.h, data.inc, rdata.inc, rebind.inc, built by src/gauges/gauge.h
with the hand-written src/gauges/<name>.c). The layout asserts stay: the imports allowed, a single
PANELS import, the element tree's validity, relocations of type 3, the installed name.

--check (tools/ofs gauges-tables-check): today's full analysis, with objdump, in memory, writing
nothing. It exits 1, naming the gauge, if a committed layout.h or rebind.inc, the manifest's line,
or the host-compiled bytes of a data.inc or rdata.inc differ from what retail gives. The
disassembler supplies instruction boundaries and operands only; no code is emitted.
"""
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

from dialogmap import Fs98Pe, SurveyError

OUT = Path('bin/gauges-all')
COMMITTED = Path('src/gauges/gauges_manifest.json')
IMAGES = Path('src/gauges/images')
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
    def __init__(self, path, disassemble=True):
        self.path = path
        self.pe = Fs98Pe(path)
        self.sections = {section['name']: section for section in self.pe.aSections}
        self.relocs = relocations(self.pe)
        self.imports = {entry['iat_va']: entry['name'] for entry in self.pe.imports()}
        if set(self.imports.values()) - {'DisableThreadLibraryCalls', '_ftol', 'wsprintfA', 'lstrcpyA'}:
            raise SurveyError(f'{path}: unsupported imports')
        self.signatures = {}
        self.float_slots = set()
        self.int_slots = set()
        self.print_slots = set()
        self.indirect_slots = set()
        if disassemble:
            self.disassemble()

    def disassemble(self):
        path = self.path
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

    def layout(self):
        """The image and the asserts every mode keeps: the imports, a single PANELS import, the element tree, the installed name."""
        pe = self.pe
        data = self.sections['.data']
        rdata = self.sections['.rdata']
        initial = bytearray(pe.read_bytes(data['raw_offset'], min(data['raw_size'], data['virtual_size'])))
        initial.extend(bytes(data['virtual_size'] - len(initial)))
        read_only = pe.read_bytes(rdata['raw_offset'], rdata['virtual_size'])
        exports = pe.exports()
        import_offset = exports['ImportTable'] - data['rva']
        linkage_offset = exports['Linkage'] - data['rva']
        if struct.unpack_from('<IIII', initial, import_offset) != (15, 0, 0, 0):
            raise SurveyError('expected a single PANELS import')
        relocs = [(address - data['rva'], target) for address, target in self.relocs
                  if data['rva'] <= address < data['rva'] + data['virtual_size']]
        for offset, target in relocs:
            struct.pack_into('<I', initial, offset, 0)
        header = pe.va_uint32(pe.uiBase + exports['Linkage'] + 0x18) - pe.uiBase
        pointer_words = {offset for offset, target in relocs}
        pointer_words |= {import_offset + 4, header - data['rva'] + 0x28}
        root_slot = pe.va_uint32(pe.uiBase + header + 8)
        root = pe.va_uint32(root_slot)
        pending, elements = [root], {}
        while pending:
            element = pending.pop()
            if not element or element in elements: continue
            offset = element - pe.uiBase - data['rva']
            if not 0 <= offset < data['virtual_size'] - 0x2c or pe.va_uint32(element) > 7:
                raise SurveyError(f'{self.path}: invalid initial element tree')
            pointer_words.add(offset + 0x24)  # tree_install writes each child's parent
            children = pe.va_uint32(element + 0x28)
            listed = []
            if children:
                for index in range(1024):
                    child = pe.va_uint32(children + 4 * index)
                    if not child: break
                    listed.append(child)
                else: raise SurveyError('unterminated gauge element children')
            elements[element] = (pe.va_uint32(element), children, listed)
            pending.extend(listed)
        installed = None
        for line in Path('docs/setup_map.tsv').read_text().splitlines():
            if line.startswith('#') or '\t' not in line:
                continue
            source_path, destination = line.split('\t', 1)
            if source_path.upper() == 'GAUGES\\' + self.path.name.upper():
                installed = destination.split('\\')[-1].upper()
        if installed is None:
            raise SurveyError(f'{self.path}: missing setup filename mapping')
        base = pe.uiBase
        return {'name': self.path.stem, 'installed': installed, 'initial': bytes(initial), 'rdata': read_only, 'relocs': relocs,
                'data_rva': data['rva'], 'data_size': data['virtual_size'], 'rdata_rva': rdata['rva'],
                'rdata_size': rdata['virtual_size'], 'base': base, 'import_offset': import_offset,
                'linkage_offset': linkage_offset, 'header': header - data['rva'],
                'root_slot': root_slot - base - data['rva'],
                'elements': {element - base - data['rva']: (kind, children - base - data['rva'] if children else 0,
                                                            [child - base - data['rva'] for child in listed])
                             for element, (kind, children, listed) in elements.items()},
                'pointer_words': pointer_words}

    def resources(self):
        """gauge.res (the bitmaps) and their [id, language, size] list."""
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
        return bytes(resources), bitmaps

    def analyze(self):
        """The full analysis (needs the disassembly): the layout and the harness manifest."""
        layout = self.layout()
        data = self.sections['.data']
        import_offset, linkage_offset = layout['import_offset'], layout['linkage_offset']
        exports = self.pe.exports()
        bindings = []
        for start, body in self.functions.items():
            if start == self.pe.uiBase + 0x1000:
                continue  # DllMain: gauge.h's own
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
        bitmaps = self.resources()[1]
        pointer_words = layout['pointer_words']
        manifest = {'name': layout['name'], 'installed': layout['installed'], 'base': self.pe.uiBase,
                    'data_rva': data['rva'], 'data_size': data['virtual_size'], 'imports': import_offset,
                    'linkage': linkage_offset, 'header': layout['header'], 'functions': bindings,
                    'code_pointers': code_pointers, 'relocs': [[offset, target] for offset, target in layout['relocs']],
                    'float_slots': sorted(self.float_slots), 'print_slots': sorted(self.print_slots), 'bitmaps': bitmaps,
                    # integers the code reads, less pointers (relocated words, the import, print and indirect slots)
                    'int_slots': sorted([offset, width] for offset, width in self.int_slots
                                        if not any(offset <= word < offset + width // 8 or word <= offset < word + 4
                                                   for word in pointer_words | set(self.print_slots) | self.indirect_slots
                                                   | set(range(import_offset, import_offset + 16, 4)))),
                    'indirect_slots': sorted(self.indirect_slots - self.print_slots - set(range(import_offset, import_offset + 16, 4))),
                    'pointer_words': sorted(pointer_words),
                    'sha256': hashlib.sha256(self.pe.byData).hexdigest()}
        return manifest, layout


def layout_text(layout):
    """src/gauges/images/<NAME>/layout.h: retail's per-gauge values for gauge.h."""
    return (f'/* {layout["name"]}.GAU\'s layout in retail: the .data and .rdata sections (RVA and virtual size), the image base, and\n'
            ' * the offsets of the ImportTable and Linkage exports from the start of .data.  Read by src/gauges/gauge.h. */\n'
            f'#define GAUGE_DATA_RVA 0x{layout["data_rva"]:x}u\n#define GAUGE_DATA_SIZE 0x{layout["data_size"]:x}u\n'
            f'#define GAUGE_RDATA_RVA 0x{layout["rdata_rva"]:x}u\n#define GAUGE_RDATA_SIZE 0x{layout["rdata_size"]:x}u\n'
            f'#define GAUGE_BASE 0x{layout["base"]:x}u\n'
            f'#define GAUGE_IMPORT_OFFSET 0x{layout["import_offset"]:x}\n#define GAUGE_LINKAGE_OFFSET 0x{layout["linkage_offset"]:x}\n')


def rebind_text(layout):
    """src/gauges/images/<NAME>/rebind.inc: one GAUGE_REBIND(off, rva) per relocated .data slot, in retail's relocation order."""
    return (f'/* {layout["name"]}.GAU\'s relocated .data slots, in retail\'s relocation order.  GAUGE_REBIND(off, rva): the slot at +off\n'
            ' * (from the start of .data) holds the image base plus rva, an RVA in .data, .rdata or .text; attach stores the address\n'
            ' * of the same place in this image (src/gauges/gauge.h, with the hand-written gauge\'s routines for the code ones). */\n'
            + ''.join(f'GAUGE_REBIND(0x{offset:x}, 0x{target:x})\n' for offset, target in layout['relocs']))


def committed():
    """(gauges, lines): src/gauges/gauges_manifest.json parsed, and its one line per gauge."""
    text = COMMITTED.read_text()
    gauges = json.loads(text)
    lines = [line.rstrip(',') for line in text.splitlines()[1:-1]]
    if len(lines) != len(gauges) or any(json.loads(line)['name'] != gauge['name'] for line, gauge in zip(lines, gauges)):
        raise SystemExit(f'{COMMITTED}: not one line per gauge')
    stems = [path.stem for path in sorted(Path('orig/Gauges').glob('*.GAU'))]
    if [gauge['name'] for gauge in gauges] != stems:
        raise SystemExit(f'{COMMITTED}: the gauges differ from orig/Gauges/*.GAU (names or order)')
    return gauges, lines


def write_if_changed(path, content):
    content = content if isinstance(content, bytes) else content.encode()
    if not path.exists() or path.read_bytes() != content:
        path.write_bytes(content)


def host_bytes(directory, layout, scratch):
    """The bytes the host compiler gives data.inc and rdata.inc: .data then .rdata."""
    source = scratch / 'image.c'
    source.write_text('#include <stdint.h>\n#include <stdio.h>\n'
                      f'static const uint8_t data[{layout["data_size"]}] = {{\n#include "data.inc"\n}};\n'
                      f'static const uint8_t rdata[{layout["rdata_size"]}] = {{\n#include "rdata.inc"\n}};\n'
                      'int main(void) { fwrite(data, 1, sizeof data, stdout); fwrite(rdata, 1, sizeof rdata, stdout); return 0; }\n')
    subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror', f'-I{directory}', '-o', str(scratch / 'image'), str(source)], check=True, capture_output=True)
    return subprocess.run([str(scratch / 'image')], check=True, capture_output=True).stdout


def check_one(line_and_text):
    """The problems of one gauge's committed tables against retail's full analysis; [] if none."""
    name, line = line_and_text
    problems = []
    try:
        manifest, layout = Gauge(Path('orig/Gauges') / (name + '.GAU')).analyze()
        directory = IMAGES / name
        for file, text in (('layout.h', layout_text(layout)), ('rebind.inc', rebind_text(layout))):
            if not (directory / file).is_file() or (directory / file).read_text() != text:
                problems.append(f'{directory / file} differs from retail')
        if json.dumps(manifest) != line:
            problems.append(f'{COMMITTED}: the line of {name} differs from retail')
        with tempfile.TemporaryDirectory() as scratch:
            try:
                built = host_bytes(directory, layout, Path(scratch))
            except (subprocess.CalledProcessError, OSError) as error:
                problems.append(f'{directory}: data.inc or rdata.inc do not compile or run ({getattr(error, "stderr", error)})')
            else:
                want = layout['initial'] + layout['rdata']
                if built != want:
                    first = next((at for at in range(min(len(built), len(want))) if built[at] != want[at]), min(len(built), len(want)))
                    part = ('data.inc', first) if first < len(layout['initial']) else ('rdata.inc', first - len(layout['initial']))
                    problems.append(f'{directory / part[0]}: differs from retail at +0x{part[1]:x} (or has the wrong length)')
    except (SurveyError, OSError, subprocess.CalledProcessError) as error:
        problems.append(f'{name}: {error}')
    return [f'{name}: {problem}' for problem in problems]


def check(names):
    gauges, lines = committed()
    unknown = set(names) - {gauge['name'] for gauge in gauges}
    if unknown:
        raise SystemExit(f'unknown gauge(s): {", ".join(sorted(unknown))}')
    jobs = [(gauge['name'], line) for gauge, line in zip(gauges, lines) if not names or gauge['name'] in names]
    with ThreadPoolExecutor(max_workers=8) as pool:
        problems = [problem for result in pool.map(check_one, jobs) for problem in result]
    for problem in problems:
        print(problem)
    if problems:
        print(f'gauges-tables-check: FAILED ({len(problems)} problems)')
        return 1
    print(f'gauges-tables-check: {len(jobs)} gauges: layout.h, rebind.inc, manifest line and the host-compiled data.inc and rdata.inc match retail')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('names', nargs='*')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.check:
        sys.exit(check(args.names))
    gauges, _ = committed()
    unknown = set(args.names) - {gauge['name'] for gauge in gauges}
    if unknown:
        raise SystemExit(f'unknown gauge(s): {", ".join(sorted(unknown))}')
    OUT.mkdir(parents=True, exist_ok=True)
    count = 0
    for gauge in gauges:
        if args.names and gauge['name'] not in args.names:
            continue
        path = Path('orig/Gauges') / (gauge['name'] + '.GAU')
        if not path.is_file():
            raise SystemExit(f'{path} is missing: the gauges need all 120 retail files (docs/building.md, "Which CD")')
        retail = Gauge(path, disassemble=False)
        layout = retail.layout()
        resources, bitmaps = retail.resources()
        if (layout['installed'], bitmaps, hashlib.sha256(retail.pe.byData).hexdigest()) != (
                gauge['installed'], gauge['bitmaps'], gauge['sha256']):
            raise SystemExit(f'{path} is not the reference gauge of {COMMITTED} (docs/building.md, "Which CD")')
        directory = OUT / gauge['name']
        directory.mkdir(parents=True, exist_ok=True)
        write_if_changed(directory / 'gauge.def', 'EXPORTS\n    ImportTable DATA\n    Linkage DATA\n')
        write_if_changed(directory / 'gauge.res', resources)
        write_if_changed(directory / 'manifest.json', json.dumps(gauge, indent=2))
        count += 1
    print(f'{count} gauges: resources written, layout checked')


if __name__ == '__main__':
    main()
