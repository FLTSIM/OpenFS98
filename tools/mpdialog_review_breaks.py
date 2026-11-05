#!/usr/bin/env python3
"""Mutations for independent descriptor/error/update review boundaries."""
import mpdialog_ui_breaks as ui


def compare(name, dll, args, marker=None):
    result = ui.run_wine(['wine', str(ui.OUT / 'mpdialoguidiff.exe'), str(dll), '12', *args], name)
    if (result.returncode != (1 if marker else 0) or
            (marker and marker not in result.stdout) or
            'modules deinitialized and unloaded' not in result.stdout or
            'MP_DIALOG UI FAULT' in result.stdout):
        raise RuntimeError(f'{name}: unexpected comparison result\n{result.stdout}')
    print(('MUTATION CAUGHT ' if marker else 'BASELINE PASSED ') + name, flush=True)


def main():
    flow = (ui.ROOT / 'src/mpdialog/mpdialog_flow.c').read_text()
    details = (ui.ROOT / 'src/mpdialog/mpdialog_details.c').read_text()
    session = (ui.ROOT / 'src/mpdialog/mpdialog_session.c').read_text()
    tables = (ui.ROOT / 'src/mpdialog/mpdialog_runtime_tables.inc').read_text()
    with ui.isolated_wine_prefix():
        compare('review boundaries', ui.OUT / 'MP_DIALOG_ours.dll', ['--construction-only'])
        compare('details matrices', ui.OUT / 'MP_DIALOG_ours.dll', ['--details-mutation-case', '0'])
        for name, old, new, marker in [
            ('details-error-return', 'return ((int32_t (TC *)(void *, int32_t))PTR(PTR(object, 0), 0xe0))(object, result);',
             '((int32_t (TC *)(void *, int32_t))PTR(PTR(object, 0), 0xe0))(object, result); return 1;', 'ERROR'),
            ('details-error-field', 'if (helper) result = status == 0x1c ? 0x40b : 0x40d;',
             'if (helper) result = status == 0x1c ? 0x40d : 0x40b;', 'ERROR'),
            ('details-update-descriptor', '    count = D32(block, 0x50);',
             '    count = D32(block, 0x50); D32(block, 0x58) = SendMessageA(list, LB_GETCURSEL, 0, 0);', 'UPDATE'),
        ]:
            if details.count(old) != 1:
                raise RuntimeError(f'{name}: ambiguous anchor')
            dll = ui.build(name, session, tables, details.replace(old, new, 1))
            compare(name, dll, ['--details-mutation-case', '0'], f'MP_DIALOG DETAILS {marker} MATRIX DIFFER')
        for name, old, new, page in [
            ('choice-unknown-mode', '    if (mode > 2) return;', '    if (mode > 2) mode = 0;', 1),
            ('connection-update-mode', '    flow_connection_mode(object, 0);', '    /* omitted mode update */', 3),
        ]:
            if flow.count(old) != 1:
                raise RuntimeError(f'{name}: ambiguous anchor')
            dll = ui.build_flow(name, flow.replace(old, new, 1), tables)
            compare(name, dll, ['--construction-only'], f'MP_DIALOG BOUNDARY DIFFER page={page}')


if __name__ == '__main__':
    main()
