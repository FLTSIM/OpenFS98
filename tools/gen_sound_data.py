#!/usr/bin/env python3
"""Extract SOUND.DLL's .data image and .rdata constants into C.

SOUND keeps its ImportTable, Linkage and every byte of state in one .data
section, and the reimplementation keeps one image laid out the same way.
Unlike ACONTAIN's, this image holds pointers -- 140-odd into itself (the 29
sound templates' names and default parameter blocks, the Morse table's
codes) and five to code (the templates' loaders and a block's free function)
-- so it is emitted a dword at a time from the file's own relocations: a
plain value as a number, a pointer into .data as an offset into the image,
a pointer to code as the C function retail's address belongs to.

Two outputs:

  sound_data.inc    .data from +0x78 (after ImportTable and Linkage, which
                    the C defines itself) to the end of the section, as
                    `void *` words.
  sound_consts.inc  every double in .rdata, and the one in .data (2^48 at
                    0x208d6fe0), as K_<offset> hex-float literals -- exact,
                    whatever the compiler's decimal conversion.

Usage: tools/gen_sound_data.py [orig/Modules/SOUND.DLL] [src/sound]
"""
import os, struct, sys

SRC = sys.argv[1] if len(sys.argv) > 1 else 'orig/Modules/SOUND.DLL'
DST = sys.argv[2] if len(sys.argv) > 2 else 'src/sound'

DATA_VA, RDATA_VA, TEXT_VA = 0x208d6000, 0x208d5000, 0x208d1000
SIZE = 0x177c                   # virtual; the file holds the first 0x1400
RAW = 0x1400

# Retail's code addresses that .data holds, and what they are in sound.c.
CODE = {
    0x208d1ce0: 'snd_free_block',
    0x208d2600: 'snd_tmpl_basic',
    0x208d2670: 'snd_tmpl_extra',
    0x208d2750: 'snd_tmpl_rate',
    0x208d2ab0: 'snd_tmpl_engine',
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
assert base + va == DATA_VA and vsz == SIZE and rsz == RAW, '.data moved or resized'
data = b[ro:ro + RAW] + bytes(SIZE - RAW)
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
cstr = lambda o: data[o:data.index(b'\0', o)].decode('latin1')

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
assert all(o % 4 == 0 for o in drel), 'unaligned pointer in .data'

# ImportTable: host, GLOBAL, ACONTAIN, CHAIN, UTIL, terminator.
assert [u32(8 * k) for k in range(6)] == [1, 2, 23, 4, 20, 0], 'ImportTable'

# Linkage, as sound.c's struct describes it.
LINKAGE = [39, 0x208d1020, 0x208d1030, 0, 0, 0,
           0x208d1ad0, 0x208d1ea0, 0x208d2190, 0x208d3810, 0x208d1470,
           0x208d1400, 0x208d1450, 0x208d1370, 0x208d13d0, 0x208d12b0,
           0x208d1310, 0x208d1680]
assert [u32(0x30 + 4 * k) for k in range(18)] == LINKAGE, 'Linkage'

STRINGS = {0x6c6c: 'laglide', 0x6ecc: 'SOUND_ENGINE', 0x6f00: 'PREFS',
           0x6f08: 'SOUND_FADER%d', 0x6f18: ', ', 0x6f1c: 'viewpoint',
           0x6f28: 'panning', 0x6f30: 'minimum_volume', 0x6f40: 'maximum_volume',
           0x6f50: 'initial_volume', 0x6f60: 'filename', 0x6f6c: 'flags',
           0x6f74: 'extra', 0x6f7c: 'maximum_speed', 0x6f8c: 'minimum_speed',
           0x6f9c: 'maximum_rate', 0x6fac: 'minimum_rate', 0x6fbc: 'vparams',
           0x6fc4: 'rparams', 0x6fcc: 'link', 0x6fd4: 'type'}
for off, text in STRINGS.items():
    assert cstr(off - 0x6000) == text, 'string at %#x' % (off + 0x208d0000)

# The 29 templates: { name, loader, 0x44-byte record }, the loader's
# address from 0x208d637c stepping 0x4c while below 0x208d6c18 -- the loop
# at .text+0x8e8.  Ids 1..32 with gaps, every loader one of the four.
ids = []
for t in range(0x637c, 0x6c18, 0x4c):
    o = t - 0x6000
    assert o - 4 in drel and o in drel, 'template pointers'
    assert u32(o) in CODE and CODE[u32(o)].startswith('snd_tmpl_'), 'loader'
    ids.append(u32(o + 4))
assert ids == [1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
               20, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 32], 'template ids'

# The Morse table at 0x208d6fe8: { character, code string } to a zero.
morse = {}
o = 0x6fe8 - 0x6000
while data[o]:
    assert o + 4 in drel, 'Morse code pointer'
    morse[chr(data[o])] = cstr(u32(o + 4) - DATA_VA)
    o += 8
assert morse['A'] == '.-' and morse['0'] == '-----' and len(morse) == 39, 'Morse table'

# Initial state the C depends on.
assert u32(0x0c14) == 1 and u32(0x0c18) == 1 and u32(0x0c68) == 1, 'flags'
assert u32(0x1128) == 10000, 'Morse volume'
assert data[0x1230:0x1274] == bytes(0x44), 'default record'
assert data[0x1274:] == bytes(SIZE - 0x1274), 'state past the default record'

out = []
for o in range(0x78, SIZE, 4):
    v = u32(o)
    if o in drel:
        if DATA_VA <= v < DATA_VA + SIZE:
            out.append('(void *)((char *)&snd_image + 0x%04x)' % (v - DATA_VA))
        elif v in CODE:
            out.append('(void *)%s' % CODE[v])
        else:
            raise SystemExit('pointer at +%#x to %#x is neither .data nor known code' % (o, v))
    else:
        assert not (TEXT_VA <= v < DATA_VA + SIZE and v & 3 == 0 and v > 0x208d0000), \
            'unrelocated value at +%#x looks like a pointer' % o
        out.append('(void *)0x%08xu' % v)

with open(os.path.join(DST, 'sound_data.inc'), 'w') as f:
    f.write('/* Generated by tools/gen_sound_data.py from %s -- do not edit.\n' % SRC)
    f.write(' *\n'
            ' * SOUND\'s .data from +0x78 to +0x%x (the file\'s 0x1400, then zeros), a dword at a time: the static\n'
            ' * engine sub-sounds from +0x78, the 29 templates from +0x378, strings,\n'
            ' * the Morse table at +0xfe8, and the state from +0x1128.  Pointers\n'
            ' * are relocated as retail\'s are.\n'
            ' */\n' % SIZE)
    for k in range(0, len(out)):
        f.write('    /* +%04x */ %s,\n' % (0x78 + 4 * k, out[k]))

# .rdata: the doubles from its start to the export directory.
va, rvsz, rrsz, rro = secs['.rdata']
assert base + va == RDATA_VA
rdata = b[rro:rro + rvsz]
with open(os.path.join(DST, 'sound_consts.inc'), 'w') as f:
    f.write('/* Generated by tools/gen_sound_data.py from %s -- do not edit.\n' % SRC)
    f.write(' *\n'
            ' * The doubles in .rdata from 0x208d5000, by offset, and the one in\n'
            ' * .data at 0x208d6fe0.  Hex literals: exact.\n'
            ' */\n')
    for o in range(0, 0x168, 8):
        v = struct.unpack_from('<d', rdata, o)[0]
        f.write('#define K_%03X %-24s /* %r */\n' % (o, '(' + v.hex() + ')', v))
    v = struct.unpack_from('<d', data, 0x0fe0)[0]
    assert v == 2.0 ** 48
    f.write('#define K_D6FE0 %-22s /* %r */\n' % ('(' + v.hex() + ')', v))
