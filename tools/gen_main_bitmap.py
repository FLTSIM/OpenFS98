#!/usr/bin/env python3
"""Write retail MAIN.DLL's bitmap 101 (the kneeboard's page) as a .bmp for
main.rc.  The resource is a DIB; a .bmp is that with a file header.

Usage: tools/gen_main_bitmap.py [orig/Modules/MAIN.DLL] [bin/main_kneeboard.bmp]
"""
import struct
import sys

SRC = sys.argv[1] if len(sys.argv) > 1 else "orig/Modules/MAIN.DLL"
DST = sys.argv[2] if len(sys.argv) > 2 else "bin/main_kneeboard.bmp"

blob = open(SRC, "rb").read()
pe = struct.unpack_from("<I", blob, 0x3C)[0]
count = struct.unpack_from("<H", blob, pe + 6)[0]
section = pe + 24 + struct.unpack_from("<H", blob, pe + 20)[0]
for _ in range(count):
    name = blob[section:section + 8].rstrip(b"\0")
    size, rva, raw_size, raw = struct.unpack_from("<IIII", blob, section + 8)
    if name == b".rsrc":
        break
    section += 40
else:
    raise SystemExit("no .rsrc")


def walk(at, path):
    named, ids = struct.unpack_from("<HH", blob, raw + at + 12)
    for i in range(named + ids):
        key, to = struct.unpack_from("<II", blob, raw + at + 16 + 8 * i)
        if to >> 31:
            found = walk(to & 0x7fffffff, path + (key,))
            if found:
                return found
        elif path == (2, 101):
            data, length = struct.unpack_from("<II", blob, raw + to)
            return blob[raw + data - rva:raw + data - rva + length]
    return None


dib = walk(0, ())
if dib is None:
    raise SystemExit("no bitmap 101")
header, width, height, planes, bits, _, _, _, _, used = struct.unpack_from("<IiiHHIIiiI", dib)
colours = used if used else (1 << bits if bits <= 8 else 0)
offset = 14 + header + colours * 4
with open(DST, "wb") as out:
    out.write(struct.pack("<2sIHHI", b"BM", 14 + len(dib), 0, 0, offset))
    out.write(dib)
