#!/usr/bin/env python3
"""The decompiled host runtime (src/host/runtime/) broken on purpose.

Each break is one edit to a copy of one source file, built into its own hostrtdiff, and must
make hostrtdiff report a difference (a crash of the harness is not a catch). Run from a frozen
copy of the tree: it builds into bin/host-rt/breaks/.

    python3 tools/host_rt_breaks.py [N=2000] [NAME...]

Proven equivalent, so expected to be missed:
  open-fallback-depth  the fallback mode's depth is 16 with an IDirect3D2 and 8 without, but a
                       failed QueryInterface has already returned: there is always one.
  initialise-word      0x416180..0x41618c are all zero in the reference executable.

Rare, missed at N=2000 and caught at N=20000: open-mode-640 (a second device, full screen, a
640-wide mode not 480 high), render-memory (an 8-bit display without Direct3D that can do 3D).
"""
EQUIVALENT = {'open-fallback-depth', 'initialise-word'}
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

SOURCE = Path('src/host/runtime')
OUT = Path('bin/host-rt/breaks')
CC = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-w', '-fno-strict-aliasing',
      '-Wl,--image-base=0x13000000', '-Wl,--disable-dynamicbase']

# name, file, before, after (the before text must occur exactly once)
BREAKS = [
    # host_rt_window.c
    ('floating-class', 'window', 'STR(0x417178)) == 0', 'STR(0x417178)) != 0'),
    ('children-skipped', 'window', '    CALL_STD(EnumChildWindows', '    if (0) CALL_STD(EnumChildWindows'),
    ('visit-parameter', 'window', 'visit.parameter = parameter;', 'visit.parameter = parameter + 1;'),
    # host_rt_ddraw.c: the device list
    ('name-driver-test', 'ddraw', "if (driver[0] == '\\\\' && driver[1] == '\\\\' && driver[2] == '.')\n        CALL_C",
     "if (driver[0] == '\\\\' && driver[1] == '\\\\')\n        CALL_C"),
    ('name-empty-suffix', 'ddraw', 'if (name != 0 && name[0] != 0) {', 'if (name != 0) {'),
    ('device-is-guid', 'ddraw', '    if (guid == 0)\n        return 1;\n    if (CALL_STD(lstrcmpiA', '    if (guid == 0)\n        return 0;\n    if (CALL_STD(lstrcmpiA'),
    ('suffix-strict', 'ddraw', 'if (length > suffix) {', 'if (length >= suffix + 1 && length != suffix + 1) {'),
    ('suffix-no-cut', 'ddraw', '        found[-1] = 0;', '        found[0] = 0;'),
    ('found-kind', 'ddraw', '        search->found |= 4;', '        search->found |= 2;'),
    ('found-continue', 'ddraw', '    if (search->result != 0)\n        return 1;', '    if (search->result != 0)\n        return 0;'),
    ('create-empty', 'ddraw', 'if (name == 0 || name[0] == 0)', 'if (name == 0)'),
    ('create-cut', 'ddraw', '    fs98_host_rt_device_suffix(search.name, 0, 1);', '    fs98_host_rt_device_suffix(search.name, 0, 0);'),
    # display, caps, release
    ('size-555', 'ddraw', 'desc[0x17] == 0x3e0', 'desc[0x17] == 0x7e0'),
    ('size-gdi-planes', 'ddraw', '*depth = bits * CALL_STD', '*depth = bits + CALL_STD'),
    ('3d-memory', 'ddraw', 'caps[0x10] < 0x25800', 'caps[0x10] <= 0x25800'),
    ('3d-bank-depth', 'ddraw', 'if (DIRECT3D != 0 && depth > 8)', 'if (DIRECT3D != 0 && depth >= 8)'),
    ('tell-sent', 'ddraw', 'if (message == 0x46d)', 'if (message == 0x46e)'),
    ('leave-keeps', 'ddraw', '    G32(0x41a540) = 0;\n}', '}'),
    ('release-order', 'ddraw', '    release(0x41a4fc);\n    release(0x41a4e0);', '    release(0x41a4e0);\n    release(0x41a4fc);'),
    ('release-d3d', 'ddraw', '    release(0x41a4dc);\n    release(0x41a4e4);', '    release(0x41a4e4);'),
    ('d3d-found-software', 'ddraw', '(software[2] & 2) && kept[1] == 0', '(software[2] & 2)'),
    ('d3d-hw-memory', 'ddraw', 'caps[0xf] >= 0x14b000', 'caps[0xf] > 0x14b000'),
    ('d3d-hw-option', 'ddraw', 'if (!(G8(0x41a4c0) & 4) || G8(0x41a470) == 0)', 'if (!(G8(0x41a4c0) & 4))'),
    # surfaces
    ('palette-depth', 'ddraw', 'depth != 8)\n        return;', 'depth != 16)\n        return;'),
    ('palette-flags', 'ddraw', "if (!(G8(0x41a4ec) & 1) || (G8(0x41a4ec) & 2))", "if (!(G8(0x41a4ec) & 1))"),
    ('palette-count', 'ddraw', '(PALETTE, 0, 0, 0x100, entries)', '(PALETTE, 0, 0, 0xff, entries)'),
    ('lock-no-retry', 'ddraw', '        result = lock(surface, 0, desc, G32(0x41a4d8), 0);\n        if (result != 0)\n            return 0;',
     '        return 0;'),
    ('lock-pitch', 'ddraw', '*pitch = (int32_t)desc[4];', '*pitch = (int32_t)desc[5];'),
    ('fill-flags', 'ddraw', '0x1000400, fx)', '0x400, fx)'),
    ('clip-x', 'ddraw', '    if (x < 0) {\n        width += x;', '    if (x < 0) {\n        width -= x;'),
    ('clip-height', 'ddraw', 'height = (int32_t)desc[2] - y;', 'height = (int32_t)desc[2] - y - 1;'),
    ('clip-order', 'ddraw', '    rect[1] = y;\n    rect[0] = x;', '    rect[1] = x;\n    rect[0] = y;'),
    ('blit-key', 'ddraw', '(keyed ? 0x8000u : 0)', '(keyed ? 0x4000u : 0)'),
    ('blit-overlay-test', 'ddraw', 'to == PRIMARY && from == BACK_BUFFER', 'to == PRIMARY'),
    ('blit-restore-one', 'ddraw', '        COM(to, 0x6c, int32_t, (void *))(to);\n        result', '        result'),
    ('blit-source-rect', 'ddraw', '    from_rect[3] = from_y + from_height;', '    from_rect[3] = from_y + from_width;'),
    # open, render surface
    ('open-busy', 'ddraw', '    if (G32(0x41a500) != 0)\n        return 1;', '    if (G32(0x41a500) != 0)\n        return 0;'),
    ('open-metrics', 'ddraw', 'if (G32(0x4183b4) != 0)\n        G32(0x4183b4)', 'if (G32(0x4183b4) == 1)\n        G32(0x4183b4)'),
    ('open-restore-back', 'ddraw', 'if (back == 0 || COM(back, 0x6c', 'if (back != 0 && COM(back, 0x6c'),
    ('open-flip-flag', 'ddraw', '                G8(0x41a4c0) |= 0x40;', '                G8(0x41a4c0) |= 0x20;'),
    ('open-cooperative', 'ddraw', 'FULL_SCREEN != 0 ? 0x11 : 8);', 'FULL_SCREEN != 0 ? 0x13 : 8);'),
    ('open-busy-count', 'ddraw', '        --G32(0x41a500);\n', ''),
    ('open-error-mode', 'ddraw', 'set_error_mode(set_error_mode(0) & ~2u);', 'set_error_mode(set_error_mode(0) & ~1u);'),
    ('open-d3d-kept', 'ddraw', '            COM(directdraw, 8, uint32_t, (void *))(directdraw);\n            G_PTR(0x41a4e4) = 0;\n            return 0;',
     '            COM(directdraw, 8, uint32_t, (void *))(directdraw);\n            return 0;'),
    ('open-lock-flags', 'ddraw', 'G32(0x41a4d8) = 0x801;', 'G32(0x41a4d8) = 0x1;'),
    ('open-mode-640', 'ddraw', '(MODE_WIDTH != 0x280 || MODE_HEIGHT != 0x1e0)', '(MODE_WIDTH != 0x280)'),
    ('open-fallback-depth', 'ddraw', 'uint32_t depth = DIRECT3D != 0 ? 0x10 : 8;', 'uint32_t depth = 0x10;'),
    ('open-back-count', 'ddraw', '        desc[5] = 1;\n        G32(0x41a4f4) = 1;', '        desc[5] = 2;\n        G32(0x41a4f4) = 1;'),
    ('open-no-clear', 'ddraw', '        fs98_host_rt_fill(PRIMARY, 0, 0);', '        ;'),
    ('open-tell', 'ddraw', '(void *)fs98_host_rt_tell_window, 0x46c);', '(void *)fs98_host_rt_tell_window, 0x46d);'),
    ('render-depth', 'ddraw', '    } else if (depth > 16)\n        return 0;', '    } else if (depth > 15)\n        return 0;'),
    ('render-memory', 'ddraw', 'desc[0x1a] = DIRECT3D != 0 ? 0x6040 : 0x4040;', 'desc[0x1a] = 0x6040;'),
    ('render-sizes', 'ddraw', '    desc[2] = (uint32_t)height;\n    desc[3] = (uint32_t)width;', '    desc[2] = (uint32_t)width;\n    desc[3] = (uint32_t)height;'),
    ('render-overlay-limit', 'ddraw', 'caps[0x22] <= 1000', 'caps[0x22] < 1000'),
    ('render-overlay-alpha', 'ddraw', '            desc[0x19] = 0x8000;', '            desc[0x19] = 0;'),
    ('render-undo', 'ddraw', 'desc[1] &= 0xffffefdfu;', 'desc[1] &= 0xffffefffu;'),
    ('render-no-clear', 'ddraw', '    fs98_host_rt_fill_clipped(surface, 0, 0, width, height, 0);\n    return surface;', '    return surface;'),
    # host_rt_config.c
    ('flag-clear', 'config', '        D32(settings, 0xd0) &= ~mask;', '        ;'),
    ('primary-guid', 'config', '    if (guid != 0)\n        return 1;', '    if (guid == 0)\n        return 1;'),
    ('display-defaults', 'config', '                if (G32(entry + 8) != 0)', '                if (G32(entry + 4) != 0)'),
    ('display-size', 'config', '(char *)settings, 0x80, CONFIGURATION);', '(char *)settings, 0x7f, CONFIGURATION);'),
    ('display-enumerate', 'config', 'if (D8(settings, 0) == 0 && !(D8(settings, 0xd0) & 1))', 'if (D8(settings, 0) == 0)'),
    ('mode-skip-x', 'config', '    if (**text != 0)\n        ++*text;', ''),
    ('mode-fourth', 'config', 'D32(settings, 0xe0) = (uint32_t)CALL_C(atoi, int32_t, (const char *))(mode);', 'D32(settings, 0xe0) = 0;'),
    ('mode-default-depth', 'config', "D32(settings, 0xdc) = D8(settings, 0x80) != 0 ? 0x10 : 8;", "D32(settings, 0xdc) = 0x10;"),
    # host_rt_startup.c
    ('initialise-word', 'startup', 'G32(0x41a510) = G32(0x416188);', 'G32(0x41a510) = G32(0x41618c);'),
    # host_rt_cursor.c
    ('cursor-same', 'cursor', 'if (CURSOR != cursor) {', 'if (CURSOR == cursor) {'),
    ('cursor-hotspot', 'cursor', 'y = point[1] - GI32(ICON_INFO + 8);', 'y = point[1] - GI32(ICON_INFO + 4);'),
    ('cursor-size', 'cursor', '    now[3] = y + 0x20;', '    now[3] = y + 0x1f;'),
    ('cursor-unchanged', 'cursor', 'y != GI32(LAST_RECT + 4) || changed)', 'y != GI32(LAST_RECT + 4))'),
    ('cursor-cut', 'cursor', 'if (joined[3] < bottom)', 'if (joined[3] <= bottom - 1 && 0)'),
    ('cursor-left', 'cursor', 'joined[0] <= 0 ? 0 : joined[0]', 'joined[0]'),
    ('cursor-combine', 'cursor', '(region, REDRAW, SCRATCH_REGION, 2);', '(region, REDRAW, SCRATCH_REGION, 1);'),
    ('cursor-last', 'cursor', '    G32(LAST_RECT + 8) = G32(CURSOR_RECT + 8);', '    G32(LAST_RECT + 8) = G32(CURSOR_RECT + 0xc);'),
    ('draw-dc', 'cursor', '    uint32_t dc = region;\n', '    uint32_t dc = 0;\n'),
    ('draw-icon-at', 'cursor', '(dc, GI32(CURSOR_RECT), GI32(CURSOR_RECT + 4), CURSOR);', '(dc, GI32(CURSOR_RECT + 4), GI32(CURSOR_RECT), CURSOR);'),
    ('frame-empty', 'cursor', 'if (GI32(0x41a560) <= GI32(0x41a558))', 'if (GI32(0x41a560) < GI32(0x41a558))'),
    ('copy-pitch', 'cursor', '    int32_t words = to_pitch / 2, at;\n    if (rows <= 0)\n        return;\n    do {\n        for (at = 0; at < width; ++at)\n            *to++',
     '    int32_t words = to_pitch >> 1, at;\n    if (rows <= 0)\n        return;\n    do {\n        for (at = 0; at < width; ++at)\n            *to++'),
    ('copy-source-step', 'cursor', '            *to++ = palette[*from++];\n        from += from_pitch - width;', '            *to++ = palette[*from++];\n        from += from_pitch;'),
    ('keyed-key', 'cursor', '            if (*from != key)', '            if (*from != key && *from != 0)'),
    ('keyed-step', 'cursor', '            ++to;\n            ++from;\n        }\n        from += from_pitch - width;\n        to += words - width;',
     '            ++to;\n            ++from;\n        }\n        from += from_pitch - width;\n        to += words - width + 1;'),
    ('piece-palette', 'cursor', "(D8(window, 9) & 2) ? 0x41a568 : 0x41a768", "(D8(window, 9) & 1) ? 0x41a568 : 0x41a768"),
    ('piece-key-pixel', 'cursor', '                                        from->bits[0], palette);', '                                        from->bits[1], palette);'),
    ('piece-clip-left', 'cursor', '        from_x -= to_x;\n', ''),
    ('piece-clip-height', 'cursor', '        height = to->height - to_y;', '        height = to->height - to_y + 1;'),
    ('piece-source-width', 'cursor', 'if (from_x + width > from->width)', 'if (from_x + width > from->width + 1)'),
    ('floating-shown', 'cursor', "(D32(data, 8) & 0x80000000u) && D32(data, 0x7c) == 0", "D32(data, 0x7c) == 0"),
    ('floating-part', 'cursor', '                frame.part[3] = frame.meets[3] - I32(bitmap, 0x10);', '                frame.part[3] = frame.meets[3] - I32(bitmap, 0xc);'),
    ('floating-last', 'cursor', 'window = get_window(get_window(G32(0x41880c), 5), 1);', 'window = get_window(get_window(G32(0x41880c), 5), 0);'),
    ('floating-next', 'cursor', '        window = get_window(window, 3);', '        window = get_window(window, 2);'),
    ('floating-size', 'cursor', 'frame.back.height = GI32(0x4183ac);', 'frame.back.height = GI32(0x4183a8);'),
]


def build(name, file, before, after):
    work = OUT / name
    if work.exists():
        shutil.rmtree(work)
    shutil.copytree(SOURCE, work / 'runtime')
    path = work / 'runtime' / f'host_rt_{file}.c'
    text = path.read_text()
    if text.count(before) != 1:
        return f'before text occurs {text.count(before)} times'
    path.write_text(text.replace(before, after))
    sources = sorted(str(p) for p in (work / 'runtime').glob('*.c'))
    exe = work / 'hostrtdiff.exe'
    result = subprocess.run(CC + ['-I', str(work / 'runtime'), '-o', str(exe), 'tools/difftest/hostrtdiff.c'] + sources,
                            capture_output=True, text=True)
    return None if result.returncode == 0 else result.stderr[-2000:]


def run(name, count):
    exe = OUT / name / 'hostrtdiff.exe'
    environment = dict(os.environ, WINEDEBUG='-all')
    try:
        result = subprocess.run(['wine', str(exe), str(count)], capture_output=True, text=True, errors='replace', timeout=1800,
                                env=environment)
    except subprocess.TimeoutExpired:
        return 'timeout', ''
    text = result.stdout + result.stderr
    if result.returncode == 1 and 'DIFFER' in text:
        return 'caught', text[text.index('DIFFER'):].split('\n')[0]
    if result.returncode == 0 and '0 differ' in text:
        return 'missed', ''
    return 'error', text[-800:]


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 2000
    names = [a for a in sys.argv[1:] if not a.isdigit()]
    breaks = [b for b in BREAKS if not names or b[0] in names]
    OUT.mkdir(parents=True, exist_ok=True)
    results = {}
    for name, file, before, after in breaks:
        problem = build(name, file, before, after)
        if problem:
            results[name] = ('error', problem)
    pending = [b[0] for b in breaks if b[0] not in results]
    with ThreadPoolExecutor(max_workers=max(1, (os.cpu_count() or 2) - 2)) as pool:
        for name, outcome in zip(pending, pool.map(lambda n: run(n, count), pending)):
            results[name] = outcome
    caught = [n for n, (o, _) in results.items() if o == 'caught']
    for name, (outcome, detail) in sorted(results.items()):
        expected = name in EQUIVALENT and outcome == 'missed'
        print(f'{name:24s} {outcome}{" (equivalent)" if expected else ""} {detail.strip()[:160] if outcome != "missed" else ""}')
    unexpected = [n for n, (o, _) in results.items() if o != 'caught' and not (n in EQUIVALENT and o == 'missed')]
    print(f'{len(results)} breaks, {len(caught)} caught, {len(results) - len(caught) - len(unexpected)} equivalent, '
          f'{len(unexpected)} unexpected: {" ".join(sorted(unexpected))}')
    (OUT / 'results.json').write_text(json.dumps(results, indent=1))
    return 1 if unexpected else 0


if __name__ == '__main__':
    sys.exit(main())
