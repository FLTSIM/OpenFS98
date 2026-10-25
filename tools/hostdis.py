#!/usr/bin/env python3
"""Disassemble a range of FLTSIM98.EXE's .text, at the host's own addresses.

The host reimplementation (src/host/host.c, docs/modules/host.md) is done a
service at a time; reading the callee is how each prototype and body is found.
This prints intel-syntax disassembly with the real VAs, so an address from the
service table (tools/hostmap.py) or a `call`/`ds:` reference can be followed
directly.

Usage: tools/hostdis.py <lo_va> <hi_va>     e.g. tools/hostdis.py 0x408530 0x408560
       tools/hostdis.py <lo_va>             disassembles 0x80 bytes from lo

The companion for the module DLLs disassembles from their .text; this one is
fixed to the executable because the host is the only reloc-stripped image.
"""
import subprocess
import sys

PATH = 'orig/FLTSIM98.EXE'
TEXT_OFF = 0x400
TEXT_VMA = 0x401000
TEXT_END = 0x401000 + 0x14060


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    lo = int(sys.argv[1], 16)
    hi = int(sys.argv[2], 16) if len(sys.argv) > 2 else lo + 0x80
    lo = max(lo, TEXT_VMA)
    hi = min(hi, TEXT_END)
    seg = open(PATH, 'rb').read()[TEXT_OFF + (lo - TEXT_VMA):TEXT_OFF + (hi - TEXT_VMA)]
    open('/tmp/_hostdis.bin', 'wb').write(seg)
    out = subprocess.run(['i686-w64-mingw32-objdump', '-D', '-b', 'binary', '-mi386',
                          '-Mintel', '--adjust-vma=0x%x' % lo, '/tmp/_hostdis.bin'],
                         capture_output=True, text=True)
    for line in out.stdout.splitlines():
        s = line.strip()
        if ':\t' in s:
            a, rest = s.split(':\t', 1)
            parts = rest.split('\t', 1)
            ins = parts[1].strip() if len(parts) > 1 else ''
            print("%08x  %s" % (int(a, 16), ins))
    return 0


if __name__ == '__main__':
    sys.exit(main())
