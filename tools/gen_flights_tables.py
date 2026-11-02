#!/usr/bin/env python3
"""Extract Flights' control lifetimes, DDX bindings and context-help tables: ABI metadata only.

    python3 -B tools/gen_flights_tables.py

Writes src/flights/flights_tables.inc and src/flights/flights_objects_tables.inc from
orig/English/Flights.DLL.  Each class added to CLASSES is checked against the image: its
constructor and destructor must build and tear down the same members in mirror order on
CSimsDialog, and its DoDataExchange must bind the expected number of controls.
"""
from pathlib import Path
import re
import subprocess
from dialogmap import Fs98Pe, SurveyError

IMAGE = 'orig/English/Flights.DLL'
BASE = 0x22310000
DIALOG_CONSTRUCT, DIALOG_DESTROY = 0x135b0, 0x135c8      # SIMSUI CSimsDialog ctor/dtor slots
PAGE_CONSTRUCT, PAGE_DESTROY = 0x13504, 0x13508          # SIMSUI CSimsTabPage's
# member constructors and destructors reached through thunks: the CString's
THUNK_SLOTS = {0x2231bc3e: 0x133dc, 0x2231bc2c: 0x133e8,
               # and a CSimsStatic built inline: MFC's CWnd() and ~CWnd (#567, #795)
               0x2231bcb0: 0x1338c, 0x2231bcb6: 0x13388}
NOT_MEMBER_THUNKS = {0x2231bc62}                          # CString::operator=
DDX_THUNK = 0x2231bc0e                                    # jmp MFC42 #2302, DDX_Control
NOT_MEMBERS = {0x13364}                                   # lstrcpyA: Save's constructor clears a name

# name, object size, template, descriptor offset, constructor, destructor, deleting
# destructor, factory, vtable, DoDataExchange, its end, context-help table, DDX count
CLASSES = (
    ('adventures', 0x15e4, 0x200, 0x10dc, 0x2c10, 0x2cf0, 0x2cd0, 0x12f0, 0xe510, 0x2db0, 0x2e40, 0xe4d0, 7),
    ('challenges', 0x15e4, 0x201, 0x10dc, 0x5000, 0x50e0, 0x50c0, 0x1380, 0xe9a0, 0x51a0, 0x5230, 0xe960, 7),
    ('save', 0x17d0, 0x207, 0x17cc, 0x5f60, 0x6070, 0x6050, 0x1250, 0xed28, 0x6140, 0x61e0, 0xece0, 8),
    ('comms', 0x1fe0, 0x1fd, 0x1fdc, 0x55a0, 0x56f0, 0x56d0, 0x1530, 0xeba0, 0x5810, 0x5920, 0xeb28, 14),
    ('create', 0x1c64, 0x1fe, 0x10e0, 0x2350, 0x2480, 0x2460, 0x1130, 0xe360, 0x2590, 0x2690, 0xe210, 13),
    ('aircraft', 0x1868, 0x1fb, 0x1864, 0x18a0, 0x19c0, 0x19a0, 0x1410, 0xe118, 0x1ab0, 0x1b90, 0xe000, 11),
    ('modeless', 0x10e4, 0x20a, 0x10dc, 0x9d10, 0x9d50, 0x9d30, 0x16e0, 0xf8f8, 0x9d60, 0x9d70, 0xf9e8, 0),
    ('opening', 0x1830, 0x206, 0x1828, 0x9e40, 0x9f40, 0x9f20, 0x1650, 0xfb40, 0xa000, 0xa0b0, 0xf9e8, 8),
    ('flight_menu', 0x165c, 0x1ff, 0x1658, 0xa9d0, 0xaab0, 0xaa90, 0x1770, 0xfd30, 0xab60, 0xabe0, 0xfc30, 6),
    ('learning_menu', 0x165c, 0x209, 0x1658, 0xb240, 0xb320, 0xb300, 0x1800, 0xff20, 0xb3d0, 0xb450, 0xfee8, 6),
    ('select', 0x1810, 0x208, 0x180c, 0x6650, 0x6760, 0x6740, 0x11c0, 0xef08, 0x6850, 0x6910, 0xee18, 10),
    ('autopilot', 0x2178, 0x1fc, 0x2174, 0x3180, 0x3350, 0x3330, 0x15c0, 0xe7f0, 0x34f0, 0x36b0, 0xe728, 24),
    ('navigation', 0x2178, 0x205, 0x10dc, 0x7d80, 0x7e60, 0x7e40, 0x14a0, 0xf2f8, 0x7f40, 0x7fe0, 0xf2b8, 7),
    ('nav_efis', 0x1b64, 0x202, 0x1118, 0x7040, 0x71b0, 0x7190, 0, 0xf158, 0x72f0, 0x7430, 0xf0d8, 15),
    ('nav_radios', 0x244c, 0x203, 0x1118, 0x8560, 0x8760, 0x8740, 0, 0xf518, 0x8940, 0x8b60, 0xf438, 29),
    ('nav_adf', 0x1a24, 0x204, 0x1118, 0x9490, 0x95e0, 0x95c0, 0, 0xf6d8, 0x96f0, 0x9800, 0xf660, 14),
)


def disassemble(start, end):
    return subprocess.check_output([
        'i686-w64-mingw32-objdump', '-d', '--no-show-raw-insn', '-M', 'intel',
        f'--start-address={BASE + start}', f'--stop-address={BASE + end}', IMAGE], text=True)


def exchange_controls(start, end):
    rows = []
    offset = control = None
    for line in disassemble(start, end).splitlines():
        match = re.search(r'(?:lea\s+\w+,\[esi\+0x|add\s+esi,0x)([0-9a-f]+)', line)
        if match:
            offset = int(match[1], 16)
        match = re.search(r'push\s+0x([0-9a-f]+)', line)
        if match:
            control = int(match[1], 16)
        if re.search(rf'call\s+0x{DDX_THUNK:x}\b', line):
            if offset is None or control is None:
                raise SurveyError('unrecognised Flights DDX binding')
            rows.append((offset, control))
            offset = control = None
    return rows


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
        match = re.fullmatch(r'mov\s+(\w+),DWORD PTR ds:0x2232([0-9a-f]+)', instruction)
        if match:
            registers[match[1]] = ('import', 0x10000 + int(match[2], 16))
            continue
        if instruction.startswith('call') or (instruction.startswith('jmp') and 'ds:0x2232' in instruction):
            direct = re.search(r'ds:0x2232([0-9a-f]+)', instruction)
            thunk = re.fullmatch(r'call\s+0x([0-9a-f]+)', instruction)
            if thunk and int(thunk[1], 16) in NOT_MEMBER_THUNKS:
                continue
            if thunk and int(thunk[1], 16) in THUNK_SLOTS:
                import_slot = THUNK_SLOTS[int(thunk[1], 16)]
            elif direct:
                import_slot = 0x10000 + int(direct[1], 16)
            else:
                value = registers.get(instruction.split()[-1])
                if not value or value[0] != 'import':
                    raise SurveyError(f'unrecognised Flights object call: {instruction}')
                import_slot = value[1]
            if import_slot in NOT_MEMBERS:
                continue
            target = registers.get('ecx')
            if not target or target[0] != 'object':
                raise SurveyError('unrecognised Flights member offset')
            calls.append((target[1], import_slot))
    return calls


def main():
    pe = Fs98Pe(IMAGE)
    if pe.uiBase != BASE or pe.linkage()['id'] != 0x80000002:
        raise SurveyError('expected the reference Flights image')
    tables = ['/* Generated by tools/gen_flights_tables.py; ABI metadata only. */']
    objects = ['/* Generated by tools/gen_flights_tables.py; member ABI metadata only. */']
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
        objects.append(f'static const struct fs98_flights_subobject s_controls_{name}[] = {{')
        for (offset, constructor), (_, destructor) in zip(constructors[1:], destructors[1:]):
            objects.append(f'    {{0x{offset:x}, 0x{constructor:x}, 0x{destructor:x}}},')
        if len(constructors) == 1:
            objects.append('    {0, 0, 0},   /* none */')
        objects.append('};')
        objects.append(f'enum {{ s_controls_count_{name} = {len(constructors) - 1} }};')
    objects.append('static const struct fs98_flights_class s_classes[] = {')
    for (name, size, template, descriptor, ctor, dtor, delete, factory, vtable, *_rest) in CLASSES:
        objects.append(f'    {{0x{size:x}, 0x{template:x}, 0x{descriptor:x}, 0x{ctor:x}, 0x{dtor:x}, '
                       f'0x{delete:x}, 0x{factory:x}, 0x{vtable:x}, '
                       f's_controls_count_{name}, s_controls_{name}, {bases[name]}}},')
    objects.append('};')
    Path('src/flights/flights_tables.inc').write_text('\n'.join(tables) + '\n')
    Path('src/flights/flights_objects_tables.inc').write_text('\n'.join(objects) + '\n')


if __name__ == '__main__':
    main()
