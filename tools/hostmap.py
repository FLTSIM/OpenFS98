#!/usr/bin/env python3
"""Map FLTSIM98.EXE's service table.

The host publishes the same kind of table the modules do -- id 1 followed by
service pointers -- but has no export to find it by, so it is located the same
way the module tables were: a dword 1 in .data followed by a long run of
pointers into .text.

Most interesting output is the thunks.  A service that is nothing but
`jmp dword ptr [iat]` is an import re-exported under a slot number, and the
import's name says exactly what the service is -- which names a chunk of the
host's API for free.  The rest are sized and reported with whatever imports
they reference, so the big ones can be told from the small.

Usage: tools/hostmap.py [orig/FLTSIM98.EXE]
"""
import struct, sys

SRC = sys.argv[1] if len(sys.argv) > 1 else 'orig/FLTSIM98.EXE'
b = open(SRC, 'rb').read()
e = struct.unpack_from('<I', b, 0x3c)[0]
opt = e + 24
nsec = struct.unpack_from('<H', b, e + 6)[0]
base = struct.unpack_from('<I', b, opt + 28)[0]
so = opt + struct.unpack_from('<H', b, e + 20)[0]
secs = []
for _ in range(nsec):
    nm = b[so:so + 8].rstrip(b'\0').decode()
    vsz, va, rsz, ro = struct.unpack_from('<IIII', b, so + 8)
    secs.append((nm, vsz, va, rsz, ro))
    so += 40


def off(va):
    for nm, vsz, v, rsz, ro in secs:
        if base + v <= va < base + v + max(vsz, rsz):
            return ro + (va - base - v)
    return None


txt = [s for s in secs if s[0] == '.text'][0]
LO, HI = base + txt[2], base + txt[2] + txt[1]

# import address table -> name
iat = {}
iva = struct.unpack_from('<I', b, opt + 96 + 8)[0]
o = off(base + iva)
while True:
    nr = struct.unpack_from('<I', b, o + 12)[0]
    if not nr:
        break
    dll = b[off(base + nr):].split(b'\0')[0].decode()
    ft = struct.unpack_from('<I', b, o + 16)[0]
    t, rva = off(base + ft), ft
    while True:
        v = struct.unpack_from('<I', b, t)[0]
        if not v:
            break
        if v & 0x80000000:
            iat[base + rva] = '%s:#%d' % (dll, v & 0xffff)
        else:
            fo = off(base + v)
            iat[base + rva] = '%s:%s' % (dll, b[fo + 2:].split(b'\0')[0].decode())
        t += 4
        rva += 4
    o += 20

# find the table
table = None
for nm, vsz, va, rsz, ro in secs:
    if nm != '.data':
        continue
    data = b[ro:ro + min(vsz, rsz)]
    for p in range(0, len(data) - 8, 4):
        if struct.unpack_from('<I', data, p)[0] != 1:
            continue
        good = sum(1 for k in range(1, 100)
                   if p + k * 4 + 4 <= len(data)
                   and LO <= struct.unpack_from('<I', data, p + k * 4)[0] < HI)
        if good >= 40:
            table = base + va + p
            break
if table is None:
    sys.exit('no service table found')

# Walk until something that is neither a code pointer nor zero: the table has
# gaps, so a zero is a hole rather than the end.
o = off(table)
slots = {}
i = 1
while True:
    v = struct.unpack_from('<I', b, o + i * 4)[0]
    if v and not (LO <= v < HI):
        break
    if v:
        slots[i * 4] = v
    i += 1

addrs = sorted(set(slots.values()))
size = {a: (addrs[j + 1] - a if j + 1 < len(addrs) else HI - a)
        for j, a in enumerate(addrs)}

print('service table at 0x%08x: %d slots, %d distinct functions'
      % (table, len(slots), len(addrs)))

def classify(a):
    """Name a service from its code shape, where the shape is simple enough.

    The host's table turns out to be mostly accessors onto host globals, so a
    handful of patterns name a large part of it without reading any of it by
    hand."""
    fo = off(a)
    c = b[fo:fo + 16]

    def isdata(v):
        for nm, vsz, va, rsz, ro in secs:
            if nm in ('.data', '.rdata') and base + va <= v < base + va + vsz:
                return True
        return False

    if c[0:2] == b'\xff\x25':
        tgt = struct.unpack_from('<I', c, 2)[0]
        return 'thunk', iat.get(tgt, 'import 0x%08x' % tgt)
    if c[0] == 0xb8 and c[5] == 0xc3:
        v = struct.unpack_from('<I', c, 1)[0]
        if isdata(v):
            return 'addr-of-global', '&0x%08x' % v
        return 'constant', 'returns %d' % v
    if c[0] == 0xa1 and c[5] == 0xc3:
        return 'get-global', '*0x%08x' % struct.unpack_from('<I', c, 1)[0]
    if c[0:2] == b'\xc7\x05' and c[10] == 0xc3:
        return 'set-global', '*0x%08x = %d' % (struct.unpack_from('<I', c, 2)[0],
                                               struct.unpack_from('<I', c, 6)[0])
    if c[0:2] == b'\x33\xc0' and c[2] == 0xc3:
        return 'stub', 'returns 0, no args'
    if c[0:2] == b'\x33\xc0' and c[2] == 0xc2:
        return 'stub', 'returns 0, %d bytes of args' % struct.unpack_from('<H', c, 3)[0]
    return None, None


thunks, plain = [], []
for s in sorted(slots):
    a = slots[s]
    fo = off(a)
    if b[fo:fo + 2] == b'\xff\x25':                 # jmp dword ptr [addr]
        tgt = struct.unpack_from('<I', b, fo + 2)[0]
        thunks.append((s, a, iat.get(tgt, '?0x%08x' % tgt)))
    else:
        plain.append((s, a, size[a]))

print('\n%d services are plain import thunks:' % len(thunks))
for s, a, nm in thunks:
    print('  +0x%03x  %s' % (s, nm))

named = {}
for s_ in sorted(slots):
    kind, what = classify(slots[s_])
    if kind and kind != 'thunk':
        named.setdefault(kind, []).append((s_, what))
total_named = sum(len(v) for v in named.values()) + len(thunks)
print('\n%d of %d services named from their shape:' % (total_named, len(slots)))
for kind in sorted(named):
    rows = named[kind]
    print('  %s (%d):' % (kind, len(rows)))
    for s_, what in rows:
        print('    +0x%03x  %s' % (s_, what))

print('\n%d services have their own code:' % len(plain))
by_size = sorted(plain, key=lambda r: -r[2])
print('  largest:')
for s, a, sz in by_size[:12]:
    print('    +0x%03x  0x%08x  %5d bytes' % (s, a, sz))
# Which libraries does each service reach for?  A service that calls into
# USER32 is windowing, one that calls DDRAW is graphics, and so on -- which is
# the breakdown a native host has to cover.
def libs_used(a, sz):
    fo = off(a)
    found = set()
    for q in range(fo, min(fo + sz, len(b)) - 6):
        if b[q] == 0xff and b[q + 1] in (0x15, 0x25):
            tgt = struct.unpack_from('<I', b, q + 2)[0]
            nm = iat.get(tgt)
            if nm:
                found.add(nm.split(':')[0].upper().replace('.DLL', ''))
    return found

subsys = {}
for s_ in sorted(slots):
    a = slots[s_]
    used = libs_used(a, size[a])
    key = ' '.join(sorted(used)) if used else '(no library calls)'
    subsys.setdefault(key, []).append(s_)
print('\nservices by the libraries they call:')
for key in sorted(subsys, key=lambda k: -len(subsys[k])):
    rows = subsys[key]
    print('  %-28s %3d  %s' % (key, len(rows),
          ' '.join('+0x%03x' % x for x in rows[:10]) + (' ...' if len(rows) > 10 else '')))

small = sorted([r for r in by_size if r[2] <= 48], key=lambda r: r[0])
print('  %d are 48 bytes or less -- the cheap tier:' % len(small))
for s_, a, sz in small:
    print('    +0x%03x  0x%08x  %3d bytes' % (s_, a, sz))

# Sizes are the gap to the next *table-referenced* address, so they are upper
# bounds: internal helpers that no slot points at fall inside the gaps.
