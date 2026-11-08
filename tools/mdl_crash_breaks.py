#!/usr/bin/env python3
"""The decompiled crash models (src/mdl/mdl_crash.c) broken on purpose.

Each break is one edit to a copy of mdl_crash.c or mdl_crash_entry.S, built for the
model(s) it applies to, and must make mdlcrashdiff report a mismatch (a fault counts
only if it is the harness's own failure report, not a crash of the run). Run from a
frozen copy of the tree: it builds into bin/mdl-crash/breaks/.

    python3 tools/mdl_crash_breaks.py [N=2000] [NAME...]

The lifted oracle has its own suite, tools/mdl_crash_lift_breaks.py.

Proven equivalent, so expected to be missed:
  decay-strict     both callers decay only while the word is at least 0x12c (0x320), and the
                   amount starts at 0x20 and only halves: the word never equals it.
  settle-negative  differs only when the sum is exactly 0, and retail then stores the 0 it holds.
  end-signed       the counter at 0x508a starts at 0, gains at most 0xffff a frame and is reset
                   at 0x27ffb or 0x50000: it never reaches the sign bit.
  roll-bank-zero   the cartwheel is picked only for |bank| over 30 degrees, never a bank of 0.
"""
EQUIVALENT = {'decay-strict', 'settle-negative', 'end-signed', 'roll-bank-zero'}
import json
import os
from pathlib import Path
import subprocess
import sys

from gen_mdl_crash import CRASH_NAMES, build_hand

RG, S, X = 'CESSNA182RRG_C', 'CESSNA182S_C', 'EXTRA300S_C'
ALL = (RG, S, X)

# name, models, file ('c' or 'S'), before, after, occurrences of before (all replaced)
BREAKS = [
    ('clamp-limit', ALL, 'c', 'a = limit;\n', 'a = (uint16_t)(limit - 1);\n', 1),
    ('north-unsigned', ALL, 'c', 'add48_32(p + P_NORTH, imul16(speed, G16(G_DELTA)));',
     'add48_32(p + P_NORTH, (uint32_t)speed * G16(G_DELTA));', 1),
    ('east-leftover-lost', (RG,), 'c', 'return east48(p + P_EAST, imul16(speed, G16(G_DELTA)));',
     'east48(p + P_EAST, imul16(speed, G16(G_DELTA))); return 0;', 1),
    ('sink-gain', ALL, 'c', '* 0xfffffff1u;', '* 0xfffffff0u;', 1),
    ('sink-shift', ALL, 'c', 'imul16((uint16_t)(rate >> 15)', 'imul16((uint16_t)(rate >> 16)', 1),
    ('spread-half', ALL, 'c', 'large + (small >> 1)', 'large + (small >> 2)', 1),
    ('tick-once', ALL, 'c', '    SET_V32(rva, V32(rva) + G16(G_DELTA));\n    SET_V32(rva, V32(rva) + G16(G_DELTA));\n',
     '    SET_V32(rva, V32(rva) + G16(G_DELTA));\n', 1),
    ('shrink-zero', ALL, 'c', 'if ((int16_t)a < 0)\n        return;\n    SET_V16(rva, (a ^ sign) - sign);',
     'if ((int16_t)a <= 0)\n        return;\n    SET_V16(rva, (a ^ sign) - sign);', 1),
    ('decay-strict', ALL, 'c', 'if ((int16_t)V16(rva) >= (int16_t)amount)', 'if ((int16_t)V16(rva) > (int16_t)amount)', 1),
    ('place-util-leftover', ALL, 'c', 'return (PTR(UTIL) & 0xffff0000u) | (ax & 0xffff);', 'return ax & 0xffff;', 1),
    ('place-north-sign', ALL, 'c', 'r = imul16(q15(imul16(ch, sp)), sb) - t;', 'r = imul16(q15(imul16(ch, sp)), sb) + t;', 1),
    ('place-halving', ALL, 'c', 'return (uint32_t)((int32_t)imul16(v, scale) >> 1); }',
     'return (uint32_t)((int32_t)imul16(v, scale) >> 2); }', 1),
    ('doubled-shl', ALL, 'c', 'uint16_t new_low = (uint16_t)(low << 1 | low >> 15);', 'uint16_t new_low = (uint16_t)(low << 1);', 1),
    ('place-flat-add', ALL, 'c', 'sub48(piece + P_NORTH, (uint16_t)v,', 'add48(piece + P_NORTH, (uint16_t)v,', 1),
    ('lift-factor', ALL, 'c', 'factor = 0xb;', 'factor = 0xa;', 1),
    ('lift-floor', ALL, 'c', 'if ((int16_t)dx > (int16_t)floor)', 'if ((int16_t)dx >= (int16_t)floor)', 1),
    ('grounded-stage', ALL, 'c', '    if (V32(0x5038) == 2)\n        return 0;', '    if (V32(0x5038) == 3)\n        return 0;', 1),
    ('grounded-sign', ALL, 'c', '(fraction < rd16(piece + P_ALT))) >= 0;', '(fraction < rd16(piece + P_ALT))) > 0;', 1),
    ('ground-compare-signed', ALL, 'c', 'if (rd32(p + 2) > whole)', 'if ((int32_t)rd32(p + 2) > (int32_t)whole)', 1),
    ('ground-compare-48', ALL, 'c', '    if (rd16(p) > fraction)\n        return;',
     '    if (rd32(p + 2) == whole && rd16(p) > fraction)\n        return;', 1),
    ('settle-step', ALL, 'c', '* 0x3334u;', '* 0x3333u;', 1),
    ('settle-negative', ALL, 'c', 'if ((int32_t)(v + step) < 0)', 'if ((int32_t)(v + step) <= 0)', 1),
    ('settle-lower', ALL, 'c', 'sub48(p, 0x3789, 0);', 'sub48(p, 0x3788, 0);', 1),
    ('aircraft-carry', ALL, 'c', 'ground_whole() + (offset >> 16));',
     'ground_whole() + (offset >> 16) + ((uint32_t)ground_fraction() + (uint16_t)offset > 0xffff));', 1),
    ('ground-scale', ALL, 'c', 'return (uint32_t)((int32_t)G32(G_GROUND) >> 8); }', 'return (uint32_t)((int32_t)G32(G_GROUND) >> 7); }', 1),
    ('end-state', ALL, 'c', '    SET_V32(0x508a, 0);\n    GLOBAL[G_CRASH] = 4;\n}', '    SET_V32(0x508a, 0);\n    GLOBAL[G_CRASH] = 3;\n}', 1),
    ('end-limit', ALL, 'c', 'end_after(0x27ffb);', 'end_after(0x27ffc);', 1),
    ('end-signed', ALL, 'c', 'if ((int32_t)V32(0x508a) < (int32_t)limit)', 'if (V32(0x508a) < limit)', 1),
    ('drop-piece', ALL, 'c', 'sub48(alt, 0xa000, 1);', 'sub48(alt, 0x9000, 1);', 1),
    ('gather-last', ALL, 'c', 'for (i = 0; i < CRASH_PIECES; i++) {\n        uint8_t *p = st + PIECE(i);',
     'for (i = 0; i < CRASH_PIECES - 1; i++) {\n        uint8_t *p = st + PIECE(i);', 1),
    ('gather-constant', (RG, X), 'c', 'SET_S32(0x154, 0x40000000u);', 'SET_S32(0x154, 0xc0000000u);', 1),
    ('gather-attitude', (S,), 'c', 'wr32(p + P_PITCH, G32(G_PITCH));', 'wr32(p + P_PITCH, G32(G_BANK));', 1),
    ('piece-114-fixed', ALL, 'c', '#define ZERO_F0 2, {{0x108, 0}, {0x10c, 0}}', '#define ZERO_F0 2, {{0x12c, 0}, {0x130, 0}}', 1),
    ('extra-skip-fixed', (X,), 'c', '#define DEBRIS_SKIP_FROM 0x210', '#define DEBRIS_SKIP_FROM 0x211', 1),
    ('extra-2a0-fixed', (X,), 'c', '{0x2a0, 0x5074, 0x5076, 0x234,', '{0x2a0, 0x5074, 0x5076, 0x2a0,', 1),
    ('extra-order', (X,), 'c',
     '    {0x1c8, 0x5080, 0x507c, 0x1c8, 9, 1, {UP(0x1e0, 0x8000)}, ZERO2(0x1c8)},\n'
     '    {0x180, 0x5080, 0x5076, 0x180, 7, 3, {DOWN(0x198, 0x6000), DOWN(0x19c, 0x6000), DOWN(0x1a0, 0x6000)}, ZERO2(0x180)},\n'
     '    {0x1a4,',
     '    {0x180, 0x5080, 0x5076, 0x180, 7, 3, {DOWN(0x198, 0x6000), DOWN(0x19c, 0x6000), DOWN(0x1a0, 0x6000)}, ZERO2(0x180)},\n'
     '    {0x1c8, 0x5080, 0x507c, 0x1c8, 9, 1, {UP(0x1e0, 0x8000)}, ZERO2(0x1c8)},\n'
     '    {0x1a4,', 1),
    ('piece-north-var', (RG, S), 'c', '{0x1a4, 0x5080, 0x507c, 0x1a4, 8,', '{0x1a4, 0x507a, 0x507c, 0x1a4, 8,', 1),
    ('piece-rate', ALL, 'c', '{0x138, 0x507a, 0x5070, 0x138, 5,', '{0x138, 0x507a, 0x5070, 0x138, 6,', 2),
    ('piece-spin-rate', ALL, 'c', 'UP(0x108, 0x6000)', 'UP(0x108, 0x5000)', 2),
    ('piece-spin-sign', ALL, 'c', 'DOWN(0x228, 0x4000)', 'UP(0x228, 0x4000)', 2),
    ('piece-stop-value', ALL, 'c', '{0x1e4, 0x40000000u}', '{0x1e4, 0}', 1),
    ('last-piece-timer', ALL, 'c', 'if (i == n - 1 && !flew)', 'if (i == n - 1 && (flew || !flew))', 1),
    ('first-piece-carry', ALL, 'c', '        step_north(GLOBAL + G_POS, V16(0x5074));\n        step_east(p, V16(0x5070));',
     '        step_east(p, V16(0x5070));', 1),
    ('first-piece-spin', ALL, 'c', 'SET_S32(0xa4, S32(0xa4) + G16(G_DELTA) * 0x6000u);', 'SET_S32(0xa4, S32(0xa4) + G16(G_DELTA) * 0x4000u);', 1),
    ('slide-cbw', ALL, 'c', '(uint32_t)-((int32_t)(int8_t)V16(0x506c) * 0x640000)', '(uint32_t)-((int32_t)(int16_t)V16(0x506c) * 0x640000)', 1),
    ('slide-bounce', ALL, 'c', '&& G16(G_ALT) <= V16(0x504e)))', '&& G16(G_ALT) < V16(0x504e)))', 1),
    ('slide-stop', ALL, 'c', 'if ((int16_t)V16(0x5072) < 5)', 'if ((int16_t)V16(0x5072) < 6)', 1),
    ('slide-climb-carry', ALL, 'c', 'ax = (uint16_t)((V16(0x506c) << 1) + (V16(0x506a) >> 15));', 'ax = (uint16_t)(V16(0x506c) << 1);', 1),
    ('slide-bank-flag', ALL, 'c', 'else if ((int32_t)V32(0x5058) < (int32_t)0xfc720000u)\n            FLAG(0x20);',
     'else if ((int32_t)V32(0x5058) < (int32_t)0xfc720000u)\n            FLAG(0x40);', 1),
    ('wing-heading-leftover', ALL, 'c', 'SET_G32(G_HEADING, G32(G_HEADING) + (eax >> 16));', 'SET_G32(G_HEADING, G32(G_HEADING) + (eax >> 16 & 0));', 1),
    ('wing-second-timer', ALL, 'c', '&& (int32_t)V32(0x5038) <= 2)', '&& (int32_t)V32(0x5038) <= 1)', 1),
    ('wing-settle', ALL, 'c', 'if ((int32_t)G32(G_PITCH) <= 0xb60000)', 'if ((int32_t)G32(G_PITCH) <= 0xb70000)', 1),
    ('wing-slow', ALL, 'c', 'if (V16(T(0x20)) < 0x12c)', 'if (V16(T(0x20)) < 0x12d)', 1),
    ('roll-end', ALL, 'c', '>= 0x50000)', '>= 0x40000)', 1),
    ('roll-bank-zero', ALL, 'c', 'if ((int32_t)V32(0x5058) >= 0) {', 'if ((int32_t)V32(0x5058) > 0) {', 1),
    ('roll-spread', ALL, 'c', 'if ((int16_t)spread(V16(0x5070), V16(0x5074)) >= 3)', 'if ((int16_t)spread(V16(0x5070), V16(0x5074)) >= 4)', 1),
    ('nose-pitch-leftover', ALL, 'c', 'crash_step(st, G32(0xd48));', 'crash_step(st, 0);', 1),
    ('nose-limit', ALL, 'c', 'SET_V32(T(0), 0x80000000u - V32(0x5054));', 'SET_V32(T(0), 0x7fffffffu - V32(0x5054));', 1),
    ('nose-limit-signed', ALL, 'c', 'if (G32(G_PITCH) > V32(T(0))', 'if ((int32_t)G32(G_PITCH) > (int32_t)V32(T(0))', 1),
    ('nose-back', ALL, 'c', 'pitch > 0x75560000', 'pitch > 0x75550000', 1),
    ('nose-speed', ALL, 'c', 'if ((int16_t)V16(T(0x22)) >= 0x320)', 'if ((int16_t)V16(T(0x22)) >= 0x321)', 1),
    ('dive-fan', ALL, 'c', 'fan(clamp(V16(0x5068), 8), 0x5070, 0x507c, 0x5076);', 'fan(clamp(V16(0x5068), 8), 0x5070, 0x5076, 0x507c);', 1),
    ('dive-rate-step', ALL, 'c', 'ax = (uint16_t)(ax + 1);\n        SET_V16(0x5078, ax);', 'ax = (uint16_t)(ax + 2);\n        SET_V16(0x5078, ax);', 1),
    ('dive-group', (RG, S), 'c', 'group_a[] = {0, 1, 2, 7, 3, 5}, group_b[] = {4, 6, 8, 9}',
     'group_a[] = {0, 1, 2, 3, 5}, group_b[] = {4, 6, 7, 8, 9}', 1),
    ('dive-group-extra', (X,), 'c', 'group_c[] = {11, 12, 13, 14, 15, 16}', 'group_c[] = {11, 12, 13, 14, 15}', 1),
    ('impact-6-side', ALL, 'c', 'if (below ? (int16_t)speeds[k] >= 0', 'if (below ? (int16_t)speeds[k] > 0', 1),
    ('impact-12-speeds', ALL, 'c', 'impact_speeds(2, 1, 0, 0);', 'impact_speeds(2, 1, 1, 0);', 1),
    ('step-impact-12', ALL, 'c', 'else if (G32(G_IMPACT) == 12)\n        crash_impact_12(st);', 'else if (G32(G_IMPACT) == 13)\n        crash_impact_12(st);', 1),
    ('step-bank-level', ALL, 'c', 'if (bank <= 0x71c0000)', 'if (bank < 0x71c0000)', 1),
    ('step-pitch-signed', ALL, 'c', 'else if (V32(0x5054) >= 0xaaa0000)', 'else if ((int32_t)V32(0x5054) >= 0xaaa0000)', 1),
    ('init-impact', ALL, 'c', 'if (G32(G_IMPACT) != 6 && G32(G_IMPACT) != 12)\n        copy_bytes', 'if (G32(G_IMPACT) != 6)\n        copy_bytes', 1),
    ('init-clamp', ALL, 'c', 'clamp(V16(0x5062), 0x14)', 'clamp(V16(0x5062), 0x15)', 1),
    ('init-phase', ALL, 'c', 'SET_S16(CRASH_PHASE, 1);', 'SET_S16(CRASH_PHASE, 0);', 1),
    ('detach-unsigned', ALL, 'c', 'if ((int8_t)GLOBAL[G_CRASH] >= 6)', 'if (GLOBAL[G_CRASH] >= 6)', 1),
    ('update-11b8', (S,), 'c', '    SET_S16(0x7e, G16(0x11b8));\n', '', 1),
    ('update-move', ALL, 'c', 'host_move(st + 0x28, GLOBAL + 0x5b0, 0x18);', 'host_move(st + 0x28, GLOBAL + 0x5b0, 0x14);', 1),
    ('chain-phase', ALL, 'c', 'rd16(st + CRASH_PHASE) == 2', 'rd16(st + CRASH_PHASE) == 1', 1),
    ('east-unsigned', ALL, 'c', 'wr32(in + 4, (uint32_t)(int32_t)(int16_t)(metres >> 16));', 'wr32(in + 4, (uint16_t)(metres >> 16));', 1),
    ('add48-carry', ALL, 'c', 'wr32(p + 2, rd32(p + 2) + whole + (low >> 16));', 'wr32(p + 2, rd32(p + 2) + whole);', 1),
    ('sub48-borrow', ALL, 'c', 'wr32(p + 2, rd32(p + 2) - whole - (low < fraction));', 'wr32(p + 2, rd32(p + 2) - whole);', 1),
    ('update-edi', ALL, 'S', '    movl _ImportTable+12, %edi\n', '', 1),
    ('chain-ecx', ALL, 'S', '    movl 12(%esp), %ecx\n', '', 1),
]
MARKERS = ('crash state', 'crash data', 'crash GLOBAL', 'crash calls', 'event count', 'crash registers',
           'attach writes', 'canary')


def main():
    root = Path.cwd(); output = root / 'bin/mdl-crash'
    records = {Path(r['path']).stem: r for r in json.loads((output / 'modules.json').read_text())}
    source = Path('src/mdl/mdl_crash.c').read_text()
    entry = Path('src/mdl/mdl_crash_entry.S').read_text()
    env = dict(os.environ, WINEPREFIX=str(root / 'bin/mdl-diff/wineprefix'), WINEARCH='win32', WINEDEBUG='-all')
    args = sys.argv[1:]
    vectors = next((a[2:] for a in args if a.startswith('N=')), '2000')
    wanted = {a for a in args if not a.startswith('N=')}
    caught = missed = equivalent = 0
    for name, models, kind, before, after, count in BREAKS:
        if wanted and name not in wanted:
            continue
        text = source if kind == 'c' else entry
        if text.count(before) != count:
            raise ValueError(f'{name}: anchor found {text.count(before)} times, expected {count}')
        broken = text.replace(before, after)
        directory = output / 'breaks' / name
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'mdl_crash.c').write_text(broken if kind == 'c' else source)
        (directory / 'mdl_crash_entry.S').write_text(broken if kind == 'S' else entry)
        results = []
        for model in models:
            record = dict(records[model])
            data = output / model
            record['rebuilt'] = str((directory / (model + '.MDL')).resolve())
            Path(record['rebuilt']).unlink(missing_ok=True)
            try:
                build_hand(record, data, CRASH_NAMES.index(model), source=directory / 'mdl_crash.c',
                           entry=directory / 'mdl_crash_entry.S')
            except RuntimeError as error:
                results.append((model, False, 'does not build: ' + str(error).strip().splitlines()[-1]))
                continue
            run = subprocess.run(['wine', str(output / 'mdlcrashdiff.exe'), vectors, 'relocate', record['rebuilt']],
                                 env=env, text=True, capture_output=True, timeout=1800)
            transcript = run.stdout + run.stderr
            (directory / (model + '.log')).write_text(transcript)
            hit = run.returncode == 1 and any(marker in transcript for marker in MARKERS)
            results.append((model, hit, transcript.strip().splitlines()[-1] if transcript.strip() else ''))
            if hit:
                break
        if any(hit for _, hit, _ in results):
            caught += 1
            model, _, line = next(r for r in results if r[1])
            print(f'{name}: caught ({model}: {line})', flush=True)
        elif name in EQUIVALENT:
            equivalent += 1
            print(f'{name}: not caught, proven equivalent', flush=True)
        else:
            missed += 1
            print(f'{name}: MISSED ' + '; '.join(f'{m}: {l}' for m, _, l in results), flush=True)
    print(f'{caught + missed + equivalent} breaks, {caught} caught, {equivalent} proven equivalent, {missed} missed')
    return 1 if missed else 0


if __name__ == '__main__':
    sys.exit(main())
