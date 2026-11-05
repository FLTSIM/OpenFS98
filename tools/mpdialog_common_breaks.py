#!/usr/bin/env python3
"""Require shared virtual state, call and ABI mutations to differ from retail."""
import mpdialog_ui_breaks as ui
from mpdialog_review_breaks import compare


def main():
    session = (ui.ROOT / 'src/mpdialog/mpdialog_session.c').read_text()
    tables = (ui.ROOT / 'src/mpdialog/mpdialog_runtime_tables.inc').read_text()
    with ui.isolated_wine_prefix():
        compare('common virtual baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--construction-only'])
        for name, old, new in [
            ('common-disable-direction', '{ EnableWindow((HWND)D32(object, 0x20), FALSE); }',
             '{ EnableWindow((HWND)D32(object, 0x20), TRUE); }'),
            ('common-enable-omitted', '{ EnableWindow((HWND)D32(object, 0x20), TRUE); }',
             '{ (void)object; }'),
            ('common-disable-redundant', '{ EnableWindow((HWND)D32(object, 0x20), FALSE); }',
             '{ if (IsWindowEnabled((HWND)D32(object, 0x20))) EnableWindow((HWND)D32(object, 0x20), FALSE); }'),
            ('common-noop-write', 'fs98_mpdialog_empty(void *object) { (void)object; }',
             'fs98_mpdialog_empty(void *object) { D32(object, 0x24) = 0; }'),
        ]:
            if session.count(old) != 1:
                raise RuntimeError(f'{name}: ambiguous anchor')
            dll = ui.build(name, session.replace(old, new, 1), tables)
            compare(name, dll, ['--construction-only'], 'MP_DIALOG COMMON DIFFER')
        for name, old, new in [
            ('common-argument-cleanup', '{2, -1, (void *)fs98_mpdialog_empty_arg}',
             '{2, -1, (void *)fs98_mpdialog_empty}'),
            ('common-noargument-cleanup', '{3, -1, (void *)fs98_mpdialog_empty}',
             '{3, -1, (void *)fs98_mpdialog_empty_arg}'),
        ]:
            if tables.count(old) != 7:
                raise RuntimeError(f'{name}: expected seven class bindings')
            dll = ui.build(name, session, tables.replace(old, new))
            compare(name, dll, ['--construction-only'], 'MP_DIALOG COMMON DIFFER')


if __name__ == '__main__':
    main()
