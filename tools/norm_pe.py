#!/usr/bin/env python3
"""norm_pe.py [--peh2] FILE... | --self-test

Prints the SHA-256 of each PE32 file after zeroing what differs between identical builds:
the COFF timestamp, the optional-header checksum, the export directory's timestamp, and the
numbering of GCC's CSWTCH.<n> symbols.

Default mode is the identity gate's normaliser (Story 1.7, Story 1.8): the CSWTCH digits are zeroed in
place (width kept) in the symbol string table only, from PointerToSymbolTable + 18*NumberOfSymbols to EOF.
Output: "<sha256>  <path>".
--peh2 is Story 1.4 slices 10-17's normaliser: the same first three edits, then every
_CSWTCH.<digits> in the whole file becomes the literal _CSWTCH.N (the image may shrink).
Output: "<first 16 hex digits of the sha256> <path>".
Exit 0 if every file was hashed, 2 if any was not a readable PE32 (an error naming it is printed).
"""
import hashlib
import re
import struct
import sys


def check(b):
    """Raise ValueError unless b is a PE32 image whose headers are in bounds; return (pe, opt, nsec, sectab)."""
    if len(b) < 0x40 or b[:2] != b'MZ':
        raise ValueError('no MZ header')
    pe = struct.unpack_from('<I', b, 0x3c)[0]
    if pe + 24 > len(b) or b[pe:pe + 4] != b'PE\0\0':
        raise ValueError('no PE signature')
    opt = pe + 24
    nsec, osize = struct.unpack_from('<H', b, pe + 6)[0], struct.unpack_from('<H', b, pe + 20)[0]
    if osize < 100 or opt + osize > len(b):
        raise ValueError('optional header out of bounds')
    if struct.unpack_from('<H', b, opt)[0] != 0x10b:
        raise ValueError('not PE32')
    if opt + osize + 40 * nsec > len(b):
        raise ValueError('section headers out of bounds')
    return pe, opt, nsec, opt + osize


def zero_common(b):
    """The timestamp, checksum and export-timestamp edits shared by both modes (b: bytearray)."""
    pe, opt, nsec, s = check(b)
    b[pe + 8:pe + 12] = bytes(4)
    b[opt + 64:opt + 68] = bytes(4)
    erva = struct.unpack_from('<I', b, opt + 96)[0]
    for i in range(nsec):
        vs, va, rs, raw = struct.unpack_from('<IIII', b, s + 40 * i + 8)
        if erva and va <= erva < va + max(vs, rs):
            at = raw + erva - va + 4
            if at + 4 <= len(b):  # the gate script would grow the file here; a damaged image is not worth that
                b[at:at + 4] = bytes(4)
    return pe


def norm(data):
    b = bytearray(data)
    pe = zero_common(b)
    sym, nsym = struct.unpack_from('<II', b, pe + 12)
    if sym and nsym:
        st = sym + 18 * nsym
        b[st:] = re.sub(rb'(CSWTCH\.)(\d+)', lambda m: m.group(1) + b'0' * len(m.group(2)), bytes(b[st:]))
    return bytes(b)


def norm_peh2(data):
    b = bytearray(data)
    zero_common(b)
    return re.sub(rb'(_CSWTCH\.)\d+', lambda m: m.group(1) + b'N', bytes(b))


def line(path, data, peh2):
    if peh2:
        return '%s %s' % (hashlib.sha256(norm_peh2(data)).hexdigest()[:16], path)
    return '%s  %s' % (hashlib.sha256(norm(data)).hexdigest(), path)


def synth(sections=1, export=False, symbols=None, osize=224, magic=0x10b):
    """A minimal PE32: one section at raw 0x200, optionally an export directory and a COFF string table."""
    b = bytearray(0x400)
    b[:2] = b'MZ'
    struct.pack_into('<I', b, 0x3c, 0x80)
    pe = 0x80
    b[pe:pe + 4] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', b, pe + 4, 0x14c, sections, 0x12345678, 0, 0, osize, 0x2102)
    opt = pe + 24
    struct.pack_into('<H', b, opt, magic)
    struct.pack_into('<I', b, opt + 64, 0xdeadbeef)
    s = opt + osize
    struct.pack_into('<8sIIII', b, s, b'.rdata', 0x100, 0x1000, 0x100, 0x200)
    if export:
        struct.pack_into('<I', b, opt + 96, 0x1000)
        struct.pack_into('<I', b, 0x200 + 4, 0xcafebabe)
    if symbols is not None:
        tab = b'\0\0\0\0' + symbols
        struct.pack_into('<II', b, pe + 12, len(b), 1)
        b += bytes(18) + tab
    return bytes(b)


def self_test():
    import hashlib as h
    ok = True

    def expect(name, cond):
        nonlocal ok
        if not cond:
            ok = False
            print('FAIL', name)

    plain = synth()
    expect('plain: no export or symbol table', norm(plain)[0x88:0x8c] == bytes(4) and norm(plain) != plain)
    exp = norm(synth(export=True))
    expect('export timestamp zeroed', exp[0x204:0x208] == bytes(4))
    a, b2 = synth(symbols=b'_CSWTCH.12\0'), synth(symbols=b'_CSWTCH.97\0')
    expect('digits differ only in the tail', a != b2 and norm(a) == norm(b2))
    n = norm(synth(symbols=b'_CSWTCH.7\0_CSWTCH.123\0'))
    expect('digit widths preserved', n.endswith(b'_CSWTCH.0\0_CSWTCH.000\0') and len(n) == len(synth(symbols=b'_CSWTCH.7\0_CSWTCH.123\0')))
    # a match inside the file but outside the string table is left alone in default mode
    early = bytearray(synth(symbols=b'x\0')); early[0x300:0x30a] = b'CSWTCH.123'
    expect('match before the string table untouched', b'CSWTCH.123' in norm(bytes(early)))
    # a match past the declared string table's end (the tail runs to EOF, so it is still normalised)
    past = synth(symbols=b'x\0') + b'junk CSWTCH.42'
    expect('match past the table end normalised', norm(past).endswith(b'CSWTCH.00'))
    p2 = norm_peh2(synth(symbols=b'_CSWTCH.12\0'))
    expect('peh2 shortens', p2.endswith(b'_CSWTCH.N\0') and len(p2) == len(synth(symbols=b'_CSWTCH.12\0')) - 1)
    for name, bad in (('truncated', plain[:0x90]), ('empty', b''), ('PE32+', synth(magic=0x20b)),
                      ('short sections', plain[:0x80 + 24 + 224 + 10])):
        try:
            check(bad)
            expect(name + ' rejected', False)
        except ValueError:
            pass
    ref = synth(export=True, symbols=b'_CSWTCH.5\0')
    d1 = h.sha256(norm(ref)).hexdigest()
    d2 = h.sha256(norm_peh2(ref)).hexdigest()[:16]
    expect('pinned default digest ' + d1, d1 == PIN_DEFAULT)
    expect('pinned peh2 digest ' + d2, d2 == PIN_PEH2)
    # the CLI over several files: a bad one first must not stop the valid one after it
    import contextlib, io, os, tempfile
    with tempfile.TemporaryDirectory() as d:
        bad, good = os.path.join(d, 'bad.bin'), os.path.join(d, 'good.dll')
        open(bad, 'wb').write(b'not a pe')
        open(good, 'wb').write(ref)
        for flag, want in (([], '%s  %s' % (PIN_DEFAULT, good)), (['--peh2'], '%s %s' % (PIN_PEH2, good))):
            out, err = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                rc = main(flag + [bad, good])
            expect('cli %s exit 2' % (flag or 'default'), rc == 2)
            expect('cli %s names the bad file' % (flag or 'default'), bad in err.getvalue())
            expect('cli %s valid file hashed' % (flag or 'default'), out.getvalue().strip() == want)
    print('self-test', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


PIN_DEFAULT = '453a222345d2e8c17209f7f24eaaeacb6373aab9fe18a0e9912f0e591e59719b'
PIN_PEH2 = 'a10b93b602916c14'


def main(argv):
    if argv == ['--self-test']:
        return self_test()
    peh2 = bool(argv) and argv[0] == '--peh2'
    files = argv[1:] if peh2 else argv
    if not files or any(f.startswith('--') for f in files):
        print(__doc__, file=sys.stderr)
        return 2
    rc = 0
    for f in files:
        try:
            with open(f, 'rb') as fh:
                print(line(f, fh.read(), peh2))
        except (OSError, ValueError, struct.error) as e:
            print('norm_pe: %s: %s' % (f, e), file=sys.stderr)
            rc = 2
    return rc


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
