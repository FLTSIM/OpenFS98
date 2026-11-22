#!/usr/bin/env python3
"""Check x86_record's output against an independent model of the integer ops.

    python3 -B tools/guest/check_x86_results.py bin/guest/x86_results.txt

The model is written from the SDM, not from the recorder, so a wrong wrapper,
constraint or flag mask shows up as a disagreement. x87 lines are checked only
where the answer is exact (integer and float loads, fabs, fchs).
"""
from fractions import Fraction
import struct
import sys

CF, PF, AF, ZF, SF, OF = 0x1, 0x4, 0x10, 0x40, 0x80, 0x800
ALL = CF | PF | AF | ZF | SF | OF


def szp(v, w):
    m = (1 << w) - 1
    v &= m
    f = (ZF if v == 0 else 0) | (SF if v >> (w - 1) else 0)
    return f | (PF if bin(v & 0xff).count('1') % 2 == 0 else 0)


def merge(a, v, w):
    m = (1 << w) - 1
    return (a & ~m & 0xffffffff) | (v & m)


def arith(op, a, b, f, w):
    m, cin = (1 << w) - 1, f & CF
    x, y = a & m, b & m
    if op in ('add', 'adc'):
        c = cin if op == 'adc' else 0
        r = x + y + c
        of = (~(x ^ y) & (x ^ r)) >> (w - 1) & 1
        cf = r >> w & 1
    else:  # sub sbb cmp
        c = cin if op == 'sbb' else 0
        r = x - y - c
        of = ((x ^ y) & (x ^ r)) >> (w - 1) & 1
        cf = 1 if r < 0 else 0
    af = ((x ^ y ^ r) >> 4) & 1
    out = szp(r, w) | (CF if cf else 0) | (OF if of else 0) | (AF if af else 0)
    return r & m, out


def op_width(name):
    """'add16' -> ('add', 16); names without a plain width suffix (imul3_3, cdq) are 32-bit."""
    base = name.rstrip('0123456789')
    suffix = name[len(base):]
    if suffix in ('8', '16', '32') and not base.endswith('_'):
        return base, int(suffix)
    return name, 32


def model(name, a, b, c, f):
    """Return (r, r2, flags) or None when the op isn't modelled."""
    base, w = op_width(name)
    m = (1 << w) - 1
    if base in ('add', 'adc', 'sub', 'sbb', 'cmp'):
        r, fl = arith(base, a, b, f, w)
        return (a if base == 'cmp' else merge(a, r, w)), 0, fl
    if base in ('and', 'or', 'xor', 'test'):
        r = {'and': a & b, 'or': a | b, 'xor': a ^ b, 'test': a & b}[base] & m
        return (a if base == 'test' else merge(a, r, w)), 0, szp(r, w)
    if base in ('inc', 'dec', 'neg'):
        r, fl = arith('add' if base == 'inc' else 'sub', 0 if base == 'neg' else a, 1 if base != 'neg' else a, 0, w)
        if base != 'neg':
            fl = (fl & ~CF) | (f & CF)  # inc/dec leave CF alone
        return merge(a, r, w), 0, fl
    if base == 'not':
        return merge(a, ~a, w), 0, f & ALL
    if base == 'shl' or base == 'shr' or base == 'sar':
        n = b & 31
        if n == 0:
            return a, 0, f & ALL
        x = a & m
        sx = x - (1 << w) if x >> (w - 1) else x
        if base == 'shl':
            r, cf = x << n, (x << n) >> w & 1
        elif base == 'shr':
            r, cf = x >> n, (x >> (n - 1)) & 1
        else:
            r, cf = sx >> n, (sx >> (n - 1)) & 1
        fl = szp(r, w) | (CF if cf else 0)
        if n == 1:
            of = ((r >> (w - 1)) ^ cf) & 1 if base == 'shl' else (x >> (w - 1) if base == 'shr' else 0)
            fl |= OF if of else 0
        return merge(a, r, w), 0, fl  # caller masks undefined bits
    if base in ('rol', 'ror') and w == 32:
        n = b & 31
        if n == 0:
            return a, 0, f & ALL
        r = ((a << n) | (a >> (32 - n))) & m if base == 'rol' else ((a >> n) | (a << (32 - n))) & m
        cf = r & 1 if base == 'rol' else r >> 31
        fl = (f & ALL & ~(CF | OF)) | (CF if cf else 0)
        if n == 1:
            of = (r >> 31) ^ cf if base == 'rol' else (r >> 31) ^ (r >> 30 & 1)
            fl |= OF if of else 0
        return r, 0, fl
    if name == 'mul32':
        p = a * b
        hi = p >> 32
        return p & m, hi, (CF | OF) if hi else 0
    if name in ('imul1_32', 'imul2_32') or name.startswith('imul3_'):
        def s32(v): return v - (1 << 32) if v >> 31 else v
        y = {'imul3_3': 3, 'imul3_m1': -1, 'imul3_10001': 0x10001, 'imul3_7fffffff': 0x7fffffff}.get(name, s32(b))
        p = s32(a) * y
        lo = p & m
        fits = p == s32(lo)
        return lo, ((p >> 32) & m if name == 'imul1_32' else 0), 0 if fits else CF | OF
    if name == 'div32':
        d = (c << 32) | a
        return d // b, d % b, 0
    if name == 'idiv32':
        def s(v, bits): return v - (1 << bits) if v >> (bits - 1) else v
        d, y = s((c << 32) | a, 64), s(b, 32)
        q = abs(d) // abs(y) * (1 if (d < 0) == (y < 0) else -1)
        return q & m, (d - q * y) & m, 0
    if name == 'bswap32':
        return int.from_bytes(a.to_bytes(4, 'little'), 'big'), 0, f & ALL
    if name in ('movsx8', 'movsx16', 'movzx8', 'movzx16'):
        bits = 8 if name.endswith('8') else 16
        v = b & ((1 << bits) - 1)
        if name.startswith('movsx') and v >> (bits - 1):
            v |= 0xffffffff ^ ((1 << bits) - 1)
        return v, 0, f & ALL
    if name in ('bsf32', 'bsr32'):
        if b == 0:
            return None, 0, ZF
        return ((b & -b).bit_length() - 1 if name == 'bsf32' else b.bit_length() - 1), 0, 0
    if name == 'cdq':
        return a, 0xffffffff if a >> 31 else 0, 0
    return None


SETCC = {'o': lambda f: f & OF, 'b': lambda f: f & CF, 'e': lambda f: f & ZF,
         'be': lambda f: f & (CF | ZF), 's': lambda f: f & SF, 'p': lambda f: f & PF,
         'l': lambda f: bool(f & SF) != bool(f & OF),
         'le': lambda f: (f & ZF) or bool(f & SF) != bool(f & OF)}
for _k, _v in list(SETCC.items()):
    SETCC[{'o': 'no', 'b': 'ae', 'e': 'ne', 'be': 'a', 's': 'ns', 'p': 'np', 'l': 'ge', 'le': 'g'}[_k]] = \
        (lambda fn: lambda f: not fn(f))(_v)


def f80_value(text):
    se, mant = text.split('.')
    return int(se, 16), int(mant, 16)


def f80_magnitude(se, mant):
    e = (se & 0x7fff) - 16383
    return Fraction(mant) * Fraction(2) ** (e - 63) if mant else Fraction(0)


def check_x87(fields, out):
    name = fields[0].split('(')[0]
    a_se, a_m = f80_value(fields[2][2:])
    raw = (a_se << 64 | a_m).to_bytes(10, 'big')[::-1]  # little-endian memory image of a
    st0 = out.get('st0')
    if name in ('fabs', 'fchs') and (a_se & 0x7fff) != 0x7fff:
        se, mant = f80_value(st0)
        want = (a_se & 0x7fff) | (0 if name == 'fabs' else (~a_se & 0x8000))
        return se == want and mant == a_m
    if name.startswith('fild_m'):
        bits = int(name[6:])
        v = int.from_bytes(raw[:bits // 8], 'little', signed=True)
        se, mant = f80_value(st0)
        if v == 0:
            return se == 0 and mant == 0
        return (se >> 15) == (v < 0) and f80_magnitude(se, mant) == abs(v)
    if name == 'fld_m64':
        d = struct.unpack('<d', raw[:8])[0]
        if d != d or d in (float('inf'), float('-inf')):
            return True
        se, mant = f80_value(st0)
        return f80_magnitude(se, mant) == abs(Fraction(d)) and (se >> 15) == (struct.unpack('<Q', raw[:8])[0] >> 63)
    return None


def main(path):
    checked = skipped = bad = 0
    for line in open(path):
        if line.startswith('#'):
            continue
        left, right = line.split(' -> ')
        fields = left.split()
        out = dict(kv.split('=') for kv in right.split())
        name = fields[0]
        if name.startswith('set') and len(fields) == 2:
            f = int(fields[1][2:], 16)
            ok = int(out['r'], 16) == (1 if SETCC[name[3:]](f) else 0)
        elif 'cw=' in left:
            ok = check_x87(fields, out)
        else:
            a, b, c, f = (int(x.split('=')[1], 16) for x in fields[1:5])
            want = model(name, a, b, c, f)
            if want is None:
                ok = None
            else:
                fo = int(out['f'], 16)
                r_ok = want[0] is None or out['r'] == f'{want[0] & 0xffffffff:08x}'
                # every flag bit the recorder calls defined must equal the model's
                ok = r_ok and int(out['r2'], 16) == want[1] and \
                    ((want[2] ^ fo) & mask_of(name, a, b, c)) == 0 and (fo & ~mask_of(name, a, b, c)) == 0  # undefined bits recorded as 0
        if ok is None:
            skipped += 1
        elif ok:
            checked += 1
        else:
            bad += 1
            if bad <= 20:
                print('MISMATCH', line.rstrip())
    print(f'{checked} lines agree with the model, {bad} disagree, {skipped} not modelled')
    return 1 if bad else 0


def mask_of(name, a, b, c):
    """The defined-flag mask x86_record applies, restated independently."""
    base, w = op_width(name)
    if base in ('and', 'or', 'xor', 'test'):
        return ALL & ~AF
    if base in ('shl', 'shr', 'sar'):
        n = b & 31
        if n == 0:
            return ALL
        if w == 8 and n >= 8 and base != 'sar':
            return ALL & ~(AF | OF | CF)
        return ALL & ~AF if n == 1 else ALL & ~(AF | OF)
    if base in ('rol', 'ror'):
        return ALL if (b & 31) <= 1 else ALL & ~OF
    if name.startswith(('mul', 'imul')):
        return CF | OF
    if name in ('bsf32', 'bsr32'):
        return ZF
    if name in ('div32', 'idiv32', 'cdq'):
        return 0
    return ALL


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
