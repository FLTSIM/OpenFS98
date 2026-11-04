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
PAGE_CONSTRUCT, PAGE_DESTROY = 0x1d788, 0x1d798          # CSimsTabPage's
THUNK_SLOTS = {0x223a2e80: 0x1d530, 0x223a2e6e: 0x1d53c,  # CString() and ~CString, through thunks
               0x223a2e8c: 0x1d528, 0x223a2e92: 0x1d524,  # CWnd() and ~CWnd, under an inline spin button
               0x223a2f40: 0x1d4b0}                      # ~CScrollBar, under an inline scroll bar
NOT_MEMBER_THUNKS = {0x223a2ed4}                         # CString::operator=
DDX_THUNK = 0x223a2e26                                   # jmp MFC42 #2302, DDX_Control
NOT_MEMBERS = {0x1d464}                                  # lstrcpyA: Save video's constructor clears a name

# name, object size, template, descriptor offset (0: the factory stores none), constructor,
# destructor, deleting destructor, factory, vtable, DoDataExchange, its end, context-help
# table, DDX count. A tab page has no factory and no descriptor.
CLASSES = (
    ('video_finished', 0x1290, 0x39f, 0, 0xde00, 0xde90, 0xde70, 0x1810, 0x18ae0, 0xdf00, 0xdf30, 0x18a40, 2),
    ('video_message', 0x13fc, 0x3a0, 0x10dc, 0xe030, 0xe0e0, 0xe0c0, 0x18a0, 0x18c48, 0xe170, 0xe1c0, 0x18bd0, 4),
    ('save_video', 0x1900, 0x3a1, 0x11e4, 0xd510, 0xd620, 0xd600, 0x1250, 0x18950, 0xd6f0, 0xd7b0, 0x18778, 9),
    ('flight_video', 0x1a10, 0x3a2, 0x10e4, 0xc800, 0xc950, 0xc930, 0x11c0, 0x18860, 0xca60, 0xcb60, 0x18620, 13),
    ('add_logbook', 0x1408, 0x394, 0x10e4, 0xa650, 0xa730, 0xa710, 0x1400, 0x17ed8, 0xa7d0, 0xa840, 0x17e60, 4),
    ('logbook_entry', 0x1ca4, 0x393, 0x10e0, 0x9720, 0x9940, 0x9920, 0x1490, 0x17d70, 0x9ae0, 0x9cc0, 0x17c88, 18),
    ('book', 0x1c00, 0x392, 0x10fc, 0x80f0, 0x8260, 0x8230, 0x1370, 0x17ac0, 0x83a0, 0x84c0, 0x178d8, 13),
    ('landing', 0x1724, 0x38a, 0x10e4, 0x7900, 0x79f0, 0x79d0, 0x1660, 0x177e8, 0x7ab0, 0x7b50, 0x17718, 8),
    ('manoeuvre', 0x1444, 0x38b, 0x10e0, 0xab90, 0xac50, 0xac30, 0x16f0, 0x18060, 0xace0, 0xad40, 0x17fc8, 4),
    ('analysis', 0x1b24, 0x38e, 0x10dc, 0x4ab0, 0x4bf0, 0x4bd0, 0x12e0, 0x16e80, 0x4d00, 0x4e00, 0x16d30, 13),
    ('replay', 0x1a38, 0x391, 0x10e0, 0x6990, 0x6b00, 0x6ae0, 0x1130, 0x173f0, 0x6c30, 0x6d60, 0x17328, 14),
    ('demo', 0x1650, 0x39c, 0x10e0, 0xc140, 0xc230, 0xc210, 0x1780, 0x18530, 0xc300, 0xc3a0, 0x18480, 8),
    ('display', 0x21c4, 0x385, 0x10dc, 0x3b20, 0x3c20, 0x3c00, 0x15c0, 0x169c0, 0x3cf0, 0x3d90, 0x16928, 8),
    ('display_aircraft', 0x168c, 0x386, 0, 0x2aa0, 0x2b80, 0x2b60, 0, 0x16470, 0x2c30, 0x2ca0, 0x16438, 5),
    ('display_map', 0x13a4, 0x387, 0, 0x2f30, 0x2fe0, 0x2fc0, 0, 0x165e0, 0x3070, 0x30d0, 0x16568, 4),
    ('display_scenery', 0x19ac, 0x388, 0, 0x43e0, 0x44f0, 0x44d0, 0, 0x16c38, 0x45d0, 0x46b0, 0x16b70, 11),
    ('display_hardware', 0x18ac, 0x3a6, 0, 0x3240, 0x3360, 0x3340, 0, 0x16830, 0x3450, 0x3530, 0x166d8, 11),
    ('prefs', 0x2358, 0x39b, 0x10dc, 0xb480, 0xb5a0, 0xb580, 0x1520, 0x18390, 0xb690, 0xb770, 0x182c8, 11),
    ('prefs_general', 0x1c4c, 0x398, 0, 0x57f0, 0x5950, 0x5930, 0, 0x17090, 0x5a80, 0x5bb0, 0x16f70, 16),
    ('prefs_instrument', 0x150c, 0x399, 0, 0x7500, 0x75c0, 0x75a0, 0, 0x17620, 0x7650, 0x76c0, 0x175a0, 5),
    ('prefs_international', 0x1714, 0x39a, 0, 0x63f0, 0x64f0, 0x64d0, 0, 0x17230, 0x65d0, 0x66a0, 0x17188, 10),
    ('prefs_display', 0x16ec, 0x396, 0, 0x2430, 0x2540, 0x2520, 0, 0x16340, 0x2620, 0x26c0, 0x16248, 8),
    ('prefs_sound', 0x2dbc, 0x395, 0, 0x19d0, 0x1b00, 0x1ae0, 0, 0x16150, 0x1c00, 0x1d00, 0x16000, 13),
    ('prefs_flight', 0x14f8, 0x397, 0, 0xb0e0, 0xb1a0, 0xb180, 0, 0x181d0, 0xb230, 0xb2a0, 0x18150, 5),
    ('controls', 0x2178, 0x3a5, 0x10dc, 0xe470, 0xe550, 0xe530, 0x1930, 0x18dc8, 0xe610, 0xe6a0, 0x18d38, 7),
    ('controls_sensitivity', 0x2584, 0x3a3, 0, 0xec00, 0xed90, 0xed70, 0, 0x19078, 0xeef0, 0xf050, 0x18eb8, 18),
    ('controls_keys', 0x1b64, 0x3a4, 0, 0x10ab0, 0x10c50, 0x10c30, 0, 0x19408, 0x10db0, 0x10ee0, 0x19170, 15),
    ('controls_force', 0x168c, 0x3a7, 0, 0x127c0, 0x128a0, 0x12880, 0, 0x196e0, 0x12950, 0x129e0, 0x195c0, 7),
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
