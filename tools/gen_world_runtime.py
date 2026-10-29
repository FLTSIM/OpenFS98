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
    imports = [binding for binding in pe.imports() if binding['dll'].upper() in ('MFC42.DLL', 'SIMSUI.DLL', 'USER32.DLL', 'GDI32.DLL', 'KERNEL32.DLL', 'COMCTL32.DLL', 'MSVCRT.DLL')]
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
    local_bindings.update({
        0x139c0: ('void *', ', uint32_t flags', 'return fs98_world_position_scalar_delete(object, flags);'),
        0x13e00: ('void *', '', '(void)object; return &s_position_message_map;'),
        0x13be0: ('void', ', void *exchange', 'fs98_world_position_exchange(object, exchange);'),
        0x13e10: ('int', '', 'return fs98_world_position_init(object);'),
        0x14a40: ('void', '', 'fs98_world_position_accept(object);'),
        0x144e0: ('int', ', uint32_t unused', 'return fs98_world_position_validate(object, unused);'),
        0x145a0: ('int', ', uint32_t unused', 'return fs98_world_position_error(object, unused);'),
        0x143f0: ('void', '', 'fs98_world_position_populate(object);'),
        0x14950: ('void', '', 'fs98_world_position_collect(object);'),
        0x14770: ('void', '', 'fs98_world_position_reload(object);'),
        0x14790: ('void', '', 'fs98_world_position_revert(object);'),
        0x14a80: ('void', '', 'fs98_world_position_facilities(object);'),
        0x14a90: ('void', '', 'fs98_world_position_changed(object, 0x78);'),
        0x14ab0: ('void', '', 'fs98_world_position_changed(object, 0x90);'),
        0x14ad0: ('void', '', 'fs98_world_position_changed(object, 0xa8);'),
        0x14af0: ('void', '', 'fs98_world_position_changed(object, 0x34);'),
        0x14b10: ('void', '', 'fs98_world_position_changed(object, 0x4c);'),
        0x143b0: ('int', ', uint32_t unused, void *info', 'return fs98_world_position_help(object, unused, info);'),
        0x143d0: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_position_context(object, window, unused);'),
    })
    local_bindings.update({
        0x16ff0: ('void *', ', uint32_t flags', 'return fs98_world_facilities_scalar_delete(object, flags);'),
        0x171b0: ('void *', '', '(void)object; return &s_facilities_message_map;'),
        0x170f0: ('void', ', void *exchange', 'fs98_world_facilities_exchange(object, exchange);'),
        0x18c80: ('int', '', 'return fs98_world_facilities_init(object);'),
        0x196c0: ('void', '', 'fs98_world_facilities_accept(object);'),
        0x19670: ('void', '', 'fs98_world_facilities_close(object);'),
        0x19230: ('int', ', uint32_t command, uint32_t parameter', 'return fs98_world_facilities_command(object, command, parameter);'),
        0x193f0: ('int', ', void *window, uint32_t hit, uint32_t message', '(void)object; (void)window; (void)hit; (void)message; return 1;'),
        0x19400: ('void', ', uint32_t hit, int x, int y', 'fs98_world_facilities_mouse(object, hit, x, y, 0);'),
        0x19530: ('void', ', uint32_t hit, int x, int y', 'fs98_world_facilities_mouse(object, hit, x, y, 1);'),
        0x195e0: ('int', ', uint32_t unused, void *info', 'return fs98_world_facilities_help(object, unused, info);'),
        0x19600: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_facilities_context(object, window, unused);'),
        0x19750: ('void', '', 'fs98_world_facilities_home(object);'),
        0x19770: ('void', '', 'fs98_world_facilities_refresh(object);'),
        0x197b0: ('void', '', 'fs98_world_facilities_back(object);'),
        0x19810: ('void *', ', uint32_t flags', 'return fs98_world_facilities_control_delete(object, flags, 0);'),
        0x19830: ('void *', ', uint32_t flags', 'return fs98_world_facilities_control_delete(object, flags, 1);'),
    })
    local_bindings.update({
        0x14d50: ('void *', ', uint32_t flags', 'return fs98_world_airport_scalar_delete(object, flags);'),
        0x15180: ('void *', '', '(void)object; return &s_airport_message_map;'),
        0x14f60: ('void', ', void *exchange', 'fs98_world_airport_exchange(object, exchange);'),
        0x15190: ('int', '', 'return fs98_world_airport_init(object);'),
        0x16680: ('void', '', 'fs98_world_airport_accept(object);'),
        0x16700: ('void', '', 'fs98_world_airport_cancel(object);'),
        0x15a70: ('int', ', uint32_t unused', '(void)object; (void)unused; return 0;'),
        0x155a0: ('int', ', uint32_t unused, void *info', 'return fs98_world_airport_help(object, unused, info);'),
        0x155c0: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_airport_context(object, window, unused);'),
        0x15ea0: ('void', '', 'fs98_world_airport_refresh(object);'),
        0x15de0: ('void', '', 'fs98_world_airport_airport(object);'),
        0x15e70: ('void', '', 'fs98_world_airport_double_click(object);'),
        0x15ec0: ('void', '', 'fs98_world_airport_airport_edit(object);'),
        0x163a0: ('void', '', 'fs98_world_airport_name_edit(object);'),
        0x16550: ('void', '', 'fs98_world_airport_name(object);'),
        0x99f0: ('void', '', '(void)object;'),
        0x15a80: ('void', '', 'fs98_world_airport_mode_button(object, 0);'),
        0x15aa0: ('void', '', 'fs98_world_airport_mode_button(object, 1);'),
        0x15ac0: ('void', '', 'fs98_world_airport_mode_button(object, 2);'),
        0x15ae0: ('void', '', 'fs98_world_airport_place(object, 0);'),
        0x15b50: ('void', '', 'fs98_world_airport_place(object, 1);'),
        0x15bc0: ('void', '', 'fs98_world_airport_place(object, 2);'),
        0x15c30: ('void', '', 'fs98_world_airport_place(object, 3);'),
        0x15ca0: ('void', '', 'fs98_world_airport_place(object, 4);'),
        0x16070: ('void', '', 'fs98_world_airport_dropdown(object, 0, 0);'),
        0x16090: ('void', '', 'fs98_world_airport_dropdown(object, 1, 0);'),
        0x160b0: ('void', '', 'fs98_world_airport_dropdown(object, 2, 0);'),
        0x160d0: ('void', '', 'fs98_world_airport_dropdown(object, 3, 0);'),
        0x16140: ('void', '', 'fs98_world_airport_dropdown(object, 0, 1);'),
        0x161d0: ('void', '', 'fs98_world_airport_dropdown(object, 1, 1);'),
        0x16250: ('void', '', 'fs98_world_airport_dropdown(object, 2, 1);'),
        0x162d0: ('void', '', 'fs98_world_airport_dropdown(object, 3, 1);'),
    })
    local_bindings.update({
        0xae30: ('void *', ', uint32_t flags', 'return fs98_world_complexity_scalar_delete(object, flags);'),
        0xafc0: ('void *', '', '(void)object; return &s_complexity_message_map;'),
        0xaf20: ('void', ', void *exchange', 'fs98_world_complexity_exchange(object, exchange);'),
        0xafd0: ('int', '', 'return fs98_world_complexity_init(object);'),
        0xb210: ('void', '', 'fs98_world_complexity_accept(object);'),
        0xb1f0: ('void', '', 'fs98_world_complexity_cancel(object);'),
        0xb250: ('int', ', uint32_t unused, void *info', 'return fs98_world_complexity_help(object, unused, info);'),
        0xb270: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_complexity_context(object, window, unused);'),
        0xb8b0: ('void', '', 'fs98_world_complexity_close(object);'),
        0x2700: ('void *', ', uint32_t flags', 'return fs98_world_complexity_notify_delete(object, flags);'),
    })
    local_bindings.update({
        0xb460: ('void *', ', uint32_t flags', 'return fs98_world_dynamic_scalar_delete(object, flags);'),
        0xb600: ('void *', '', '(void)object; return &s_dynamic_message_map;'),
        0xb550: ('void', ', void *exchange', 'fs98_world_dynamic_exchange(object, exchange);'),
        0xb610: ('int', '', 'return fs98_world_dynamic_init(object);'),
        0xb870: ('void', '', 'fs98_world_dynamic_accept(object);'),
        0xb850: ('void', '', 'fs98_world_dynamic_cancel(object);'),
        0xb8d0: ('int', ', uint32_t unused, void *info', 'return fs98_world_dynamic_help(object, unused, info);'),
        0xb8f0: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_dynamic_context(object, window, unused);'),
    })
    local_bindings.update({
        0x38e0: ('void *', ', uint32_t flags', 'return fs98_world_copy_scalar_delete(object, flags);'),
        0x3b10: ('void *', '', '(void)object; return &s_copy_message_map;'),
        0x3a10: ('void', ', void *exchange', 'fs98_world_copy_exchange(object, exchange);'),
        0x3b20: ('int', '', 'return fs98_world_copy_init(object);'),
        0x3d60: ('void', '', 'fs98_world_copy_accept(object);'),
        0x3d40: ('void', '', 'fs98_world_copy_cancel(object);'),
        0x3dc0: ('int', ', uint32_t unused, void *info', 'return fs98_world_copy_help(object, unused, info);'),
        0x3de0: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_copy_context(object, window, unused);'),
    })
    local_bindings.update({
        0x2e50: ('void *', ', uint32_t flags', 'return fs98_world_weather_scalar_delete(object, flags);'),
        0x3070: ('void *', '', '(void)object; return &s_weather_message_map;'),
        0x2f80: ('void', ', void *exchange', 'fs98_world_weather_exchange(object, exchange);'),
        0x3080: ('int', '', 'return fs98_world_weather_init(object);'),
        0x3490: ('void', '', 'fs98_world_weather_accept(object);'),
        0x34d0: ('void', '', 'fs98_world_weather_cancel(object);'),
        0x33e0: ('void', ', uint32_t id, void *measure', '(void)fs98_world_weather_measure(object, id, measure);'),
        0x3400: ('int', ', uint32_t unused, void *info', 'return fs98_world_weather_help(object, unused, info);'),
        0x3420: ('int', ', uint32_t window, uint32_t unused', 'return fs98_world_weather_context(object, window, unused);'),
        0x3730: ('void', '', 'fs98_world_weather_changed(object);'),
    })
    local_bindings.update({
        0x5bd0: ('void *', ', uint32_t flags', 'return fs98_world_time_delete(object, flags, 0);'),
        0x4270: ('void *', ', uint32_t flags', 'return fs98_world_time_delete(object, flags, 1);'),
        0x4eb0: ('void *', ', uint32_t flags', 'return fs98_world_time_delete(object, flags, 2);'),
        0x5d50: ('void *', '', '(void)object; return &s_time_message_map;'),
        0x4510: ('void *', '', '(void)object; return &s_time_season_message_map;'),
        0x51d0: ('void *', '', '(void)object; return &s_time_clock_message_map;'),
        0x5cc0: ('void', ', void *exchange', 'fs98_world_time_exchange(object, exchange, 0);'),
        0x43d0: ('void', ', void *exchange', 'fs98_world_time_exchange(object, exchange, 1);'),
        0x5040: ('void', ', void *exchange', 'fs98_world_time_exchange(object, exchange, 2);'),
        0x5d60: ('int', '', 'return fs98_world_time_init(object);'),
        0x4520: ('void', '', 'fs98_world_time_page_init(object, 1);'),
        0x51e0: ('void', '', 'fs98_world_time_page_init(object, 2);'),
        0x6080: ('void', '', 'fs98_world_time_accept(object);'),
        0x6060: ('void', '', 'fs98_world_time_cancel(object);'),
        0x6100: ('void', '', 'fs98_world_time_close(object);'),
        0x5ca0: ('int', ', uint32_t unused', 'return fs98_world_time_frame_validate(object, unused);'),
        0x4870: ('int', ', uint32_t id', 'return fs98_world_time_validate(object, id, 1);'),
        0x55e0: ('int', ', uint32_t id', 'return fs98_world_time_validate(object, id, 2);'),
        0x4a20: ('int', ', uint32_t id', 'return fs98_world_time_error(object, id, 1);'),
        0x5870: ('int', ', uint32_t id', 'return fs98_world_time_error(object, id, 2);'),
        0x4af0: ('void', '', 'fs98_world_time_radio(object, 1, 1);'),
        0x4bc0: ('void', '', 'fs98_world_time_radio(object, 0, 1);'),
        0x5980: ('void', '', 'fs98_world_time_radio(object, 1, 2);'),
        0x5a20: ('void', '', 'fs98_world_time_radio(object, 0, 2);'),
        0x4ca0: ('void', '', 'fs98_world_time_selection(object, 1);'),
        0x4ce0: ('void', '', 'fs98_world_time_selection(object, 1);'),
        0x5ac0: ('void', '', 'fs98_world_time_selection(object, 2);'),
        0x5b00: ('void', '', 'fs98_world_time_selection(object, 2);'),
        0x4cf0: ('int', ', uint32_t unused, uint32_t parameter', '(void)unused; (void)parameter; return fs98_world_time_reload(object, 1);'),
        0x5b10: ('int', ', uint32_t unused, uint32_t parameter', '(void)unused; (void)parameter; return fs98_world_time_reload(object, 2);'),
        0x4c90: ('void', '', '((void (TC *)(void *))world_slot(0x25594))(object);'),
        0x6130: ('int', ', uint32_t unused, void *help', '(void)unused; return fs98_world_time_help(object, WORLD_U32(help, 0xc), 12, 0);'),
        0x4750: ('int', ', uint32_t unused, void *help', '(void)unused; return fs98_world_time_help(object, WORLD_U32(help, 0xc), 12, 1);'),
        0x5490: ('int', ', uint32_t unused, void *help', '(void)unused; return fs98_world_time_help(object, WORLD_U32(help, 0xc), 12, 2);'),
        0x6150: ('int', ', uint32_t window, uint32_t unused', '(void)unused; fs98_world_time_help(object, window, 10, 0); return 1;'),
        0x4770: ('int', ', uint32_t window, uint32_t unused', '(void)unused; fs98_world_time_help(object, window, 10, 1); return 1;'),
        0x54b0: ('int', ', uint32_t window, uint32_t unused', '(void)unused; fs98_world_time_help(object, window, 10, 2); return 1;'),
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
    for table, vtable in enumerate((0x2240fc38, 0x2240fe50, 0x2240ee68, 0x22410320, 0x2240f750, 0x2240f338, 0x22410040, 0x22410f30, 0x224116b8, 0x224117a8, 0x22411868, 0x22411468, 0x2240f8d8, 0x2240e1e8, 0x2240fa68, 0x2240e540, 0x2240e390, 0x2240ec48, 0x2240e7a0, 0x2240eac8)):
        for slot in range(48 if vtable in (0x224117a8, 0x22411868, 0x2240e1e8) else 62 if vtable == 0x22410f30 else 61 if vtable in (0x2240e7a0, 0x2240eac8) else 60):
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
    for name, message_map in (('messages', 0x2240fbd0), ('priority_messages', 0x2240fda0), ('booster_messages', 0x2240ede8), ('search_messages', 0x224101c8), ('list_messages', 0x2240f658), ('cache_messages', 0x2240f270), ('files_messages', 0x2240ff90), ('position_messages', 0x22410e20), ('facilities_messages', 0x224115a8), ('airport_messages', 0x224111d8), ('complexity_messages', 0x2240f888), ('dynamic_messages', 0x2240fa18), ('copy_messages', 0x2240e4f0), ('weather_messages', 0x2240e310), ('time_messages', 0x2240ebf8), ('time_season_messages', 0x2240e6c0), ('time_clock_messages', 0x2240ea00)):
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
        if dialog_record['id'] == 0x1301:
            lines.append(f'    {{0x1301, fs98_world_time_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x1302:
            lines.append(f'    {{0x1302, fs98_world_weather_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x13050001:
            lines.append(f'    {{0x13050001, fs98_world_copy_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x1305:
            lines.append(f'    {{0x1305, fs98_world_dynamic_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x1304:
            lines.append(f'    {{0x1304, fs98_world_complexity_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x1309:
            lines.append(f'    {{0x1309, fs98_world_airport_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x1307:
            lines.append(f'    {{0x1307, fs98_world_facilities_create, {dialog_record["flags"]}}},')
        if dialog_record['id'] == 0x1308:
            lines.append(f'    {{0x1308, fs98_world_position_create, {dialog_record["flags"]}}},')
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
