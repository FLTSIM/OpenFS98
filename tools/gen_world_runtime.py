#!/usr/bin/env python3
"""Extract partial World native bindings and build-only resources."""
from pathlib import Path
import struct
from dialogmap import Fs98Pe, SurveyError
from gen_views_runtime import resource_name


def main():
    pe = Fs98Pe('orig/English/World.DLL')
    if pe.uiBase != 0x223f0000 or pe.linkage()['id'] != 0x80000006:
        raise SurveyError('expected retail World')
    output_dir = Path('bin/world-diff')
    output_dir.mkdir(parents=True, exist_ok=True)
    imports = [binding for binding in pe.imports() if binding['dll'].upper() in ('MFC42.DLL', 'SIMSUI.DLL')]
    indices = {binding['iat_va']: index for index, binding in enumerate(imports)}
    local_bindings = {0xbc70: ('void *', ', uint32_t flags', 'return fs98_world_area_delete(object, flags);'),
        0x2d00: ('void', ', uint32_t unused', '(void)object; (void)unused;'),
        0x2d10: ('void', '', '(void)object;'),
        0x2d20: ('void', '', 'EnableWindow((HWND)WORLD_U32(object, 0x20), 0);'),
        0x2d30: ('void', '', 'EnableWindow((HWND)WORLD_U32(object, 0x20), 1);'),
        0xbed0: ('void *', '', '(void)object; return &s_message_map;'),
        0xbdc0: ('void', ', void *exchange', 'fs98_world_area_exchange(object, exchange);'),
        0xbef0: ('int', '', 'return fs98_world_area_init(object);'),
        0xc2e0: ('void', '', 'fs98_world_area_accept(object);'),
        0xc2c0: ('void', '', 'fs98_world_area_cancel(object);'),
        0xc610: ('int', ', uint32_t value', 'return fs98_world_area_validate(object, value);'),
        0xc660: ('int', ', uint32_t value', 'return fs98_world_area_error(object, value);'),
        0xc720: ('void', '', 'fs98_world_area_path_changed(object);'),
        0xc5d0: ('int', ', uint32_t unused, void *info', 'return fs98_world_area_help(object, unused, info);'),
        0xc5f0: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_area_context(object, window, unused);')}
    # 0x7240 consumes a nonclient handler's 12 bytes, but calls MFC Default()
    # with no stack arguments. Its callee (#2379) ends in plain ret.
    local_bindings.update({
        0x6400: ('void *', ', uint32_t flags', 'return fs98_world_booster_delete(object, flags);'),
        0x6730: ('void *', '', '(void)object; return &s_booster_message_map;'),
        0x65a0: ('void', ', void *exchange', 'fs98_world_booster_exchange(object, exchange);'),
        0x68b0: ('int', '', 'return fs98_world_booster_init(object);'),
        0x6740: ('void', '', 'fs98_world_booster_accept(object);'),
        0x7160: ('int', ', uint32_t value', 'return fs98_world_booster_validate(object, value);'),
        0x71a0: ('int', ', uint32_t value', 'return fs98_world_booster_error(object, value);'),
        0x6fd0: ('void', '', 'fs98_world_booster_path_changed(object);'),
        0x6d90: ('int', ', uint32_t unused, void *info', 'return fs98_world_booster_help(object, unused, info);'),
        0x6db0: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_booster_context(object, window, unused);'),
        0x7240: ('void', ', uint32_t hit, int x, int y', '(void)hit; (void)x; (void)y; ((void (TC *)(void *))world_slot(0x25584))(object);'),
        0xc940: ('void *', ', uint32_t flags', 'return fs98_world_priority_delete(object, flags);'),
        0xcb90: ('void *', '', '(void)object; return &s_priority_message_map;'),
        0xca80: ('void', ', void *exchange', 'fs98_world_priority_exchange(object, exchange);'),
        0xcba0: ('int', '', 'return fs98_world_priority_init(object);'),
        0xcf50: ('void', '', 'fs98_world_priority_accept(object);'),
        0xe4e0: ('void', '', 'fs98_world_priority_cancel(object);'),
        0xd390: ('int', ', uint32_t value', 'return fs98_world_priority_validate(object, value);'),
        0xd2f0: ('int', ', uint32_t value', 'return fs98_world_priority_error(object, value);'),
        0x3d30: ('void', '', 'fs98_world_priority_post_destroy(object);'),
        0xce30: ('void', '', 'fs98_world_priority_action(object, 0x10);'),
        0xce50: ('void', '', 'fs98_world_priority_action(object, 0xc);'),
        0xce70: ('void', '', 'fs98_world_priority_action(object, 8);'),
        0xce90: ('void', '', 'fs98_world_priority_selection(object);'),
        0xcf90: ('int', ', uint32_t unused, void *info', 'return fs98_world_priority_help(object, unused, info);'),
        0xcfb0: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_priority_context(object, window, unused);'),
    })
    local_bindings.update({
        0xdbf0: ('void *', ', uint32_t flags', 'return fs98_world_search_delete(object, flags);'),
        0xdeb0: ('void *', '', '(void)object; return &s_search_message_map;'),
        0xdd60: ('void', ', void *exchange', 'fs98_world_search_exchange(object, exchange);'),
        0xdec0: ('int', '', 'return fs98_world_search_init(object);'),
        0xe210: ('void', '', 'fs98_world_search_add_all(object);'),
        0xe270: ('void', '', 'fs98_world_search_add(object);'),
        0xe2d0: ('void', '', 'fs98_world_search_collect(object);'),
        0xe320: ('void', '', 'fs98_world_search_next(object);'),
        0xe370: ('void', '', 'fs98_world_search_next_dir(object);'),
        0xe3c0: ('void', '', 'fs98_world_search_scan(object);'),
        0xe430: ('void', '', 'fs98_world_search_drive(object);'),
        0xe460: ('void', '', 'fs98_world_search_layer(object);'),
        0xe470: ('void', '', 'fs98_world_search_layer(object);'),
        0xe500: ('void', '', 'fs98_world_search_cancel(object);'),
        0xe520: ('int', ', uint32_t unused, void *info', 'return fs98_world_search_help(object, unused, info);'),
        0xe540: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_search_context(object, window, unused);'),
    })
    local_bindings.update({
        0xa380: ('void *', ', uint32_t flags', 'return fs98_world_list_scalar_delete(object, flags);'),
        0xa5c0: ('void *', '', '(void)object; return &s_list_message_map;'),
        0xa4b0: ('void', ', void *exchange', 'fs98_world_list_exchange(object, exchange);'),
        0xa5d0: ('int', '', 'return fs98_world_list_init(object);'),
        0xab80: ('void', '', 'fs98_world_list_accept(object);'),
        0xaba0: ('void', '', 'fs98_world_list_cancel(object);'),
        0xabb0: ('void', '', 'fs98_world_list_action(object, 8);'),
        0xabd0: ('void', '', 'fs98_world_list_action(object, 0x18);'),
        0xabf0: ('void', '', 'fs98_world_list_delete(object);'),
        0xac30: ('void', '', 'fs98_world_list_action(object, 0xc);'),
        0xac50: ('void', '', 'fs98_world_list_action(object, 0x14);'),
        0xac70: ('void', '', 'fs98_world_list_selection(object);'),
        0xacd0: ('int', ', uint32_t unused, void *info', 'return fs98_world_list_help(object, unused, info);'),
        0xacf0: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_list_context(object, window, unused);'),
        0xad10: ('void', '', 'fs98_world_list_double_click(object);'),
    })
    local_bindings.update({
        0x92f0: ('void *', ', uint32_t flags', 'return fs98_world_cache_scalar_delete(object, flags);'),
        0x9550: ('void *', '', '(void)object; return &s_cache_message_map;'),
        0x9430: ('void', ', void *exchange', 'fs98_world_cache_exchange(object, exchange);'),
        0x9560: ('int', '', 'return fs98_world_cache_init(object);'),
        0x97d0: ('void', '', 'fs98_world_cache_accept(object);'),
        0x9810: ('void', '', 'fs98_world_cache_cancel(object);'),
        0x9830: ('void', '', 'fs98_world_cache_action(object, 8);'),
        0x9850: ('void', '', 'fs98_world_cache_delete(object);'),
        0x98c0: ('void', '', 'fs98_world_cache_action(object, 0xc);'),
        0x9910: ('void', '', 'fs98_world_cache_action(object, 0x14);'),
        0x9930: ('void', '', 'fs98_world_cache_selection(object);'),
        0x99b0: ('int', ', uint32_t unused, void *info', 'return fs98_world_cache_help(object, unused, info);'),
        0x99d0: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_cache_context(object, window, unused);'),
    })
    local_bindings.update({
        0xd500: ('void *', ', uint32_t flags', 'return fs98_world_files_scalar_delete(object, flags);'),
        0xd6b0: ('void *', '', '(void)object; return &s_files_message_map;'),
        0xd600: ('void', ', void *exchange', 'fs98_world_files_exchange(object, exchange);'),
        0xd6c0: ('int', '', 'return fs98_world_files_init(object);'),
        0xd8b0: ('void', '', 'fs98_world_files_accept(object);'),
        0xd890: ('void', '', 'fs98_world_files_cancel(object);'),
        0xd870: ('void', '', 'fs98_world_files_action(object, 8);'),
        0xd880: ('void', '', 'fs98_world_files_action(object, 0xc);'),
        0xda10: ('void', '', 'fs98_world_files_selection(object);'),
        0xda60: ('int', ', void *dc', 'return fs98_world_files_erase(object, dc);'),
        0xd8f0: ('int', ', uint32_t unused, void *info', 'return fs98_world_files_help(object, unused, info);'),
        0xd910: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_files_context(object, window, unused);'),
    })
    lines = ['/* Generated by tools/gen_world_runtime.py; native ABI metadata only. */']
    for rva, (return_type, arguments, body) in local_bindings.items():
        lines.append(f'static {return_type} TC world_rva_{rva:x}(void *object{arguments}) {{ {body} }}')
    lines += ['static const struct world_import_binding s_imports[] = {']
    for binding in imports:
        name = binding['name']
        ordinal = int(name[1:]) if name.startswith('#') else 0
        lines.append(f'    {{"{binding["dll"]}", "{name}", {ordinal}, 0x{binding["iat_va"] - pe.uiBase:x}}},')
    lines += ['};', 'static const struct world_virtual_binding s_virtuals[] = {']
    for table, vtable in enumerate((0x2240fc38, 0x2240fe50, 0x2240ee68, 0x22410320, 0x2240f750, 0x2240f338, 0x22410040)):
        for slot in range(60):
            va = pe.va_uint32(vtable + slot * 4)
            rva = va - pe.uiBase
            if rva in local_bindings:
                binding_spec = f'-1, (void *)world_rva_{rva:x}'
            else:
                offset = pe.file_offset(rva, 6)
                if pe.read_bytes(offset, 2) != b'\xff\x25':
                    raise SurveyError(f'unknown area virtual {va:#x}')
                iat = pe.read_uint32(offset + 2)
                binding_spec = f'{indices[iat]}, 0'
            lines.append(f'    {{{table}, {slot}, {binding_spec}}},')
    lines.append('};')
    for name, message_map in (('messages', 0x2240fbd0), ('priority_messages', 0x2240fda0), ('booster_messages', 0x2240ede8), ('search_messages', 0x224101c8), ('list_messages', 0x2240f658), ('cache_messages', 0x2240f270), ('files_messages', 0x2240ff90)):
        lines.append(f'static const struct world_message_entry s_{name}[] = {{')
        entries = pe.va_uint32(message_map + 4)
        for index in range(64):
            fields = [pe.va_uint32(entries + index * 24 + field * 4) for field in range(6)]
            if not any(fields):
                lines.append('    {0, 0, 0, 0, 0, 0}')
                break
            rva = fields[5] - pe.uiBase
            if rva not in local_bindings:
                raise SurveyError('unknown World handler')
            lines.append('    {' + ', '.join(hex(field) for field in fields[:5]) + f', (void *)world_rva_{rva:x}' + '},')
        else:
            raise SurveyError('unterminated World map')
        lines.append('};')
    lines += ['static const struct world_dialog s_dialogs[] = {']
    for dialog_record in pe.linkage()['dialogs']:
        if dialog_record['id'] in (0x13080007, 0x13080008):
            kind = 'add' if dialog_record['id'] == 0x13080008 else 'edit'
            lines.append(f'    {{0x{dialog_record["id"]:x}, fs98_world_booster_create_{kind}, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x1306:
            lines.append(f'    {{0x1306, fs98_world_files_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x13080006:
            lines.append(f'    {{0x13080006, fs98_world_cache_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x13080001:
            lines.append(f'    {{0x13080001, fs98_world_list_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x13080002:
            lines.append(f'    {{0x13080002, fs98_world_search_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x13080005:
            lines.append(f'    {{0x13080005, fs98_world_priority_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] in (0x13080003, 0x13080004):
            kind = 'add' if dialog_record['id'] == 0x13080003 else 'edit'
            lines.append(f'    {{0x{dialog_record["id"]:x}, fs98_world_area_create_{kind}, {dialog_record["flags"]}}},')
    lines += ['    {0, 0, 0}', '};']
    Path('src/world/world_runtime_tables.inc').write_text('\n'.join(lines) + '\n')
    ui = Fs98Pe('orig/SIMSUI.DLL')
    export = ui.file_offset(ui.directory(0)[0], 40)
    base = ui.read_uint32(export + 16)
    names, ordinals = ui.read_uint32(export + 32), ui.read_uint32(export + 36)
    for index in range(ui.read_uint32(export + 24)):
        name = ui.read_uint32(ui.file_offset(names + index * 4, 4))
        if ui.c_string(name) == '?ui_new@@YAPAXI@Z':
            ordinal = base + ui.read_uint16(ui.file_offset(ordinals + index * 2, 2))
            break
    else:
        raise SurveyError('SIMSUI anchor missing')
    (output_dir / 'simsui-anchor.def').write_text(f'LIBRARY SIMSUI.DLL\nEXPORTS\n    fs98_world_simsui_anchor @{ordinal} NONAME\n')
    resource_bytes = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0))
    for resource in pe.resources():
        if resource['type'] == 16:
            continue
        encoded_names = resource_name(resource['type']) + resource_name(resource['id'])
        encoded_names += b'\0' * (-len(encoded_names) % 4)
        resource_bytes += struct.pack('<II', resource['size'], 8 + len(encoded_names) + 16)
        resource_bytes += encoded_names + struct.pack('<IHHII', 0, 0x1030, resource['language'], 0, 0)
        resource_bytes += pe.read_bytes(pe.file_offset(resource['rva'], resource['size']), resource['size'])
        resource_bytes += b'\0' * (-len(resource_bytes) % 4)
    (output_dir / 'world.res').write_bytes(resource_bytes)


if __name__ == '__main__':
    main()
