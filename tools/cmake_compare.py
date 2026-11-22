#!/usr/bin/env python3
"""Compare two trees' bin/ directories: the Makefile build against the CMake build (Story 1.1).

usage: cmake_compare.py <makefile-tree> <cmake-tree>

Files must match byte for byte, except for the differences that cannot be avoided, which are
listed with their reason: the PE TimeDateStamp and CheckSum (header, export, resource and
debug directories), and the tree's own absolute path wherever a generator baked it in
(modules.json, the MDL fixtures inside three difftests). Exit 1 on anything else.
"""
import os, struct, sys


def norm_pe(d):
    d = bytearray(d)
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    if d[:2] != b'MZ' or d[pe:pe + 4] != b'PE\0\0':
        return bytes(d)
    struct.pack_into('<I', d, pe + 8, 0)
    opt = pe + 24
    magic = struct.unpack_from('<H', d, opt)[0]
    struct.pack_into('<I', d, opt + 64, 0)
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    osz = struct.unpack_from('<H', d, pe + 20)[0]
    dd = opt + (96 if magic == 0x10b else 112)
    secs = []
    for i in range(nsec):
        s = opt + osz + i * 40
        vs, va, rs, rp = struct.unpack_from('<IIII', d, s + 8)
        secs.append((va, max(vs, rs), rp))

    def off(rva):
        for va, sz, rp in secs:
            if va <= rva < va + sz:
                return rp + rva - va

    def walk(base, o):
        struct.pack_into('<I', d, o + 4, 0)
        a, b = struct.unpack_from('<HH', d, o + 12)
        for i in range(a + b):
            e = struct.unpack_from('<I', d, o + 20 + i * 8)[0]
            if e & 0x80000000:
                walk(base, base + (e & 0x7fffffff))

    for idx in (0, 2, 6):
        rva, size = struct.unpack_from('<II', d, dd + idx * 8)
        o = off(rva) if rva else None
        if o is None:
            continue
        if idx == 0:
            struct.pack_into('<I', d, o + 4, 0)
        elif idx == 2:
            walk(o, o)
        else:
            for k in range(size // 28):
                struct.pack_into('<I', d, o + k * 28 + 4, 0)
    return bytes(d)


def load(root, rel):
    d = open(os.path.join(root, 'bin', rel), 'rb').read()
    if d[:2] == b'MZ':
        d = norm_pe(d)
    return d.replace(root.encode(), b'<TREE>')


def files(root):
    top = os.path.join(root, 'bin')
    return {os.path.relpath(os.path.join(p, f), top) for p, _, fs in os.walk(top) for f in fs}


a, b = (os.path.abspath(x) for x in sys.argv[1:3])
fa, fb = files(a), files(b)
bad = 0
for rel in sorted(fa | fb):
    if rel not in fb or rel not in fa:
        print('only in %s: %s' % ('makefile' if rel in fa else 'cmake', rel)); bad += 1
    elif load(a, rel) != load(b, rel):
        print('DIFFERS:', rel); bad += 1
print('%d files compared, %d unexplained differences' % (len(fa & fb), bad))
sys.exit(1 if bad else 0)
