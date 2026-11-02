#!/usr/bin/env python3
"""Extract Flights' vtable and message-map bindings and build-only Windows resources.

    python3 -B tools/gen_flights_runtime.py

No machine code or resource payload is written to tracked source. The include holds ABI
metadata (import names, which vtable slot is which import or which of our functions, the
message maps, the registrations); the .res and the SIMSUI ordinal anchor go under bin/.
"""
from pathlib import Path
import struct

from dialogmap import Fs98Pe, SurveyError

BASE = 0x22310000
# name, vtable, a tab page (as in gen_flights_tables.py's CLASSES), vtable slots: CSimsDialog's
# 60, and a 61st (+0xf0) where the class adds a virtual: the pages' OnInitPage, and Select
# Aircraft's populate (0x22311f40), which its OnInitDialog and selection call through it.
# After the others' last slot comes the next class's message map, never called.
CLASSES = (
    ('adventures', 0xe510, 0, 60), ('challenges', 0xe9a0, 0, 60), ('save', 0xed28, 0, 60),
    ('comms', 0xeba0, 0, 60), ('create', 0xe360, 0, 60), ('aircraft', 0xe118, 0, 61),
    ('modeless', 0xf8f8, 0, 60), ('opening', 0xfb40, 0, 60), ('flight_menu', 0xfd30, 0, 60),
    ('learning_menu', 0xff20, 0, 60), ('select', 0xef08, 0, 60), ('autopilot', 0xe7f0, 0, 60),
    ('navigation', 0xf2f8, 0, 60), ('nav_efis', 0xf158, 1, 61), ('nav_radios', 0xf518, 1, 61),
    ('nav_adf', 0xf6d8, 1, 61),
)
FACTORIES = {
    0x1130: 'create', 0x11c0: 'select', 0x1250: 'save', 0x12f0: 'adventures', 0x1380: 'challenges',
    0x1410: 'aircraft', 0x14a0: 'navigation', 0x1530: 'comms', 0x15c0: 'autopilot', 0x1650: 'opening',
    0x16e0: 'modeless', 0x1770: 'flight_menu', 0x1800: 'learning_menu',
}
# every function of ours a vtable slot or message map names, by retail RVA
LOCAL = {
        0x19a0: 'fs98_flights_aircraft_delete',
        0x1ab0: 'fs98_flights_aircraft_exchange',
        0x1b90: 'flights_map_aircraft',
        0x1ba0: 'fs98_flights_aircraft_cancel',
        0x1f40: 'fs98_flights_aircraft_populate',
        0x1be0: 'fs98_flights_aircraft_accept',
        0x1c40: 'fs98_flights_aircraft_frame',
        0x1c80: 'fs98_flights_aircraft_init',
        0x20a0: 'fs98_flights_aircraft_select',
        0x21c0: 'fs98_flights_aircraft_double_click',
        0x21e0: 'fs98_flights_aircraft_help',
        0x2200: 'fs98_flights_aircraft_context',
        0x2220: 'fs98_flights_aircraft_details',
        0x2310: 'fs98_flights_empty_arg',
        0x2320: 'fs98_flights_empty',
        0x2330: 'fs98_flights_disable',
        0x2340: 'fs98_flights_enable',
        0x2460: 'fs98_flights_create_delete',
        0x2590: 'fs98_flights_create_exchange',
        0x2690: 'flights_map_create',
        0x26a0: 'fs98_flights_post_destroy',
        0x26b0: 'fs98_flights_create_init',
        0x29d0: 'fs98_flights_create_save',
        0x29f0: 'fs98_flights_create_aircraft',
        0x2a20: 'fs98_flights_create_location',
        0x2a50: 'fs98_flights_create_weather',
        0x2a80: 'fs98_flights_create_time',
        0x2ab0: 'fs98_flights_create_cancel',
        0x2ae0: 'fs98_flights_create_accept',
        0x2b10: 'fs98_flights_create_help',
        0x2b30: 'fs98_flights_create_context',
        0x2b50: 'fs98_flights_nothing',
        0x2c00: 'fs98_flights_create_paint',
        0x2cd0: 'fs98_flights_adventures_delete',
        0x2db0: 'fs98_flights_adventures_exchange',
        0x2e40: 'flights_map_adventures',
        0x2e50: 'fs98_flights_adventures_accept',
        0x2ec0: 'fs98_flights_adventures_init',
        0x3040: 'fs98_flights_picker_select',
        0x3090: 'fs98_flights_adventures_double_click',
        0x30b0: 'fs98_flights_adventures_help',
        0x30d0: 'fs98_flights_adventures_context',
        0x3330: 'fs98_flights_autopilot_delete',
        0x34f0: 'fs98_flights_autopilot_exchange',
        0x36b0: 'flights_map_autopilot',
        0x36c0: 'fs98_flights_autopilot_init',
        0x3af0: 'fs98_flights_autopilot_cancel',
        0x3b10: 'fs98_flights_autopilot_accept',
        0x3b50: 'fs98_flights_autopilot_active',
        0x3ea0: 'fs98_flights_autopilot_level',
        0x3f30: 'fs98_flights_autopilot_airspeed',
        0x3ff0: 'fs98_flights_autopilot_mach',
        0x40c0: 'fs98_flights_autopilot_throttle',
        0x4230: 'fs98_flights_autopilot_takeoff',
        0x42f0: 'fs98_flights_autopilot_heading',
        0x43c0: 'fs98_flights_autopilot_altitude',
        0x44e0: 'fs98_flights_autopilot_attitude',
        0x45f0: 'fs98_flights_autopilot_help',
        0x4610: 'fs98_flights_autopilot_context',
        0x4db0: 'fs98_flights_autopilot_validate',
        0x4e90: 'fs98_flights_autopilot_error',
        0x50c0: 'fs98_flights_challenges_delete',
        0x51a0: 'fs98_flights_challenges_exchange',
        0x5230: 'flights_map_challenges',
        0x5240: 'fs98_flights_picker_cancel',
        0x5260: 'fs98_flights_challenges_accept',
        0x52e0: 'fs98_flights_challenges_init',
        0x5460: 'fs98_flights_challenges_double_click',
        0x5480: 'fs98_flights_challenges_help',
        0x54a0: 'fs98_flights_challenges_context',
        0x56d0: 'fs98_flights_comms_delete',
        0x5810: 'fs98_flights_comms_exchange',
        0x5920: 'flights_map_comms',
        0x5930: 'fs98_flights_comms_init',
        0x5b80: 'fs98_flights_comms_land',
        0x5bc0: 'fs98_flights_comms_none',
        0x5bf0: 'fs98_flights_comms_take_off',
        0x5c30: 'fs98_flights_comms_cancel',
        0x5c50: 'fs98_flights_comms_accept',
        0x5c90: 'fs98_flights_comms_help',
        0x5cb0: 'fs98_flights_comms_context',
        0x5e90: 'fs98_flights_comms_validate',
        0x5f20: 'fs98_flights_comms_error',
        0x6050: 'fs98_flights_save_delete',
        0x6140: 'fs98_flights_save_exchange',
        0x61e0: 'flights_map_save',
        0x61f0: 'fs98_flights_save_init',
        0x6390: 'fs98_flights_save_accept',
        0x63e0: 'fs98_flights_save_cancel',
        0x6400: 'fs98_flights_save_help',
        0x6420: 'fs98_flights_save_context',
        0x6500: 'fs98_flights_save_validate',
        0x6540: 'fs98_flights_save_error',
        0x6740: 'fs98_flights_select_delete',
        0x6850: 'fs98_flights_select_exchange',
        0x6910: 'flights_map_select',
        0x6920: 'fs98_flights_select_help',
        0x6940: 'fs98_flights_select_context',
        0x6960: 'fs98_flights_select_init',
        0x6b60: 'fs98_flights_select_cancel',
        0x6b80: 'fs98_flights_select_accept',
        0x6bd0: 'fs98_flights_select_remove',
        0x6c40: 'fs98_flights_select_choose',
        0x6d30: 'fs98_flights_select_double_click',
        0x6e30: 'fs98_flights_select_validate',
        0x6f50: 'fs98_flights_select_error',
        0x7190: 'fs98_flights_nav_efis_delete',
        0x72f0: 'fs98_flights_nav_efis_exchange',
        0x7430: 'flights_map_nav_efis',
        0x7440: 'fs98_flights_nav_efis_help',
        0x7460: 'fs98_flights_nav_efis_context',
        0x7480: 'fs98_flights_nav_efis_init',
        0x76b0: 'fs98_flights_nav_efis_four',
        0x76f0: 'fs98_flights_nav_efis_two',
        0x7750: 'fs98_flights_nav_efis_first',
        0x7760: 'fs98_flights_nav_efis_second',
        0x7770: 'fs98_flights_nav_efis_enable',
        0x7980: 'fs98_flights_nav_efis_validate',
        0x7af0: 'fs98_flights_nav_efis_error',
        0x7d70: 'fs98_flights_nav_efis_message',
        0x7e40: 'fs98_flights_navigation_delete',
        0x7f20: 'fs98_flights_navigation_validate',
        0x7f40: 'fs98_flights_navigation_exchange',
        0x7fd0: 'flights_dialog_base_map',
        0x7fe0: 'flights_map_navigation',
        0x7ff0: 'fs98_flights_navigation_init',
        0x8350: 'fs98_flights_navigation_close',
        0x8370: 'fs98_flights_navigation_cancel',
        0x8390: 'fs98_flights_navigation_accept',
        0x8420: 'fs98_flights_navigation_help',
        0x8440: 'fs98_flights_navigation_context',
        0x8740: 'fs98_flights_nav_radios_delete',
        0x8940: 'fs98_flights_nav_radios_exchange',
        0x8b60: 'flights_page_base_map',
        0x8b70: 'flights_map_nav_radios',
        0x8b80: 'fs98_flights_nav_radios_init',
        0x8de0: 'fs98_flights_nav_radios_help',
        0x8e00: 'fs98_flights_nav_radios_context',
        0x8e20: 'fs98_flights_nav_radios_validate',
        0x8f90: 'fs98_flights_nav_radios_error',
        0x95c0: 'fs98_flights_nav_adf_delete',
        0x96f0: 'fs98_flights_nav_adf_exchange',
        0x9800: 'flights_map_nav_adf',
        0x9810: 'fs98_flights_nav_adf_init',
        0x9970: 'fs98_flights_nav_adf_help',
        0x9990: 'fs98_flights_nav_adf_context',
        0x99b0: 'fs98_flights_nav_adf_validate',
        0x9a40: 'fs98_flights_nav_adf_error',
        0x9d30: 'fs98_flights_modeless_delete',
        0x9d60: 'fs98_flights_modeless_exchange',
        0x9d70: 'flights_map_modeless',
        0x9d80: 'fs98_flights_modeless_init',
        0x9db0: 'fs98_flights_modeless_check',
        0x9de0: 'fs98_flights_modeless_palette',
        0x9e00: 'fs98_flights_opening_help',
        0x9e20: 'fs98_flights_opening_context',
        0x9f20: 'fs98_flights_opening_delete',
        0xa000: 'fs98_flights_opening_exchange',
        0xa0b0: 'flights_map_opening',
        0xa0c0: 'fs98_flights_opening_init',
        0xa420: 'fs98_flights_opening_multiplayer',
        0xa500: 'fs98_flights_opening_lessons',
        0xa5e0: 'fs98_flights_opening_fly',
        0xa620: 'fs98_flights_opening_quit',
        0xa660: 'fs98_flights_opening_more',
        0xa740: 'fs98_flights_opening_new_flight',
        0xa870: 'fs98_flights_opening_fly',
        0xa880: 'fs98_flights_opening_hover',
        0xaa90: 'fs98_flights_flight_menu_delete',
        0xab60: 'fs98_flights_flight_menu_exchange',
        0xabe0: 'flights_map_flight_menu',
        0xabf0: 'fs98_flights_flight_menu_init',
        0xaea0: 'fs98_flights_flight_menu_dual',
        0xaf60: 'fs98_flights_flight_menu_video',
        0xb020: 'fs98_flights_flight_menu_review',
        0xb0e0: 'fs98_flights_flight_menu_help',
        0xb100: 'fs98_flights_flight_menu_context',
        0xb120: 'fs98_flights_flight_menu_hover',
        0xb300: 'fs98_flights_learning_menu_delete',
        0xb3d0: 'fs98_flights_learning_menu_exchange',
        0xb450: 'flights_map_learning_menu',
        0xb460: 'fs98_flights_learning_menu_init',
        0xb710: 'fs98_flights_learning_menu_lessons',
        0xb7c0: 'fs98_flights_learning_menu_about',
        0xb870: 'fs98_flights_learning_menu_quit',
        0xb920: 'fs98_flights_menu_back',
        0xb940: 'fs98_flights_learning_menu_help',
        0xb960: 'fs98_flights_learning_menu_context',
        0xb980: 'fs98_flights_learning_menu_hover',
}


def resource_name(name):
    if isinstance(name, int):
        if name > 0xffff:
            raise SurveyError('resource ordinal exceeds WORD')
        return struct.pack('<HH', 0xffff, name)
    return name.encode('utf-16-le') + b'\0\0'


def thunk_slot(pe, rva):
    """The import slot a `jmp [slot]` thunk goes through, or None."""
    offset = pe.file_offset(rva, 6)
    if pe.read_bytes(offset, 2) != b'\xff\x25':
        return None
    return pe.read_uint32(offset + 2)


def main():
    pe = Fs98Pe('orig/English/Flights.DLL')
    if pe.uiBase != BASE or pe.linkage()['id'] != 0x80000002:
        raise SurveyError('expected the reference Flights image at 0x22310000')
    directory = Path('bin/flights-diff')
    directory.mkdir(parents=True, exist_ok=True)
    imports = [binding for binding in pe.imports()
               if binding['dll'].upper() in ('SIMSUI.DLL', 'MFC42.DLL')]
    import_indices = {binding['iat_va']: index for index, binding in enumerate(imports)}
    lines = ['/* Generated by tools/gen_flights_runtime.py; ABI metadata only. */',
             'static const struct flights_import_binding s_import_bindings[] = {']
    for binding in imports:
        name = binding['name']
        ordinal = int(name[1:]) if name.startswith('#') else 0
        lines.append(f'    {{"{binding["dll"]}", "{name}", {ordinal}, 0x{binding["iat_va"] - BASE:x}}},')
    # The word after each vtable must not be one more slot: either a message map follows
    # (its first word, a getter, is code but the struct is not a slot) or it is no code.
    maps = set()
    for name, vtable, page, slots in CLASSES:
        getter = pe.va_uint32(BASE + vtable + 0x30) - BASE
        maps.add(pe.read_uint32(pe.file_offset(getter, 6) + 1) - BASE)
    for name, vtable, page, slots in CLASSES:
        after = vtable + slots * 4
        word = pe.va_uint32(BASE + after)
        if after not in maps and pe.is_code(word):
            raise SurveyError(f'{name}: the word after its {slots} slots is code ({word - BASE:#x}): one more slot?')
    lines += ['};', 'static const struct flights_virtual_binding s_virtual_bindings[] = {']
    for table, (name, vtable, page, slots) in enumerate(CLASSES):
        for slot in range(slots):
            function_rva = pe.va_uint32(BASE + vtable + slot * 4) - BASE
            if function_rva in LOCAL:
                lines.append(f'    {{{table}, {slot}, -1, (void *){LOCAL[function_rva]}}},')
                continue
            iat = thunk_slot(pe, function_rva)
            if iat not in import_indices:
                raise SurveyError(f'{name} +{slot * 4:#x}: unrecognised vtable target {function_rva:#x}')
            lines.append(f'    {{{table}, {slot}, {import_indices[iat]}, 0}},')
    lines.append('};')
    for name, vtable, page, _ in CLASSES:
        getter = pe.va_uint32(BASE + vtable + 0x30) - BASE
        offset = pe.file_offset(getter, 6)
        if pe.read_bytes(offset, 1) != b'\xb8' or pe.read_bytes(offset + 5, 1) != b'\xc3':
            raise SurveyError(f'{name}: message map getter moved')
        message_map = pe.read_uint32(offset + 1)
        base_getter = pe.va_uint32(message_map) - BASE
        if base_getter != (0x8b60 if page else 0x7fd0):
            raise SurveyError(f'{name}: unexpected base message map')
        lines.append(f'static const struct flights_message_entry s_{name}_messages[] = {{')
        entries = pe.va_uint32(message_map + 4)
        for index in range(64):
            fields = [pe.va_uint32(entries + index * 24 + field * 4) for field in range(6)]
            if not any(fields):
                lines.append('    {0, 0, 0, 0, 0, 0}')
                break
            handler = fields[5] - BASE
            if handler not in LOCAL:
                raise SurveyError(f'{name}: unrecognised message handler {handler:#x}')
            field_literals = ', '.join(f'0x{value:x}' for value in fields[:5])
            lines.append(f'    {{{field_literals}, (void *){LOCAL[handler]}}},')
        else:
            raise SurveyError(f'{name}: unterminated message map')
        lines.append('};')
    lines.append('static const struct flights_message_entry *const s_messages[] = {')
    lines += [f'    s_{name}_messages,' for name, _, _, _ in CLASSES] + ['};']
    lines.append('static const uint8_t s_pages[] = {' + ', '.join(str(page) for _, _, page, _ in CLASSES) + '};')
    lines.append('static const struct flights_dialog s_dialogs[] = {')
    for dialog in pe.linkage()['dialogs']:
        factory = FACTORIES[dialog['create_va'] - BASE]
        lines.append(f'    {{0x{dialog["id"]:x}, fs98_flights_{factory}_create, {dialog["flags"]}}},')
    lines += ['    {0, 0, 0}', '};', '']
    Path('src/flights/flights_runtime_tables.inc').write_text('\n'.join(lines))

    # A PE import by ordinal makes the loader resolve SIMSUI and its MFC before our DllMain.
    ui = Fs98Pe('orig/SIMSUI.DLL')
    export = ui.file_offset(ui.directory(0)[0], 40)
    base = ui.read_uint32(export + 16)
    names_rva = ui.read_uint32(export + 32)
    ordinals_rva = ui.read_uint32(export + 36)
    anchor_ordinal = None
    for index in range(ui.read_uint32(export + 24)):
        name_rva = ui.read_uint32(ui.file_offset(names_rva + index * 4, 4))
        if ui.c_string(name_rva) == '?ui_new@@YAPAXI@Z':
            anchor_ordinal = base + ui.read_uint16(ui.file_offset(ordinals_rva + index * 2, 2))
    if anchor_ordinal is None:
        raise SurveyError('SIMSUI ui_new export missing')
    (directory / 'simsui-anchor.def').write_text(
        f'LIBRARY SIMSUI.DLL\nEXPORTS\n    fs98_flights_simsui_anchor @{anchor_ordinal} NONAME\n')

    resources = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0))
    for resource in pe.resources():
        if resource['type'] == 16:
            continue  # Do not copy the retail company's version resource.
        resource_names = resource_name(resource['type']) + resource_name(resource['id'])
        resource_names += b'\0' * (-len(resource_names) % 4)
        header = struct.pack('<II', resource['size'], 8 + len(resource_names) + 16)
        header += resource_names + struct.pack('<IHHII', 0, 0x1030, resource['language'], 0, 0)
        resources += header
        resources += pe.read_bytes(pe.file_offset(resource['rva'], resource['size']), resource['size'])
        resources += b'\0' * (-len(resources) % 4)
    (directory / 'flights.res').write_bytes(resources)


if __name__ == '__main__':
    main()
