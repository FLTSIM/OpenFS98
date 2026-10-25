#!/usr/bin/env python3
"""Extract Views' help mapping as data; no retail code or resource assets."""
from pathlib import Path
import re
import subprocess
import struct
from dialogmap import Fs98Pe, SurveyError


def control_states(start, end):
    """Extract literal control states from five straight-line mode handlers.

    Track only object-relative register values and immediate call arguments.
    Require the exact enable/show pairing rather than guessing unfamiliar code.
    """
    text = subprocess.check_output([
        'objdump', '-d', '--no-show-raw-insn', '-M', 'intel',
        f'--start-address={0x223e0000 + start}',
        f'--stop-address={0x223e0000 + end}', 'orig/English/Views.DLL'], text=True)
    registers = {'esi': 0}
    argument = None
    rows = []
    for line in text.splitlines():
        match = re.match(r'\s*[0-9a-f]+:\s+(.+)', line)
        if not match:
            continue
        instruction = match[1].strip()
        match = re.fullmatch(r'lea\s+(\w+),\[esi\+0x([0-9a-f]+)\]', instruction)
        if match:
            registers[match[1]] = int(match[2], 16)
            continue
        match = re.fullmatch(r'mov\s+(e\w+),(e\w+)', instruction)
        if match:
            registers[match[1]] = registers.get(match[2])
            continue
        match = re.fullmatch(r'push\s+0x([0-9a-f]+)', instruction)
        if match:
            argument = int(match[1], 16)
            continue
        if instruction == 'call   0x223e3c1c':
            if argument != 0x188e:
                raise SurveyError('unexpected Views preview control')
            registers['eax'] = -1
        elif instruction == 'call   0x223e3c22':
            if registers.get('ecx') is None or argument not in (0, 1):
                raise SurveyError('unrecognised Views enable state')
            rows.append([registers['ecx'], argument, None])
        elif instruction == 'call   0x223e3ad2':
            if not rows or rows[-1][0] != registers.get('ecx') or argument not in (0, 5):
                raise SurveyError('unrecognised Views show state')
            rows[-1][2] = argument
    return rows


def mode_controls(start, end):
    rows = control_states(start, end)
    if len(rows) != 16 or rows[0] != [-1, int(start == 0x2ad0), None]:
        raise SurveyError('Views mode control layout moved')
    if any(row[2] is None for row in rows[1:]):
        raise SurveyError('missing Views show state')
    return rows[1:]


def exchange_controls():
    text = subprocess.check_output([
        'objdump', '-d', '--no-show-raw-insn', '-M', 'intel',
        '--start-address=0x223e1690', '--stop-address=0x223e1900',
        'orig/English/Views.DLL'], text=True)
    rows = []
    offset = control = None
    for line in text.splitlines():
        match = re.search(r'lea\s+\w+,\[esi\+0x([0-9a-f]+)\]', line)
        if match:
            offset = int(match[1], 16)
        match = re.search(r'add\s+esi,0x([0-9a-f]+)', line)
        if match:
            offset = int(match[1], 16)
        match = re.search(r'push\s+0x([0-9a-f]+)', line)
        if match:
            control = int(match[1], 16)
        if re.search(r'call\s+0x223e3c10', line):
            if offset is None or control is None:
                raise SurveyError('unrecognised Views exchange binding')
            rows.append((offset, control))
            offset = control = None
    if len(rows) != 34:
        raise SurveyError(f'Views exchange layout moved: {len(rows)} controls')
    return rows


def object_calls(start, end):
    text = subprocess.check_output([
        'objdump', '-d', '--no-show-raw-insn', '-M', 'intel',
        f'--start-address={0x223e0000 + start}',
        f'--stop-address={0x223e0000 + end}', 'orig/English/Views.DLL'], text=True)
    registers = {'ecx': ('object', 0)}
    calls = []
    for line in text.splitlines():
        match = re.match(r'\s*[0-9a-f]+:\s+(.+)', line)
        if not match:
            continue
        instruction = match[1].strip()
        match = re.fullmatch(r'lea\s+(\w+),\[esi\+0x([0-9a-f]+)\]', instruction)
        if match:
            registers[match[1]] = ('object', int(match[2], 16))
            continue
        match = re.fullmatch(r'mov\s+(e\w+),(e\w+)', instruction)
        if match:
            registers[match[1]] = registers.get(match[2])
            continue
        match = re.fullmatch(r'mov\s+(\w+),DWORD PTR ds:0x223e([0-9a-f]+)', instruction)
        if match:
            registers[match[1]] = ('import', int(match[2], 16))
            continue
        if instruction.startswith('call'):
            target = instruction.split()[-1]
            if target.startswith('DWORD PTR'):
                raise SurveyError('unrecognised Views object call')
            direct = re.search(r'ds:0x223e([0-9a-f]+)', instruction)
            if direct:
                imported = int(direct[1], 16)
            elif target in ('0x223e3c04', '0x223e3c0a'):
                imported = 0x7318 if target.endswith('04') else 0x7314
            else:
                value = registers.get(target)
                if value is None or value[0] != 'import':
                    raise SurveyError(f'unrecognised Views object target: {instruction}')
                imported = value[1]
            object_value = registers.get('ecx')
            if object_value is None or object_value[0] != 'object':
                raise SurveyError('unrecognised Views subobject offset')
            calls.append((object_value[1], imported))
    return calls


def main():
    pe = Fs98Pe('orig/English/Views.DLL')
    if pe.uiBase != 0x223e0000 or pe.linkage()['id'] != 0x80000005:
        raise SurveyError('expected the reference Views image at 0x223e0000')
    values = []
    for index in range(64):
        control = pe.va_uint32(pe.uiBase + 0x5000 + index * 8)
        topic = pe.va_uint32(pe.uiBase + 0x5004 + index * 8)
        values.extend((control, topic))
        if control == 0 and topic == 0:
            break
    else:
        raise SurveyError('unterminated Views help mapping')
    lines = ['/* Generated by tools/gen_views_tables.py from retail Views; do not edit. */',
             'static const uint32_t s_help_map[] = {']
    lines += [f'    0x{values[index]:x}, 0x{values[index + 1]:x},'
              for index in range(0, len(values), 2)]
    lines += ['};', 'static const uint16_t s_mode_controls[5][15][3] = {']
    for start, end in ((0x24f0, 0x26f0), (0x26f0, 0x28f0), (0x28f0, 0x2ad0),
                       (0x2d50, 0x2f30), (0x2ad0, 0x2d50)):
        lines.append('    {')
        lines += [f'        {{0x{offset:x}, {enabled}, {show}}},'
                  for offset, enabled, show in mode_controls(start, end)]
        lines.append('    },')
    lines += ['};', 'static const uint16_t s_exchange_controls[][2] = {']
    lines += [f'    {{0x{offset:x}, 0x{control:x}}},' for offset, control in exchange_controls()]
    lines += ['};', '']
    lines.append('static const uint16_t s_empty_controls[][3] = {')
    empty = control_states(0x1f40, 0x20cc)
    if len(empty) != 19:
        raise SurveyError('Views empty control layout moved')
    lines += [f'    {{0x{offset:x}, {enabled}, {0xffff if show is None else show}}},'
              for offset, enabled, show in empty]
    lines += ['};']
    for name, rva in (('zoom_offset', 0x5308), ('zoom_scale', 0x5310), ('position_scale', 0x5318)):
        value = struct.unpack('<d', pe.read_bytes(pe.file_offset(rva, 8), 8))[0]
        lines.append(f'static const long double s_{name} = {value.hex()}L;')
    constructors = object_calls(0x11d0, 0x1430)
    destructors = object_calls(0x1450, 0x1690)
    if len(constructors) != 35 or len(destructors) != 35 or constructors[0] != (0, 0x7438):
        raise SurveyError('Views object lifetime layout moved')
    reverse = list(reversed(destructors))
    if [row[0] for row in constructors] != [row[0] for row in reverse]:
        raise SurveyError('Views subobjects do not destroy in reverse order')
    lines.append('')
    Path('src/views/views_tables.inc').write_text('\n'.join(lines))
    object_lines = ['/* Generated by tools/gen_views_tables.py; subobject ABI metadata only. */',
                    'static const struct fs98_views_subobject s_subobjects[] = {']
    for index, ((offset, constructor), (_, destructor)) in enumerate(zip(constructors[1:], reverse[1:])):
        limit = constructors[index + 2][0] if index + 2 < len(constructors) else 0x2804
        object_lines.append(f'    {{0x{offset:x}, 0x{constructor:x}, 0x{destructor:x}, 0x{limit - offset:x}}},')
    object_lines += ['};', '']
    Path('src/views/views_objects_tables.inc').write_text('\n'.join(object_lines))


if __name__ == '__main__':
    main()
