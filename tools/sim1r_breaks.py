#!/usr/bin/env python3
"""SIM1's decompiled routines broken on purpose: each break must make sim1rdiff differ.

    python3 -B tools/sim1r_breaks.py [suite ...]      (N=scenarios, default 400; ONLY=name,name)

Run it from a frozen copy of the tree (src, tools, include, bin/sim1-diff/snapshots.bin, orig
linked), never the live one: each break rebuilds bin/sim1-diff/sim1rdiff.exe in place. A
suite is a source file of src/sim1/; a break replaces text that occurs exactly once.

Left out as proven equivalent:
- cross-order (0x2200a3db's two products added the other way round): x87 addition is
  commutative, rounding included.
- surf-copy (a double copied without sim1_x87): GCC -O2 compiles the plain copy as fldl/fstpl
  as well, so it quiets a signalling NaN the same; sim1_x87 keeps that from depending on it.
- mark-width (0x22009e62's 16-bit shift without the count test): (uint16_t)(1u << n) is 0 for n
  of 16 to 31, as the 16-bit shl gives.
- c16/c32 at the boundary (>= 0x3fff against > 0x3fff, and the like): at the boundary the
  value is returned either way.
- angle-cmp (0x2200d750's C0|C3 as C0): equality with 90 is unreachable. The value is
  0.52356 + |i| * 0.00104712 for an integer i, and crosses 90 between 85449 and 85450, missing it
  by about 3.6e-5 at the nearest, more than half an ulp even at 24-bit precision.
- rise-positive (0x22008a77's 0 < k as 0 <= k): at k = 0 the stiffness is 0 either way.
- pow-kept (0x22011810's base rounded to a double before _CIpow): _CIpow stores both operands
  as doubles before it computes, so it rounds the base itself.
- damp-sign (0x22009e92's sum >= 0 as > 0): at a zero sum +0x10 is minus the base, so both
  branches give the same factor.
- frac-cap and lev-follow (> as >= against a cap): at equality the value already is the cap.
- slip-floor (0x22009f11's <= 0x64 as < 0x64): at 0x64 the value is 0x64 either way.
- phold-cap (0x2200e173's >= 0x400 as >= 0x401): at 0x400 the value is 0x400 either way.
- struct-fifth (0x22010a00 without the sign fix of power / 5): the power is a word, never negative.
- gov-cap (0x2200ea69's >= as > against its cap): at the cap the value is the cap either way.
- steer-slow (0x22006bb0's < 15 as <= 15), equivalent in practice, not proven: at exactly 15
  the break scales the angle by 15 * R220 = 1 + 6e-17, which the word truncations after it see
  only at an exact integer boundary.
- travel-first (0x22004990's first travel compared after rounding): rounding changes the
  comparison only when the rounded travel equals the floor, and then storing it or copying the
  floor gives the same bits.
- eff-speed (0x220119c0's floor as < rather than <=): at the floor the speed is the floor.
- pass-mask (0x2200a7b0's third engine's bit as 6): bit 1 is already set by then.
- alt-bound (0x2200e2cf's bound test < as <=): at equality the climb is the bound either way.
- yawm-engines (0x22005c00's store after the third engine left out): every intermediate store
  of +0x12d0 is overwritten by the final one.
- atm-count (0x2200e4fa's second engine-count test as < 2): with the tests inverted
  (SIM1-AUTOTHROTTLE-ENGINES), the inner tests are reached only with the count below 2, so
  they are always true.
- acc-early (0x22002eb0's first upper bound tested on the stored double, not the unrounded
  sum): the bound is the clamp value (325) and representable, and rounding is monotonic, so a
  sum above it either rounds above it (clamped either way) or onto it (325 either way).
- acc-old and acc-kept (0x22002eb0 reading +0x1368 or +0x1360 again for the kept value): neither
  is rewritten until after, so the reload is the kept value.
- acc-floor (0x22002eb0's lower bound as C0|C3): at the bound the value is the bound either way.
- acc-quiet (0x22002eb0's kept value copied without sim1_x87): as surf-copy, GCC holds it in
  an x87 register, so the copy is quieted either way.
- rotor-near (0x220125c0's coning kept unrounded), equivalent in practice, not proven: the two
  differ by about 2^-64 relative (64-bit precision only), and adding 0.6 and storing the result
  as a double removes it.
- Exception flags are not compared (sim1rdiff.c says why), so a break that only changes
  which x87 exceptions are raised is not one.
"""
import os
from pathlib import Path
import subprocess
import sys

BREAKS = {
    'sim1_small.c': [
        ('bits', 'S32(0x2594) = 0x20;', 'S32(0x2594) = 0x21;'),
        ('clear', '0x2588, 0x2aa8}', '0x2588, 0x2aa0}'),
        ('pairs', '0x12e0 + at) = 0', '0x12e4 + at) = 0'),
        ('step-shift', '(product << 1) >> 16', '(product << 0) >> 16'),
        ('step-dx', '*dx_out = high;', '*dx_out = high + 1;'),
        ('t32-mask', '(position & 0x7ff) << 4', '(position & 0x3ff) << 4'),
        ('t32-entry', '>> 2) & 0x3e;', '>> 2) & 0x3c;'),
        ('t16-entry', '>> 3) & 0x1e;', '>> 3) & 0x0e;'),
        ('isqrt-signed', '(long double)(uint64_t)value', '(long double)(int32_t)value'),
        ('square-14', '* 14u;', '* 13u;'),
        ('square-high', '(uint32_t)(product >> 32);', '(uint32_t)(product >> 31);'),
        ('cross-double', 'SF64(0x1470) = (double)sum;',
         '{ double p1 = SF64(0x1560) * SF64(0x13b0), p2 = SF64(0x1568) * SF64(0x13a8); SF64(0x1470) = p1 + p2; (void)sum; }'),
        ('cross-store', 'SF64(0x1470) = (double)sum;', 'SF64(0x1478) = (double)sum;'),
        ('isqrt64-low', 'sim1_fsqrt((long double)value)', 'sim1_fsqrt((long double)(uint32_t)value)'),
        ('hold-flag', 'if (!P8(global, 0x366)) S32(0x1010) = 1;', 'if (!P8(global, 0x367)) S32(0x1010) = 1;'),
        ('hold-c0', 'if (X87_C0(scaled, RF64(0x15030)))', 'if (X87_C0C3(scaled, RF64(0x15030)))'),
        ('hold-words', 'PI16(global, 0x9bc) > 0', 'PI16(global, 0x9bc) > 1'),
        ('hold-unsigned', '(long double)(int64_t)P32(global, 0x2b4)', '(long double)PI32(global, 0x2b4)'),
        ('surf-sign', 'if (value > 0) SF64(0x1630)', 'if (value >= 0) SF64(0x1630)'),
        ('surf-swap', '        SF64(0x1640) = (double)(second * PI16(global, 0x11c0));\n    } else {', '        SF64(0x1640) = (double)(first * PI16(global, 0x11c0)); (void)second;\n    } else {'),
        ('surf-one', 'S32(at + 4) = 0x3ff00000;', 'S32(at + 4) = 0x3ff80000;'),
        ('surf-unsigned', '(long double)(int64_t)P32(global, 0xbf0)', '(long double)PI32(global, 0xbf0)'),
        ('sums-order', '(((long double)SF64(0x1288 + at) + SF64(0x1258 + at)) + SF64(0x1240 + at)) + SF64(0x1228 + at)', '(((long double)SF64(0x1288 + at) + SF64(0x1240 + at)) + SF64(0x1258 + at)) + SF64(0x1228 + at)'),
        ('count-cmp', 'if (X87_C0C3(value, PF64(record, 8)))', 'if (X87_C0(value, PF64(record, 8)))'),
        ('count-rounded', 'value = (long double)PF64(record, 0) - step;\n    PF64(record, 0) = (double)value;', 'value = (double)((long double)PF64(record, 0) - step);\n    PF64(record, 0) = (double)value;'),
        ('count-owner', 'P32(owner, 0x94) = 1;', 'P32(owner, 0x90) = 1;'),
        ('weight-skip', 'if (kind == -1) continue;', 'if (kind == -1) break;'),
        ('weight-high', 'return (quotient & 0xffff0000u) | result;', 'return result;'),
        ('weight-sentinel', 'if (result == 0x7fff) result', 'if (result == 0x7ffe) result'),
        ('pitch-sign', 'sum -= ((long double)SF64(0x1698)', 'sum += ((long double)SF64(0x1698)'),
        ('side-term', '* SF64(0x18c8)) * SF64(0x15b8)', '* SF64(0x18c0)) * SF64(0x15b8)'),
        ('thrust-count', 'if (engines > 2)', 'if (engines >= 2)'),
        ('turn-term', '+ (long double)SF64(0x14b0) * SF64(0x12a0));\n    SF64(0x1230)', '- (long double)SF64(0x14b0) * SF64(0x12a0));\n    SF64(0x1230)'),
        ('follow-c3', 'if (!flag && X87_C3(', 'if (!flag && X87_C0('),
        ('follow-early', '        SF64(0x11b0) = (double)(long double)SI32(0x118c);\n        return;', '        return;'),
        ('wrap-unrounded', 'value -= (long double)SF64(0x770) * RF64(0x151f8);\n    return value;', 'value -= (long double)SF64(0x770) * RF64(0x151f8);\n    return (double)value;'),
        ('wrap-low', 'if (X87_C0(value, (long double)RF64(0x151f8)))', 'if (X87_C0C3(value, (long double)RF64(0x151f8)))'),
        ('axes-kept', '(long double)kept * PF64(part, 0x88)', '(sine + 0 * kept) * PF64(part, 0x88)'),
        ('axes-sign', '(double)(sim1_fchs(cosine * PF64(part, 0x88)) - sine', '(double)((cosine * PF64(part, 0x88)) - sine'),
        ('rotate-row', '(long double)SF64(0x1210) * PF64(part, 0x90)', '(long double)SF64(0x1210) * PF64(part, 0x98)'),
        ('add-moment', '(long double)PF64(part, 0x38) * PF64(part, 0x60));', '(long double)PF64(part, 0x38) * PF64(part, 0x68));'),
        ('scale-shift', 'if (shift) factor = (factor >> 4)', 'if (shift) factor = (factor >> 3)'),
        ('scale-carry', 'high = (uint32_t)((int32_t)high >> 16) + carry;', 'high = (uint32_t)((int32_t)high >> 16) + 0 * carry;'),
        ('scale-borrow', 'dx = (uint16_t)(dx - 0xffff - borrow);', 'dx = (uint16_t)(dx - 0xffff - 0 * borrow);'),
        ('scale-divisor', 'sim1_div16(1, 0xa3f7, S16(0x16f8)', 'sim1_div16(1, 0xa3f8, S16(0x16f8)'),
        ('mark-signed', 'if (PI32(part, 0xc) >= PI32(part, 8)) return;', 'if (P32(part, 0xc) >= P32(part, 8)) return;'),
        ('mul48-shift', 'eax = (uint32_t)((int32_t)eax >> 15);', 'eax = (uint32_t)(eax >> 15);'),
        ('mul48-sign', "S16(0x85f) = (int32_t)high < 0 ? 0xffff : 0;", "S16(0x85f) = 0;"),
        ('c16-signed', 'if ((int16_t)value >= 0x3fff) return 0x3fff;', 'if (value >= 0x3fff) return 0x3fff;'),
        ('c16-low', 'if ((int16_t)value <= -0x3fff) return 0xc001;', 'if ((int16_t)value <= -0x3ffe) return 0xc001;'),
        ('c32-signed', 'if ((int32_t)value >= 0x3fff) return 0x3fff;', 'if (value >= 0x3fff) return 0x3fff;'),
        ('g384-null', 'return pointer ? P32(pointer, 0) : 0;', 'return pointer ? P32(pointer, 0) : 1;'),
        ('limits', 'P16(global, 0x892) = 3;', 'P16(global, 0x892) = 2;'),
        ('fold-gt', 'if (!(a > b)) return a;', 'if (!(a >= b)) return a;'),
        ('fold-neg', 'if (X87_C0(negated, b)) return b;', 'if (X87_C0(negated, b)) return a;'),
        ('rudder-list', 'sim1_weighted(sim1_data + 0x308)', 'sim1_weighted(sim1_data + 0x338)'),
        ('elev-store', 'S16(0x344) = (uint16_t)value;', 'S16(0x346) = (uint16_t)value;'),
        ('ail-dest', 'P16(SIM1_GLOBAL, 0x11c0) =', 'P16(SIM1_GLOBAL, 0x11c2) ='),
        ('elev-trim', 'if (!(S32(0x3b68) & 1)) value', 'if (S32(0x3b68) & 1) value'),
        ('ail-word', 'P16(global, 0xbb6)', 'P16(global, 0xbb4)'),
        ('rud-table', 'sim1_table16(sim1_data + 0x368, P16(global, 0xbba))', 'sim1_table32(sim1_data + 0x368, P16(global, 0xbba))'),
        ('mode3', 'if (S32(0x1628) == 3)', 'if (S32(0x1628) >= 3)'),
        ('ratio', '/ ((long double)SF64(0x1790) - SF64(0x1798))', '/ ((long double)SF64(0x1798) - SF64(0x1790))'),
        ('tscaled', 'return (long double)SI16(0x1718) * SF64(0x6b0);', 'return (long double)S16(0x1718) * SF64(0x6b0);'),
        ('bearing-sub', 'return (uint16_t)(0x4000 - (uint16_t)angle);', 'return (uint16_t)(0x4000 + (uint16_t)angle);'),
        ('bearing-order', '(y, x);', '(x, y);'),
        ('angle-abs', 'fabsl((long double)SI32(0x242c))', '((long double)SI32(0x242c))'),
        ('c874-mode', 'if (SI32(0x162c) < 3) return;', 'if (SI32(0x162c) < 2) return;'),
        ('c874-cmp', 'if (!(fabsl((long double)SF64(0x1470)) > (long double)sim1_f64(0x976)))', 'if (!(fabsl((long double)SF64(0x1470)) >= (long double)sim1_f64(0x976)))'),
        ('band-low', 'if (!X87_C0(speed, (long double)SF64(0x1790))) return SF64(0x1780);', 'if (!X87_C0C3(speed, (long double)SF64(0x1790))) return SF64(0x1780);'),
        ('band-high', 'if (!(speed > SF64(0x1798))) return SF64(0x1788);', 'if (!(speed >= SF64(0x1798))) return SF64(0x1788);'),
        ('alert-limit', 'if (P16(global, 0xc1a) <= limit) return;', 'if (P16(global, 0xc1a) < limit) return;'),
        ('alert-air', 'limit = 0x400;', 'limit = 0x401;'),
        ('alert-args', '(2, 2, 0xf480, 0xf481, 0xf482, 0xf483, 0, 0)', '(2, 2, 0xf480, 0xf481, 0xf483, 0xf482, 0, 0)'),
        ('heading-hold', 'if (!sim1_u32(0xadf))', 'if (sim1_u32(0xadf))'),
        ('heading-bias', '+ 0x7fff) * 0x168u', '+ 0x8000) * 0x168u'),
        ('886-pair', '{0xb98, 0xb94}', '{0xb98, 0xb90}'),
        ('att-unsigned', 'if ((uint32_t)((int32_t)value >> 16) > 0x2aab) goto tipped;\n    value = P32(global, 0x57c);', 'if ((int32_t)((int32_t)value >> 16) > 0x2aab) goto tipped;\n    value = P32(global, 0x57c);'),
        ('att-ground', 'if ((int32_t)(P32(global, 0x574) - (P32(global, 0x20) >> 8)) > 0x1e) return;', 'if ((int32_t)(P32(global, 0x574) - (P32(global, 0x20) >> 8)) > 0x1f) return;'),
        ('att-shr', '(P32(global, 0x20) >> 8)) > 0x1e', '(uint32_t)((int32_t)P32(global, 0x20) >> 8)) > 0x1e'),
        ('height-borrow', 'high = high - (uint32_t)((int32_t)ground >> 8) - borrow;', 'high = high - (uint32_t)((int32_t)ground >> 8) - 0 * borrow;'),
        ('height-cap', 'if ((int32_t)eax > 0x7feb) { eax = 0x7feb; bx |= 0xff00; }', 'if ((int32_t)eax > 0x7feb) { eax = 0x7feb; }'),
        ('filter-limit', 'if (X87_C0(fabsl(x), (long double)PF64(record, 0x28)))', 'if (X87_C0C3(fabsl(x), (long double)PF64(record, 0x28)))'),
        ('filter-kept', 'PF64(record, 0x38) = (double)term;\n        y += term;', 'PF64(record, 0x38) = (double)term;\n        y += (double)term;'),
        ('filter-rate', '/ SF64(0x16f0);\n    PF64(record, 0x48)', '* SF64(0x16f0);\n    PF64(record, 0x48)'),
        ('filter-last', 'PF64(record, 0x48) = (double)x;', 'PF64(record, 0x48) = (double)y;'),
        ('hold-entry', 'S16(0x2d8 + 0xe) = 0x333;', 'S16(0x2d8 + 0xe) = 0x334;'),
        ('hold-round', 'SI32(0x950) = sim1_fistp32(', 'SI32(0x950) = (int32_t)sim1_ftol('),
        ('contact-any', 'P32(contact, 4) = (a | b | c) != 0;', 'P32(contact, 4) = (a | b) != 0;'),
        ('contact-scale', 'PF64(contact, 0x40) = (double)((long double)c * SF64(0x738));', 'PF64(contact, 0x40) = (double)((long double)b * SF64(0x738));'),
        ('limit-word', 'if (X87_C0(load, (long double)PF64(part, 0x50)) && !P16(global, 0x832))', 'if (X87_C0(load, (long double)PF64(part, 0x50)))'),
        ('limit-unsigned', '(long double)(int64_t)P32(part, 0x20)', '(long double)PI32(part, 0x20)'),
        ('limit-on', 'if (P32(part, 4) != 1 || P32(part, 0xc) || P32(part, 8) != 1) return;', 'if (P32(part, 4) != 1 || P32(part, 8) != 1) return;'),
        ('t2d-cell', '(uint8_t)(high >> 4) + (uint8_t)(high >> 1)', '(uint8_t)(high >> 4) + (uint8_t)(high >> 2)'),
        ('t2d-row', 'P16(table, cell + 0x14) - P16(table, cell + 0x12)', 'P16(table, cell + 0x16) - P16(table, cell + 0x12)'),
        ('t2d-frac', '(uint16_t)((across & 0x1fff) << 2)', '(uint16_t)((across & 0x0fff) << 2)'),
        ('trim-which', 'if (which == 1) sim1_set_u32(0x65a, value);', 'if (which == 2) sim1_set_u32(0x65a, value);'),
        ('trim-neg', 'step = (uint16_t)-S16(0x654);', 'step = S16(0x654);'),
        ('trim-off', 'else SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, uint32_t))(4, 0);', 'else SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, uint32_t))(4, 2);'),
        ('gear-signed', 'if ((int8_t)P8(global, 0x838) < 7) return;', 'if (P8(global, 0x838) < 7) return;'),
        ('gear-five', 'P8(global, 0x838) = 5; return;', 'P8(global, 0x838) = 4; return;'),
        ('gear-same', 'if (state != current)', 'if (state == current)'),
        ('stall-trunc', 'P16(SIM1_GLOBAL, 0x11ba) = (uint16_t)sim1_ftol(value);', 'P16(SIM1_GLOBAL, 0x11ba) = (uint16_t)sim1_fistp32(value);'),
        ('stall-half', 'half = (int16_t)(level / 2);', 'half = (int16_t)(level >> 1);'),
        ('stall-flaps', 'if ((flaps && half > SI16(0x3588))', 'if ((flaps && half >= SI16(0x3588))'),
        ('sched-wait', 'S32(0x918) = (0x12 - S32(0x910)) * (0x12 - S32(0x910));', 'S32(0x918) = (0x12 - S32(0x910));'),
        ('sched-speed', 'if ((int32_t)value <= 0x190) value = 0;', 'if ((int32_t)value < 0x190) value = 0;'),
        ('sched-scale', 'value = (uint32_t)((int32_t)value * 0x51e);', 'value = (uint32_t)((int32_t)value * 0x51f);'),
        ('sched-mode', '{ S32(0x91c) = (uint32_t)mode; S32(0x910) = 0xd;', '{ S32(0x91c) = 4; S32(0x910) = 0xd;'),
        ('rise-clamp', 'if (!X87_C0(0.0L, stiffness)) stiffness = 0.0L;', '(void)0;'),
        ('rise-pi', '3.14159265358979323846264338327950288L', '3.14159265358979L'),
        ('rise-speed', '- SF64(0x1388));', '- speed);'),
        ('pow-order', 'sim1_cipow(base, RF64(0x15308))', 'sim1_cipow(RF64(0x15308), base)'),
        ('bof-scale', 'ix = sim1_fistp32((long double)x * SF64(0x6d8))', 'ix = sim1_fistp32((long double)x * SF64(0x6e0))'),
        ('bof-order', '(iy, ix);', '(ix, iy);'),
        ('turn-flags', 'if (!S16(0x996) && (P32(global, 0x854) || S8(0xa28) || S8(0x9ae)))', 'if (!S16(0x996) && (P32(global, 0x854) || S8(0xa28)))'),
        ('turn-fold', 'value = sim1_fold(sim1_band_value(global), value);', 'value = sim1_fold(value, sim1_band_value(global));'),
        ('turn-keep', 'S16(0xa0c) = (uint16_t)(P16(global, 0x7cc) + P16(global, 0x2a0));', 'S16(0xa0c) = P16(global, 0x7cc);'),
        ('words-size', 'SIM1_GLOBAL + 0x560, 0x24);', 'SIM1_GLOBAL + 0x560, 0x20);'),
        ('words-trunc', 'S16(0x1756) = (uint16_t)sim1_ftol(', 'S16(0x1756) = (uint16_t)sim1_fistp32('),
        ('rhold-bias', '- P16(global, 0x870) + 0x8000);', '- P16(global, 0x870) + 0x7fff);'),
        ('rhold-clamp', 'S16(0x308 + 0x14) = sim1_clamp16((uint16_t)S32(0x950));', 'S16(0x308 + 0x14) = (uint16_t)S32(0x950);'),
        ('crash-five', 'if (P32(global, 0x348) < 5) return;', 'if (P32(global, 0x348) < 4) return;'),
        ('crash-fast', 'if ((long double)SF64(0x13b0) > (long double)sim1_f64(0x7dc)) return;', 'if ((long double)SF64(0x13b0) >= (long double)sim1_f64(0x7dc)) return;'),
        ('crash-timer', 'if (SI16(0x7d6) < 0x14) return;', 'if (SI16(0x7d6) < 0x13) return;'),
        ('damp-carry', 'sum = (uint64_t)first + (uint64_t)second;', 'sum = (uint64_t)(uint32_t)first + (uint64_t)second;'),
        ('damp-floor', 'if (!X87_C0(0.0L, value)) value = 0.0L;\n    PF64(part, 0x80)', 'PF64(part, 0x80)'),
        ('stop-band', 'if (!high) { if (low > 0x200) return; }', 'if (!high) { if (low >= 0x200) return; }'),
        ('stop-neg', 'else if (low < 0xfe00) return;', 'else if (low < 0xfe01) return;'),
        ('stop-centre', 'P16(global, 0xbba) = 0;\n}', 'P16(global, 0xbb8) = 0;\n}'),
        ('trig-fcos', 'SF64(0x1560 + at * 0x10) = (double)sim1_fcos(angle);', 'SF64(0x1560 + at * 0x10) = (double)sim1_fsin(angle);'),
        ('trig-word', 'S16(0x1594 + at * 8) = (uint16_t)sim1_fistp16(', 'S16(0x1594 + at * 8) = (uint16_t)sim1_fistp32('),
        ('climb-shift', '* 0x8000;\n    uint32_t remainder', '* 0x4000;\n    uint32_t remainder'),
        ('climb-slots', 'for (slot = 0; slot < 8; ++slot) {', 'for (slot = 0; slot < 7; ++slot) {'),
        ('climb-mean', 'sim1_set_u32(0xa6f, (uint32_t)(sum >> 3));', 'sim1_set_u32(0xa6f, (uint32_t)((uint32_t)sum >> 3));'),
        ('climb-abs', 'if (product < 0) product = (int32_t)(0u - (uint32_t)product);', '(void)0;'),
        ('climb-angle', '(uint16_t)(S16(0x9a6) - (uint16_t)(S32(0x1558) >> 16))', '(uint16_t)(S16(0x9a6) + (uint16_t)(S32(0x1558) >> 16))'),
        ('yaw-quarter', 'if (P8(global, 0x36d)) rate >>= 2;', 'if (P8(global, 0x36d)) rate >>= 1;'),
        ('yaw-sat', 'if (wide != delta) delta = delta < 0 ? 0x7fff : (int16_t)0x8001;', '(void)0;'),
        ('yaw-gain', '((int32_t)delta * 0x7530) >> 16', '((int32_t)delta * 0x7531) >> 16'),
        ('yaw-off', 'if (!P8(global, 0x366) && (P32(global, 0x854) || P32(global, 0x808) || P32(global, 0x278))) goto on;', 'if (!P8(global, 0x366) && (P32(global, 0x854) || P32(global, 0x808))) goto on;'),
        ('gear-cross', 'SF64(0x1300) = (double)((long double)SF64(0x10d8) * SF64(0x1260) - (long double)SF64(0x10e0) * SF64(0x1258));', 'SF64(0x1300) = (double)((long double)SF64(0x10d8) * SF64(0x1260) - (long double)SF64(0x10e0) * SF64(0x1268));'),
        ('drag-base', 'sum = S32(0x35bc);', 'sum = S32(0x35b0);'),
        ('drag-pair', 'S32(0x1604) + S32(0x1608)', 'S32(0x1604) + S32(0x160c)'),
        ('station-borrow', 'high = P32(global, 0x864) - P32(global, 0x56c) - borrow;', 'high = P32(global, 0x864) - P32(global, 0x56c);'),
        ('station-shift', 'int32_t scaled = (int32_t)(uint32_t)((uint64_t)product >> 8);', 'int32_t scaled = (int32_t)(uint32_t)((uint64_t)product >> 7);'),
        ('station-ptr', 'word = P16((uint8_t *)(uintptr_t)P32((uint8_t *)(uintptr_t)pointer, 0), 0x1a);', 'word = P16((uint8_t *)(uintptr_t)P32((uint8_t *)(uintptr_t)pointer, 0), 0x18);'),
        ('track-sub', 'S16(0x1518) = (uint16_t)(S16(0x3900) - sim1_bearing_of', 'S16(0x1518) = (uint16_t)(S16(0x3900) + sim1_bearing_of'),
        ('track-args', 'sim1_bearing_of(SF64(0x1388), SF64(0x1398))', 'sim1_bearing_of(SF64(0x1398), SF64(0x1388))'),
        ('air-sqrt-store', 'PF64(global, 0x11c8) = (double)value;', 'PF64(global, 0x11c0) = (double)value;'),
        ('air-reload', 'ratio /= (long double)SF64(0x16e8) * sim1_f64(0x59c);', 'ratio /= value * sim1_f64(0x59c);'),
        ('air-cap', 'S16(0x1704) = S32(0x1700) > 0xffff ? 0xffff', 'S16(0x1704) = SI32(0x1700) > 0xffff ? 0xffff'),
        ('air-sum', 'value += sim1_f64(0x5b4);', 'value += sim1_f64(0x5bc);'),
        ('frac-zero', 'if (X87_C0(low, angle) && X87_C0(high, angle))', 'if (X87_C0(low, angle) || X87_C0(high, angle))'),
        ('frac-high', 'ratio = (high - angle) / (high - SF64(0x14c8));', 'ratio = (angle - high) / (high - SF64(0x14c8));'),
        ('lev-cap', 'if ((int16_t)bank >= 0x1554) bank = 0x1554;', 'if ((int16_t)bank > 0x1554) bank = 0x1555;'),
        ('lev-sum', 'sim1_clamp32((uint32_t)first + S32(0x950))', 'sim1_clamp32((uint32_t)first)'),
        ('lev-hold', 'if (S8(0xa2a)) S16(0x9e4) = 0;', '(void)0;'),
        ('flare-gt', 'if (SI32(0x1d18) > (int32_t)sim1_u32(0xafb)) return;', 'if (SI32(0x1d18) >= (int32_t)sim1_u32(0xafb)) return;'),
        ('flare-idle', 'if (P32(global, 0x2bc) <= sim1_u32(0xb0f)) sim1_set_u32(0xb27, 0);', 'if (P32(global, 0x2bc) < sim1_u32(0xb0f)) sim1_set_u32(0xb27, 0);'),
        ('flare-engines', 'if (at && P16(global, 0xaec) < at + 1) return;', 'if (at && P16(global, 0xaec) < at) return;'),
        ('flare-ease', '* sim1_f64(0xb1f) + PI16(global, throttles[at]));', '* sim1_f64(0xb17) + PI16(global, throttles[at]));'),
        ('copy-skip', '{0, 2, 4, 6, 8, 0xa, 0xc, 0xe, 0x14, 0x16, 0x18}', '{0, 2, 4, 6, 8, 0xa, 0xc, 0xe, 0x10, 0x16, 0x18}'),
        ('mat-kept', 'SF64(0x11f8) = (double)((long double)SF64(0x1568) * a - b);', 'SF64(0x11f8) = (double)((long double)SF64(0x1568) * ((long double)SF64(0x1580) * SF64(0x1578)) - b);'),
        ('mat-sign', 'SF64(0x1218) = (double)((long double)SF64(0x1568) * b - a);', 'SF64(0x1218) = (double)((long double)SF64(0x1568) * b + a);'),
        ('wind-ground', 'PF64(part, 0x50) = (double)-(long double)SF64(0x13c0);', 'PF64(part, 0x50) = SF64(0x13c0);'),
        ('wind-arm', '- (long double)SF64(0x13f8) * PF64(part, 0x30));', '- (long double)SF64(0x13f8) * PF64(part, 0x38));'),
        ('wind-rounded', '+ rounded) - (long double)SF64(0x13f0)', '+ kept + 0 * rounded) - (long double)SF64(0x13f0)'),
        ('ann-gs', 'S8(0xa27) = (uint8_t)glideslope;', 'S8(0xa27) = glideslope != 0;'),
        ('ann-loc', 'if (!(P8(station, 0x1c) & 0x20) && P32(global, 0x804))', 'if (!(P8(station, 0x1c) & 0x10) && P32(global, 0x804))'),
        ('ann-clear', 'P32(global, 0x7fc) = 0; P32(global, 0x7c4) = 0;', 'P32(global, 0x7fc) = 0;'),
        ('ann-alt', 'if (!P8(global, 0xb67) && P32(global, 0x7d8)) S8(0xa24) = 1;', 'if (!P8(global, 0xb67)) S8(0xa24) = 1;'),
        ('slip-max', 'if (!((int32_t)largest > (int32_t)other)) largest = other;', 'if (!(largest > other)) largest = other;'),
        ('slip-shift', 'divisor = (largest >> 15) + 1;', 'divisor = (largest >> 14) + 1;'),
        ('slip-sincos', 'sim1_data + 0x80c, sim1_data + 0x80a);', 'sim1_data + 0x80a, sim1_data + 0x80c);'),
        ('yawr-clamp', 'if (rate >= 0x7f) rate = 0x7f;\n    if (rate <= -0x7f) rate = -0x7f;\n    SI32(0x1720)', 'if (rate >= 0x7f) rate = 0x7e;\n    if (rate <= -0x7f) rate = -0x7f;\n    SI32(0x1720)'),
        ('yawr-gain', '(rate < (int32_t)sim1_u32(0x8b6) ? 0x8ba : 0x8c2)', '(rate < (int32_t)sim1_u32(0x8b6) ? 0x8c2 : 0x8ba)'),
        ('yawr-eighth', '(uint8_t)((int8_t)rate >> 3)', '(uint8_t)((uint8_t)rate >> 3)'),
        ('yawr-one', '((1.0L + SF64(0x1450)) * SF64(0x1570))', '((SF64(0x1450)) * SF64(0x1570))'),
        ('side-two', 'if (P32(global, 0x60c) != 2)', 'if (P32(global, 0x60c) != 1)'),
        ('side-sign', "if ((int16_t)P16(part, 0x16) < 0) factor = (uint16_t)-factor;", "(void)0;"),
        ('side-limit', 'if ((int16_t)b_high >= 0 && (int16_t)d_high >= 0)', 'if ((int16_t)b_high >= 0 && (int16_t)d_high > 0)'),
        ('side-borrow', 'd_high = (uint16_t)(a_high - b_high - borrow2);', 'd_high = (uint16_t)(a_high - b_high - 0 * borrow2);'),
        ('side-own', 'if (!P16(part, 0x14)) {', 'if (P16(part, 0x14)) {'),
        ('ice-power', 'if (!X87_C0C3(power * RF64(0x15138), (long double)RF64(0x15168)))', 'if (!X87_C0(power * RF64(0x15138), (long double)RF64(0x15168)))'),
        ('ice-both', 'if (P16(global, 0xf10) && P16(global, 0xf0e) == 1)', 'if (P16(global, 0xf10) || P16(global, 0xf0e) == 1)'),
        ('ice-cap', 'S32(0x1764) = 0x40100000; }', 'S32(0x1764) = 0x40080000; }'),
        ('ice-unsigned', '(long double)(int64_t)S32(0x30e4)', '(long double)SI32(0x30e4)'),
        ('rates-floor', 'if (!X87_C0C3(speed2, (long double)RF64(0x152e8)))', 'if (!X87_C0(speed2, (long double)RF64(0x152e8)))'),
        ('rates-cos', 'cosine = sim1_fcos((long double)(int32_t)SI16(0x151c)', 'cosine = sim1_fsin((long double)(int32_t)SI16(0x151c)'),
        ('rates-div', 'total /= SF64(0x1450);', 'total *= SF64(0x1450);'),
        ('phold-xor', '!(S8(0xa26) ^ S8(0xa2b))', '!(S8(0xa26) | S8(0xa2b))'),
        ('phold-bank', 'if ((int32_t)bank < 0) bank = 0u - bank;', '(void)0;'),
        ('phold-trim2', 'P16(global, 0xbc2) = trim;\n    SIM1_CALL', 'P16(global, 0xbc4) = trim;\n    SIM1_CALL'),
        ('athr-shift', '- (P32(global, 0x2bc) >> 7));', '- (P32(global, 0x2bc) >> 6));'),
        ('athr-stop', 'if (throttle >= 0x3fff) { throttle = 0x3fff; PF64(record, 0x38) = 0.0;', 'if (throttle >= 0x3fff) { throttle = 0x3fff; PF64(record, 0x30) = 0.0;'),
        ('athr-sound', '(0x16, 0);', '(0x17, 0);'),
        ('athr-gear', '&& !P8(global, 0x366)) {', ') {'),
        ('hterm-sine', 'first = product * sine;', 'first = product * sim1_fsin(latitude * RF64(0x15358));'),
        ('hterm-clamp', 'else if (X87_C0(first, (long double)RF64(0x15368))) first = RF64(0x15368);', '(void)0;'),
        ('hterm-wrap', 'if (X87_C0(sum, (long double)RF64(0x152e8))) sum -= RF64(0x15378);', 'if (X87_C0(sum, (long double)RF64(0x152e8))) sum += RF64(0x15378);'),
        ('turb-cap', 'if (strength >= 0xff) strength = 0xff;', 'if (strength >= 0xff) strength = 0xfe;'),
        ('turb-gain', '{0x14, 0x14, 0x1e}', '{0x14, 0x1e, 0x1e}'),
        ('turb-ground', 'if (P32(global, 0x854)) { SF64(0x60c) = 0.0;', 'if (P32(global, 0x858)) { SF64(0x60c) = 0.0;'),
        ('timers-shift', 'step *= (long double)(int32_t)(factor >> 10);', 'step *= (long double)(int32_t)(factor >> 9);'),
        ('timers-list', '{0x23e0, 0x26c0, 0x1d00, 0x1de0, 0x2580, 0x2aa0, 0x2000}', '{0x23e0, 0x26c0, 0x1d00, 0x1de0, 0x2580, 0x2aa0, 0x2008}'),
        ('timers-clear', 'P32(owner, 0x94) = 0;\n    for', 'for'),
        ('coup-limit', 'S16(0x9a2) = S8(0xa29) ? 0x16c : 0x1c7;', 'S16(0x9a2) = S8(0xa29) ? 0x16d : 0x1c7;'),
        ('coup-back', 'if (S8(0xa25)) heading = (uint16_t)(heading + 0x8000);', 'if (S8(0xa25)) heading = (uint16_t)(heading + 0x7fff);'),
        ('coup-band', 'if ((int16_t)magnitude > (int16_t)sim1_u16(0xa93)) return;', 'if ((int16_t)magnitude >= (int16_t)sim1_u16(0xa93)) return;'),
        ('coup-clamp', 'if (!S16(0x9ae)) return;\nsteer:', 'steer:'),
        ('coup-sign', 'if (S8(0xa25)) steer = -steer;', '(void)0;'),
        ('coup-take', 'P32(global, 0x7d8) = 0;\n    S8(0xa24) = 0;\n}', 'P32(global, 0x7d8) = 0;\n}'),
        ('struct-mode', 'if (S32(0x162c) && P16(global, 0x830)) {', 'if (P16(global, 0x830)) {'),
        ('struct-limit', 'if (S32(0x2510) > 0xbb7e890u)', 'if (S32(0x2510) >= 0xbb7e890u)'),
        ('struct-gear', 'if (P8(global, 0x11ec) == 2) P16(global, 0x832) = 8;', 'if (P8(global, 0x11ec) == 1) P16(global, 0x832) = 8;'),
        ('struct-words', 'if (!P16(global, 0x832)) sim1_record_words();', '(void)0;'),
        ('eng-scale', '* divisor) * SF64(0x690);', '* divisor) * SF64(0x698);'),
        ('eng-flag', 'S32(0x118c) = X87_C0(', 'S32(0x118c) = !X87_C0('),
        ('eng-off', 'if (P8(global, 0xb73)) { P32(global, 0xb18) = 0;', 'if (P8(global, 0xb74)) { P32(global, 0xb18) = 0;'),
        ('gov-quirk', 'if ((uint32_t)value >= sim1_u32(0xac7)) value = (int32_t)PI16(global, throttles[at]) + (int32_t)sim1_u32(0xabf);', 'if ((uint32_t)value >= sim1_u32(0xac7)) value = (int32_t)PI16(global, throttles[at]) + (int32_t)sim1_u32(0xabf); else value = PI16(global, throttles[at]);'),
        ('gov-stop', 'if (value <= 0) value = 0;\n        P16(global, throttles[at])', 'if (value < 0) value = 1;\n        P16(global, throttles[at])'),
        ('steer-off', '*cosine_out = 0x7fff; return; }', '*cosine_out = 0x7ffe; return; }'),
        ('steer-fixed', 'angle = RF64(0x15210);\n    else {', 'angle = RF64(0x15218);\n    else {'),
        ('steer-sign', 'angle = X87_C0(aileron + rudder, (long double)RF64(0x15210)) ? -magnitude : magnitude;', 'angle = magnitude;'),
        ('steer-trunc', '*sine_out = (uint16_t)sim1_ftol(', '*sine_out = (uint16_t)sim1_fistp32('),
        ('travel-bit', 'if (!(P8(global, 0x364) & (4u << group))) continue;', 'if (!(P8(global, 0x364) & (2u << group))) continue;'),
        ('travel-copy', 'memcpy(sim1_data + groups[group][at], sim1_data + groups[group][at] + 8, 8);', 'SF64(groups[group][at]) = SF64(groups[group][at] + 8) + 0.0 * 1;'),
        ('gs-shift', 'value = (uint32_t)((int32_t)quotient >> 17);\n    value = (uint32_t)((int64_t)(int32_t)value * (int32_t)sim1_u32(0x9f4)) << 1;\n    sim1_set_u32(0x99c, value);\n    sim1_set_u32(0x99c, sim1_u32(0x99c) + (P32(global, 0x86c) << 8) * 0x348u);\n    {\n        int64_t product = (int64_t)(int32_t)(sim1_u32(0x99c) - sim1_u32(0x9c2)) * (int16_t)S16(0x99a);\n        sim1_set_u32(0x9f0, (uint32_t)((uint64_t)product >> 15));\n    }\n    {\n        uint32_t height = (uint32_t)(((uint64_t)(S32(0x1d18) - (P32(global, 0x86c) << 16)) * 0x348u) >> 32);\n        uint32_t square = height * height + (uint32_t)sim1_u16(0x9f4) * sim1_u16(0x9f4);\n        uint16_t distance = (uint16_t)sim1_isqrt64((int64_t)(uint64_t)square);\n        if (distance <= 0xa) distance = 0xa;\n        sim1_set_u16(0x9f8, distance);\n    }\n    deviation = (int32_t)sim1_idiv32((uint32_t)((int32_t)sim1_u32(0x9f0) >> 31), sim1_u32(0x9f0), sim1_u32(0x9f4), &remainder);\n    if (sim1_u16(0xa2b)', 'value = (uint32_t)((int32_t)quotient >> 16);\n    value = (uint32_t)((int64_t)(int32_t)value * (int32_t)sim1_u32(0x9f4)) << 1;\n    sim1_set_u32(0x99c, value);\n    sim1_set_u32(0x99c, sim1_u32(0x99c) + (P32(global, 0x86c) << 8) * 0x348u);\n    {\n        int64_t product = (int64_t)(int32_t)(sim1_u32(0x99c) - sim1_u32(0x9c2)) * (int16_t)S16(0x99a);\n        sim1_set_u32(0x9f0, (uint32_t)((uint64_t)product >> 15));\n    }\n    {\n        uint32_t height = (uint32_t)(((uint64_t)(S32(0x1d18) - (P32(global, 0x86c) << 16)) * 0x348u) >> 32);\n        uint32_t square = height * height + (uint32_t)sim1_u16(0x9f4) * sim1_u16(0x9f4);\n        uint16_t distance = (uint16_t)sim1_isqrt64((int64_t)(uint64_t)square);\n        if (distance <= 0xa) distance = 0xa;\n        sim1_set_u16(0x9f8, distance);\n    }\n    deviation = (int32_t)sim1_idiv32((uint32_t)((int32_t)sim1_u32(0x9f0) >> 31), sim1_u32(0x9f0), sim1_u32(0x9f4), &remainder);\n    if (sim1_u16(0xa2b)'),
        ('gs-window', 'if (sim1_u16(0xa2b) && (deviation > 0x19 || deviation < -0x19)) return;', 'if (sim1_u16(0xa2b) && (deviation > 0x19 || deviation < -0x18)) return;'),
        ('gs-off', "if (S8(0xa26) != 1) { sim1_set_u32(0x9f0, 0u - S32(0x1d18) * 0x348u); return; }", "if (S8(0xa26) != 1) { sim1_set_u32(0x9f0, S32(0x1d18) * 0x348u); return; }"),
        ('rec-x87', 'PF64(record, 0) = sim1_x87(SF64(0x1538));', 'memcpy(record, sim1_data + 0x1538, 8);'),
        ('rec-flag', 'P32(record, 0x54) = P8(SIM1_GLOBAL, 0x366);', 'P32(record, 0x54) = P8(SIM1_GLOBAL, 0x367);'),
        ('look-empty', 'if (!count) return x;', 'if (!count) return 0;'),
        ('look-first', 'if (X87_C0C3((long double)x, (long double)TABLE_X(0))) return TABLE_Y(0);', 'if (X87_C0((long double)x, (long double)TABLE_X(0))) return TABLE_Y(0);'),
        ('look-round', 'double y0 = TABLE_Y(middle);', 'long double y0 = TABLE_Y(middle) + 0.0L;'),
        ('look-exact', 'if (!X87_C0((long double)x, (long double)TABLE_X(middle))) return TABLE_Y(middle);', 'if (0) return TABLE_Y(middle);'),
        ('cap-clear', "if (deviation >= 0x38e) { S16(0x9de) = 0; S16(0x9e0) = 0; target = 0x38e; }", "if (deviation >= 0x38e) { S16(0x9de) = 0; target = 0x38e; }"),
        ('cap-ease', 'S16(0x9e2) = (uint16_t)((uint32_t)-ease + (uint32_t)(int32_t)SI16(0x9e2));', 'S16(0x9e2) = (uint16_t)((uint32_t)ease + (uint32_t)(int32_t)SI16(0x9e2));'),
        ('cap-idle', 'if (P16(global, 0x88c)) return;\n    {', '{'),
        ('rota-kept', '*second = (double)(w * SF64(0x1570) - (long double)kept2 * SF64(0x1578));', '*second = (double)(w * SF64(0x1570) - t2 * SF64(0x1578)); (void)kept2;'),
        ('rota-neg', '*first = (double)-(u1 * SF64(0x1570) - u2 * SF64(0x1578));', '*first = (double)(u1 * SF64(0x1570) - u2 * SF64(0x1578));'),
        ('rotb-swap', 'double swapped_a = sim1_x87(b), swapped_b = (double)-(long double)a;', 'double swapped_a = sim1_x87(a), swapped_b = (double)-(long double)b;'),
        ('rotb-kept', '*third = (double)(w * SF64(0x1580) - (long double)kept2 * SF64(0x1588));', '*third = (double)(w * SF64(0x1580) - v2 * SF64(0x1588)); (void)kept2;'),
        ('inertia-kept', 'SF64(0x1120) = (double)((long double)kept2 * mass', '(void)kept2; SF64(0x1120) = (double)(m2 * mass'),
        ('inertia-arm', 'SF64(0x15c0) = (double)((long double)arm * chord);', 'SF64(0x15c0) = (double)((long double)arm * span);'),
        ('inertia-density', 'SF64(0x1138) = (double)(density * mass * chord * span * RF64(0x15110));', 'SF64(0x1138) = (double)(density * mass * span * span * RF64(0x15110));'),
        ('quat-kept', 'sp = sim1_fsin(half_pitch), ch = sim1_fcos(kept_heading);', 'sp = sim1_fsin(half_pitch), ch = sim1_fcos(half_heading + 0 * kept_heading);'),
        ('quat-sign', 'long double bank = -((long double)(int64_t)P32(global, 0x578)', 'long double bank = ((long double)(int64_t)P32(global, 0x578)'),
        ('quat-term', 'SF64(0x1068) = (double)((long double)t10 * cb - (long double)t18 * dsb);', 'SF64(0x1068) = (double)((long double)t10 * cb + (long double)t18 * dsb);'),
        ('park-mode', 'if (S32(0x162c) < 3) return;\n    if (!X87_C0((long double)(int64_t)P32(SIM1_GLOBAL, 0x2b4)', 'if (S32(0x162c) < 2) return;\n    if (!X87_C0((long double)(int64_t)P32(SIM1_GLOBAL, 0x2b4)'),
        ('park-floor', 'if (!X87_C0(SQUARES(0x1418), (long double)RF64(0x150b0))) return;', 'if (!X87_C0(SQUARES(0x1418), (long double)RF64(0x150a8))) return;'),
        ('park-list', '0x1070, 0x1078, 0x1080, 0x1088};', '0x1070, 0x1078, 0x1080, 0x1080};'),
        ('ffb-axes', 'if (!axes[0] || !axes[1] || !axes[2]) {', 'if (!axes[0] || !axes[1]) {'),
        ('ffb-ground', 'forces[0] = 0.0; forces[1] = 0.0;', 'forces[0] = 0.0; forces[1] = 1.0;'),
        ('ffb-neg', 'SI32(0x1718) = sim1_fistp32(-elevator);', 'SI32(0x1718) = sim1_fistp32(elevator);'),
        ('ffb-smooth', 'forces[1] = (double)(((long double)sim1_f64(0x39a) - sim1_f64(0x3aa))', 'forces[1] = (double)(((long double)sim1_f64(0x392) - sim1_f64(0x3aa))'),
        ('alloc-free', 'if (P32(slot, 0)) SIM1_CALL(SIM1_HOST, 0x214', 'if (0) SIM1_CALL(SIM1_HOST, 0x214'),
        ('alloc-size', '(1, 0xc0, SIM1_GLOBAL + 0x11d8, 1);', '(1, 0xb0, SIM1_GLOBAL + 0x11d8, 1);'),
        ('save-tag', '(0x38e, 0xc0))', '(0x38f, 0xc0))'),
        ('save-cond', 'if (!SIM1_CALL(5, 0x5c,', 'if (SIM1_CALL(5, 0x5c,'),
        ('terms-ptr', 'S32(0x1fd8) = (uint32_t)(uintptr_t)(global + 0x9bc);', 'S32(0x1fd8) = (uint32_t)(uintptr_t)(global + 0x9b8);'),
        ('terms-kept', 'SF64(0x25f0) = (double)(((long double)kept - sim1_table_lookup', 'SF64(0x25f0) = (double)(((long double)kept + sim1_table_lookup'),
        ('terms-div', 'SF64(0x1770) = (double)(count / b - (a * count + b * SF64(0x3fd8)) / (b * a));', 'SF64(0x1770) = (double)(count / a - (a * count + b * SF64(0x3fd8)) / (b * a));'),
        ('power-mode', 'if (P8(SIM1_GLOBAL, 0x609) == 1) {', 'if (P8(SIM1_GLOBAL, 0x609) == 2) {'),
        ('power-root', 'long double root = sim1_fsqrt((long double)SF64(0x7b0) * SF64(0x7a0));', 'long double root = ((long double)SF64(0x7b0) * SF64(0x7a0));'),
        ('thrust-rounded', 'return root * ((long double)first * advance / second);', 'return root * ((long double)first * advance / (second + 0.0L));'),
        ('thrust-k', 'long double k = (long double)SF64(0x6e0) / (long double)(int32_t)(uint32_t)S16(0x2c78);', 'long double k = (long double)SF64(0x6e8) / (long double)(int32_t)(uint32_t)S16(0x2c78);'),
        ('eff-floor', 'if (X87_C0(value, (long double)RF64(0x152e8))) value = RF64(0x152e8);', '(void)0;'),
        ('pm-flaps', 'flaps = (double)(sim1_table_lookup(sim1_data + 0x3100, SF64(0x14d0)) + flaps);', 'flaps = (double)(sim1_table_lookup(sim1_data + 0x3100, SF64(0x14c8)) + flaps);'),
        ('pm-tail', '* RF64(0x15118));\n    total', '* RF64(0x15120));\n    total'),
        ('pm-rate', 'sum = (long double)SF64(0x1528) * SF64(0x17b8) + (long double)SF64(0x1400) * SF64(0x17a8);', 'sum = (long double)SF64(0x1528) * SF64(0x17b8) - (long double)SF64(0x1400) * SF64(0x17a8);'),
        ('setup-carry', '+ ((uint32_t)(uint16_t)raise + P16(global, 0x572) > 0xffff);', ';'),
        ('setup-gear', 'P8(global, 0x365) = 7;', 'P8(global, 0x365) = 6;'),
        ('setup-msg', 'if (P32(global, 0xbe8) != 0x3fff) SIM1_CALL', 'if (P32(global, 0xbe8) == 0x3fff) SIM1_CALL'),
        ('setup-578', 'P32(global, 0x578) = (raise & 0xffff0000u) | low;', 'P32(global, 0x578) = raise;'),
        ('wheel-friction', 'friction = P8(global, 0x11ec) == 1 ? 0xf5c : 0xa3d;', 'friction = P8(global, 0x11ec) == 1 ? 0xa3d : 0xf5c;'),
        ('wheel-sign', 'if (SI32(0x1718) < 0) cx = (uint16_t)-cx;', '(void)0;'),
        ('wheel-order', 'sim1_set_u32(0x7fe, (uint32_t)product);', 'sim1_set_u16(0x7fe, (uint16_t)product);'),
        ('wheel-pick', 'int use_b = (int16_t)a_high >= 0 ? d_high >= 0 : d_high < 0;', 'int use_b = (int16_t)a_high >= 0 ? d_high > 0 : d_high < 0;'),
        ('wheel-park', 'wheel == sim1_data + 0x2200)', 'wheel == sim1_data + 0x2208)'),
        ('flare-level', 'level = (uint32_t)(((uint64_t)(uint32_t)height * 0x68fe) >> 29);', 'level = (uint32_t)(((uint64_t)(uint32_t)height * 0x68fe) >> 28);'),
        ('flare-sink', 'if (sink <= (int32_t)level) goto off;', 'if (sink < (int32_t)level) goto off;'),
        ('flare-cut', 'uint16_t step = (uint16_t)(((int32_t)(int16_t)cut * 0x1400) >> 16);', 'uint16_t step = (uint16_t)(((int32_t)(int16_t)cut * 0x1401) >> 16);'),
        ('flare-sat', 'S16(0xa18) = sim1_add16_saturated(S16(0xa18), word);', 'S16(0xa18) = (uint16_t)(S16(0xa18) + word);'),
        ('flare-floor', "if ((int16_t)word <= (int16_t)0xf556) word = 0xf556;", "if ((int16_t)word < (int16_t)0xf556) word = 0xf557;"),
        ('course-wind', 'uint16_t wind_angle = (uint16_t)(P16(global, 0xe92) - P16(global, 0x870) + 0x8000)', 'uint16_t wind_angle = (uint16_t)(P16(global, 0xe92) - P16(global, 0x870))'),
        ('course-shift', 'divisor, &remainder) >> 1;\n    x =', 'divisor, &remainder) >> 2;\n    x ='),
        ('course-max', 'magnitude = (int16_t)scale_y > (int16_t)scale_x ? scale_y : scale_x;', 'magnitude = (int16_t)scale_y > (int16_t)scale_x ? scale_x : scale_y;'),
        ('course-limit', 'if ((int16_t)target >= (int16_t)S16(0x9a2)) { target = S16(0x9a2); S16(0x9aa) = 1; }', 'if ((int16_t)target > (int16_t)S16(0x9a2)) { target = S16(0x9a2); S16(0x9aa) = 1; }'),
        ('course-fixed', 'heading = (uint16_t)(S16(0x9ac) + sim1_u16(0xa99));', 'heading = (uint16_t)(S16(0x9ac));'),
        ('course-intercept', 'S16(0x9ac) = (uint16_t)(((int16_t)error >= 0 ? 0x1554 : -0x1554) + S16(0x9a8));', 'S16(0x9ac) = (uint16_t)(((int16_t)error > 0 ? 0x1554 : -0x1554) + S16(0x9a8));'),
        ('alt-sign', 'if ((int32_t)((uint32_t)((int32_t)P32(global, 0x7d4) >> 16) - P32(global, 0x574)) < 0) value = 0u - value;', 'if ((int32_t)((int32_t)P32(global, 0x7d4) >> 16) < (int32_t)P32(global, 0x574)) value = 0u - value;'),
        ('alt-rate', 'rate = (int32_t)((uint32_t)rate * 0x3cu);', 'rate = (int32_t)((uint32_t)rate * 0x3du);'),
        ('alt-capture', 'if (!(magnitude_climb < magnitude_rate)) { climb = rate;', 'if (!(magnitude_climb <= magnitude_rate)) { climb = rate;'),
        ('alt-floor', 'if ((int32_t)(sim1_u32(0x9c2) >> 16) >= 0x4e20 && (int16_t)pitch <= (int16_t)0xf8e4) pitch = 0xf8e4;', '(void)0;'),
        ('yawm-legs', '((long double)SF64(0x26c0) - SF64(0x2580)) * (long double)SI32(0x2b90))', '((long double)SF64(0x26c0) - SF64(0x2580)) * (long double)SI32(0x2c50))'),
        ('yawm-slip', '* (long double)(int32_t)(uint32_t)P16(global, 0xc44) * RF64(0x15138);', '* (long double)(int32_t)(uint32_t)P16(global, 0xc44) * RF64(0x15140);'),
        ('carb-heat', 'if (P32(global, 0x29c) == 1) SF64(0x1900) = (double)((long double)SF64(0x1900) + SF64(0x16f0));', 'if (P32(global, 0x29c) == 1) SF64(0x1900) = (double)((long double)SF64(0x1900) - SF64(0x16f0));'),
        ('carb-cap', 'if (!X87_C0((long double)SF64(0x1900), (long double)RF64(0x15328))) { S32(0x1900) = 0; S32(0x1904) = 0x40590000; }', '(void)0;'),
        ('carb-ice', "S32(0x18f0) = 0; S32(0x18f4) = 0x3ff00000; S32(0x18f8) = 1; }", "S32(0x18f0) = 0; S32(0x18f4) = 0x3ff00000; }"),
        ('carb-lag', 'value = (long double)SF64(0x1488) - ((long double)demand - SF64(0x1488)) * RF64(0x15350);', 'value = (long double)SF64(0x1488) + ((long double)demand - SF64(0x1488)) * RF64(0x15350);'),
        ('rotor-ground', "if (!P32(global, 0x920) && P16(global, 0x33c) == 1) { S32(0x1c00) = 0; S32(0x1c04) = 0; }", "(void)0;"),
        ('rotor-swap', 'SF64(0x1c58) = (double)(sum * kept_pitch * RF64(0x153e0));', 'SF64(0x1c58) = (double)(sum * kept_roll * RF64(0x153e0));'),
        ('height-below', 'high = high - eax - (bx < word);', 'high = high - eax - 0 * (bx < word);'),
        ('pprop-set', 'if (P32(global, 0x7e4)) {\n        value = (long double)PI32(global, 0x7e8)', 'if (!P32(global, 0x7e4)) {\n        value = (long double)PI32(global, 0x7e8)'),
        ('pprop-curve', 'value = sim1_prop_efficiency(SF64(0x1718)); }', 'value = sim1_prop_thrust(SF64(0x1718)); }'),
        ('pprop-div', '* SF64(0x740) / SF64(0x1468) * SF64(0x6d8));', '* SF64(0x740) * SF64(0x1468) * SF64(0x6d8));'),
        ('pprop-park', 'if (P16(global, 0x612)) { SF64(0x1718) = (double)value; value = sim1_prop_thrust', 'if (P16(global, 0x612)) { value = sim1_prop_thrust'),
        ('ppass-index', 'S16(0x7fc) = index;', 'S16(0x7fc) = index + 1;'),
        ('ppass-off', 'if (P32(part, 4) && P32(part, 8)) {', 'if (P32(part, 4)) {'),
        ('ppass-mark', 'if (!P32(part, 0)) sim1_mark_part(global, part);', 'sim1_mark_part(global, part);'),
        ('ppass-count', 'sim1_set_u32(0x812, sim1_u32(0x812) + 1);', '(void)0;'),
        ('ppass-steer', '(uint16_t *)(sim1_data + 0x7d2), (uint16_t *)(sim1_data + 0x7d0));', '(uint16_t *)(sim1_data + 0x7d0), (uint16_t *)(sim1_data + 0x7d2));'),
        ('deinit-list', '(5, sim1_callbacks[1]);', '(6, sim1_callbacks[1]);'),
        ('deinit-ground', 'if (P32(global, 0x604) == 1) P32(global, 0x278) = S32(0x1c80);', '(void)0;'),
        ('deinit-jet', 'if (P8(global, 0x609) == 1 && !S32(0x1778)) {', 'if (P8(global, 0x609) == 1) {'),
        ('deinit-start', 'P16(global, 0x892) = 3; P16(global, 0x92a) = 3;', 'P16(global, 0x892) = 2; P16(global, 0x92a) = 3;'),
        ('r16-round', 'return (uint16_t)((sum >> 16) + ((sum >> 15) & 1));', 'return (uint16_t)(sum >> 16);'),
        ('r16-order', 'if (S32(0x7c0) != 1) {\n        uint16_t c = di', 'if (S32(0x7c0) == 1) {\n        uint16_t c = di'),
        ('r16-neg', 'uint16_t c = di, d = (uint16_t)-cx;', 'uint16_t c = di, d = cx;'),
        ('r16-di', 'out_di = sim1_fixed_round(FIXED_PRODUCT(S16(0x1594), second) - FIXED_PRODUCT(S16(0x1590), si));', 'out_di = sim1_fixed_round(FIXED_PRODUCT(S16(0x1594), second) + FIXED_PRODUCT(S16(0x1590), si));'),
        ('srest-angle', "angle = ((uint32_t)P16(record, 2) << 16) + P16(record, 0x30);", "angle = ((uint32_t)P16(record, 2) << 16) + P16(record, 0x32);"),
        ('srest-signed', "SF64(0x1538) = (double)((long double)(int64_t)angle * scale);", "SF64(0x1538) = (double)((long double)(int32_t)angle * scale);"),
        ('srest-rate', "SF64(0x13f0) = (double)((long double)(int32_t)PI16(record, 0xa) * SF64(0x680));", "SF64(0x13f0) = (double)((long double)(int32_t)PI16(record, 8) * SF64(0x680));"),
        ('srest-clear', "0x1418, 0x1420, 0x1428, 0x1760};", "0x1418, 0x1420, 0x1428, 0x1768};"),
        ('srest-ground', "P8(SIM1_GLOBAL, 0x366) = 0;\n    sim1_attitude_trig();\n    sim1_rotate_b(SF64(0x13a0), SF64(0x13a8), SF64(0x13b0), &SF64(0x13b8), &SF64(0x13c0), &SF64(0x13c8));\n    sim1_track();\n}\n\n/* 0x22011290", "sim1_attitude_trig();\n    sim1_rotate_b(SF64(0x13a0), SF64(0x13a8), SF64(0x13b0), &SF64(0x13b8), &SF64(0x13c0), &SF64(0x13c8));\n    sim1_track();\n}\n\n/* 0x22011290"),
        ('sset-full', 'if (size == 0xc0) {', 'if (size >= 0xb8) {'),
        ('sset-chop', 'S32(0x1554) = (uint32_t)sim1_ftol(', 'S32(0x1554) = (uint32_t)sim1_fistp32('),
        ('sset-366', 'P8(SIM1_GLOBAL, 0x366) = P8(record, 0x54);', 'P8(SIM1_GLOBAL, 0x366) = P8(record, 0x55);'),
        ('sset-rest', 'memcpy(sim1_data + rest[at], record + 0x58 + at * 8, 8);', 'memcpy(sim1_data + rest[at], record + 0x60 + at * 8, 8);'),
        ('smsg-mark', '        S32(0x1778) = 1;\n', ''),
        ('smsg-tag', 'if (P32(event, 0) == 0x38e) sim1_situation_set', 'if (P32(event, 0) != 0x5b) sim1_situation_set'),
        ('smsg-size', '(const uint8_t *)(uintptr_t)P32(event, 0xc), P32(event, 8));', '(const uint8_t *)(uintptr_t)P32(event, 0xc), 0xc0);'),
        ('smsg-save', 'if (message == 0x23) sim1_record_save();', 'if (message >= 0x23) sim1_record_save();'),
        ('sstart-kept', 'if (kept) { sim1_situation_set(kept, 0xc0); return; }', 'if (kept) { sim1_situation_set(kept, 0xc0); }'),
        ('sstart-tag', '(0x5a, &record[0], &record[1]))', '(0x5b, &record[0], &record[1]))'),
        ('sstart-free', '    else return;\n    SIM1_CALL(SIM1_HOST, 0x214, uint32_t (__stdcall *)(void *))(&record[0]);', '    else return;'),
        ('sstart-size', 'sim1_situation_set((const uint8_t *)(uintptr_t)record[0], record[1]);', 'sim1_situation_set((const uint8_t *)(uintptr_t)record[0], 0xc0);'),
        ('gpass-996', '    if (S16(0x996)) goto off;\n    if (S32(0x1624) == 2) return;', '    if (S32(0x1624) == 2) return;'),
        ('gpass-1624', '    if (S32(0x1624) == 2) return;\n    sim1_prop_pitch(global);', '    if (S32(0x1624) == 3) return;\n    sim1_prop_pitch(global);'),
        ('gpass-7dc', 'if (!P32(global, 0x7dc)) goto off;', 'if (!P32(global, 0x7dc)) return;'),
        ('gpass-fixed', '    if (!P16(global, 0x612)) {\ncurve:', '    if (P16(global, 0x612)) {\ncurve:'),
        ('gpass-park', '        S32(0x1718) = rpm;\n        SF64(0x1718)', '        SF64(0x1718)'),
        ('gpass-load', 'sim1_set_u32(0xa9f, rpm - (P32(global, 0x2bc) >> 7));', 'sim1_set_u32(0xa9f, rpm - (P32(global, 0x2bc) >> 8));'),
        ('gpass-cap', 'if (step >= (int32_t)sim1_u32(0xac3)) step = (int32_t)sim1_u32(0xac3);', 'if (step >= (int32_t)sim1_u32(0xac3)) step = (int32_t)sim1_u32(0xac3) - 1;'),
        ('gpass-sign', 'if ((int32_t)sim1_u32(0xabf) >= 0) {', 'if ((int32_t)sim1_u32(0xabf) > 0) {'),
        ('gpass-fourth', '|| P16(global, 0xa60) >= limit) {', '|| P16(global, 0xa60) > limit) {'),
        ('gpass-third', '|| P16(global, 0x9c8) > limit ||', '|| P16(global, 0x9c8) >= limit ||'),
        ('gpass-cleared', '            sim1_governor(global);\n            goto off;', '            sim1_governor(global);\n            return;'),
        ('gpass-acf', '    sim1_set_u32(0xacf, 0);\n    if ((int32_t)sim1_u32(0xabf) >= 0) {', '    if ((int32_t)sim1_u32(0xabf) >= 0) {'),
        ('below-scale', '(uint16_t)((int32_t)(S32(point + 0x28) * 0xd) >> 1),', '(uint16_t)((int32_t)(S32(point + 0x28) * 0xd) >> 2),'),
        ('below-up', '    uint32_t height = 0x10000, below;', '    uint32_t height = 0x8000, below;'),
        ('below-wheel', 'if (S32(0x29e4)) S32(0x1628) += sim1_contact_below(0x29e0, 1, 1);', 'if (S32(0x29e4)) S32(0x1628) += sim1_contact_below(0x29e0, 1, 0);'),
        ('below-park', 'if (wheel) height += (uint32_t)(SI32(0x1718) = sim1_fistp32(', 'if (wheel) height += (uint32_t)(sim1_fistp32('),
        ('below-history', '    S32(point + 0xc) = S32(point + 8);\n', ''),
        ('below-edi', 'sim1_contact_below(0x2200, P32(global, 0xbf4) != 0, 1)', 'sim1_contact_below(0x2200, P32(SIM1_GLOBAL + 0 * (uintptr_t)global, 0xbf4) != 0, 1)'),
        ('below-gear', 'P32(SIM1_GLOBAL, 0xbf0) != 0, 1); S32(0x1624) += count;', 'P32(SIM1_GLOBAL, 0xbec) != 0, 1); S32(0x1624) += count;'),
        ('below-1624', 'if (S32(0x2404)) S32(0x1628) += sim1_contact_below(0x2400', 'if (S32(0x2404)) S32(0x1624) += sim1_contact_below(0x2400'),
        ('below-sum', 'S32(0x1620) = S32(0x1620) + S32(0x1618) + S32(0x161c);', 'S32(0x1620) = S32(0x1620) + S32(0x1618);'),
        ('below-366', 'P8(SIM1_GLOBAL, 0x366) = S32(0x1628) != 0;', 'P8(SIM1_GLOBAL, 0x366) = S32(0x162c) != 0;'),
        ('below-sign', 'below = (int32_t)height < 0;', 'below = (int32_t)height <= 0;'),
        ('atm-any', 'if (!P32(global, 0x774) && !P32(global, 0x76c) && !P32(global, 0x770)) goto done;', 'if (!P32(global, 0x774) && !P32(global, 0x76c)) goto done;'),
        ('atm-810', '            P32(global, 0x80c) = 0; P32(global, 0x7dc) = 0; P32(global, 0x7e4) = 0;\n            goto done;', '            P32(global, 0x80c) = 0; P32(global, 0x7dc) = 0;\n            goto done;'),
        ('atm-new', '            if (!sim1_u32(0xae7)) {', '            if (!sim1_u32(0xad7)) {'),
        ('atm-takeoff', 'sim1_set_u32(0xad3, sim1_u32(0xad3) | 1); sim1_set_u32(0xadb, 0);', 'sim1_set_u32(0xad3, 1); sim1_set_u32(0xadb, 0);'),
        ('atm-height', '} else if (!(SI32(0x1d18) > (int32_t)sim1_u32(0xae3))) {', '} else if (SI32(0x1d18) < (int32_t)sim1_u32(0xae3)) {'),
        ('atm-a21', "P32(global, 0x7dc) = 0; P32(global, 0x7e4) = 0; S8(0xa21) = 0;\n                } else {", "P32(global, 0x7dc) = 0; P32(global, 0x7e4) = 0;\n                } else {"),
        ('atm-ended', '} else if (sim1_u32(0xae7)) { sim1_set_u32(0xad3, 0); sim1_set_u32(0xadb, 0); }', '}'),
        ('atm-hold', '        sim1_governor_pass(global);\n        goto done;', '        goto done;'),
        ('atm-armed', '    if (!P32(global, 0x774)) goto done;\n    if (P8(global, 0x366)', '    if (P8(global, 0x366)'),
        ('atm-max', 'if (!((int32_t)rpm > (int32_t)P16(global, 0x930))) rpm = P16(global, 0x930);', '(void)0;'),
        ('atm-aeb', ') >> 16) < sim1_u32(0xaeb)) goto done;', ') >> 16) <= sim1_u32(0xaeb)) goto done;'),
        ('atm-heading', '    sim1_heading_9e6(global);\n    sim1_governor(global);\ndone:', '    sim1_governor(global);\ndone:'),
        ('atm-kept', '    sim1_set_u32(0xad7, sim1_u32(0xad3));', '    sim1_set_u32(0xad7, sim1_u32(0xadb));'),
        ('cterm-pair', '{0xb98, 0xb94, 0x1de0}', '{0xb98, 0xb98, 0x1de0}'),
        ('cterm-signed', '(long double)(int64_t)(uint64_t)P32(global, pairs[at][0]) *', '(long double)(int32_t)P32(global, pairs[at][0]) *'),
        ('cterm-scale', 'long double scale = (long double)(int32_t)PI16(global, 0xaf4) * SF64(0x698) * SF64(0x690);', 'long double scale = (long double)(int32_t)P16(global, 0xaf4) * SF64(0x698) * SF64(0x690);'),
        ('cterm-round', '* scale);\n}', '* (double)scale);\n}'),
        ('rlim-equal', '    if (X87_C3(value, (long double)target)) return value;\n', ''),
        ('rlim-step', 'step = (long double)rate * SF64(0x16f0) / divisor;', 'step = (long double)rate / SF64(0x16f0) * divisor;'),
        ('rlim-stop', 'return X87_C0(value, (long double)target) ? value : (long double)target;', 'return value;'),
        ('rlim-at', 'return X87_C0C3(value, (long double)target) ? (long double)target : value;', 'return X87_C0(value, (long double)target) ? (long double)target : value;'),
        ('rlim-round', '        value += step;\n', '        value = (double)(value + step);\n'),
        ('bump-none', 'if (!type || type == 4 || type == 2) {', 'if (!type || type == 4) {'),
        ('bump-change', '    if (type != S8(0x11b8)) {', '    if (1) {'),
        ('bump-table', '{0x70a3d70a, 0x3fb70a3d, 0x33333333, 0x3fc33333}', '{0x70a3d70a, 0x3fb70a3d, 0x33333333, 0x3fc33334}'),
        ('bump-past', 'const uint32_t *rate = type <= 7 ? rates[type - 1] : rates[1];', 'const uint32_t *rate = type <= 8 ? rates[(type - 1) % 7] : rates[1];'),
        ('bump-speed', '(long double)(int64_t)(uint64_t)P32(SIM1_GLOBAL, 0x2b4) * SF64(0x720)', '(long double)(int32_t)P32(SIM1_GLOBAL, 0x2b4) * SF64(0x720)'),
        ('bump-order', 'checked_steps[3] = {0x11c0, 0x11c8, 0x11d8};', 'checked_steps[3] = {0x11c8, 0x11c0, 0x11d8};'),
        ('bump-bound', 'if (!X87_C0C3((long double)SF64(checks[at]), (long double)SF64(0x11d0))) {', 'if (!X87_C0((long double)SF64(checks[at]), (long double)SF64(0x11d0))) {'),
        ('bump-floor', '            S32(checks[at]) = 0; S32(checks[at] + 4) = 0;\n', ''),
        ('bump-quiet', '    SF64(offset) = -sim1_x87(SF64(offset));', '    S32(offset + 4) ^= 0x80000000u;'),
        ('bump-kept', '    S8(0x11b8) = P8(SIM1_GLOBAL, 0x11ec);\n}\n', '}\n'),
        ('acc-mass', 'double mass = (double)(long double)(int64_t)(uint64_t)S32(0x1728);', 'double mass = (double)(long double)(int32_t)S32(0x1728);'),
        ('acc-cross', '+ (long double)SF64(0x13e8) * SF64(0x13b0)) - (long double)SF64(0x13f0) * SF64(0x13a0);', '- (long double)SF64(0x13e8) * SF64(0x13b0)) - (long double)SF64(0x13f0) * SF64(0x13a0);'),
        ('acc-hist', 'SF64(0x1340) = sim1_x87(old_x); SF64(0x1348) = sim1_x87(old_y);', 'SF64(0x1340) = sim1_x87(old_y); SF64(0x1348) = sim1_x87(old_x);'),
        ('acc-inertia', '+ SF64(0x1310)) / SF64(0x15e0);', '+ SF64(0x1310)) / SF64(0x15d8);'),
        ('acc-bound', "sim1_clamp_stored(0x1360, 0x15048, 0x15050, 0xd70a3d71, 0x4024f0a3);", "sim1_clamp_stored(0x1360, 0x15038, 0x15040, 0, 0x40745000);"),
        ('afol-keep', '    SF64(0x11a8) = sim1_x87(SF64(0x1198));\n', ''),
        ('afol-unsigned', 'value = scale * (long double)(int64_t)(uint64_t)P32(global, 0x578);', 'value = scale * (long double)PI32(global, 0x578);'),
        ('afol-wrap', 'SF64(0x1190) = (double)((long double)SF64(0x1190) - (long double)SF64(0x770) * RF64(0x151d0));', 'SF64(0x1190) = (double)((long double)SF64(0x1190) - (long double)SF64(0x770) * RF64(0x151d8));'),
        ('afol-path', 'if (!P8(global, 0xb67) && !S32(0x118c)) {', 'if (!P8(global, 0xb67)) {'),
        ('afol-time', 'if (S32(0x1188) && X87_C0((long double)SF64(0x1180), (long double)SF64(0x410))) {', 'if (S32(0x1188) && X87_C0C3((long double)SF64(0x1180), (long double)SF64(0x410))) {'),
        ('afol-decay', 'SF64(0x1178) = (double)(((long double)SF64(0x3f8) * SF64(0x16f0) - RF64(0x151d8)) * SF64(0x1178));', 'SF64(0x1178) = (double)(((long double)SF64(0x3f0) * SF64(0x16f0) - RF64(0x151d8)) * SF64(0x1178));'),
        ('afol-rise', 'else { S32(0x3d0) = 0; S32(0x3d4) = 0x3ff00000; }', 'else { S32(0x3d0) = 0; S32(0x3d4) = 0; }'),
        ('afol-signed', 'SF64(0x1160) = (double)((long double)SF64(0x690) * SF64(0x680) * (long double)PI32(global, 0x578));', 'SF64(0x1160) = (double)((long double)SF64(0x690) * SF64(0x680) * (long double)P32(global, 0x578));'),
        ('afol-reset', '            S32(0x1188) = 0;\n', ''),
        ('afol-ease', 'if (!X87_C0C3((long double)SF64(0x3d0), (long double)RF64(0x151e8))) SF64(0x3d0)', 'if (!X87_C0((long double)SF64(0x3d0), (long double)RF64(0x151e8))) SF64(0x3d0)'),
        ('afol-first', 'if (!X87_C0C3(first, (long double)SF64(0x400)))', 'if (!X87_C0C3((long double)(double)first, (long double)SF64(0x400)))'),
        ('afol-back', '-sim1_x87(SF64(0x11a0))) * SF64(0x3d0) + SF64(0x1190) + SF64(0x1170));', 'sim1_x87(SF64(0x11a0))) * SF64(0x3d0) + SF64(0x1190) + SF64(0x1170));'),
        ('afol-bound', 'if (X87_C0C3(value, (long double)SF64(0x400))) { S32(0x1160) = S32(0x400);', 'if (X87_C0(value, (long double)SF64(0x400))) { S32(0x1160) = S32(0x400);'),
        ('afol-second', 'if (X87_C0((long double)SF64(0x1178), (long double)SF64(0x408)))', 'if (X87_C0C3((long double)SF64(0x1178), (long double)SF64(0x408)))'),
        ('afol-max', 'if (!X87_C0(value, (long double)SF64(0x408))) { S32(0x1168)', 'if (X87_C0(value, (long double)SF64(0x408))) { S32(0x1168)'),
        ('afol-scale', 'SF64(0x1158) = (double)((long double)SF64(0x1168) * RF64(0x151f0));', 'SF64(0x1158) = (double)((long double)SF64(0x1160) * RF64(0x151f0));'),
        ('roll-steady', '- RF64(0x15190)) * SF64(0x1898))', '- RF64(0x15198)) * SF64(0x1898))'),
        ('roll-rates', '+ (long double)SF64(0x1408) * SF64(0x1878)) * SF64(0x1118);', '- (long double)SF64(0x1408) * SF64(0x1878)) * SF64(0x1118);'),
        ('roll-gate', 'if (!P8(global, 0x609) && !P32(global, 0x278) && P16(global, 0xc44)) {', 'if (!P8(global, 0x609) && P16(global, 0xc44)) {'),
        ('roll-prop', 'SF64(0x12d8) = (double)((long double)SF64(0x12d8) - (long double)(int32_t)(uint32_t)P16(global, 0x898) * torque);', 'SF64(0x12d8) = (double)((long double)SF64(0x12d8) - (long double)(int32_t)(uint32_t)P16(global, 0x8c8) * torque);'),
        ('roll-count', 'if (P16(global, 0xaec) < engine + 2) break;', 'if (P16(global, 0xaec) < engine + 3) break;'),
        ('roll-throttle', '* (long double)(int32_t)PI16(throttle, 0x36) * SF64(0x738));', '* (long double)(int32_t)PI16(throttle, 0x34) * SF64(0x738));'),
        ('roll-band', 'X87_C0((long double)SF64(0x768) * RF64(0x15148), (long double)SF64(0x1490))) {', 'X87_C0C3((long double)SF64(0x768) * RF64(0x15148), (long double)SF64(0x1490))) {'),
        ('roll-stall', 'long double stall = (long double)SF64(0x1138) * SF64(0x14a8) * SF64(0x14a8)', 'long double stall = (long double)SF64(0x1138) * SF64(0x14a8)'),
        ('roll-mass', '(int64_t)((uint32_t)S16(0x38f2) * S32(0x4158))', '(int64_t)((uint32_t)S16(0x38f2) + S32(0x4158))'),
    ],
}


def main():
    root = Path.cwd()
    scenarios = os.environ.get('N', '400')
    suites = sys.argv[1:] or list(BREAKS)
    only = set(filter(None, os.environ.get('ONLY', '').split(',')))
    total = caught = 0
    for suite in suites:
        path = root / 'src/sim1' / suite
        original = path.read_text()
        try:
            for name, before, after in BREAKS[suite]:
                if only and name not in only:
                    continue
                total += 1
                if original.count(before) != 1:
                    print(f'{suite} {name}: text not found once')
                    continue
                path.write_text(original.replace(before, after))
                build = subprocess.run(['make', '-s', '-f', 'tools/sim1.mk', 'sim1rdiff'], cwd=root,
                                       capture_output=True, text=True)
                if build.returncode:
                    print(f'{suite} {name}: does not build')
                    continue
                run = subprocess.run(['wine', 'bin/sim1-diff/sim1rdiff.exe', scenarios], cwd=root,
                                     capture_output=True, text=True, env={**os.environ, 'WINEDEBUG': '-all'})
                if 'DIFFER' in run.stderr:
                    caught += 1
                    print(f'{suite} {name}: caught')
                elif 'SIM1:' in run.stdout:
                    print(f'{suite} {name}: MISSED')
                else:
                    print(f'{suite} {name}: CRASHED ({run.returncode})')
        finally:
            path.write_text(original)
    subprocess.run(['make', '-s', '-f', 'tools/sim1.mk', 'sim1rdiff'], cwd=root)
    print(f'{caught} of {total} caught')


if __name__ == '__main__':
    main()
