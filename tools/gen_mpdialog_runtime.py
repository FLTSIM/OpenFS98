#!/usr/bin/env python3
"""Extract MP_DIALOG's isolated dialog ABI metadata and resources.

The generated include contains addresses and import bindings only. No retail
machine-code bytes are copied into source or output.
"""
from pathlib import Path
import struct

from dialogmap import Fs98Pe, SurveyError, disassemble, read_message_maps


def resource_name(name):
    if isinstance(name, int):
        return struct.pack('<HH', 0xffff, name)
    return name.encode('utf-16-le') + b'\0\0'


def make_resource(pe, resource):
    resources = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0,
                                      0xffff, 0, 0, 0, 0, 0, 0))
    names = resource_name(resource['type']) + resource_name(resource['id'])
    names += b'\0' * (-len(names) % 4)
    resources += struct.pack('<II', resource['size'], 8 + len(names) + 16)
    resources += names + struct.pack('<IHHII', 0, 0x1030,
                                     resource['language'], 0, 0)
    resources += pe.read_bytes(pe.file_offset(resource['rva'], resource['size']),
                               resource['size'])
    resources += b'\0' * (-len(resources) % 4)
    return resources


def combine_resources(resources):
    if not resources:
        raise SurveyError('cannot emit an empty MP_DIALOG support resource')
    combined = bytearray(resources[0][:32])
    for resource in resources:
        combined += resource[32:]
    return bytes(combined)


def simsui_anchor(ui):
    export_rva, _ = ui.directory(0)
    export = ui.file_offset(export_rva, 40)
    base = ui.read_uint32(export + 16)
    name_count = ui.read_uint32(export + 24)
    names_rva = ui.read_uint32(export + 32)
    ordinals_rva = ui.read_uint32(export + 36)
    for index in range(name_count):
        name_rva = ui.read_uint32(ui.file_offset(names_rva + index * 4, 4))
        if ui.c_string(name_rva) == '?ui_new@@YAPAXI@Z':
            return base + ui.read_uint16(ui.file_offset(ordinals_rva + index * 2, 2))
    raise SurveyError('SIMSUI ui_new export missing')


def main():
    pe = Fs98Pe('orig/English/mpdialog.dll')
    if pe.uiBase != 0x22340000 or pe.linkage()['id'] != 0x8000000a:
        raise SurveyError('expected retail MP_DIALOG at 0x22340000')
    out_dir = Path('bin/mpdialog-diff')
    out_dir.mkdir(parents=True, exist_ok=True)
    imports = [item for item in pe.imports()
               if item['dll'].upper() in ('SIMSUI.DLL', 'MFC42.DLL')]
    import_by_iat = {item['iat_va']: index for index, item in enumerate(imports)}
    local = {
        0x2040: 'fs98_mpdialog_empty', 0x2050: 'fs98_mpdialog_hide',
        0x2060: 'fs98_mpdialog_show', 0x2cb0: 'fs98_mpdialog_empty_arg',
        0x3c30: 'fs98_mpdialog_exchange', 0x3d50: 'fs98_mpdialog_message_map',
        0x3aa0: 'fs98_mpdialog_delete',
        0x3d60: 'fs98_mpdialog_host_settings', 0x3d70: 'fs98_mpdialog_get_conditions',
        0x3d80: 'fs98_mpdialog_set_conditions', 0x3d90: 'fs98_mpdialog_on_ok',
        0x3dc0: 'fs98_mpdialog_on_cancel', 0x3de0: 'fs98_mpdialog_on_init',
        0x4340: 'fs98_mpdialog_help', 0x4360: 'fs98_mpdialog_context',
        0x4820: 'fs98_mpdialog_post_destroy', 0x40f0: 'fs98_mpdialog_update',
        0x42a0: 'fs98_mpdialog_save',
        0x4510: 'fs98_mpdialog_details_delete',
        0x4820: 'fs98_mpdialog_details_post_destroy',
        0x46a0: 'fs98_mpdialog_details_exchange',
        0x4810: 'fs98_mpdialog_details_message_map',
        0x48c0: 'fs98_mpdialog_details_on_init',
        0x4d10: 'fs98_mpdialog_details_update',
        0x4f00: 'fs98_mpdialog_details_capture',
        0x4f60: 'fs98_mpdialog_help',
        0x4f80: 'fs98_mpdialog_context',
        0x4fa0: 'fs98_mpdialog_details_on_refresh',
        0x4fd0: 'fs98_mpdialog_details_on_selection',
        0x5020: 'fs98_mpdialog_details_validation_result',
        0x50c0: 'fs98_mpdialog_details_validate',
        0x4850: 'fs98_mpdialog_details_on_ok',
        0x48a0: 'fs98_mpdialog_details_on_cancel',
        0x4830: 'fs98_mpdialog_details_on_remove',
        0x5120: 'fs98_mpdialog_details_on_ok',
        0x5130: 'fs98_mpdialog_details_on_cancel',
        0x5140: 'fs98_mpdialog_details_on_remove',
        0x5c90: 'fs98_mpdialog_flow_delete_intro',
        0x2040: 'fs98_mpdialog_empty',
        0x2050: 'fs98_mpdialog_hide', 0x2060: 'fs98_mpdialog_show',
        0x5da0: 'fs98_mpdialog_flow_exchange_intro',
        0x5e80: 'fs98_mpdialog_flow_message_map_intro',
        0x5f20: 'fs98_mpdialog_flow_on_init_intro',
        0x5f00: 'fs98_mpdialog_flow_cancel_intro',
        0x6120: 'fs98_mpdialog_flow_update_intro',
        0x6150: 'fs98_mpdialog_flow_capture_intro',
        0x5e90: 'fs98_mpdialog_flow_back_intro', 0x5ea0: 'fs98_mpdialog_flow_next_intro',
        0x6180: 'fs98_mpdialog_help', 0x61a0: 'fs98_mpdialog_context',
        0x5270: 'fs98_mpdialog_flow_delete_choice',
        0x53b0: 'fs98_mpdialog_flow_exchange_choice',
        0x54c0: 'fs98_mpdialog_flow_message_map_choice',
        0x55c0: 'fs98_mpdialog_flow_on_init_choice',
        0x5590: 'fs98_mpdialog_flow_next_choice',
        0x55a0: 'fs98_mpdialog_flow_cancel_choice',
        0x5ab0: 'fs98_mpdialog_flow_validate_choice',
        0x5a40: 'fs98_mpdialog_flow_error_choice',
        0x58a0: 'fs98_mpdialog_flow_update_choice',
        0x5920: 'fs98_mpdialog_flow_capture_choice',
        0x54d0: 'fs98_mpdialog_flow_back_choice',
        0x5b00: 'fs98_mpdialog_flow_route_standard',
        0x5b50: 'fs98_mpdialog_flow_route_online',
        0x5500: 'fs98_mpdialog_flow_next_choice',
        0x5a00: 'fs98_mpdialog_help', 0x5a20: 'fs98_mpdialog_context',
        0x21f0: 'fs98_mpdialog_flow_delete_host',
        0x2380: 'fs98_mpdialog_flow_exchange_host',
        0x24e0: 'fs98_mpdialog_flow_message_map_host',
        0x25e0: 'fs98_mpdialog_flow_on_init_host',
        0x25a0: 'fs98_mpdialog_flow_next_host',
        0x25b0: 'fs98_mpdialog_flow_cancel_host',
        0x2c40: 'fs98_mpdialog_flow_validate_host',
        0x2b60: 'fs98_mpdialog_flow_error_host',
        0x2990: 'fs98_mpdialog_flow_update_host',
        0x2aa0: 'fs98_mpdialog_flow_capture_host',
        0x24f0: 'fs98_mpdialog_flow_back_host',
        0x25d0: 'fs98_mpdialog_flow_settings_host',
        0x2520: 'fs98_mpdialog_flow_next_host',
        0x2b20: 'fs98_mpdialog_help', 0x2b40: 'fs98_mpdialog_context',
        0x1910: 'fs98_mpdialog_flow_delete_connection',
        0x1a10: 'fs98_mpdialog_flow_exchange_connection',
        0x1af0: 'fs98_mpdialog_flow_message_map_connection',
        0x1c40: 'fs98_mpdialog_flow_on_init_connection',
        0x1c00: 'fs98_mpdialog_flow_next_connection',
        0x1c20: 'fs98_mpdialog_flow_cancel_connection',
        0x1e60: 'fs98_mpdialog_flow_update_connection',
        0x1ef0: 'fs98_mpdialog_flow_capture_connection',
        0x1b00: 'fs98_mpdialog_flow_back_connection',
        0x1c10: 'fs98_mpdialog_flow_provider_connection',
        0x1b30: 'fs98_mpdialog_flow_next_connection',
        0x2010: 'fs98_mpdialog_flow_activate_connection',
        0x1f20: 'fs98_mpdialog_help', 0x1f40: 'fs98_mpdialog_context',
        0x2de0: 'fs98_mpdialog_flow_delete_browser',
        0x2f00: 'fs98_mpdialog_flow_exchange_browser',
        0x2ff0: 'fs98_mpdialog_flow_message_map_browser',
        0x3160: 'fs98_mpdialog_flow_on_init_browser',
        0x30d0: 'fs98_mpdialog_flow_next_browser',
        0x30e0: 'fs98_mpdialog_flow_cancel_browser',
        0x3420: 'fs98_mpdialog_flow_update_browser',
        0x3480: 'fs98_mpdialog_flow_capture_browser',
        0x3000: 'fs98_mpdialog_flow_back_browser',
        0x3110: 'fs98_mpdialog_flow_details_browser',
        0x3150: 'fs98_mpdialog_flow_settings_browser',
        0x3060: 'fs98_mpdialog_flow_next_browser',
        0x3850: 'fs98_mpdialog_flow_refresh_browser',
        0x37a0: 'fs98_mpdialog_flow_observer_browser',
        0x3510: 'fs98_mpdialog_flow_select_browser',
        0x3570: 'fs98_mpdialog_flow_select_type_browser',
        0x3780: 'fs98_mpdialog_flow_idle_browser',
        0x34d0: 'fs98_mpdialog_help', 0x34f0: 'fs98_mpdialog_context',
        0x38b0: 'fs98_mpdialog_flow_mouse_activate_browser',
        0x38d0: 'fs98_mpdialog_flow_show_browser',
        0x3910: 'fs98_mpdialog_flow_timer_browser',
    }
    lines = ['/* Generated by tools/gen_mpdialog_runtime.py; ABI metadata only. */',
             'static const struct fs98_mpdialog_import_binding s_import_bindings[] = {']
    for item in imports:
        name = item['name']
        ordinal = int(name[1:]) if name.startswith('#') else 0
        lines.append(f'    {{"{item["dll"]}", "{name}", {ordinal}, 0, 0x{item["iat_va"]:x}}},')
    lines.append('};')
    for symbol, table_va, slots in (('s_virtual_bindings', 0x223488e8, 62),
                                    ('s_details_virtual_bindings', 0x22348b60, 64)):
        # Both retail classes point PostNcDestroy at RVA 0x44820, but keep
        # each replacement class bound to its own equivalent wrapper. This
        # lets class-specific differential mutations stay isolated.
        virtual_local = local.copy()
        if table_va == 0x223488e8:
            virtual_local[0x4820] = 'fs98_mpdialog_post_destroy'
        lines.append(f'static const struct fs98_mpdialog_virtual_binding {symbol}[] = {{')
        for slot in range(slots):
            target = pe.va_uint32(table_va + slot * 4)
            rva = target - pe.uiBase
            if rva in virtual_local:
                binding = f'-1, (void *){virtual_local[rva]}'
            else:
                offset = pe.file_offset(rva, 6)
                if pe.read_bytes(offset, 2) != b'\xff\x25':
                    raise SurveyError(f'unmapped MP_DIALOG vtable target {target:#x} at slot {slot}')
                iat = pe.read_uint32(offset + 2)
                if iat not in import_by_iat:
                    raise SurveyError(f'vtable target {target:#x} is not an imported thunk')
                binding = f'{import_by_iat[iat]}, 0'
            lines.append(f'    {{{slot}, {binding}}},')
        lines.append('};')

    flow_vtables = [0x22348f88, 0x22348db0, 0x22348348, 0x22348120, 0x223486d8]
    for page_index, table_va in enumerate(flow_vtables):
        lines.append(f'static const struct fs98_mpdialog_virtual_binding s_flow_virtual_bindings_{page_index}[] = {{')
        for slot in range(62):
            target = pe.va_uint32(table_va + slot * 4)
            rva = target - pe.uiBase
            if slot == 59:
                # CSimsDialog::Close only marks the dialog HWND, optionally
                # tears down its auxiliary HWND, and hides it. Reproduce that
                # operation locally so the isolated non-modal harness does not
                # enter MFC's unsupported EndModalState path for a modeless page.
                binding = '-1, (void *)fs98_mpdialog_flow_close'
            elif rva in local:
                binding = f'-1, (void *){local[rva]}'
            else:
                offset = pe.file_offset(rva, 6)
                if pe.read_bytes(offset, 2) != b'\xff\x25':
                    raise SurveyError(f'unmapped flow vtable target {target:#x} at slot {slot}')
                iat = pe.read_uint32(offset + 2)
                if iat not in import_by_iat:
                    raise SurveyError(f'flow vtable target {target:#x} is not an imported thunk')
                binding = f'{import_by_iat[iat]}, 0'
            lines.append(f'    {{{slot}, {binding}}},')
        lines.append('};')

    maps = [item for item in read_message_maps(pe, disassemble(pe))
            if item['getter_va'] == 0x22343d50]
    if len(maps) != 1:
        raise SurveyError(f'expected one 0x110b message map, got {len(maps)}')
    all_maps = read_message_maps(pe, disassemble(pe))
    for getter, symbol in ((0x22343d50, 's_messages'),
                           (0x22344810, 's_details_messages'),
                           (0x22345e80, 's_flow_messages_intro'),
                           (0x223454c0, 's_flow_messages_choice'),
                           (0x223424e0, 's_flow_messages_host'),
                           (0x22341af0, 's_flow_messages_connection'),
                           (0x22342ff0, 's_flow_messages_browser')):
        selected = [item for item in all_maps if item['getter_va'] == getter]
        if len(selected) != 1:
            raise SurveyError(f'expected one message map for {getter:#x}, got {len(selected)}')
        lines.append(f'static const struct fs98_mpdialog_message_entry {symbol}[] = {{')
        for item in selected[0]['entries']:
            handler_rva = item['handler_va'] - pe.uiBase
            if handler_rva not in local:
                raise SurveyError(f'unmapped message handler {item["handler_va"]:#x}')
            handler = local[handler_rva]
            # The connection provider notification is an MFC virtual thunk:
            # it dispatches vtable +0xcc, which is the same Next handler used
            # by button 0x3e8. The map entry must retain that indirect route.
            if getter == 0x22341af0 and item['handler_va'] == 0x22341c10:
                handler = 'fs98_mpdialog_flow_provider_connection'
            if getter == 0x223454c0:
                handler = {
                    0x41c: 'fs98_mpdialog_flow_route_join',
                    0x41e: 'fs98_mpdialog_flow_route_host',
                    0x41d: 'fs98_mpdialog_flow_route_online',
                }.get(item['first_id'], handler)
            values = (item['message'], item['code'], item['first_id'], item['last_id'],
                      item['signature'])
            lines.append('    {' + ', '.join(f'0x{value:x}' for value in values) +
                         f', (void *){handler}' + '},')
        lines += ['    {0, 0, 0, 0, 0, 0}', '};']
    lines += ['static const struct fs98_mpdialog_dialog s_dialogs[] = {',
              '    {0x110a, fs98_mpdialog_flow_create, 2},',
              '    {0x110b, fs98_mpdialog_create, 0},',
              '    {0x110c, fs98_mpdialog_details_create, 0},',
              '    {0, 0, 0}', '};', '']
    Path('src/mpdialog/mpdialog_runtime_tables.inc').write_text('\n'.join(lines))

    ui = Fs98Pe('orig/SIMSUI.DLL')
    ordinal = simsui_anchor(ui)
    (out_dir / 'simsui-anchor.def').write_text(
        f'LIBRARY SIMSUI.DLL\nEXPORTS\n    fs98_mpdialog_simsui_anchor @{ordinal} NONAME\n')
    for resource_id in (0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b):
        template = next((item for item in pe.resources()
                         if item['type'] == 5 and item['id'] == resource_id), None)
        if template is None:
            raise SurveyError(f'MP_DIALOG RT_DIALOG {resource_id:#x} missing')
        suffix = {0x65: '110a-intro', 0x66: '110a-choice', 0x67: '110a-host',
                  0x68: '110a-connection', 0x69: '110a-browser',
                  0x6a: '110b', 0x6b: '110c'}[resource_id]
        (out_dir / f'mpdialog-0x{suffix}.res').write_bytes(make_resource(pe, template))
    support = [item for item in pe.resources() if item['type'] in (2, 6, 16)]
    if not support:
        raise SurveyError('MP_DIALOG bitmap/string/version support resources missing')
    (out_dir / 'mpdialog-flow-support.res').write_bytes(
        combine_resources([make_resource(pe, item) for item in support]))


if __name__ == '__main__':
    main()
