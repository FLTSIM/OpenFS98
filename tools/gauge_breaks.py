#!/usr/bin/env python3
"""Break the decompiled gauges (src/gauges/<name>.c) on purpose, one change at a time, and check
tools/difftest/allgaugesdiff.c catches each: rebuild the collection, run that gauge alone
against retail.

    python3 tools/gauge_breaks.py [gauge...]     N=vectors (default 4000), ONLY=name,name

Each entry is (name, before, after[, vectors]): before must occur once in the gauge's file; a
fourth field raises the vector count for a rare edge. A break that
does not build, or that the harness does not report as a mismatch, is listed. Run it from a
frozen copy of the tree: it rewrites src/gauges/ while it runs.

Left out as proven equivalent:
- flow-tenths (EXFLOW's whole part taken unsigned in the fraction): the flow is kept at or above
  its floor, 0.0 (a NaN becomes the floor too), so the whole part is never negative.

Not observable here: 737START reads the switch's state again after its three events; the
harness's PANELS stub never changes the state in between, so dropping the second read is not
seen (in the game it is kept as retail has it).
"""
import os
from pathlib import Path
import subprocess
import sys

BREAKS = {
    '737flaps': [
        ('install-order', '#define GAUGE_MOUSE_ARGUMENT 0x325cu', '#define GAUGE_MOUSE_ARGUMENT 0x3260u'),
    ],
    '737omi': [
        ('omi-equal', 'GR64(0x2000), GAUGE_C3)) {', 'GR64(0x2000), GAUGE_C0)) {'),
        ('omi-set', 'GD32(0x30a8) |= 0x40000000u;', 'GD32(0x30a8) |= 0x20000000u;'),
        ('omi-clear', 'GD32(0x30a8) &= 0xbfffffffu;', '(void)0;'),
        ('omi-offset', '- GR64(0x2008);', '+ GR64(0x2008);'),
    ],
    '737vert': [
        ('vert-scale', 'value * GR64(0x2000)', 'value * GR64(0x2008)'),
    ],
    '737rmi': [
        ('rmi-mode', 'GAUGE_TEST(GD64(0x3490), GR64(0x2008), GAUGE_C3)', 'GAUGE_TEST(GD64(0x3490), GR64(0x2008), GAUGE_C0)'),
        ('rmi-speed-text', "(const char *)GAUGE_D(0x36d4)", "(const char *)GAUGE_D(0x36dc)"),
        ('rmi-speed-cap', 'GD64(0x3474) = 999.0;', 'GD64(0x3474) = 998.0;'),
        ('rmi-speed-floor', 'GD64(0x3474) = 0.0;', '(void)0;'),
        ('rmi-dist-cap', 'GD64(0x3458) = 9999.0;', 'GD64(0x3458) = 9998.0;'),
        ('rmi-dist-floor', 'GD64(0x3458) = 0.0;', '(void)0;'),
        ('rmi-dist-text', "(const char *)GAUGE_D(0x36b8)", "(const char *)GAUGE_D(0x36cc)"),
        ('rmi-whole', 'GR64(0x2030), GAUGE_C0C3)) {', 'GR64(0x2030), GAUGE_C0)) {'),
        ('rmi-tenths', '* GR64(0x2038));', '* GR64(0x2030));'),
    ],
    'bellturn': [
        ('turn-empty', '    return 0;\n}\nGAUGE_MAYBE_FP', '    *value = 0.0;\n    return 1;\n}\nGAUGE_MAYBE_FP'),
        ('turn-zero', 'if (state == 0) { *value = GR64(0x2008);', 'if (state == 0) { *value = GR64(0x2000);'),
        ('turn-ball', 'GD64(0x3194) = (double)gauge_fchs(GD64(0x3194));', '(void)0;'),
    ],
    'belltemp': [
        ('temp-units', 'if (units & GAUGE_C3)', 'if (!(units & GAUGE_C3))'),
        ('temp-park', 'double temperature = (double)((long double)value * GR64(0x2010));', 'long double temperature = (long double)value * GR64(0x2010);'),
        ('temp-cap', 'temperature = 999.0;', 'temperature = 998.0;'),
        ('temp-floor', 'temperature = -99.0;', 'temperature = -98.0;'),
    ],
    'sopcomp': [
        ('comp-park', 'gauge_fst(&value, heading);     /* the fst, kept though the next may overwrite it */\n    }\n    if', '(void)0;\n    }\n    if'),
        ('comp-low-park', 'gauge_fst(&value, heading);     /* the fst, kept though the next may overwrite it */\n    }\n    wsprintfA', '(void)0;\n    }\n    wsprintfA'),   # (adding instead of subtracting there loops for ever)
    ],
    'schairsp': [
        ('airsp-store', 'volatile double shown', 'double shown'),
        ('airsp-zero', 'shown = 0.0;', 'shown = 1.0;'),
    ],
    'exflow': [
        ('flow-low', 'if (GAUGE_TEST(flow, GR64(0x2000), GAUGE_C0)) flow = GR64(0x2000);', 'if (0) flow = GR64(0x2000);'),
        ('flow-high', 'else if (!GAUGE_TEST(flow, GR64(0x2008), GAUGE_C0C3)) flow = GR64(0x2008);', ''),
    ],
    'lrspoil': [
        ('spoil-scale', 'y * 16390u', 'y * 16391u'),
        ('spoil-band', 'position > 0xcdf &&', 'position >= 0xcdf &&', 20000),
        ('spoil-flag', 'flags & 0x20000000u', 'flags & 0x10000000u'),
        ('spoil-cap', 'if (position > 0x3fff) position = 0x3fff;', '(void)0;'),
    ],
    'learwarn': [
        ('warn-less', '(GD32(0x3670) - 1)', '(GD32(0x3670) + 1)'),
        ('warn-toggle', 'GD32(0x3670) = GD32(0x3670) == 0;', 'GD32(0x3670) = 1;'),
    ],
    'bkadf': [
        ('adf-tenths', 'if ((uint32_t)gauge_ftol(GD64(0x31c8)))', 'if ((uint32_t)gauge_ftol(GD64(0x31c8)) > 1)'),
        ('adf-point', 'point = 0x2e;', 'point = 0x2c;'),
        ('adf-order', '(int32_t)(int8_t)tens, (int32_t)(int8_t)ones', '(int32_t)(int8_t)ones, (int32_t)(int8_t)tens'),
    ],
    'leargear': [
        ('gear-last', 'GAUGE_TEST(GD64(0x30c0), PF64(element, 0xc), GAUGE_C3)', 'GAUGE_TEST(GD64(0x30c4), PF64(element, 0xc), GAUGE_C3)'),
        ('gear-bits', '0x40cfff80u', '0x40cfff00u'),
    ],
    '737hsi': [
        ('hsi-bit7', 'if (bits & 0x80) return GR64(0x2008);', 'if (bits & 0x40) return GR64(0x2008);'),
        ('hsi-source2', '        return ((long double)PF64(element, 0xc) + GD64(0x4200)) - GR64(0x2028);', '        return ((long double)PF64(element, 0xc) + GD64(0x4200)) - GR64(0x2018);'),
        ('hsi-blank', 'memcpy((void *)(uintptr_t)GD32(0x38b8), GAUGE_D(0x4390), 4);', 'memcpy((void *)(uintptr_t)GD32(0x38b8), GAUGE_D(0x4390), 3);'),
        ('hsi-flag', '    if (GD8(0x3114) & 8) {', '    if (GD8(0x3114) & 4) {'),
        ('hsi-dme', 'GD32(0x4188) |= 0x40000000u;', '(void)0;'),
    ],
    'learpfd': [
        ('pfd-div', '(uint32_t)(rate / 256) * 60u', '(uint32_t)(rate >> 8) * 60u'),
        ('pfd-floor', 'if (needle < -4000) needle = -4000;', '(void)0;'),
        ('pfd-hold', 'PF64(element, 0xc) = 1000.0;', 'PF64(element, 0xc) = 999.0;'),
        ('pfd-rest', '* 5u) << 2) - scaled;', '* 5u) << 2) + scaled;'),
        ('pfd-30dc', '        LEARPFD_OFF(0x3f08);\n', '\n'),
        ('pfd-4138', 'if (GD32(0x5b20)) LEARPFD_OFF(0x4138); else LEARPFD_ON(0x4138);', 'LEARPFD_ON(0x4138);'),
        ('pfd-toggle', 'GD32(0x5b24) = GD32(0x5b24) == 0;', 'GD32(0x5b24) = 1;'),
    ],
    'lreicas': [
        ('eicas-neg', '(int32_t)(0u - (uint32_t)shown)', '(int32_t)shown'),
        ('eicas-step', 'step = 8;', 'step = 9;'),
        ('eicas-cap', 'if (shown > 999) shown = 999;\n    else if (shown < -99)', 'if (shown > 999) shown = 998;\n    else if (shown < -99)'),
    ],
    '737apilt': [
        ('ap-heading', 'GD32(0x3138), (const char *)GAUGE_D(0x46b0), heading);', 'GD32(0x3138), (const char *)GAUGE_D(0x46b0), heading - 1);'),
        ('ap-mach', 'return (long double)parked - GR64(0x2010);', 'return (long double)value - GR64(0x2010);'),
        ('ap-light', 'GD32(0x4708) = 1;\n        return GR64(0x2018);', 'GD32(0x4708) = 0;\n        return GR64(0x2018);'),
    ],
    '737fuelq': [
        ('fuel-tank', 'GD64(0x32b0) * GD64(0x30b4)', 'GD64(0x32b0) * GD64(0x30d4)'),
        ('fuel-sum', 'GD64(0x3404) * GD64(0x3094)', 'GD64(0x3404) * GD64(0x3074)'),
    ],
    'extach': [
        ('tach-mode', 'if (EXTACH_MODE == 4) { EXTACH_MODE = 5;', 'if (EXTACH_MODE == 4) { EXTACH_MODE = 4;'),
        ('tach-cap', 'GD64(0x35a0) = 9999.0;', 'GD64(0x35a0) = 9998.0;'),
    ],
    '737start': [
        ('start-step', '{{0x1000d, 1}, {0, 0}, {0x1000d, 2}}', '{{0x1000d, 1}, {0, 0}, {0x1000d, 1}}'),
    ],
    'bkradap': [
        ('ap-fd', '    if (!GAUGE_TEST(GD64(0x3150), GR64(0x2000), GAUGE_C3)) {', '    if (!GAUGE_TEST(GD64(0x3150), GR64(0x2008), GAUGE_C3)) {'),
        ('ap-text', "else if (GAUGE_TEST(GD64(0x354c), GR64(0x2008), GAUGE_C3)) text = (const char *)GAUGE_D(0x3e1c);", "else if (0) text = (const char *)GAUGE_D(0x3e1c);"),
    ],
    'exgmeter': [
        ('g-min', 'GAUGE_TEST(load, GD64(0x42c8), GAUGE_C0)', 'GAUGE_TEST(load, GD64(0x42c0), GAUGE_C0)'),
        ('g-annunciator', 'GD64(0x3844 + at * 0x84) = GAUGE_TEST(GD64(0x42d0), GD64(0x30e8 + at * 8), GAUGE_C0) ? -1.0 : 0.0;', 'GD64(0x3844 + at * 0x84) = GAUGE_TEST(GD64(0x42d0), GD64(0x30e8 + at * 8), GAUGE_C0C3) ? -1.0 : 0.0;'),
    ],
    'learchrn': [
        ('chrn-minutes', '(int32_t)(elapsed / 60) % 60', '(int32_t)elapsed / 60 % 60'),
    ],
}


def main():
    root = Path.cwd()
    scenarios = os.environ.get('N', '4000')
    only = set(filter(None, os.environ.get('ONLY', '').split(',')))
    gauges = sys.argv[1:] or list(BREAKS)
    total = caught = 0
    for gauge in gauges:
        path = root / 'src/gauges' / f'{gauge}.c'
        original = path.read_text()
        try:
            for entry in BREAKS[gauge]:
                name, before, after = entry[:3]
                wanted = str(max(int(scenarios), entry[3])) if len(entry) > 3 else scenarios
                if only and name not in only:
                    continue
                total += 1
                if original.count(before) != 1:
                    print(f'{gauge} {name}: text not found once')
                    continue
                path.write_text(original.replace(before, after))
                build = subprocess.run(['python3', '-B', 'tools/build_gauges.py'], cwd=root, capture_output=True, text=True)
                if build.returncode:
                    print(f'{gauge} {name}: does not build')
                    continue
                run = subprocess.run(['bash', 'tools/run_gaugesdiff.sh', '--all', wanted, gauge.upper()], cwd=root,
                                     capture_output=True, text=True)
                if 'mismatch' in run.stderr or 'mismatch' in run.stdout:
                    caught += 1
                    print(f'{gauge} {name}: caught')
                elif 'match' in run.stdout:
                    print(f'{gauge} {name}: MISSED')
                else:
                    print(f'{gauge} {name}: CRASHED ({run.returncode}) {run.stderr[-200:]}')
        finally:
            path.write_text(original)
    subprocess.run(['python3', '-B', 'tools/build_gauges.py'], cwd=root, capture_output=True)
    print(f'{caught} of {total} caught')


if __name__ == '__main__':
    main()
