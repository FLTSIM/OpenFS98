#!/usr/bin/env python3
"""Extract DYNAMIC.DLL's .data image into C.

DYNAMIC keeps its ImportTable, Linkage and state in one 0x250-byte .data
section, and the reimplementation keeps one image laid out the same way.  The
Linkage is mostly the bytecode interpreter's dispatch table: from Linkage+0x20
on, entry n is the handler for opcode n, 68 of them.  Retail's handler
addresses are emitted as the C functions that transliterate them, so the
table keeps retail's sharing -- one function wherever retail repeats an
address (the 2-byte `stc; ret` stands in 19 slots).

One output, dynamic_data.inc: the 68 handler names, then the image from
+0x170 (the six dynamic-library ids) to the end of the section as bytes.

Usage: tools/gen_dynamic_data.py [orig/Modules/DYNAMIC.DLL] [src/dynamic/dynamic_data.inc]
"""
import struct, sys

SRC = sys.argv[1] if len(sys.argv) > 1 else 'orig/Modules/DYNAMIC.DLL'
DST = sys.argv[2] if len(sys.argv) > 2 else 'src/dynamic/dynamic_data.inc'

DATA_VA = 0x206a6000
SIZE = 0x250
NOPS = 0x44

# Retail's handler addresses and the functions in dynamic.c that are them.
HANDLERS = {
    0x206a16f7: 'op_halt',
    0x206a16f9: 'op_ease_pitch',
    0x206a1700: 'op_ease_bank',
    0x206a1707: 'op_ease_heading',
    0x206a1cfe: 'op_flag4',
    0x206a1754: 'op_set_pitch',
    0x206a1763: 'op_set_bank',
    0x206a1772: 'op_set_heading',
    0x206a1a2b: 'op_move_e',
    0x206a1a32: 'op_move_alt',
    0x206a1a39: 'op_move_n',
    0x206a199b: 'op_move_e_n',
    0x206a19a7: 'op_move_e_alt',
    0x206a19b3: 'op_move_alt_n',
    0x206a19e2: 'op_move_3',
    0x206a1a99: 'op_ramp_e',
    0x206a1aa0: 'op_ramp_alt',
    0x206a1aa7: 'op_ramp_n',
    0x206a1b2f: 'op_ramp_e_n',
    0x206a1b3b: 'op_ramp_e_alt',
    0x206a1b47: 'op_ramp_alt_n',
    0x206a1b76: 'op_ramp_3',
    0x206a1bbf: 'op_turn',
    0x206a18f2: 'op_jump',
    0x206a18fa: 'op_call',
    0x206a190a: 'op_return',
    0x206a187b: 'op_wait',
    0x206a18ed: 'op_hold',
    0x206a194a: 'op_release',
    0x206a1d24: 'op_jne',
    0x206a1d2d: 'op_je',
    0x206a1d3f: 'op_jge',
    0x206a1d36: 'op_jle',
    0x206a1d1f: 'op_nop',
    0x206a186a: 'op_param',
    0x206a3d58: 'op_object',
    0x206a1781: 'op_place',
    0x206a17e8: 'op_origin',
    0x206a1e74: 'op_path_mark',
    0x206a1eaa: 'op_path',
    0x206a1eb5: 'op_path_slow',
    0x206a1ec0: 'op_path_n',
    0x206a2232: 'op_meet',
    0x206a22ff: 'op_to_aircraft',
    0x206a297e: 'op_refuel',
    0x206a31fe: 'op_circuit',
    0x206a2cf4: 'op_turn_to',
    0x206a221d: 'op_hide',
    0x206a361e: 'op_jnear',
}

b = open(SRC, 'rb').read()
e = struct.unpack_from('<I', b, 0x3c)[0]
nsec = struct.unpack_from('<H', b, e + 6)[0]
opt = e + 24
base = struct.unpack_from('<I', b, opt + 28)[0]
so = opt + struct.unpack_from('<H', b, e + 20)[0]
secs = {}
for _ in range(nsec):
    name = b[so:so + 8].rstrip(b'\0').decode()
    vsz, va, rsz, ro = struct.unpack_from('<IIII', b, so + 8)
    secs[name] = (va, vsz, rsz, ro)
    so += 40

va, vsz, rsz, ro = secs['.data']
assert base + va == DATA_VA and vsz == SIZE, '.data moved or resized'
data = b[ro:ro + min(rsz, SIZE)] + bytes(max(0, SIZE - rsz))
u32 = lambda o: struct.unpack_from('<I', data, o)[0]

# The relocations: every dword the loader patches.
rva, rvsz, rrsz, rro = secs['.reloc']
r = b[rro:rro + rvsz]
relocs, i = set(), 0
while i + 8 <= len(r):
    page, sz = struct.unpack_from('<II', r, i)
    if sz == 0:
        break
    for j in range(8, sz, 2):
        ent = struct.unpack_from('<H', r, i + j)[0]
        if ent >> 12 == 3:
            relocs.add(base + page + (ent & 0xfff))
        else:
            assert ent >> 12 == 0, 'unexpected relocation type'
    i += sz
drel = sorted(a - DATA_VA for a in relocs if DATA_VA <= a < DATA_VA + SIZE)

# ImportTable: host, GLOBAL, CHAIN, FE, G3D, UTIL, MULTIP, terminator.
assert [u32(8 * k) for k in range(8)] == [1, 2, 4, 18, 14, 20, 35, 0], 'ImportTable'
assert all(u32(8 * k + 4) == 0 for k in range(8)), 'ImportTable links'

# Linkage at +0x40: id 6, init, deinit, three empty slots, the frame, an
# empty slot, then the dispatch table.
L = 0x40
assert u32(L) == 6, 'module id'
assert (u32(L + 4), u32(L + 8)) == (0x206a1020, 0x206a1027), 'init/deinit'
assert [u32(L + o) for o in (0x0c, 0x10, 0x14, 0x1c)] == [0, 0, 0, 0], 'empty slots'
assert u32(L + 0x18) == 0x206a1188, 'frame'
ops = [u32(L + 0x20 + 4 * n) for n in range(NOPS)]
assert set(ops) <= set(HANDLERS), 'unknown handler %s' % [hex(a) for a in set(ops) - set(HANDLERS)]
assert len(set(ops)) == 49 and ops.count(0x206a16f7) == 19, 'handler sharing'

# Every relocated dword is in ImportTable/Linkage; the rest is plain data.
assert all(L + 4 <= o < L + 0x20 + 4 * NOPS for o in drel), 'pointer outside Linkage'
assert len(drel) == 3 + NOPS, 'relocation count'

# The six library ids from +0x170 (id word, then the 16-byte object id),
# which op_object looks up; then the state, all zero but the step count at
# +0x228, which starts at 1.
LIB, LIB_END = 0x170, 0x1e8
for k in range(6):
    assert struct.unpack_from('<I', data, LIB + 0x14 * k)[0] == 0xfe0b + k, 'library id %d' % k
assert u32(0x228) == 1 and not any(data[LIB_END:0x228] + data[0x22c:]), 'initial state'

with open(DST, 'w') as f:
    f.write('/* Generated by tools/gen_dynamic_data.py from %s -- do not edit.\n' % SRC)
    f.write(' *\n * DYNAMIC\'s .data: the dispatch table at Linkage+0x20 (opcode n\'s\n')
    f.write(' * handler, as the function that transliterates it), then the image from\n')
    f.write(' * +0x170 -- the six dynamic-library ids -- to +0x250, as bytes.\n */\n')
    f.write('#ifdef DYN_OPS\n')
    for n, a in enumerate(ops):
        f.write('    /* %02x */ %s,\n' % (n, HANDLERS[a]))
    f.write('#else\n')
    for o in range(LIB, SIZE, 16):
        f.write('    /* +%04x */ %s\n' % (o, ' '.join('0x%02x,' % x for x in data[o:min(o + 16, SIZE)])))
    f.write('#endif\n')
print('wrote', DST)
