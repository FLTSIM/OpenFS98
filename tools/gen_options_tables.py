#!/usr/bin/env python3
"""Extract Options' control lifetimes, DDX bindings and context-help tables: ABI metadata only.

    python3 -B tools/gen_options_tables.py

Writes src/options/options_tables.inc and src/options/options_objects_tables.inc from
orig/English/Options.DLL, as tools/gen_flights_tables.py does for Flights.  Each class in
CLASSES is checked against the image: its constructor and destructor must build and tear
down the same members in mirror order on CSimsDialog, and its DoDataExchange must bind the
expected number of controls.
"""
from pathlib import Path
import re
import subprocess
from dialogmap import Fs98Pe, SurveyError

IMAGE = 'orig/English/Options.DLL'
BASE = 0x22390000
MODULE_ID = 0x80000007
DIALOG_CONSTRUCT, DIALOG_DESTROY = 0x1d71c, 0x1d72c      # SIMSUI CSimsDialog ctor/dtor slots
PAGE_CONSTRUCT, PAGE_DESTROY = None, None                # CSimsTabPage's, once a page is in
THUNK_SLOTS = {0x223a2e80: 0x1d530, 0x223a2e6e: 0x1d53c}  # CString() and ~CString, through thunks
NOT_MEMBER_THUNKS = {0x223a2ed4}                         # CString::operator=
DDX_THUNK = 0x223a2e26                                   # jmp MFC42 #2302, DDX_Control
NOT_MEMBERS = {0x1d464}                                  # lstrcpyA: Save video's constructor clears a name

# name, object size, template, descriptor offset (0: the factory stores none), constructor,
# destructor, deleting destructor, factory, vtable, DoDataExchange, its end, context-help
# table, DDX count
CLASSES = (
    ('video_finished', 0x1290, 0x39f, 0, 0xde00, 0xde90, 0xde70, 0x1810, 0x18ae0, 0xdf00, 0xdf30, 0x18a40, 2),
    ('video_message', 0x13fc, 0x3a0, 0x10dc, 0xe030, 0xe0e0, 0xe0c0, 0x18a0, 0x18c48, 0xe170, 0xe1c0, 0x18bd0, 4),
    ('save_video', 0x1900, 0x3a1, 0x11e4, 0xd510, 0xd620, 0xd600, 0x1250, 0x18950, 0xd6f0, 0xd7b0, 0x18778, 9),
    ('flight_video', 0x1a10, 0x3a2, 0x10e4, 0xc800, 0xc950, 0xc930, 0x11c0, 0x18860, 0xca60, 0xcb60, 0x18620, 13),
    ('add_logbook', 0x1408, 0x394, 0x10e4, 0xa650, 0xa730, 0xa710, 0x1400, 0x17ed8, 0xa7d0, 0xa840, 0x17e60, 4),
    ('logbook_entry', 0x1ca4, 0x393, 0x10e0, 0x9720, 0x9940, 0x9920, 0x1490, 0x17d70, 0x9ae0, 0x9cc0, 0x17c88, 18),
    ('book', 0x1c00, 0x392, 0x10fc, 0x80f0, 0x8260, 0x8230, 0x1370, 0x17ac0, 0x83a0, 0x84c0, 0x178d8, 13),
)


def disassemble(start, end):
    return subprocess.check_output([
        'i686-w64-mingw32-objdump', '-d', '--no-show-raw-insn', '-M', 'intel',
        f'--start-address={BASE + start}', f'--stop-address={BASE + end}', IMAGE], text=True)


def exchange_controls(start, end):
    rows = []
    offset = control = None
    for line in disassemble(start, end).splitlines():
        match = re.search(r'(?:lea\s+\w+,\[e[sd]i\+0x|add\s+e[sd]i,0x)([0-9a-f]+)', line)
        if match:
            offset = int(match[1], 16)
        match = re.search(r'push\s+0x([0-9a-f]+)', line)
        if match:
            control = int(match[1], 16)
        if re.search(rf'call\s+0x{DDX_THUNK:x}\b', line):
            if offset is None or control is None:
                raise SurveyError('unrecognised Options DDX binding')
            rows.append((offset, control))
            offset = control = None
    return rows


def import_slot(text):
    """The RVA of an import slot named as ds:0x223axxxx."""
    match = re.search(r'ds:0x223a([0-9a-f]{4})\b', text)
    return 0x10000 + int(match[1], 16) if match else None


def object_calls(start, end):
    """(member offset, import slot) for each constructor or destructor call, in order."""
    registers = {'ecx': ('object', 0)}
    calls = []
    for line in disassemble(start, end).splitlines():
        match = re.match(r'\s*[0-9a-f]+:\s+(.+)', line)
        if not match:
            continue
        instruction = match[1].strip()
        if instruction.startswith('ret'):        # one function: its first return ends it
            break
        match = re.fullmatch(r'lea\s+(\w+),\[esi\+0x([0-9a-f]+)\]', instruction)
        if match:
            registers[match[1]] = ('object', int(match[2], 16))
            continue
        match = re.fullmatch(r'mov\s+(e\w+),(e\w+)', instruction)
        if match:
            registers[match[1]] = registers.get(match[2])
            continue
        match = re.fullmatch(r'mov\s+(\w+),DWORD PTR ds:0x223a[0-9a-f]{4}', instruction)
        if match:
            registers[match[1]] = ('import', import_slot(instruction))
            continue
        if instruction.startswith('call') or (instruction.startswith('jmp') and 'ds:0x223a' in instruction):
            direct = import_slot(instruction)
            thunk = re.fullmatch(r'call\s+0x([0-9a-f]+)', instruction)
            if thunk and int(thunk[1], 16) in NOT_MEMBER_THUNKS:
                continue
            if thunk and int(thunk[1], 16) in THUNK_SLOTS:
                slot = THUNK_SLOTS[int(thunk[1], 16)]
            elif direct:
                slot = direct
            else:
                value = registers.get(instruction.split()[-1])
                if not value or value[0] != 'import':
                    raise SurveyError(f'unrecognised Options object call: {instruction}')
                slot = value[1]
            if slot in NOT_MEMBERS:
                continue
            target = registers.get('ecx')
            if not target or target[0] != 'object':
                raise SurveyError('unrecognised Options member offset')
            calls.append((target[1], slot))
    return calls


def main():
    pe = Fs98Pe(IMAGE)
    if pe.uiBase != BASE or pe.linkage()['id'] != MODULE_ID:
        raise SurveyError('expected the reference Options image')
    tables = ['/* Generated by tools/gen_options_tables.py; ABI metadata only. */']
    objects = ['/* Generated by tools/gen_options_tables.py; member ABI metadata only. */']
    bases = {}
    for (name, size, template, descriptor, ctor, dtor, delete, factory, vtable,
         ddx, ddx_end, help_table, ddx_count) in CLASSES:
        tables.append(f'static const uint32_t s_help_{name}[] = {{')
        for index in range(64):
            control = pe.va_uint32(BASE + help_table + index * 8)
            topic = pe.va_uint32(BASE + help_table + index * 8 + 4)
            tables.append(f'    0x{control:x}, 0x{topic:x},')
            if not control and not topic:
                break
        else:
            raise SurveyError(f'{name}: unterminated context-help table')
        tables.append('};')
        rows = exchange_controls(ddx, ddx_end)
        if len(rows) != ddx_count:
            raise SurveyError(f'{name}: DDX layout moved: {len(rows)} bindings')
        tables.append(f'static const uint16_t s_ddx_{name}[][2] = {{')
        tables += [f'    {{0x{offset:x}, 0x{control:x}}},' for offset, control in rows] or ['    {0, 0},   /* none */']
        tables.append('};')
        tables.append(f'enum {{ s_ddx_count_{name} = {len(rows)} }};')

        constructors = object_calls(ctor, delete)
        destructors = list(reversed(object_calls(dtor, ddx)))
        if [c[0] for c in constructors] != [d[0] for d in destructors]:
            raise SurveyError(f'{name}: constructor and destructor members differ')
        if constructors[0] not in ((0, DIALOG_CONSTRUCT), (0, PAGE_CONSTRUCT)) or \
                destructors[0] != (0, DIALOG_DESTROY if constructors[0][1] == DIALOG_CONSTRUCT else PAGE_DESTROY):
            raise SurveyError(f'{name}: base dialog changed')
        bases[name] = int(constructors[0][1] == PAGE_CONSTRUCT)
        objects.append(f'static const struct fs98_options_subobject s_controls_{name}[] = {{')
        for (offset, constructor), (_, destructor) in zip(constructors[1:], destructors[1:]):
            objects.append(f'    {{0x{offset:x}, 0x{constructor:x}, 0x{destructor:x}}},')
        if len(constructors) == 1:
            objects.append('    {0, 0, 0},   /* none */')
        objects.append('};')
        objects.append(f'enum {{ s_controls_count_{name} = {len(constructors) - 1} }};')
    objects.append('static const struct fs98_options_class s_classes[] = {')
    for (name, size, template, descriptor, ctor, dtor, delete, factory, vtable, *_rest) in CLASSES:
        objects.append(f'    {{0x{size:x}, 0x{template:x}, 0x{descriptor:x}, 0x{ctor:x}, 0x{dtor:x}, '
                       f'0x{delete:x}, 0x{factory:x}, 0x{vtable:x}, '
                       f's_controls_count_{name}, s_controls_{name}, {bases[name]}}},')
    objects.append('};')
    Path('src/options/options_tables.inc').write_text('\n'.join(tables) + '\n')
    Path('src/options/options_objects_tables.inc').write_text('\n'.join(objects) + '\n')


if __name__ == '__main__':
    main()
