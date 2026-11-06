#!/usr/bin/env python3
"""Check real native child-creation failure and detect cleanup mutations."""
import mpdialog_ui_breaks as ui
from mpdialog_review_breaks import compare
from mpdialog_construction_breaks import replace_once


def main():
    session = (ui.ROOT / 'src/mpdialog/mpdialog_session.c').read_text()
    flow = (ui.ROOT / 'src/mpdialog/mpdialog_flow.c').read_text()
    details = (ui.ROOT / 'src/mpdialog/mpdialog_details.c').read_text()
    tables = (ui.ROOT / 'src/mpdialog/mpdialog_runtime_tables.inc').read_text()
    args = ['--create-failure']
    with ui.isolated_wine_prefix():
        compare('native Create failure baseline', ui.OUT / 'MP_DIALOG_ours.dll', args)
        compare('ordinary construction baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--construction-only'])
        compare('retained destruction baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--retained-destruction'])
        for family, source, page in [('flow', flow, 0), ('settings', session, 5), ('details', details, 6)]:
            for suffix, old, new in [
                ('no-free', 'if (flags & 1) fs98_mpdialog_free(object);', '(void)flags;'),
                ('wrong-flags', '))(object, 1);', '))(object, 0);'),
            ]:
                name = f'create-failure-{family}-{suffix}'
                # Flow PostNcDestroy shares the Settings implementation.
                if family == 'flow' and suffix == 'wrong-flags':
                    continue
                mutant = replace_once(source, old, new, name)
                dll = (ui.build_flow(name, mutant, tables) if family == 'flow' else
                       ui.build(name, mutant if family == 'settings' else session, tables,
                                mutant if family == 'details' else details))
                compare(name, dll, args, f'MP_DIALOG CREATE FAILURE DIFFER page={page} ')
        for name, old, new in [
            ('create-failure-flow-order',
             '            flow_call_control(control->dtor, object, control->offset);',
             '            flow_call_control(control->dtor, object, page == 0 && control->offset == 0x10dc ? 0x11d8 :\n'
             '                page == 0 && control->offset == 0x11d8 ? 0x10dc : control->offset);'),
            ('create-failure-flow-omitted-control',
             '    else if (control->dtor)',
             '    else if (control->dtor && !(page == 0 && control->offset == 0x10dc))'),
        ]:
            dll = ui.build_flow(name, replace_once(flow, old, new, name), tables)
            compare(name, dll, args, 'MP_DIALOG CREATE FAILURE DIFFER page=0 ')


if __name__ == '__main__':
    main()
