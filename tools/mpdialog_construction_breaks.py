#!/usr/bin/env python3
"""Check paired ordinary construction, DDX and destructor-order mutations."""
import subprocess
import sys
import mpdialog_ui_breaks as ui


def compare(label, dll, expect_difference=False, page=None):
    result = ui.run_wine(['wine', str(ui.OUT / 'mpdialoguidiff.exe'), str(dll),
                          '12', '--construction-only'], label)
    difference = f'MP_DIALOG CONSTRUCTION DIFFER page={page} ' if page is not None else 'MP_DIALOG CONSTRUCTION DIFFER '
    if expect_difference:
        good = result.returncode == 1 and difference in result.stdout
    else:
        good = result.returncode == 0 and 'MP_DIALOG construction comparison: 0 differing classes' in result.stdout
    if not good or 'MP_DIALOG UI FAULT' in result.stdout or 'modules deinitialized and unloaded' not in result.stdout:
        raise RuntimeError(f'{label} did not complete the expected paired comparison:\n{result.stdout}')
    print(('MUTATION CAUGHT ' if expect_difference else 'BASELINE PASSED ') + label, flush=True)


def replace_once(text, old, new, label):
    if text.count(old) != 1:
        raise RuntimeError(f'{label} mutation anchor missing or ambiguous')
    return text.replace(old, new, 1)


def main():
    session = (ui.ROOT / 'src/mpdialog/mpdialog_session.c').read_text()
    flow = (ui.ROOT / 'src/mpdialog/mpdialog_flow.c').read_text()
    tables = (ui.ROOT / 'src/mpdialog/mpdialog_runtime_tables.inc').read_text()
    with ui.isolated_wine_prefix():
        compare('construction baseline', ui.OUT / 'MP_DIALOG_ours.dll')
        mutated = session
        for prefix, suffix in (('??0', 'QAE@XZ'), ('??1', 'UAE@XZ')):
            old = (f'    "{prefix}CSimsText@@{suffix}", "{prefix}CSimsText@@{suffix}",\n'
                   f'    "{prefix}CSimsText@@{suffix}", "{prefix}CSimsText@@{suffix}",\n'
                   f'    "{prefix}CSimsText@@{suffix}", "{prefix}CSimsCheckButton@@{suffix}",')
            new = old.replace('CSimsText', 'CSims3PieceButton')
            mutated = replace_once(mutated, old, new, 'settings control type')
        compare('settings-control-type', ui.build('settings-control-type', mutated, tables), True, 5)
        old = '        FLOW_BTN(0x10dc), FLOW_BTN(0x11d8), FLOW_BTN(0x12d4),\n'
        new = '        FLOW_BTN(0x11d8), FLOW_BTN(0x10dc), FLOW_BTN(0x12d4),\n'
        compare('intro-construction-order', ui.build_flow('intro-construction-order',
                replace_once(flow, old, new, 'intro construction'), tables), True, 0)
        old = '    {0x10dc, 2}, {0x11d8, 0x3e8}, {0x12d4, 0x3e9}, {0x1900, 0x427},'
        new = '    {0x11d8, 0x3e8}, {0x10dc, 2}, {0x12d4, 0x3e9}, {0x1900, 0x427},'
        compare('intro-ddx-order', ui.build_flow('intro-ddx-order',
                replace_once(flow, old, new, 'intro DDX'), tables), True, 0)
        old = '            flow_call_control(control->dtor, object, control->offset);'
        new = ('            flow_call_control(control->dtor, object, page == 0 && control->offset == 0x10dc ? 0x11d8 :\n'
               '                page == 0 && control->offset == 0x11d8 ? 0x10dc : control->offset);')
        compare('intro-destruction-order', ui.build_flow('intro-destruction-order',
                replace_once(flow, old, new, 'intro destruction'), tables), True, 0)
        old = '    PTR(object, s_flow_descriptor_offsets[page]) = descriptor;'
        new = ('    if (page == 0) {\n'
               '        static uint8_t alternate_descriptor[0xbc];\n'
               '        memcpy(alternate_descriptor, descriptor, sizeof(alternate_descriptor));\n'
               '        PTR(object, s_flow_descriptor_offsets[page]) = alternate_descriptor;\n'
               '    } else PTR(object, s_flow_descriptor_offsets[page]) = descriptor;')
        compare('intro-descriptor-attachment', ui.build_flow('intro-descriptor-attachment',
                replace_once(flow, old, new, 'descriptor attachment'), tables), True, 0)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'MP_DIALOG construction mutation check failed: {error}', file=sys.stderr)
        sys.exit(1)
