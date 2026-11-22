#!/usr/bin/env python3
"""The eleven MDL models' data images as committed C, written from and checked against retail.

For each model of orig/Aircraft this writes src/mdl/images/<NAME>/layout.h (retail's layout and the non-pointer
words of the image's fixed parts) and data.inc (the body of .data, typed), and src/mdl/mdl_manifest.json (the
modules' metadata).  src/mdl/mdl_image.c builds them into the image the MDL carries.  Only this oracle reads
retail's .data; tools/build_mdls.py and tools/gen_mdl_crash.py read only the resources.

  --check       (default; tools/ofs mdl-images-check) regenerate in memory and compare with the committed files
                (exit 1 naming the file and the first differing line), and host-compile every data.inc against retail's bytes
  --write       write the committed files
  --self-test   a moved slot and an extra slot in .data are rejected

The image is the model's .data at its RVA: lead (zeros, and Sopwith's literal), prefix 0x10, the header (0x24), the body up to
Linkage, Linkage (0x24) and a 0x14 tail.  Retail relocates exactly seven words inside .data: header +0x4 (a data pointer) and
+0x10 (update code), Linkage +0x4, +0x8, +0x18, +0x1c and +0x20 (code).  Anything else is an error.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

from build_mdls import NAMES
from dialogmap import Fs98Pe
from gen_gauges import relocations
from gen_mdl_crash import CRASH_NAMES
from mdl_survey import survey

ROOT = Path('src/mdl')
IMAGES = ROOT / 'images'
MANIFEST = ROOT / 'mdl_manifest.json'
HEADER, LINKAGE, TAIL, FIRST = 0x24, 0x24, 0x14, 0x34  # sizes of the fixed parts; the body's first .data offset
HEADER_AT = 0x10
SOPWITH = b'Sopwith Camel\0'


def chain_callback(path, pe):
    """The CHAIN callback: the code address pushed just before list 15 (push 0xf)."""
    output = subprocess.check_output(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn', str(path)], text=True)
    instructions = []
    for line in output.splitlines():
        match = re.fullmatch(r'\s*([0-9a-f]+):\s+(\w+)\s*(.*)', line)
        if match:
            instructions.append((match[2], match[3].strip()))
    candidates = {int(previous[1], 0) - pe.uiBase
                  for previous, (op, args) in zip(instructions, instructions[1:])
                  if op == 'push' and args == '0xf' and previous[0] == 'push'}
    if len(candidates) != 1:
        raise ValueError('CHAIN callback not unique')
    return candidates.pop()


def check_sites(name, relocs, sites, start, active_end):
    """The relocations inside .data must be exactly `sites` ({address: target}); ValueError naming the model and slot."""
    seen = set()
    for address, target in relocs:
        if address in seen:
            raise ValueError(f'{name}: duplicate relocation at slot 0x{address:x}')
        seen.add(address)
        if address < start or address + 4 > active_end:
            raise ValueError(f'{name}: relocation at slot 0x{address:x} is outside the image (CRT tail or past it)')
        if address not in sites:
            raise ValueError(f'{name}: unexpected relocation at slot 0x{address:x}')
        if sites[address] != target:
            raise ValueError(f'{name}: slot 0x{address:x} targets 0x{target:x}, expected 0x{sites[address]:x}')
    for address in sites:
        if address not in seen:
            raise ValueError(f'{name}: slot 0x{address:x} is not relocated')


def analyze(path):
    """(manifest record, layout.h text, data.inc text) of one retail model."""
    name = path.stem
    crash = name in CRASH_NAMES
    kind = 8 if crash else NAMES.index(name)
    pe = Fs98Pe(path)
    record = survey(path)
    data = record['sections']['.data']
    start, size = data['rva'], data['size']
    exports = pe.exports()
    linkage, table = exports['Linkage'], exports['ImportTable']
    header = int(record['header_rva'], 16)
    active_end = linkage + LINKAGE
    if header != start + HEADER_AT or not start + FIRST <= table < linkage or active_end + TAIL != start + size:
        raise ValueError(f'{path}: invalid model data bounds')
    update = int(record['update_rva'], 16)
    slots = {int(offset, 16): int(value, 16) for offset, value in record['linkage'].items() if value}
    code = [(header + 0x10, update, 'update'), (linkage + 4, slots[0x4], 'nop'), (linkage + 8, slots[0x8], 'nop'),
            (linkage + 0x18, slots[0x18], 'attach'), (linkage + 0x1c, slots[0x1c], 'state'), (linkage + 0x20, slots[0x20], 'detach')]
    for _, target, role in code:
        if any(t == target and r != role for _, t, r in code):
            raise ValueError(f'{path}: overlapping code entries')
    pointer = int(record['header_words'][1], 16) - pe.uiBase
    if not start + FIRST <= pointer < active_end:
        raise ValueError(f'{path}: unrecognized relocated target 0x{pointer:x}')
    sites = {header + 4: pointer, **{address: target for address, target, _ in code}}
    relocs = [(a, t) for a, t in sorted(relocations(pe)) if start <= a < start + size]
    check_sites(name, relocs, sites, start, active_end)
    raw = pe.read_bytes(pe.file_offset(start, size), size)
    if kind == 7 and pe.read_bytes(pe.file_offset(0x1037, 14), 14) != SOPWITH:
        raise ValueError(f'{path}: Sopwith literal moved')
    at = lambda rva, n: raw[rva - start:rva - start + n]
    prefix, head, lead_in = at(start, HEADER_AT), struct.unpack('<9I', at(header, HEADER)), at(linkage, LINKAGE)
    words = struct.unpack('<9I', lead_in)
    body = at(start + FIRST, linkage - start - FIRST)
    tail = raw[active_end - start:]
    for index, word in ((1, head[1]), (4, head[4])):
        if word != pe.uiBase + sites[header + 4 * index]:
            raise ValueError(f'{path}: header word {index} is not its relocation')
    for index in (1, 2, 6, 7, 8):
        if words[index] != pe.uiBase + sites[linkage + 4 * index]:
            raise ValueError(f'{path}: Linkage word {index} is not its relocation')
    resources = [{key: resource[key] for key in ('type', 'id', 'language', 'size')} for resource in pe.resources()]
    record.update(kind=kind, installed=str(path.relative_to('orig')), model_relocs=[list(r) for r in relocs],
                  active_end=active_end, resources=resources, linkage_rva=linkage, import_rva=table)
    if crash:
        record['chain_rva'] = chain_callback(path, pe)
    layout = dict(name=name, crash=crash, start=start, size=size, body=len(body), table=table, linkage=linkage, header=header,
                  pointer=pointer, prefix=prefix, head=head, words=words, tail=tail, sopwith=kind == 7, imports=len(record['imports']))
    return record, layout_text(layout), body_text(name, body, table - start, len(record['imports']))


def layout_text(layout):
    """src/mdl/images/<NAME>/layout.h: retail's layout, and the image's fixed bytes that are not pointers."""
    bytes_ = lambda data: ', '.join(f'0x{b:02x}' for b in data)
    lines = [f'/* {layout["name"]}.MDL\'s layout in retail: .data (RVA and virtual size), the body between the header and Linkage, the RVAs of',
             ' * the header, ImportTable and Linkage, and the image\'s fixed parts as bytes and words, the relocated ones left out (they are',
             ' * addresses: src/mdl/mdl_image.c).  Read by src/mdl/mdl_image.c; written by tools/gen_mdl_images.py. */',
             f'#define MDL_DATA_RVA 0x{layout["start"]:x}',
             f'#define MDL_DATA_SIZE 0x{layout["size"]:x}',
             f'#define MDL_BODY_SIZE 0x{layout["body"]:x}',
             f'#define MDL_HEADER_RVA 0x{layout["header"]:x}',
             f'#define MDL_IMPORT_RVA 0x{layout["table"]:x}',
             f'#define MDL_LINKAGE_RVA 0x{layout["linkage"]:x}',
             f'#define MDL_DATA_POINTER 0x{layout["pointer"]:x}  /* header +0x4: the body, an RVA in .data */']
    if layout['crash']:
        lines.append('#define MDL_CRASH 1  /* a crash model: the image is in .mdlimg, bound to the crash entries */')
    lines.append('#define MDL_LEAD ' + ('[0x1037] = ' + ', '.join(f"'{chr(b)}'" if b else '0' for b in SOPWITH)
                                       + '  /* Sopwith\'s literal, where mdl_normal.c reads it */' if layout['sopwith'] else '[0] = 0'))
    lines.append(f'#define MDL_PREFIX {bytes_(layout["prefix"])}')
    for index in (0, 2, 3, 5, 6, 7, 8):
        lines.append(f'#define MDL_HEADER_W{index} 0x{layout["head"][index]:x}u')
    for index in (0, 3, 4, 5):
        lines.append(f'#define MDL_LINKAGE_W{index} 0x{layout["words"][index]:x}u')
    lines.append(f'#define MDL_TAIL {bytes_(layout["tail"])}')
    return '\n'.join(lines) + '\n'


def body_text(name, body, table_at, imports):
    """src/mdl/images/<NAME>/data.inc: .data from +0x34 to Linkage as typed values, zero runs of 16 bytes or more left out."""
    base = FIRST
    breaks = {table_at: f'the ImportTable: {imports} imports (module id, link filled at load), then a 0',
              table_at + 8 * (imports + 1): 'meaning not yet known'}
    tokens, strings = [], set()  # (offset, text); AT/comment prefixes are tokens of their own
    position, cursor, size = 0, 0, len(body)
    unknown = True
    while position < size:
        run = 0
        while position + run < size and body[position + run] == 0:
            run += 1
        if run >= 16:
            position += run
            unknown = True
            continue
        if position + base in breaks:
            tokens.append((None, f'/* +0x{position + base:04x}  {breaks[position + base]} */'))
            unknown = False
        elif unknown:
            tokens.append((None, f'/* +0x{position + base:04x}  meaning not yet known */'))
            unknown = False
        prefix = f'AT(0x{position + base:x}) ' if position != cursor else ''
        stop = min([size] + [b - base for b in breaks if b - base > position])
        text = None
        length = 0
        while position + length < stop and 0x20 <= body[position + length] < 0x7f and chr(body[position + length]) not in '"\\?':
            length += 1
        padded = (length + 4) & ~3
        if (length >= 6 and position + padded <= stop and not any(body[position + length:position + padded])):
            text = f'STR{padded}("{body[position:position + length].decode()}")'
            strings.add(padded)
            step = padded
        elif position % 4 == 0 and position + 4 <= stop:
            text, step = f'U32(0x{int.from_bytes(body[position:position + 4], "little"):08x})', 4
        elif position % 2 == 0 and position + 2 <= stop:
            text, step = f'U16(0x{int.from_bytes(body[position:position + 2], "little"):04x})', 2
        else:
            text, step = f'U8(0x{body[position]:02x})', 1
        tokens.append((position + base, prefix + text))
        position += step
        cursor = position
    lines, line = [], ''
    for offset, text in tokens:
        if offset is None:
            if line:
                lines.append(line)
                line = ''
            lines.append(text)
        elif text.startswith('AT(') and line:
            lines.append(line)
            line = text + ','
        elif len(line) + len(text) > 112:
            lines.append(line)
            line = text + ','
        else:
            line += (' ' if line else '') + text + ','
    if line:
        lines.append(line)
    macros = ['#define AT(off) [(off) - 0x34] =', '#define U8(x)  (uint8_t)(x)', '#define U16(x) U8(x), U8((x) >> 8)',
              '#define U32(x) U16(x), U16((x) >> 16)']
    if strings:
        macros += ['#define NUL_PAD "\\0\\0\\0\\0"', '#define SB(s, i) U8((s NUL_PAD)[i])',
                   '#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)']
        macros += ['#define STR%d(s) %s' % (n, ', '.join(f'S4(s, {o})' for o in range(0, n, 4))) for n in sorted(strings)]
    undef = ['AT', 'U8', 'U16', 'U32'] + (['NUL_PAD', 'SB', 'S4'] + [f'STR{n}' for n in sorted(strings)] if strings else [])
    head = [f'/* {name}.MDL\'s .data body, decompiled: every non-zero value of retail\'s .data from +0x34 (just past the header) up to',
            ' * Linkage, at retail\'s offsets, one typed value per macro.  Included by src/mdl/mdl_image.c as the initialiser of the image\'s',
            ' * body.  The header, Linkage, prefix and tail are in layout.h.  Zero runs of 16 bytes or more are not written.  The ImportTable',
            ' * is named; the rest is geometry and tables, meaning not yet known.  Written by tools/gen_mdl_images.py --write.',
            ' *',
            ' * Macros: AT(off) is the offset a disassembly shows (in .data); U8/U16/U32 emit one little-endian value; STRn(s) is a string',
            ' * padded with NULs to n bytes. */']
    return '\n'.join(head + macros + lines + [f'#undef {m}' for m in undef]) + '\n'


def manifest_text(normal, crash):
    def part(records):
        return ',\n'.join(' ' + json.dumps(record) for record in records)
    return '{\n"normal": [\n' + part(normal) + '\n],\n"crash": [\n' + part(crash) + '\n]\n}\n'


def paths():
    found = sorted(path for path in Path('orig/Aircraft').rglob('*') if path.is_file() and path.suffix.upper() == '.MDL')
    normal = [p for p in found if p.stem.upper().endswith('_N')]
    crash = [p for p in found if p.stem.upper().endswith('_C')]
    if tuple(p.stem for p in normal) != NAMES or tuple(p.stem for p in crash) != CRASH_NAMES:
        raise SystemExit('expected the eleven reference MDLs of orig/Aircraft (docs/building.md, "Which CD")')
    return normal, crash


def generate():
    """{path: text} of every committed file, from retail."""
    normal, crash = paths()
    files, records = {}, []
    for path in normal + crash:
        record, layout, data = analyze(path)
        files[IMAGES / path.stem / 'layout.h'] = layout
        files[IMAGES / path.stem / 'data.inc'] = data
        records.append(record)
    files[MANIFEST] = manifest_text(records[:8], records[8:])
    return files, records


def host_bytes(directory, size):
    """The bytes the host compiler gives data.inc."""
    with tempfile.TemporaryDirectory() as scratch:
        source = Path(scratch) / 'body.c'
        source.write_text('#include <stdint.h>\n#include <stdio.h>\n'
                          f'static const uint8_t body[{size}] = {{\n#include "data.inc"\n}};\n'
                          'int main(void) { fwrite(body, 1, sizeof body, stdout); return 0; }\n')
        subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror', f'-I{directory}', '-o', str(Path(scratch) / 'body'), str(source)],
                       check=True, capture_output=True)
        return subprocess.run([str(Path(scratch) / 'body')], check=True, capture_output=True).stdout


def check():
    files, records = generate()
    problems = []
    for path, text in files.items():
        if not path.is_file():
            problems.append(f'{path}: missing')
            continue
        have = path.read_text()
        if have != text:
            old, new = have.splitlines(), text.splitlines()
            line = next((i for i in range(min(len(old), len(new))) if old[i] != new[i]), min(len(old), len(new)))
            problems.append(f'{path}: differs from retail at line {line + 1}')
    normal, crash = paths()
    for path in normal + crash:
        directory = IMAGES / path.stem
        pe = Fs98Pe(path)
        start = survey(path)['sections']['.data']['rva']
        exports = pe.exports()
        want = pe.read_bytes(pe.file_offset(start + FIRST, exports['Linkage'] - start - FIRST), exports['Linkage'] - start - FIRST)
        try:
            built = host_bytes(directory, len(want))
        except (subprocess.CalledProcessError, OSError) as error:
            problems.append(f'{directory / "data.inc"}: does not compile or run ({getattr(error, "stderr", error)})')
            continue
        if built != want:
            first = next((i for i in range(min(len(built), len(want))) if built[i] != want[i]), min(len(built), len(want)))
            problems.append(f'{directory / "data.inc"}: differs from retail at +0x{first + FIRST:x} (or has the wrong length)')
    for problem in problems:
        print(problem)
    if problems:
        print(f'mdl-images-check: FAILED ({len(problems)} problems)')
        return 1
    print(f'mdl-images-check: {len(records)} models: layout.h, data.inc (host-compiled) and {MANIFEST} match retail')
    return 0


def write():
    files, _ = generate()
    for path, text in files.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    print(f'{len(files)} files written')
    return 0


def self_test():
    sites = {0x3014: 0x3034, 0x3020: 0x1037, 0x4000: 0x1220, 0x4004: 0x1220, 0x4014: 0x1230, 0x4018: 0x1020, 0x401c: 0x1030}
    good = sorted(sites.items())
    check_sites('M', good, sites, 0x3000, 0x4020)
    cases = [('a moved slot', [(a + (4 if a == 0x4004 else 0), t) for a, t in good], '0x4008'),
             ('an extra slot', good + [(0x3100, 0x1000)], '0x3100'),
             ('a missing slot', good[:-1], '0x401c'),
             ('a duplicate', good + [good[0]], '0x3014'),
             ('a changed target', [(a, t + (2 if a == 0x3014 else 0)) for a, t in good], '0x3014')]
    for what, relocs, slot in cases:
        try:
            check_sites('M', sorted(relocs), sites, 0x3000, 0x4020)
        except ValueError as error:
            if 'M:' not in str(error) or slot not in str(error):
                raise SystemExit(f'self-test: {what}: the error does not name the model and slot: {error}')
        else:
            raise SystemExit(f'self-test: {what} was accepted')
    print(f'gen_mdl_images self-test passed ({len(cases)} cases)')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--write', action='store_true')
    mode.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    sys.exit(self_test() if args.self_test else write() if args.write else check())


if __name__ == '__main__':
    main()
