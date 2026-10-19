#!/usr/bin/env python3
"""Extract CONTROLS.DLL's .data image, its .rdata constants and the command
table it keeps in .text, into C.

CONTROLS keeps one .data section of 0xc020 bytes, 0xc00 of them initialised:
ImportTable at +0x0, Linkage at +0x78, the state of the hand-written half
from +0xe0, the keyboard and joystick section names, the DirectInput data
format's object table at +0x510 and the force-feedback key names.  The rest
is the C half's buffers.  The reimplementation keeps that layout so the
harness can compare the whole image.  Its relocations are code pointers
(zeroed here and installed by controls.c), two pointers into the image and
44 pointers into .rdata (likewise).

The second file carries .rdata up to the export directory: the DirectInput
GUIDs, the joystick data format (whose object-table pointer at +0x264 is
zeroed here and installed), and the doubles.

The third is the command table, 383 records of 20 bytes at .text+0x15f9
(its address at .text+0x15f1, its count at +0x15f5): the handler, a kind
mask, the command id, its display name's string id and the name, which the hand-written
assembly keeps among its code.  Handlers are written as H(offset) and names
as string literals.

Usage: tools/gen_controls_data.py [orig/Modules/CONTROLS.DLL]
                                  [src/controls/controls_data.inc]
                                  [src/controls/controls_rdata.inc]
                                  [src/controls/controls_table.inc]
                                  [src/controls/controls_pointers.inc]

The fourth lists .data's pointers into .rdata (the data format's object
GUIDs), for controls.c to install.
"""
import struct
import sys

SRC = sys.argv[1] if len(sys.argv) > 1 else "orig/Modules/CONTROLS.DLL"
DST = sys.argv[2] if len(sys.argv) > 2 else "src/controls/controls_data.inc"
RDS = sys.argv[3] if len(sys.argv) > 3 else "src/controls/controls_rdata.inc"
TBL = sys.argv[4] if len(sys.argv) > 4 else "src/controls/controls_table.inc"
PTR = sys.argv[5] if len(sys.argv) > 5 else "src/controls/controls_pointers.inc"

BASE = 0x20670000
TEXT_VA = 0x20671000
TEXT_SIZE = 0xbd16
RDATA_VA = 0x2067d000
RDATA_SIZE = 0x2b8          # up to the export directory
DATA_VA = 0x2067e000
SIZE = 0xc020
IMPORTS = 0x000
LINKAGE = 0x078
TABLE_AT = 0x15f1           # .text: the table's address, then its count
TABLE = 0x15f9
COUNT = 383
RDATA_POINTER = 0x264       # .rdata: the data format's object table

blob = open(SRC, "rb").read()
pe = struct.unpack_from("<I", blob, 0x3C)[0]
section_count = struct.unpack_from("<H", blob, pe + 6)[0]
optional = pe + 24
image_base = struct.unpack_from("<I", blob, optional + 28)[0]
section = optional + struct.unpack_from("<H", blob, pe + 20)[0]
sections = {}
for _ in range(section_count):
    name = blob[section:section + 8].rstrip(b"\0").decode()
    virtual_size, rva, raw_size, raw = struct.unpack_from(
        "<IIII", blob, section + 8)
    sections[name] = (rva, virtual_size, raw_size, raw)
    section += 40
assert image_base == BASE, "image base moved"


def read(name, va, size):
    rva, vsize, raw_size, raw = sections[name]
    start = va - BASE - rva
    out = bytearray(blob[raw + start:raw + min(raw_size, start + size)])
    out.extend(bytes(size - len(out)))
    return out


rva, vsize, raw_size, raw = sections[".data"]
assert BASE + rva == DATA_VA and vsize == SIZE, ".data moved or resized"
rva, vsize, raw_size, raw = sections[".text"]
assert BASE + rva == TEXT_VA and vsize == TEXT_SIZE, ".text moved or resized"

data = read(".data", DATA_VA, SIZE)
rdata = read(".rdata", RDATA_VA, RDATA_SIZE)
text = read(".text", TEXT_VA, TEXT_SIZE)
u32 = lambda b, o: struct.unpack_from("<I", b, o)[0]

assert [u32(data, IMPORTS + 8 * i) for i in range(15)] == \
    [1, 2, 23, 21, 24, 4, 7, 13, 3, 17, 15, 39, 20, 10, 0], "ImportTable"
assert u32(data, LINKAGE) == 16, "module id"
assert data[0x192:0x19b] == b"CONTROLS\0" and data[0x19b:0x1a4] == b"PAN_RATE\0"
assert u32(text, TABLE_AT) == TEXT_VA + TABLE and u32(text, TABLE_AT + 4) == COUNT

# relocations
rva, size = struct.unpack_from("<II", blob, optional + 96 + 5 * 8)
rrva, rvsize, rraw_size, rraw = sections[".reloc"]
at, end, relocs = rraw + rva - rrva, rraw + rva - rrva + size, []
while at < end:
    page, block = struct.unpack_from("<II", blob, at)
    if block == 0:
        break
    for i in range((block - 8) // 2):
        w = struct.unpack_from("<H", blob, at + 8 + 2 * i)[0]
        if w >> 12 == 3:
            relocs.append(BASE + page + (w & 0xFFF))
    at += block

code, inner, constant = [], [], []
for r in relocs:
    if not DATA_VA <= r < DATA_VA + SIZE:
        continue
    o, v = r - DATA_VA, u32(data, r - DATA_VA)
    if TEXT_VA <= v < TEXT_VA + TEXT_SIZE:
        code.append(o)
    elif DATA_VA <= v < DATA_VA + SIZE:
        inner.append((o, v - DATA_VA))
    elif RDATA_VA <= v < RDATA_VA + 0x300:
        constant.append((o, v - RDATA_VA))
    else:
        raise SystemExit("unexpected relocation at .data+%#x" % o)
    data[o:o + 4] = bytes(4)
assert len(code) == 24 and inner == [(0x344, 0xb00), (0x350, 0xd48)], "pointers"
assert len(constant) == 44 and constant[0] == (0x510, 0xb0), "format objects"
rpointers = [r - RDATA_VA for r in relocs if RDATA_VA <= r < RDATA_VA + RDATA_SIZE]
assert rpointers == [RDATA_POINTER] and u32(rdata, RDATA_POINTER) == DATA_VA + 0x510
rdata[RDATA_POINTER:RDATA_POINTER + 4] = bytes(4)


def write_bytes(path, b, what):
    with open(path, "w") as f:
        f.write("/* %s -- generated by tools/gen_controls_data.py from "
                "CONTROLS.DLL; do not edit. */\n" % what)
        for i in range(0, len(b), 16):
            f.write("    " + "".join("0x%02x," % c for c in b[i:i + 16]) + "\n")


write_bytes(DST, data, ".data, 0x%x bytes, pointers zeroed" % SIZE)
write_bytes(RDS, rdata, ".rdata+0..+0x%x, the object-table pointer zeroed" % RDATA_SIZE)
with open(PTR, "w") as f:
    f.write("/* .data's pointers into .rdata, (.data offset, .rdata offset) -- "
            "generated by tools/gen_controls_data.py; do not edit. */\n")
    for o, t in constant:
        f.write("    { 0x%03x, 0x%03x },\n" % (o, t))


def cstr(o):
    s = bytes(text[o:text.index(b"\0", o)])
    assert all(0x20 <= c < 0x7F and c not in b'"\\' for c in s), s
    return '"%s"' % s.decode()


handlers = set()
with open(TBL, "w") as f:
    f.write("/* The command table, .text+0x%x, %d records -- generated by "
            "tools/gen_controls_data.py from CONTROLS.DLL; do not edit.\n"
            " * handler, kind mask, command, display string id, name */\n" % (TABLE, COUNT))
    for i in range(COUNT):
        h, kind, command, key, name = struct.unpack_from("<IIIII", text, TABLE + 20 * i)
        assert TEXT_VA <= h < TEXT_VA + TEXT_SIZE, "handler %d" % i
        handlers.add(h - TEXT_VA)
        n = cstr(name - TEXT_VA) if name else "0"
        f.write("    { H(%04x), 0x%x, 0x%05x, 0x%04x, %s },\n"
                % (h - TEXT_VA, kind, command, key, n))
print("%d handlers" % len(handlers), file=sys.stderr)
