/* SIM1.sim, decompiled: small routines -- state resets, the 16-bit table interpolation of
 * the hand-written flight model, integer helpers. Addresses are retail's (base 0x22000000);
 * offsets are .data's (sim1_internal.h). */
#include "sim1_internal.h"
#include "sim1_routines.h"

/* 0x22004b30: the seven parts' (stride 0x2e0, not in address order) identifying bits. */
void sim1_part_bits(void)
{
    S32(0x2014) = 0x01;
    S32(0x23f4) = 0x02;
    S32(0x26d4) = 0x04;
    S32(0x1d14) = 0x08;
    S32(0x1df4) = 0x10;
    S32(0x2594) = 0x20;
    S32(0x2ab4) = 0x40;
}

/* 0x22004b80: the same seven parts' double at +0x...8 (as two words), cleared. */
void sim1_part_clear(void)
{
    static const uint16_t parts[7] = {0x2008, 0x23e8, 0x26c8, 0x1d08, 0x1de8, 0x2588, 0x2aa8};
    uint32_t at;
    for (at = 0; at < 7; ++at) {
        S32(parts[at]) = 0;
        S32(parts[at] + 4) = 0;
    }
}

/* 0x22006e20: two triples of doubles cleared, at +0x1240 and +0x12e0. */
void sim1_clear_pairs(void)
{
    uint32_t at;
    for (at = 0; at < 0x18; at += 4) S32(0x1240 + at) = 0;
    for (at = 0; at < 0x18; at += 4) S32(0x12e0 + at) = 0;
}

/* 0x2200a460: base + fraction * delta in 1.15 fixed point: the high word of twice the
 * signed product of fraction and delta, added to base. Retail returns it in ax and cx and
 * leaves the product's doubled high word in dx (dx_out). */
uint16_t sim1_fixed_step(uint16_t fraction, uint16_t delta, uint16_t base, uint16_t *dx_out)
{
    uint32_t product = (uint32_t)((int32_t)(int16_t)fraction * (int16_t)delta);
    uint16_t high = (uint16_t)((product << 1) >> 16);
    if (dx_out) *dx_out = high;
    return (uint16_t)(base + high);
}

/* 0x2200a470: a 33-entry word table (esi) read at a 16-bit position (ax): the high five
 * bits pick the entry, the next eleven interpolate to the next one. */
uint16_t sim1_table32(const uint8_t *table, uint16_t position)
{
    uint32_t entry = (uint32_t)((uint8_t)(position >> 8) >> 2) & 0x3e;
    uint16_t base = P16(table, entry), delta = (uint16_t)(P16(table, entry + 2) - base);
    return sim1_fixed_step((uint16_t)((position & 0x7ff) << 4), delta, base, 0);
}

/* 0x2200a498: the same over a 17-entry table: four bits pick, twelve interpolate. */
uint16_t sim1_table16(const uint8_t *table, uint16_t position)
{
    uint32_t entry = (uint32_t)((uint8_t)(position >> 8) >> 3) & 0x1e;
    uint16_t base = P16(table, entry), delta = (uint16_t)(P16(table, entry + 2) - base);
    return sim1_fixed_step((uint16_t)((position & 0xfff) << 3), delta, base, 0);
}

/* 0x22007e5f: the square root of an unsigned 32-bit value, rounded to an integer as the x87
 * rounds (the control word's rounding), the low 32 bits of it. */
uint32_t sim1_isqrt(uint32_t value)
{
    return (uint32_t)sim1_fistp64(sim1_fsqrt((long double)(uint64_t)value));
}

/* 0x2200a628: the same over a signed 64-bit value (edx:eax). */
uint32_t sim1_isqrt64(int64_t value)
{
    return (uint32_t)sim1_fistp64(sim1_fsqrt((long double)value));
}

/* 0x220096a2: accumulates 14 * value^2 * scale / 65536 into a sum (ebx), the multiplies
 * wrapping as retail's do: the square's low 32 bits, times scale (unsigned, to 64), times 14
 * (the low 32 bits of that, unsigned, to 64), shifted down 16, low 32. A zero value adds
 * nothing and leaves ecx and edx alone. Returns what it adds; retail leaves ecx 14 and edx
 * the last product's high word (high_out). */
uint32_t sim1_square_term(uint32_t value, uint32_t scale, uint32_t *high_out)
{
    uint64_t product;
    uint32_t low;
    if (!value) return 0;
    low = (uint32_t)((int64_t)(int32_t)value * (int32_t)value);
    product = (uint64_t)low * scale;
    product = (uint64_t)(uint32_t)product * 14u;
    if (high_out) *high_out = (uint32_t)(product >> 32);
    return (uint32_t)(product >> 16);
}

/* 0x2200a3db: +0x1470 = +0x1560 * +0x13b0 + +0x1568 * +0x13a8, on the x87. */
void sim1_cross_term(void)
{
    long double sum = (long double)SF64(0x1560) * SF64(0x13b0);
    sum += (long double)SF64(0x1568) * SF64(0x13a8);
    SF64(0x1470) = (double)sum;
}

/* 0x22001ee0: the brakes' lock-out (GLOBAL +0x366 the parking brake, +0x2b4 the airspeed?):
 * with +0x36c0 set, the flag +0x1010 is set when +0x366 is clear, and cleared (with GLOBAL
 * +0xbca) once the scaled +0x2b4 falls below a constant; while it is set, in mode 2
 * (+0x1624) with the four words +0x88c, +0x924, +0x9bc, +0xa54 not positive, GLOBAL +0xbc4
 * and +0xbc6 become 0x36b0 and +0xbca 1. */
void sim1_hold_check(void)
{
    uint8_t *global = SIM1_GLOBAL;
    if (S32(0x36c0) != 1) return;
    if (!P8(global, 0x366)) S32(0x1010) = 1;
    else {
        long double scaled = (long double)(int64_t)P32(global, 0x2b4) * SF64(0x720) * SF64(0x690);
        if (X87_C0(scaled, RF64(0x15030))) {
            if (S32(0x1010) != 1) return;
            S32(0x1010) = 0;
            P16(global, 0xbca) = 0;
        }
    }
    if (S32(0x1010) != 1 || S32(0x1624) != 2) return;
    global = SIM1_GLOBAL;
    if (PI16(global, 0x88c) > 0 || PI16(global, 0x924) > 0 || PI16(global, 0x9bc) > 0 || PI16(global, 0xa54) > 0) return;
    P16(global, 0xbc4) = 0x36b0;
    P16(SIM1_GLOBAL, 0xbc6) = 0x36b0;
    P16(SIM1_GLOBAL, 0xbca) = 1;
}

/* 0x22001fc0: the control surfaces' previous positions kept (+0x1630.. to +0x1018..), then
 * the new ones from GLOBAL's words, each side of zero scaled by its own throw. */
void sim1_surfaces(void)
{
    uint8_t *global = SIM1_GLOBAL;
    int16_t value;
    SF64(0x1018) = sim1_x87(SF64(0x1630));
    SF64(0x1020) = sim1_x87(SF64(0x1638));
    SF64(0x1028) = sim1_x87(SF64(0x1640));
    SF64(0x1030) = sim1_x87(SF64(0x1648));
    SF64(0x1650) = (double)((long double)PI16(global, 0xbc0) * SF64(0x3b60) * SF64(0x6a0));
    value = PI16(global, 0x11c2);
    if (value > 0) SF64(0x1630) = (double)((long double)SF64(0x3708) * SF64(0x6a0) * value);
    else SF64(0x1630) = (double)((long double)SF64(0x3df8) * SF64(0x6a0) * value);
    value = PI16(global, 0x11c0);
    if (value > 0) {
        long double first = (long double)SF64(0x3de0) * SF64(0x6a0), second = (long double)SF64(0x4228) * SF64(0x6a0);
        SF64(0x1638) = (double)(first * value);
        SF64(0x1640) = (double)(second * PI16(global, 0x11c0));
    } else {
        long double first = (long double)SF64(0x4228) * SF64(0x6a0), second = (long double)SF64(0x3de0) * SF64(0x6a0);
        SF64(0x1638) = (double)(first * value);
        SF64(0x1640) = (double)(second * PI16(global, 0x11c0));
    }
    SF64(0x1648) = (double)((long double)PI16(global, 0x11c4) * SF64(0x35a8) * SF64(0x6a0));
    if (P8(global, 0x610) == 1) {
        SF64(0x1698) = (double)((long double)(int64_t)P32(global, 0xbec) * SF64(0x6a0));
        SF64(0x16a0) = (double)((long double)(int64_t)P32(global, 0xbf0) * SF64(0x6a0));
        SF64(0x16a8) = (double)((long double)(int64_t)P32(global, 0xbf4) * SF64(0x6a0));
    } else {
        uint32_t at;
        for (at = 0x1698; at <= 0x16c0; at += 8) { S32(at) = 0; S32(at + 4) = 0x3ff00000; }
    }
}

/* 0x22002de0: two sums of four vectors and one of three, component by component. */
void sim1_vector_sums(void)
{
    uint32_t at;
    for (at = 0; at < 0x18; at += 8)
        SF64(0x1270 + at) = (double)((((long double)SF64(0x1288 + at) + SF64(0x1258 + at)) + SF64(0x1240 + at)) + SF64(0x1228 + at));
    for (at = 0; at < 0x18; at += 8)
        SF64(0x1310 + at) = (double)(((long double)SF64(0x12f8 + at) + SF64(0x12e0 + at)) + SF64(0x12c8 + at));
}

/* 0x22004940, stdcall: a countdown (record +0) by step, held at its floor (+8) once reached,
 * while the record's bit (+0x14) is in the owner's mask (+0x84); not yet down, the owner's
 * +0x94 is set. */
void __stdcall sim1_countdown(uint8_t *record, uint8_t *owner, double step)
{
    long double value;
    if (!(P32(owner, 0x84) & P32(record, 0x14))) return;
    value = (long double)PF64(record, 0) - step;
    PF64(record, 0) = (double)value;
    if (X87_C0C3(value, PF64(record, 8))) { memcpy(record, record + 8, 8); return; }
    P32(owner, 0x94) = 1;
}

/* 0x22005129, stdcall: a weighted mean over a list of 8-byte records ({kind, value, weight}:
 * kind 0 ends it, -1 is skipped, 2 sets the result outright), the division as the game's
 * (an empty list divides by zero). Returns eax: the quotient's high half, the result's low. */
uint32_t __stdcall sim1_weighted(const uint8_t *list)
{
    int32_t weights = 0, sum = 0;
    uint16_t result = 0x7fff;
    uint32_t quotient, remainder;
    for (;; list += 8) {
        int32_t kind = PI32(list, 0);
        if (!kind) break;
        if (kind == -1) continue;
        if (kind == 2) { result = P16(list, 4); continue; }
        weights += PI16(list, 6);
        sum += (int32_t)((uint32_t)PI16(list, 6) * (uint32_t)PI16(list, 4));
    }
    quotient = sim1_idiv32(sum < 0 ? 0xffffffffu : 0, (uint32_t)sum, (uint32_t)weights, &remainder);
    if (result == 0x7fff) result = (uint16_t)quotient;
    return (quotient & 0xffff0000u) | result;
}

/* 0x22005640: +0x12a8 from the surfaces' terms. */
void sim1_pitch_sum(void)
{
    long double sum = ((long double)SF64(0x1680) + SF64(0x1678)) * SF64(0x17d0);
    sum += ((long double)SF64(0x1660) + SF64(0x1658)) * SF64(0x17e0);
    sum *= SF64(0x768);
    sum -= ((long double)SF64(0x1698) * RF64(0x15120) - ((long double)SF64(0x16a8) + SF64(0x16a0)) * RF64(0x15128)) * SF64(0x17d8);
    sum += (long double)SF64(0x1140) * SF64(0x1760);
    sum += SF64(0x1810);
    sum += SF64(0x17e8);
    sum += SF64(0x17c8);
    SF64(0x12a8) = (double)(sum * SF64(0x15b8));
}

/* 0x220056e0: +0x1228. */
void sim1_side_sum(void)
{
    long double first = ((long double)SF64(0x1408) * SF64(0x18b8) + (long double)SF64(0x1410) * SF64(0x18c0)) * SF64(0x1110);
    long double second = ((long double)SF64(0x1648) * SF64(0x18b0) + (long double)SF64(0x1498) * SF64(0x18c8)) * SF64(0x15b8);
    SF64(0x1228) = (double)(first + second);
}

/* 0x22005740: +0x12b0, the sum of the engines' thrusts (GLOBAL +0x8a8, stride 0x98), as many as
 * GLOBAL +0xaec counts up to four; with none it keeps its value. */
void sim1_thrust_sum(void)
{
    uint8_t *global = SIM1_GLOBAL;
    uint16_t engines = P16(global, 0xaec);
    if (engines > 0) SF64(0x12b0) = (double)(long double)PI32(global, 0x8a8);
    if (engines > 1) SF64(0x12b0) = (double)((long double)PI32(global, 0x940) + SF64(0x12b0));
    if (engines > 2) SF64(0x12b0) = (double)((long double)PI32(global, 0x9d8) + SF64(0x12b0));
    if (engines > 3) SF64(0x12b0) = (double)((long double)PI32(global, 0xa70) + SF64(0x12b0));
}

/* 0x22006100: +0x1230/+0x1238, a vector turned through (+0x14a8, +0x14b0). */
void sim1_turn_pair(void)
{
    SF64(0x1238) = (double)(((long double)SF64(0x12b0) - (long double)SF64(0x14a8) * SF64(0x12a8)) + (long double)SF64(0x14b0) * SF64(0x12a0));
    SF64(0x1230) = (double)((long double)SF64(0x14b0) * SF64(0x12a8) + (long double)SF64(0x14a8) * SF64(0x12a0));
}

/* 0x22006870: GLOBAL +0xb6d follows the flag +0x118c (0 or 1) when its double copy +0x11b0
 * equals the constant for that state; then the copy is refreshed. */
void sim1_flag_follow(void)
{
    int32_t flag = SI32(0x118c);
    if (flag == 1 && X87_C3((long double)SF64(0x11b0), (long double)RF64(0x151e8))) {
        P8(SIM1_GLOBAL, 0xb6d) = 1;
        SF64(0x11b0) = (double)(long double)SI32(0x118c);
        return;
    }
    if (!flag && X87_C3((long double)SF64(0x11b0), (long double)RF64(0x151e0))) P8(SIM1_GLOBAL, 0xb6d) = 0;
    SF64(0x11b0) = (double)(long double)SI32(0x118c);
}

/* 0x22006a50, stdcall: a + b, wrapped once into range by multiples of the period +0x770 at
 * each end, on the x87 (returned in st0, not rounded). */
long double __stdcall sim1_wrap(double a, double b)
{
    long double value = (long double)b + a;
    if (!X87_C0C3(value, (long double)RF64(0x151d0))) value -= (long double)SF64(0x770) * RF64(0x151d0);
    if (X87_C0(value, (long double)RF64(0x151f8))) value -= (long double)SF64(0x770) * RF64(0x151f8);
    return value;
}

/* 0x22006d10, stdcall: a part's two directions (+0x90, +0x98) from its angle word (+0x14)
 * and the heading +0x1548: one through the sine rounded to a double, as retail keeps it. */
void __stdcall sim1_part_axes(uint8_t *part)
{
    long double angle = (long double)PI16(part, 0x14) * SF64(0x680) + SF64(0x1548);
    long double cosine = sim1_fcos(angle), sine = sim1_fsin(angle);
    double kept = (double)sine;
    PF64(part, 0x90) = (double)(sim1_fchs(cosine * PF64(part, 0x88)) - sine * PF64(part, 0x78));
    PF64(part, 0x98) = (double)(cosine * PF64(part, 0x78) - (long double)kept * PF64(part, 0x88));
}

/* 0x22006d80, stdcall: a part's vector (+0x80, +0x90, +0x98) through the matrix +0x11e0. */
void __stdcall sim1_part_rotate(uint8_t *part)
{
    PF64(part, 0x70) = (double)(((long double)SF64(0x11e0) * PF64(part, 0x90) + (long double)SF64(0x11e8) * PF64(part, 0x98)) + (long double)SF64(0x11f0) * PF64(part, 0x80));
    PF64(part, 0x60) = (double)(((long double)SF64(0x1208) * PF64(part, 0x80) + (long double)SF64(0x1200) * PF64(part, 0x98)) + (long double)SF64(0x11f8) * PF64(part, 0x90));
    PF64(part, 0x68) = (double)(((long double)SF64(0x1220) * PF64(part, 0x80) + (long double)SF64(0x1210) * PF64(part, 0x90)) + (long double)SF64(0x1218) * PF64(part, 0x98));
}

/* 0x22006ea0, stdcall: a part's force (+0x60..) added to the total (+0x1240) and its moment
 * (arm +0x30.. cross force) to +0x12e0. */
void __stdcall sim1_part_add(const uint8_t *part)
{
    SF64(0x1240) = (double)((long double)SF64(0x1240) + PF64(part, 0x60));
    SF64(0x1248) = (double)((long double)SF64(0x1248) + PF64(part, 0x68));
    SF64(0x1250) = (double)((long double)SF64(0x1250) + PF64(part, 0x70));
    SF64(0x12e0) = (double)(((long double)PF64(part, 0x38) * PF64(part, 0x70) + SF64(0x12e0)) - (long double)PF64(part, 0x40) * PF64(part, 0x68));
    SF64(0x12e8) = (double)(((long double)PF64(part, 0x30) * PF64(part, 0x68) + SF64(0x12e8)) - (long double)PF64(part, 0x38) * PF64(part, 0x60));
    SF64(0x12f0) = (double)(((long double)SF64(0x12f0) - (long double)PF64(part, 0x30) * PF64(part, 0x70)) + (long double)PF64(part, 0x40) * PF64(part, 0x60));
}

/* 0x22009dfa: a signed 32-bit value scaled by the 48-bit factor at +0x7e6 (shifted when the
 * word +0x7ea is set) and by 0x1a3f7 / +0x16f8, in 16.16: returns edx:eax, edx's high half
 * kept from the product as retail's sign fix-up leaves it. Retail leaves ebx the input and ecx
 * the quotient. */
uint64_t sim1_scale48(uint32_t value)
{
    uint32_t magnitude = (int32_t)value < 0 ? 0u - value : value, factor = sim1_u32(0x7e6);
    uint16_t shift = S16(0x7ea), remainder;
    uint32_t quotient, low, high, product_high;
    uint64_t product;
    if (shift) factor = (factor >> 4) | ((uint32_t)shift << 28);
    product = (uint64_t)magnitude * factor;
    quotient = sim1_div16(1, 0xa3f7, S16(0x16f8), &remainder);
    product_high = (uint32_t)((uint64_t)(uint32_t)(product >> 32) * quotient);
    product = (uint64_t)(uint32_t)product * quotient;
    high = (uint32_t)(product >> 32) + product_high;
    low = (uint32_t)product;
    {   /* shrd eax, edx, 16; sar edx, 16; adc edx, 0 (the carry from sar: bit 15 of edx) */
        uint32_t carry = (high >> 15) & 1;
        low = (low >> 16) | (high << 16);
        high = (uint32_t)((int32_t)high >> 16) + carry;
    }
    if (shift) { high = (high << 4) | (low >> 28); low <<= 4; }
    if ((int32_t)value < 0) {
        uint16_t dx = (uint16_t)~high;
        uint32_t borrow = low != 0;
        low = 0u - low;
        dx = (uint16_t)(dx - 0xffff - borrow);
        high = (high & 0xffff0000u) | dx;
    }
    return (uint64_t)high << 32 | low;
}

/* 0x22009e62: a part (esi) whose +0xc fell below +8 sets its bit (+0x812) in GLOBAL +0x120a
 * and records its +0x24 at +0x869, unless GLOBAL +0x368 is set. */
void sim1_mark_part(uint8_t *global, const uint8_t *part)
{
    uint32_t count;
    if (P32(global, 0x368)) return;
    if (PI32(part, 0xc) >= PI32(part, 8)) return;
    count = sim1_u32(0x812) & 0x1f;
    P16(global, 0x120a) |= (uint16_t)(count < 16 ? 1u << count : 0);
    sim1_set_u32(0x869, P32(part, 0x24));
}

/* 0x2200a41b: a 48-bit value (edx:ax) times a signed word (cx) on the x87, through the scratch
 * qword at +0x859 as retail does it, returned shifted: eax the product's bits 15..46, edx its
 * high dword shifted left one with bit 31 of the low. */
uint64_t sim1_mul48(uint16_t low, uint32_t high, uint16_t factor)
{
    int64_t value, product;
    uint32_t eax, edx;
    S16(0x861) = factor;
    S16(0x859) = low;
    sim1_set_u32(0x85b, high);
    S16(0x85f) = (int32_t)high < 0 ? 0xffff : 0;
    memcpy(&value, sim1_data + 0x859, 8);
    product = sim1_fistp64((long double)value * (int16_t)factor);
    memcpy(sim1_data + 0x859, &product, 8);
    eax = (uint32_t)product;
    edx = (uint32_t)((uint64_t)product >> 32);
    edx = (edx << 1) | (eax >> 31);
    eax = (uint32_t)((int32_t)eax >> 15);
    return (uint64_t)edx << 32 | eax;
}

/* 0x220024d5 */
void sim1_set_7c0(void) { S32(0x7c0) = 1; }

/* 0x2200fb1d: a 16-bit value clamped to +-0x3fff. */
uint16_t sim1_clamp16(uint16_t value)
{
    if ((int16_t)value >= 0x3fff) return 0x3fff;
    if ((int16_t)value <= -0x3fff) return 0xc001;
    return value;
}

/* 0x2200fb33: the same over 32 bits. */
uint32_t sim1_clamp32(uint32_t value)
{
    if ((int32_t)value >= 0x3fff) return 0x3fff;
    if ((int32_t)value <= -0x3fff) return 0xffffc001u;
    return value;
}

/* 0x2200db61: the word GLOBAL +0x384 points at, or 0 without one (callers test it). */
uint32_t sim1_global_384(void)
{
    const uint8_t *pointer = (const uint8_t *)(uintptr_t)P32(SIM1_GLOBAL, 0x384);
    return pointer ? P32(pointer, 0) : 0;
}

/* 0x2200f9d8 */
void sim1_set_limits(uint8_t *global)
{
    P16(global, 0x890) = 0x3fff;
    P16(global, 0x88e) = 0x3fff;
    P16(global, 0x892) = 3;
}

/* 0x2200fb4d: of a (st0) and b (st1), what retail keeps: a unless a > b; then -a, or b when
 * -a is below it. */
long double sim1_fold(long double a, long double b)
{
    long double negated;
    if (!(a > b)) return a;
    negated = -a;
    if (X87_C0(negated, b)) return b;
    return negated;
}

/* 0x220051dc, stdcall: the rudder's input (+0x308 list's value) and its weighted mean into
 * GLOBAL +0x11c4. RETAIL BUG (kept): SIM1-SCALE-DISCARDED (docs/retail-bugs.md): with
 * GLOBAL +0x278 bit 0 set and +0x366 clear, retail scales the value by 0x1999 / 65536, then
 * pops the unscaled word back over it, so the scaling never happens; it is left out. */
void __stdcall sim1_rudder_input(uint32_t value)
{
    S16(0x314) = (uint16_t)value;
    P16(SIM1_GLOBAL, 0x11c4) = (uint16_t)sim1_weighted(sim1_data + 0x308);
}

/* 0x2200518c, stdcall: the elevator's (+0x338 list) into GLOBAL +0x11c2. */
void __stdcall sim1_elevator_input(uint32_t value)
{
    S16(0x344) = (uint16_t)value;
    P16(SIM1_GLOBAL, 0x11c2) = (uint16_t)sim1_weighted(sim1_data + 0x338);
}

/* 0x220051b4, stdcall: the ailerons' (+0x2d8 list) into GLOBAL +0x11c0. */
void __stdcall sim1_aileron_input(uint32_t value)
{
    S16(0x2e4) = (uint16_t)value;
    P16(SIM1_GLOBAL, 0x11c0) = (uint16_t)sim1_weighted(sim1_data + 0x2d8);
}

/* 0x22005229: the elevator's control word (GLOBAL +0xbb2) through the response table at
 * +0x368, plus the trim (+0xbc0) unless +0x3b68 bit 0 is set. */
void sim1_elevator_control(uint8_t *global)
{
    uint16_t value = sim1_table16(sim1_data + 0x368, P16(global, 0xbb2));
    if (!(S32(0x3b68) & 1)) value = (uint16_t)(value + P16(global, 0xbc0));
    sim1_elevator_input(value);
}

/* 0x22005259: the ailerons' (GLOBAL +0xbb6). */
void sim1_aileron_control(uint8_t *global)
{
    sim1_aileron_input(sim1_table16(sim1_data + 0x368, P16(global, 0xbb6)));
}

/* 0x22005276: the rudder's (GLOBAL +0xbba). */
void sim1_rudder_control(uint8_t *global)
{
    sim1_rudder_input(sim1_table16(sim1_data + 0x368, P16(global, 0xbba)));
}

/* 0x2200a3fc: in mode 3 (+0x1628), +0x13f0 is zero. */
void sim1_mode3_zero(void)
{
    if (S32(0x1628) == 3) SF64(0x13f0) = 0.0;
}

/* 0x2200d8a0 */
void sim1_ratio_968(void)
{
    SF64(0x968) = (double)(((long double)SF64(0x1780) - SF64(0x1788)) / ((long double)SF64(0x1790) - SF64(0x1798)));
}

/* 0x2200a786, stdcall: the table (esi) at +0x1460, kept at +0x1718, scaled by +0x6b0 (st0). */
long double __stdcall sim1_table_scaled(const uint8_t *table)
{
    S16(0x1718) = sim1_table16(table, S16(0x1460));
    return (long double)SI16(0x1718) * SF64(0x6b0);
}

/* 0x2200a5ee: UTIL's atan2 of (y, x), as 0x4000 less it (a bearing from a direction). */
uint16_t sim1_bearing(int16_t y, int16_t x)
{
    uint32_t angle = SIM1_CALL(SIM1_UTIL, 0x3c, uint32_t (__stdcall *)(int32_t, int32_t))(y, x);
    return (uint16_t)(0x4000 - (uint16_t)angle);
}

/* 0x2200d750: +0x2b68 from the integer at +0x242c, 90 when that comes out above the limit
 * (the comparison on the unrounded value). */
void sim1_angle_2b68(void)
{
    long double value = fabsl((long double)SI32(0x242c)) * RF64(0x15288);
    value = (long double)RF64(0x15290) - value;
    SF64(0x2b68) = (double)value;
    if (!X87_C0C3(value, (long double)RF64(0x15298))) { S32(0x2b68) = 0; S32(0x2b6c) = 0x40568000; }
}

/* 0x2200da32 */
void sim1_check_874(uint8_t *global)
{
    if (sim1_u32(0xaef) || !P32(global, 0x84c)) return;
    sim1_set_u16(0xab9, P16(global, 0xbc0));
    P32(global, 0x874) = 0;
    if (SI32(0x162c) < 3) return;
    if (!(fabsl((long double)SF64(0x1470)) > (long double)sim1_f64(0x976))) P32(global, 0x874) = 1;
}

/* 0x2200fb69: st0: +0x1780 on the ground (GLOBAL +0x854), else from the speed's band. */
long double sim1_band_value(const uint8_t *global)
{
    long double speed;
    if (P32(global, 0x854)) return SF64(0x1780);
    speed = (long double)SF64(0x1388) * SF64(0x748);
    if (!X87_C0(speed, (long double)SF64(0x1790))) return SF64(0x1780);
    if (!(speed > SF64(0x1798))) return SF64(0x1788);
    return (speed - SF64(0x1798)) * SF64(0x968) + SF64(0x1788);
}

/* 0x2200da83: over the limit at GLOBAL +0xc1a (0x100 on the ground, 0x400 in the air with
 * +0x7bc set), UTIL's alert 0xf480..0xf483 and the value cut to the limit. */
void sim1_overlimit_alert(uint8_t *global)
{
    uint16_t limit = 0x100;
    if (!P32(global, 0x854)) {
        limit = 0x400;
        if (P32(global, 0x7bc) != 1) return;
    }
    if (P16(global, 0xc1a) <= limit) return;
    SIM1_CALL(SIM1_UTIL, 0xa4, uint32_t (__stdcall *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))
        (2, 2, 0xf480, 0xf481, 0xf482, 0xf483, 0, 0);
    P16(global, 0xc1a) = limit;
}

/* 0x2200e6d6: +0x9e6 from +0xb43 while either of two holds lasts, else from the heading word. */
void sim1_heading_9e6(uint8_t *global)
{
    uint32_t degrees;
    if (sim1_u32(0xad3)) { if (!sim1_u32(0xad7)) sim1_set_f64(0x9e6, sim1_x87(sim1_f64(0xb43))); return; }
    if (sim1_u32(0xadb)) { if (!sim1_u32(0xadf)) sim1_set_f64(0x9e6, sim1_x87(sim1_f64(0xb43))); return; }
    degrees = (uint32_t)(uint16_t)(P16(global, 0x870) + 0x7fff) * 0x168u;
    P32(global, 0x7f8) = degrees;
    sim1_set_f64(0x9e6, (double)((long double)PI32(global, 0x7f8) * SF64(0x690)));
}

/* 0x22008ad0 */
void sim1_reset_886(uint8_t *global)
{
    static const uint16_t pairs[5][2] = {{0xb78, 0xb74}, {0xb80, 0xb7c}, {0xb98, 0xb94}, {0xb88, 0xb84}, {0xba0, 0xb9c}};
    uint32_t at;
    P16(global, 0x886) = 0x5000;
    for (at = 0; at < 5; ++at) if (P32(global, pairs[at][0])) P32(global, pairs[at][1]) = 0x7fffff;
}

/* 0x2200faac: on the ground, pitched or banked past 0x2aab (the high words of +0x578 and
 * +0x57c, magnitudes, compared unsigned), GLOBAL +0x878 becomes 0x5a and +0x9e2 and +0x9e4
 * clear; level, a non-negative +0x872 and +0x574 within 30 of the ground (+0x20) clear +0x9e4. */
void sim1_ground_attitude(uint8_t *global)
{
    uint32_t value;
    if (!P32(global, 0x854)) return;
    value = P32(global, 0x578);
    if ((int32_t)value < 0) value = 0u - value;
    if ((uint32_t)((int32_t)value >> 16) > 0x2aab) goto tipped;
    value = P32(global, 0x57c);
    if ((int32_t)value < 0) value = 0u - value;
    if ((uint32_t)((int32_t)value >> 16) > 0x2aab) goto tipped;
    if (PI16(global, 0x872) < 0) return;
    if ((int32_t)(P32(global, 0x574) - (P32(global, 0x20) >> 8)) > 0x1e) return;
    S16(0x9e4) = 0;
    return;
tipped:
    P16(global, 0x878) = 0x5a;
    S16(0x9e2) = 0;
    S16(0x9e4) = 0;
}

/* 0x2200aca9: the height above the ground in 32.8 (GLOBAL +0x574:+0x573 less +0x20),
 * capped at 0x7feb.ff, into +0x1d18 and scaled into +0x1fc8; then, when +0x270 is set, GLOBAL
 * +0x366 says whether the uncapped height is below +0x120e:+0x120d. Returns edx:eax as retail
 * leaves them. */
uint64_t sim1_height(uint8_t *global)
{
    uint32_t high = P32(global, 0x574), ground = P32(global, 0x20), eax;
    uint16_t bx = P16(global, 0x572);
    uint8_t fraction = (uint8_t)(bx >> 8), ground_fraction = (uint8_t)ground;
    uint32_t borrow = fraction < ground_fraction;
    bx = (uint16_t)((bx & 0xff) | (uint16_t)(uint8_t)(fraction - ground_fraction) << 8);
    high = high - (uint32_t)((int32_t)ground >> 8) - borrow;
    eax = high;
    if ((int32_t)eax > 0x7feb) { eax = 0x7feb; bx |= 0xff00; }
    S16(0x1d18) = bx;
    S16(0x1d1a) = (uint16_t)eax;
    SF64(0x1fc8) = (double)((long double)SI32(0x1d18) * SF64(0x690) * SF64(0x710));
    if (P32(global, 0x270)) {     /* in retail's order: a wild pointer faults at the same place */
        uint16_t word;
        P8(global, 0x366) = 0;
        __asm__ volatile("" : : : "memory");     /* stored before the reads, as retail does */
        eax = (uint32_t)(int32_t)PI16(global, 0x120e);
        word = P16(global, 0x120c);
        high = high - eax - (bx < word);
        if ((int32_t)high < 0) P8(global, 0x366) = 1;
    }
    return (uint64_t)high << 32 | eax;
}

/* 0x2200fbc5: a three-term filter of x (st0) over its record (esi): x times the gain, plus
 * the integral terms below their limits (each kept, or cleared past its limit), plus the rate
 * term within its limit; the record keeps x as the last input. Returns st0. */
long double sim1_filter(uint8_t *record, long double x)
{
    long double y = x * PF64(record, 0), term;
    if (X87_C0(fabsl(x), (long double)PF64(record, 0x20))) {
        term = x * SF64(0x16f0) * PF64(record, 8) + PF64(record, 0x38);
        PF64(record, 0x38) = (double)term;
        y += term;
    } else PF64(record, 0x38) = 0.0;
    if (X87_C0(fabsl(x), (long double)PF64(record, 0x28))) {
        term = x * SF64(0x16f0) * PF64(record, 0x10) + PF64(record, 0x40);
        PF64(record, 0x40) = (double)term;
        y += term;
    } else PF64(record, 0x40) = 0.0;
    if (!(fabsl(x) > PF64(record, 0x30))) y += (x - PF64(record, 0x48)) * PF64(record, 0x18) / SF64(0x16f0);
    PF64(record, 0x48) = (double)x;
    return y;
}

/* 0x2200df6f: while +0x996 is set, the heading error (+0xa16 less +0xa14) through the filter
 * at +0x2660 into +0x9e4, with the aileron list's entry set to a fixed 0x333. */
void sim1_heading_hold(void)
{
    if (!S16(0x996)) return;
    S32(0x2d8 + 0x10) = 1;
    S16(0x2d8 + 0xe) = 0x333;
    SI32(0x920) = (int16_t)(S16(0xa16) - S16(0xa14));
    SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x2660, (long double)SI32(0x920) * SF64(0x930)));
    S16(0x9e4) = (uint16_t)S32(0x950);
}

/* 0x2200ad20, stdcall: a contact's three integer offsets (+0x24..) scaled to doubles (+0x30..);
 * the value given kept at +0; returns, and keeps at +4, whether any offset is non-zero. */
uint32_t __stdcall sim1_contact_offsets(uint8_t *contact, uint32_t value)
{
    int32_t a = PI32(contact, 0x24), b = PI32(contact, 0x28), c;
    P32(contact, 0) = value;
    c = PI32(contact, 0x2c);
    PF64(contact, 0x30) = (double)((long double)a * SF64(0x738));
    PF64(contact, 0x38) = (double)((long double)b * SF64(0x738));
    PF64(contact, 0x40) = (double)((long double)c * SF64(0x738));
    P32(contact, 4) = (a | b | c) != 0;
    return (a | b | c) != 0;
}

/* 0x22010bb0, stdcall: a part on (+4 1, +8 1, +0xc 0) whose load (its +0x20 by 200 less
 * GLOBAL +0xc44, scaled) is below its limit (+0x50) sets the damage word +0x832 to 4, if no
 * other is set. */
void __stdcall sim1_part_limit(const uint8_t *part)
{
    uint8_t *global;
    long double load;
    if (P32(part, 4) != 1 || P32(part, 0xc) || P32(part, 8) != 1) return;
    global = SIM1_GLOBAL;
    load = (long double)(int32_t)(0xc8 - (uint32_t)P16(global, 0xc44)) * (long double)(int64_t)P32(part, 0x20);
    load = load * SF64(0x768) * SF64(0x710) * SF64(0x6d0) * SF64(0x690);
    if (X87_C0(load, (long double)PF64(part, 0x50)) && !P16(global, 0x832)) P16(global, 0x832) = 4;
}

/* 0x220089e4: a 9-by-n word table (esi) read bilinearly at the positions +0x527 and +0x529:
 * three bits of each pick the cell, thirteen interpolate, 1.15 fixed point. Returns ax. */
uint16_t sim1_table2d(const uint8_t *table)
{
    uint16_t across = sim1_u16(0x527), down = sim1_u16(0x529);
    uint8_t high = (uint8_t)((across >> 8) & 0xe0);
    uint32_t cell = (uint8_t)((uint8_t)(high >> 4) + (uint8_t)(high >> 1) + (uint8_t)((uint8_t)((down >> 8) & 0xe0) >> 4));
    uint16_t fraction = (uint16_t)((down & 0x1fff) << 2);
    uint16_t top = sim1_fixed_step(fraction, (uint16_t)(P16(table, cell + 2) - P16(table, cell)), P16(table, cell), 0);
    uint16_t bottom = sim1_fixed_step(fraction, (uint16_t)(P16(table, cell + 0x14) - P16(table, cell + 0x12)), P16(table, cell + 0x12), 0);
    return sim1_fixed_step((uint16_t)((across & 0x1fff) << 2), (uint16_t)(bottom - top), top, 0);
}

/* 0x22008b40, stdcall: one of two holds (+0x656, +0x65a) set to a value; while either is
 * on, CONTROLS +0x5c(4, 1) and the elevator trim (+0xbb4 of the caller's GLOBAL, edi) nudged by the alternating
 * +0x654; else CONTROLS +0x5c(4, 0). */
void __stdcall sim1_trim_hold(uint8_t *global, uint32_t value, uint32_t which)
{
    if (!which) sim1_set_u32(0x656, value);
    if (which == 1) sim1_set_u32(0x65a, value);
    if (sim1_u32(0x656) | sim1_u32(0x65a)) {
        uint16_t step;
        SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, uint32_t))(4, 1);
        step = (uint16_t)-S16(0x654);
        S16(0x654) = step;
        P16(global, 0xbb4) = (uint16_t)(P16(global, 0xbb4) + step);
    } else SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, uint32_t))(4, 0);
}

/* 0x2200ab78: the gear state GLOBAL +0x838 settled (6, or 5 without +0x846 and +0x82c), and
 * ACONTAIN told the model state (1, or 2) when it differs from what +0x3c says. */
void sim1_gear_model(uint8_t *global)
{
    uint32_t state = 1, current;
    if (P8(global, 0x838) != 1) {
        if ((int8_t)P8(global, 0x838) < 7) return;
        P8(global, 0x838) = 6;
        if (!P16(global, 0x846) || !P32(global, 0x82c)) { P8(global, 0x838) = 5; return; }
        state = 2;
    }
    current = SIM1_CALL(SIM1_ACONTAIN, 0x3c, uint32_t (__stdcall *)(void *))(global + 0x11e0);
    if (state != current) SIM1_CALL(SIM1_ACONTAIN, 0x34, uint32_t (__stdcall *)(void *, uint32_t))(global + 0x11e0, state);
}

/* 0x220063b0: GLOBAL +0x11ba from +0x1278 over the scaled +0x1728 (truncated, as _ftol), and
 * SOUND +0x24(4, 0) when 1.5 times it is outside the band for the flaps' state (+0xbe0). */
void sim1_stall_warning(void)
{
    long double value = (long double)(int64_t)S32(0x1728) * SF64(0x788);
    int32_t level;
    int16_t half;
    uint32_t flaps;
    value = (long double)SF64(0x1278) / value;
    value = (value - RF64(0x151c0)) * RF64(0x151c8);
    P16(SIM1_GLOBAL, 0x11ba) = (uint16_t)sim1_ftol(value);
    level = PI16(SIM1_GLOBAL, 0x11ba) * 3;
    half = (int16_t)(level / 2);
    flaps = P32(SIM1_GLOBAL, 0xbe0);
    if ((!flaps && half > SI16(0x2b60)) || half < SI16(0x363c)) {
        SIM1_CALL(SIM1_SOUND, 0x24, uint32_t (__stdcall *)(uint32_t, uint32_t))(4, 0);
        return;
    }
    if ((flaps && half > SI16(0x3588)) || half < SI16(0x35f0))
        SIM1_CALL(SIM1_SOUND, 0x24, uint32_t (__stdcall *)(uint32_t, uint32_t))(4, 0);
}

/* 0x2200d7f0: the autopilot's pitch step (GLOBAL +0x334): -25 while the mode word +0x892 is 4,
 * then a schedule counting +0x910 down from 13 with waits (+0x918) of (18 - n)^2 passes,
 * 1 at its end, 0 below a speed of 0x190; all times 0x51e. Returns eax as retail leaves it. */
uint32_t sim1_pitch_schedule(void)
{
    uint8_t *global = SIM1_GLOBAL;
    int32_t mode = PI16(global, 0x892);
    uint32_t value;
    if (mode == 4) { S32(0x91c) = 4; value = 0xffffffe7u; }
    else if (S32(0x91c) == 4) { S32(0x91c) = (uint32_t)mode; S32(0x910) = 0xd; value = 0xd; S32(0x918) = 0xf; }
    else {
        value = S16(0x16d0);
        if ((int32_t)value <= 0x190) value = 0;
        else if (S32(0x910) == 1) value = 1;
        else if (S32(0x918)) { --S32(0x918); return value; }
        else {
            --S32(0x910);
            S32(0x918) = (0x12 - S32(0x910)) * (0x12 - S32(0x910));
            value = S32(0x910);
        }
    }
    value = (uint32_t)((int32_t)value * 0x51e);
    P32(global, 0x334) = value;
    return value;
}

/* 0x22008a77: a contact's (ebx) +0x54, in retail's order: v + sqrt(v^2 + k / (pi r^2 c1 c2))
 * less +0x1388, with v = +0x1388 * +0x768, k its +0x1c (0 when not positive), r = +0x4024 *
 * +0x738, c1 = +0x798, c2 = +0x768. fldpi is pi rounded to 64 bits, as the literal is. */
void sim1_contact_rise(uint8_t *contact)
{
    long double speed = (long double)SF64(0x1388) * SF64(0x768), stiffness = (long double)PI32(contact, 0x1c);
    long double radius = (long double)SI32(0x4024) * SF64(0x738), divisor;
    if (!X87_C0(0.0L, stiffness)) stiffness = 0.0L;
    divisor = 3.14159265358979323846264338327950288L * (radius * radius) * SF64(0x798) * SF64(0x768);
    PF64(contact, 0x54) = (double)(speed + sim1_fsqrt(speed * speed + stiffness / divisor) - SF64(0x1388));
}

/* 0x22011810: +0x7b0 = c0 - c1 * v^2 (v = +0x1458) and +0x7b8 = that to the power c2. */
void sim1_power_7b8(void)
{
    long double base = (long double)RF64(0x15300) - (long double)SF64(0x1458) * SF64(0x1458) * RF64(0x152f8);
    SF64(0x7b0) = (double)base;
    SF64(0x7b8) = (double)sim1_cipow(base, RF64(0x15308));
}

/* 0x2200a599, stdcall: UTIL's atan2 of two doubles scaled to integers (fistp, as the control
 * word rounds), as 0x4000 less it. Returns ax (callers use only the low words of eax and edx,
 * both the result). */
uint16_t __stdcall sim1_bearing_of(double y, double x)
{
    int32_t iy = sim1_fistp32((long double)y * SF64(0x6d8)), ix = sim1_fistp32((long double)x * SF64(0x6d8));
    uint32_t angle = SIM1_CALL(SIM1_UTIL, 0x3c, uint32_t (__stdcall *)(int32_t, int32_t))(iy, ix);
    return (uint16_t)(0x4000 - (uint16_t)angle);
}

/* 0x2200dfc8: the heading hold's bank term (+0x9e4): unless +0x996 is set, on the ground or with
 * either flag +0xa28/+0x9ae, the heading change since last time through the filter at +0x2860,
 * folded against the speed's band, scaled; then +0xa0c keeps the heading for next time. */
void sim1_turn_hold(uint8_t *global)
{
    if (!S16(0x996) && (P32(global, 0x854) || S8(0xa28) || S8(0x9ae))) {
        long double value;
        sim1_set_u32(0xa35, (uint32_t)(int32_t)(int16_t)((uint16_t)(S32(0x1558) >> 16) - S16(0xa0c)));
        value = sim1_filter(sim1_data + 0x2860, (long double)(int32_t)sim1_u32(0xa35) * SF64(0x930));
        value = sim1_fold(sim1_band_value(global), value);
        SI32(0x950) = sim1_fistp32(value * SF64(0x928));
        S16(0x9e4) = (uint16_t)S32(0x950);
    }
    S16(0xa0c) = (uint16_t)(P16(global, 0x7cc) + P16(global, 0x2a0));
}

/* 0x22010c40: the situation record's block (GLOBAL +0x560, 0x24 bytes) copied to +0x1730, and
 * four words from the velocities, truncated. Returns the last word's _ftol (eax). */
uint32_t sim1_record_words(void)
{
    int32_t value;
    SIM1_CALL(SIM1_HOST, 0x21c, uint32_t (__stdcall *)(void *, const void *, uint32_t))(sim1_data + 0x1730, SIM1_GLOBAL + 0x560, 0x24);
    S16(0x1754) = (uint16_t)sim1_ftol((long double)SF64(0x13b8) * SF64(0x728));
    S16(0x1756) = (uint16_t)sim1_ftol((long double)SF64(0x13c0) * SF64(0x728));
    S16(0x1758) = (uint16_t)sim1_ftol((long double)SF64(0x13c8) * SF64(0x728));
    value = sim1_ftol((long double)SF64(0x13b0) * SF64(0x728));
    S16(0x175a) = (uint16_t)value;
    return (uint32_t)value;
}

/* 0x2200deea: while +0x996 is set, the heading error through the filter at +0x1ec0 into the
 * rudder list's fixed entry (clamped), and the rudder input redone from GLOBAL +0x11c4. */
void sim1_rudder_hold(uint8_t *global)
{
    uint16_t error;
    if (!S16(0x996)) return;
    S32(0x308 + 0x10) = 1;
    S16(0x308 + 0xe) = 0x333;
    error = (uint16_t)((uint16_t)(S32(0x1558) >> 16) + S16(0x9b0) - P16(global, 0x870) + 0x8000);
    SI32(0x920) = (int16_t)(uint16_t)-error;
    SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x1ec0, (long double)SI32(0x920) * SF64(0x930)));
    S16(0x308 + 0x14) = sim1_clamp16((uint16_t)S32(0x950));
    sim1_rudder_input(P16(global, 0x11c4));
}

/* 0x22009d48: the crash counter: in mode 2 or with +0x1618, +0x2224 resets the slide; five
 * counts of GLOBAL +0x348 end the crash, and if not too fast (+0x13b0 not above +0x7dc)
 * REALITY +0x18 repairs and the limits reset. Otherwise a 20-pass timer clears GLOBAL +0xbc8. */
void sim1_crash_count(uint8_t *global)
{
    if (S32(0x1624) == 2 || S32(0x1618)) {
        if (S32(0x2224)) { S32(0x1554) = 0; SF64(0x1540) = 0.0; }
        if (P32(global, 0x348) < 5) return;
        P16(global, 0x344) = 0;
        P32(global, 0x348) = 0;
        if ((long double)SF64(0x13b0) > (long double)sim1_f64(0x7dc)) return;
        SIM1_CALL(SIM1_REALITY, 0x18, void (*)(void))();
        sim1_reset_886(global);
        return;
    }
    if (!P16(global, 0xbc8)) return;
    S16(0x7d6) = (uint16_t)(S16(0x7d6) + 1);
    if (SI16(0x7d6) < 0x14) return;
    P16(global, 0xbc8) = 0;
    S16(0x7d6) = 0;
}

/* 0x22009e92: a part's (esi) damping force (+0x80, not below zero): its spring term from
 * +0x10, +0x18 and the scaled +0x1724 (in 16.16, wrapping), plus +0x1c times its scaled +0x50,
 * as a 64-bit sum at +0x1718. */
void sim1_part_damping(uint8_t *part)
{
    uint32_t base = (uint32_t)(((uint64_t)0x55f0 * S32(0x1724)) >> 16) + 0x6180, factor;
    int64_t first, second;
    uint64_t sum;
    long double value;
    if ((int32_t)(base + P32(part, 0x10)) >= 0) factor = P32(part, 0x10);
    else factor = 0u - base;
    first = (int64_t)(int32_t)factor * (int32_t)(0u - P32(part, 0x18));
    SI32(0x1718) = sim1_fistp32((long double)PF64(part, 0x50) * SF64(0x728) * SF64(0x6d8));
    second = (int64_t)PI32(part, 0x1c) * SI32(0x1718);
    sum = (uint64_t)first + (uint64_t)second;
    memcpy(sim1_data + 0x1718, &sum, 8);
    {
        int64_t loaded;
        memcpy(&loaded, sim1_data + 0x1718, 8);
        value = (long double)loaded * SF64(0x690);
    }
    if (!X87_C0(0.0L, value)) value = 0.0L;
    PF64(part, 0x80) = (double)value;
}

/* 0x2200f9f4: on the ground in mode 3 or more, nearly stopped (+0x1470 scaled within
 * +-0x200 in 16.16): the trim back to +0xab9, CONTROLS +0x2c(0), the ailerons and rudder
 * centred. */
void sim1_ground_stop(uint8_t *global)
{
    uint32_t value;
    uint16_t low;
    int16_t high;
    if (!P32(global, 0x854) || SI32(0x162c) < 3) return;
    SI32(0x1718) = sim1_fistp32((long double)SF64(0x1470) * SF64(0x728) * SF64(0x6d8));
    value = S32(0x1718);
    low = (uint16_t)value;
    high = (int16_t)((int32_t)value >> 16);
    if (high > 0) return;
    if (!high) { if (low > 0x200) return; }
    else if (low < 0xfe00) return;
    P16(global, 0xbc0) = sim1_u16(0xab9);
    P16(global, 0xbc2) = sim1_u16(0xab9);
    SIM1_CALL(SIM1_CONTROLS, 0x2c, uint32_t (__stdcall *)(uint32_t))(0);
    S16(0x2d8 + 0x14) = 0;
    P16(global, 0xbb6) = 0;
    S16(0x308 + 0x14) = 0;
    P16(global, 0xbba) = 0;
}

/* 0x2200a4c0: the attitude's sines and cosines (pitch +0x1550, bank +0x1554, heading +0x1558),
 * as doubles and as fixed-point words. */
void sim1_attitude_trig(void)
{
    static const uint16_t angles[3] = {0x1550, 0x1554, 0x1558};
    uint32_t at;
    for (at = 0; at < 3; ++at) {
        long double angle = (long double)SI32(angles[at]) * SF64(0x680) * SF64(0x690);
        SF64(0x1568 + at * 0x10) = (double)sim1_fsin(angle);
        SF64(0x1560 + at * 0x10) = (double)sim1_fcos(angle);
    }
    for (at = 0; at < 3; ++at) {
        S16(0x1590 + at * 8) = (uint16_t)sim1_fistp16((long double)SF64(0x1568 + at * 0x10) * SF64(0x6e0));
        S16(0x1594 + at * 8) = (uint16_t)sim1_fistp16((long double)SF64(0x1560 + at * 0x10) * SF64(0x6e0));
    }
}

/* 0x2200f6ab: the climb rate (ax, a word): its change over +0xa6b in 1.15 by +0x16fc into an
 * eight-slot history at +0xa4b, their mean at +0xa6f; then the turn's lift factor from the
 * sine of the heading error (UTIL sincos) and GLOBAL +0x2bc into +0xa93 (its magnitude). */
void sim1_climb_history(uint8_t *global, uint16_t rate)
{
    int32_t value = (int16_t)rate, sum = 0, slot;
    int64_t shifted = (int64_t)(int32_t)((uint32_t)value - sim1_u32(0xa6b)) * 0x8000;
    uint32_t remainder, inserted;
    inserted = sim1_idiv32((uint32_t)((uint64_t)shifted >> 32), (uint32_t)shifted, sim1_u32(0x16fc), &remainder);
    sim1_set_u32(0xa6b, (uint32_t)value);
    for (slot = 0; slot < 8; ++slot) {
        uint32_t old = sim1_u32(0xa4b + slot * 4);
        sum += (int32_t)inserted;
        sim1_set_u32(0xa4b + slot * 4, inserted);
        inserted = old;
    }
    sim1_set_u32(0xa6f, (uint32_t)(sum >> 3));
    SIM1_CALL(SIM1_UTIL, 0x2c, void (__stdcall *)(int32_t, void *, void *))
        ((uint16_t)(S16(0x9a6) - (uint16_t)(S32(0x1558) >> 16)), sim1_data + 0xa9b, sim1_data + 0xa9d);
    {
        long double factor = (long double)(int32_t)sim1_u32(0xa6f) * ((long double)(int16_t)sim1_u16(0xa9b) * SF64(0x698));
        int32_t scaled, product;
        factor = factor * ((long double)PI32(global, 0x2bc) * SF64(0x6c8));
        SI32(0x958) = scaled = sim1_fistp32(factor * SF64(0x960));
        product = (int32_t)(uint32_t)(((uint64_t)(int64_t)scaled * 0x1d4c0) >> 16);
        if (product < 0) product = (int32_t)(0u - (uint32_t)product);
        sim1_set_u16(0xa93, (uint16_t)product);
    }
}

/* 0x2200ddf6: the yaw damper into the rudder list's fixed entry (+0x308): off (a fixed 0x2000
 * weight, no entry) in the heading hold, with +0x366, or on the ground and still; on, the
 * yaw rate (+0x1720, a quarter with GLOBAL +0x36d) through the filter at +0x20e0, the change
 * scaled by 0x7530 into the entry, each step saturating, then the rudder input redone. */
void sim1_yaw_damper(uint8_t *global)
{
    int32_t rate;
    int16_t delta, entry;
    int32_t wide;
    if (!S16(0x996)) {
        S16(0x308 + 0xe) = 0x2000;
        S32(0x308 + 0x10) = 0xffffffffu;
        if (!P8(global, 0x366) && (P32(global, 0x854) || P32(global, 0x808) || P32(global, 0x278))) goto on;
    }
    S16(0x308 + 0xe) = 0x2000;
    S32(0x308 + 0x10) = 0xffffffffu;
    S16(0x308 + 0x14) = 0;
    return;
on:
    S32(0x308 + 0x10) = 1;
    S16(0x308 + 0xe) = 0x333;
    rate = SI32(0x1720);
    if (P8(global, 0x36d)) rate >>= 2;
    SI32(0x920) = rate;
    SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x20e0, (long double)rate));
    wide = (int32_t)(int16_t)(uint16_t)sim1_clamp32((uint32_t)SI32(0x950)) - SI16(0x308 + 0x14);
    delta = (int16_t)wide;
    if (wide != delta) delta = delta < 0 ? 0x7fff : (int16_t)0x8001;
    wide = (int32_t)(((int32_t)delta * 0x7530) >> 16) + SI16(0x308 + 0x14);
    entry = (int16_t)wide;
    if (wide != entry) entry = entry < 0 ? 0x7fff : (int16_t)0x8001;
    S16(0x308 + 0x14) = (uint16_t)entry;
    sim1_rudder_input(P16(global, 0x11c4));
}

/* 0x22006160: the gear's force (+0x1258..) and its moment (+0x12f8..) from the compression. */
void sim1_gear_force(void)
{
    SF64(0x10b0) = (double)((long double)SF64(0x10c0) - SF64(0x10b8));
    SF64(0x10d0) = (double)((long double)SF64(0x10c8) * SF64(0x10b0));
    SF64(0x1258) = (double)((long double)SF64(0x1108) * SF64(0x10f0) * SF64(0x10d0));
    SF64(0x1260) = (double)((long double)SF64(0x10f8) * SF64(0x10d0));
    SF64(0x1268) = (double)((long double)SF64(0x1100) * SF64(0x10f0) * SF64(0x10d0));
    SF64(0x12f8) = (double)((long double)SF64(0x10e0) * SF64(0x1268) - (long double)SF64(0x10e8) * SF64(0x1260));
    SF64(0x1300) = (double)((long double)SF64(0x10d8) * SF64(0x1260) - (long double)SF64(0x10e0) * SF64(0x1258));
    SF64(0x1308) = (double)((long double)SF64(0x10e8) * SF64(0x1258) - (long double)SF64(0x10d8) * SF64(0x1268));
}

/* 0x2200955d: three drag sums of square terms (0x220096a2) into +0x15d8, +0x15e0 and +0x15e8,
 * and +0x15f0 from the integer +0x35b8. */
void sim1_drag_sums(void)
{
    uint32_t sum, high, ailerons = S32(0x1604) + S32(0x1608), elevators = S32(0x15fc) + S32(0x1600);
    sum = S32(0x35b0);
    sum += sim1_square_term(S32(0x3850), S32(0x15f8), &high);
    sum += sim1_square_term(S32(0x2b90), ailerons, &high);
    sum += sim1_square_term(S32(0x2c50), elevators, &high);
    SI32(0x1718) = (int32_t)sum;
    SF64(0x15d8) = (double)(long double)SI32(0x1718);
    sum = S32(0x35bc);
    sum += sim1_square_term(S32(0x3858), S32(0x15f8), &high);
    sum += sim1_square_term(S32(0x2b98), ailerons, &high);
    sum += sim1_square_term(S32(0x2c58), elevators, &high);
    SI32(0x1718) = (int32_t)sum;
    SF64(0x15e0) = (double)(long double)SI32(0x1718);
    sum = S32(0x35e4);
    sum += sim1_square_term(S32(0x3858), S32(0x15f8), &high);
    sum += sim1_square_term(S32(0x2b98), ailerons, &high);
    sum += sim1_square_term(S32(0x2c58), elevators, &high);
    sum += sim1_square_term(S32(0x3850), S32(0x15f8), &high);
    sum += sim1_square_term(S32(0x2b90), ailerons, &high);
    sum += sim1_square_term(S32(0x2c50), elevators, &high);
    SI32(0x1718) = (int32_t)sum;
    SF64(0x15e8) = (double)(long double)SI32(0x1718);
    SF64(0x15f0) = (double)(long double)SI32(0x35b8);
}

/* 0x2200db7c: the distance to the station (UTIL longitude to metres of the longitude
 * difference, and the latitude difference, both 48-bit; in metres, squared, summed, rooted)
 * into +0x9f4; the heading word +0x9ee; +0x9b0 from the word GLOBAL +0x384 leads to, if any. */
void sim1_station_distance(uint8_t *global)
{
    uint8_t local[12];      /* retail's frame: the longitude difference at +4, the result at +0 */
    uint16_t low = P16(global, 0x862);
    uint32_t high, borrow;
    int64_t east, north;
    borrow = low < P16(global, 0x56a);
    low = (uint16_t)(low - P16(global, 0x56a));
    high = P32(global, 0x864) - P32(global, 0x56c) - borrow;
    memset(local, 0, sizeof local);
    memcpy(local + 4 + 2, &low, 2);
    memcpy(local + 4 + 4, &high, 4);
    SIM1_CALL(SIM1_UTIL, 0x48, void (__stdcall *)(void *, void *, void *))(local + 4, global + 0x560, local);
    memcpy(&high, local + 4, 4);
    sim1_set_u32(0x982, high);
    borrow = P16(global, 0x85a) < P16(global, 0x562);
    high = P32(global, 0x85c) - P32(global, 0x564) - borrow;
    sim1_set_u32(0x97e, high);
    {
        int64_t product = (int64_t)(int32_t)high * 0x348;
        int32_t scaled = (int32_t)(uint32_t)((uint64_t)product >> 8);
        north = (int64_t)scaled * scaled;
        memcpy(sim1_data + 0x986, &north, 8);
        product = (int64_t)(int32_t)sim1_u32(0x982) * 0x348;
        scaled = (int32_t)(uint32_t)((uint64_t)product >> 8);
        east = (int64_t)scaled * scaled;
        memcpy(sim1_data + 0x98e, &east, 8);
    }
    sim1_set_u32(0x9f4, sim1_isqrt64((int64_t)((uint64_t)east + (uint64_t)north)));
    sim1_set_u16(0x9ee, (uint16_t)(0u - (uint32_t)(SI32(0x1550) >> 16) - (uint32_t)(int32_t)SI16(0x1518)));
    {
        uint32_t pointer = P32(SIM1_GLOBAL, 0x384), word = 0;
        if (pointer && P32((uint8_t *)(uintptr_t)pointer, 0)) word = P16((uint8_t *)(uintptr_t)P32((uint8_t *)(uintptr_t)pointer, 0), 0x1a);
        S16(0x9b0) = (uint16_t)word;
    }
}

/* 0x22011620: the attitude's trigonometry, then the track (UTIL atan2 of the ground velocity
 * +0x1388/+0x1398) against the heading +0x3900 into +0x1518. */
void sim1_track(void)
{
    sim1_attitude_trig();
    S16(0x1518) = (uint16_t)(S16(0x3900) - sim1_bearing_of(SF64(0x1388), SF64(0x1398)));
}

/* 0x2200948e: the air data: the density and speed of sound from GLOBAL +0xe8c and +0xe8e
 * (temperature, pressure) into +0x16e0.., the true airspeed into +0x1468 and GLOBAL +0x11c8,
 * the Mach-like ratio into +0x798 and its capped word +0x1704; then +0x7b8. */
void sim1_air_data(uint8_t *global)
{
    long double value = (long double)PI16(global, 0xe8c) * SF64(0x6c0) * sim1_f64(0x5a4) + sim1_f64(0x5ac), ratio;
    SF64(0x16e0) = (double)value;
    value += sim1_f64(0x5b4);
    SF64(0x16e8) = (double)value;
    value = sim1_fsqrt(value * sim1_f64(0x5ec) * sim1_f64(0x59c));
    SF64(0x1468) = (double)value;
    PF64(global, 0x11c8) = (double)value;
    SI32(0x63c) = sim1_fistp32(value * SF64(0x728) * SF64(0x6d8));
    ratio = (long double)PI16(global, 0xe8e) * sim1_f64(0x594);
    SF64(0x16d8) = (double)ratio;
    ratio /= (long double)SF64(0x16e8) * sim1_f64(0x59c);
    SF64(0x798) = (double)ratio;
    SI32(0x1700) = sim1_fistp32(ratio * SF64(0x6d8) * sim1_f64(0x5e4));
    S16(0x1704) = S32(0x1700) > 0xffff ? 0xffff : (uint16_t)S32(0x1700);
    SF64(0x7a0) = (double)((long double)SF64(0x16e8) * sim1_f64(0x5c4));
    SF64(0x7a8) = (double)((long double)SF64(0x16d8) * sim1_f64(0x5d4));
    sim1_power_7b8();
}

/* 0x22010920: +0x18d0, where the angle word +0x3802 falls between the limits +0x14c8 and
 * +0x14d0 (0 below both, the fraction toward the higher, capped at 1). */
void sim1_limit_fraction(void)
{
    long double low = SF64(0x14c8), angle = (long double)(int32_t)SI16(0x3802) * SF64(0x680), high = SF64(0x14d0), ratio;
    if (X87_C0(low, angle) && X87_C0(high, angle)) { S32(0x18d0) = 0; S32(0x18d4) = 0; return; }
    if (!X87_C0C3(high, (long double)SF64(0x14c8))) ratio = (high - angle) / (high - SF64(0x14c8));
    else if (!X87_C0C3(low, (long double)SF64(0x14d0))) ratio = (low - angle) / (low - SF64(0x14d0));
    else ratio = RF64(0x152d8);
    SF64(0x18d0) = (double)ratio;
    if (!X87_C0C3(ratio, (long double)RF64(0x152d8))) { S32(0x18d0) = 0; S32(0x18d4) = 0x3ff00000; }
}

/* 0x2200e050: the wing leveller and heading hold into the ailerons' fixed entry (+0x2d8): on
 * the ground or with any of four holds, the bank error (capped at +-0x1554) and the heading
 * term through two filters (+0x2520, +0x25a0), clamped, then the aileron input redone;
 * otherwise +0x9e4 follows the bank within +0xa04/+0xa06. Returns whether it held (retail then
 * leaves GLOBAL in edi, from the aileron input). */
int sim1_wing_leveller(uint8_t *global)
{
    uint16_t bank;
    if (P32(global, 0x854) || S8(0xa24) || S8(0xa28) || S8(0xa2a) || S8(0x9ae)) {
        int32_t first;
        S32(0x2d8 + 0x10) = 1;
        S16(0x2d8 + 0xe) = 0x333;
        if (S8(0xa2a)) S16(0x9e4) = 0;
        bank = (uint16_t)((uint16_t)(S32(0x1554) >> 16) - S16(0x9e4));
        if ((int16_t)bank >= 0x1554) bank = 0x1554;
        else if ((int16_t)bank <= (int16_t)0xeaac) bank = 0xeaac;
        sim1_set_u16(0xa43, bank);
        SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x2520, (long double)(int16_t)sim1_u16(0xa43) * SF64(0x930)));
        first = (int32_t)sim1_clamp32(S32(0x950));
        SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x25a0, (long double)SF64(0x13f0) * SF64(0x688)));
        S16(0x2d8 + 0x14) = (uint16_t)sim1_clamp32((uint32_t)first + S32(0x950));
        sim1_aileron_input(P16(global, 0x11c0));
        return 1;
    }
    bank = (uint16_t)(S32(0x1554) >> 16);
    if ((int16_t)bank >= (int16_t)sim1_u16(0xa04)) bank = sim1_u16(0xa04);
    else if ((int16_t)bank <= (int16_t)sim1_u16(0xa06)) bank = sim1_u16(0xa06);
    S16(0x9e4) = bank;
    sim1_set_u16(0xab5, 0);
    return 0;
}

/* 0x2200f387: the autothrottle's flare: near the ground (+0x1d18 not above +0xafb) the heading
 * hold on and the throttles (GLOBAL +0x88c, stride 0x98, GLOBAL +0xaec of them) eased toward
 * idle or +0xb00 by the fraction +0xb17 * +0x16f0 each pass, as the speed (+0x2bc) falls. */
void sim1_flare_throttle(uint8_t *global)
{
    static const uint16_t throttles[4] = {0x88c, 0x924, 0x9bc, 0xa54};
    uint32_t at;
    if (!S32(0x4040) || !sim1_u32(0xaff)) return;
    if (SI32(0x1d18) > (int32_t)sim1_u32(0xafb)) return;
    S16(0x996) = 1;
    S16(0x9e2) = 0xeaac;
    if (S32(0x1624) >= 2) {
        sim1_set_u32(0xb03, 1);
        if (P32(global, 0x2bc) <= sim1_u32(0xb13)) {
            sim1_set_u32(0xb07, 1);
            sim1_set_u32(0xaff, 0);
            for (at = 0; at < 4; ++at) P16(global, throttles[at]) = 0;
        }
    }
    if (sim1_u32(0xb03)) {
        if (P32(global, 0x2bc) <= sim1_u32(0xb0f)) sim1_set_u32(0xb27, 0);
        else sim1_set_u32(0xb27, (uint32_t)(int32_t)PI16(global, 0xb00));
    } else {
        if (P32(global, 0x2bc) < sim1_u32(0xb0b)) return;
        sim1_set_u32(0xb27, 0);
    }
    sim1_set_f64(0xb1f, (double)((long double)sim1_f64(0xb17) * SF64(0x16f0)));
    for (at = 0; at < 4; ++at) {
        if (at && P16(global, 0xaec) < at + 1) return;
        P16(global, throttles[at]) = (uint16_t)sim1_fistp16(((long double)(int32_t)sim1_u32(0xb27) - PI16(global, throttles[at]))
                                                             * sim1_f64(0xb1f) + PI16(global, throttles[at]));
    }
}

/* 0x2200ada0, stdcall: a contact's fields copied (not +0x10..0x13, +0x1a, +0x36..0x3b). */
void __stdcall sim1_copy_contact(uint8_t *to, const uint8_t *from)
{
    static const uint8_t words[] = {0, 2, 4, 6, 8, 0xa, 0xc, 0xe, 0x14, 0x16, 0x18};
    static const uint8_t more[] = {0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x30, 0x32, 0x34, 0x3c};
    uint32_t at;
    for (at = 0; at < sizeof words; ++at) P16(to, words[at]) = P16(from, words[at]);
    P32(to, 0x1c) = P32(from, 0x1c);
    for (at = 0; at < sizeof more; ++at) P16(to, more[at]) = P16(from, more[at]);
    for (at = 0x40; at <= 0x50; at += 4) P32(to, at) = P32(from, at);
}

/* 0x22006f30: the body-to-earth matrix +0x11e0 (nine doubles) from the attitude's sines and
 * cosines, through four rounded products as retail keeps them on its stack. */
void sim1_attitude_matrix(void)
{
    double a = (double)((long double)SF64(0x1580) * SF64(0x1578)), b = (double)((long double)SF64(0x1588) * SF64(0x1570));
    double c = (double)((long double)SF64(0x1588) * SF64(0x1578)), d = (double)((long double)SF64(0x1580) * SF64(0x1570));
    SF64(0x11e0) = (double)((long double)SF64(0x1580) * SF64(0x1560));
    SF64(0x11e8) = (double)((long double)SF64(0x1588) * SF64(0x1560));
    SF64(0x11f0) = (double)-(long double)SF64(0x1568);
    SF64(0x11f8) = (double)((long double)SF64(0x1568) * a - b);
    SF64(0x1200) = (double)((long double)SF64(0x1568) * c + d);
    SF64(0x1208) = (double)((long double)SF64(0x1578) * SF64(0x1560));
    SF64(0x1210) = (double)((long double)SF64(0x1568) * d + c);
    SF64(0x1218) = (double)((long double)SF64(0x1568) * b - a);
    SF64(0x1220) = (double)((long double)SF64(0x1570) * SF64(0x1560));
}

/* 0x22006aa0, stdcall: a part's relative wind (+0x48..+0x58): in flight (+0x7c0) the body
 * velocity plus the rotation's part at its arm (+0x30, +0x40); otherwise the earth velocity. */
void __stdcall sim1_part_wind(uint8_t *part)
{
    long double kept;
    if (S32(0x7c0) != 1) {
        PF64(part, 0x48) = sim1_x87(SF64(0x13b8));
        PF64(part, 0x50) = (double)-(long double)SF64(0x13c0);
        memcpy(part + 0x58, sim1_data + 0x13c8, 8);
        return;
    }
    PF64(part, 0x58) = (double)(((long double)SF64(0x1560) * SF64(0x13b0) + (long double)SF64(0x1568) * SF64(0x13a8))
                                - (long double)SF64(0x13f8) * PF64(part, 0x30));
    PF64(part, 0x48) = (double)((long double)SF64(0x13f8) * PF64(part, 0x40)
                                + ((long double)SF64(0x1570) * SF64(0x13a0) - (long double)SF64(0x1578) * SF64(0x13a8)));
    kept = (long double)SF64(0x1568) * SF64(0x13b0)
           - ((long double)SF64(0x1570) * SF64(0x1560) * SF64(0x13a8) + (long double)SF64(0x1578) * SF64(0x13a0));
    {
        double rounded = (double)kept;
        PF64(part, 0x50) = (double)(((long double)SF64(0x13e8) * PF64(part, 0x40) + rounded) - (long double)SF64(0x13f0) * PF64(part, 0x30));
    }
}

/* 0x2200dc7e: the autopilot annunciators (+0xa20..+0xa2a) from GLOBAL's switches and the
 * navigation receiver's flags (the station GLOBAL +0x384 leads to); the glideslope's capture
 * also arms the flare (+0xaff). */
void sim1_annunciators(uint8_t *global)
{
    static const uint16_t flags[9] = {0xa20, 0xa22, 0xa24, 0xa23, 0xa25, 0xa26, 0xa28, 0xa29, 0xa2a};
    uint32_t at;
    for (at = 0; at < 9; ++at) S8(flags[at]) = 0;
    if (P32(global, 0x854) || !P32(global, 0x7bc)) return;
    if (!P8(global, 0xb71) && P32(global, 0x7d0)) S8(0xa22) = 1;
    if (!P8(global, 0xb70)) {
        uint8_t *station = (uint8_t *)(uintptr_t)sim1_global_384();
        if (!station || !P16(station, 0x18)) {
            P32(global, 0x800) = 0; P32(global, 0x804) = 0; P32(global, 0x7fc) = 0; P32(global, 0x7c4) = 0;
        } else {
            if (P8(station, 0x1c) & 0x80) {
                if (P32(global, 0x800)) S8(0xa23) = 1;
                if (!(P8(station, 0x1c) & 0x20) && P32(global, 0x804)) { S8(0xa25) = 1; S8(0xa23) = 1; }
            }
            if (P8(station, 0x1c) & 0x40) {
                uint32_t glideslope = P32(global, 0x7fc);
                if (glideslope) {
                    S8(0xa26) = 1;
                    if (!S8(0xa27)) {
                        sim1_set_u16(0xa2b, 1);
                        if (S32(0x4040)) sim1_set_u32(0xaff, 1);
                    }
                }
                S8(0xa27) = (uint8_t)glideslope;
            }
            if (!(P8(global, 0xc4a) & 1) && P32(global, 0x7c4)) S8(0xa29) = 1;
        }
    }
    if (!P8(global, 0xb72) && P32(global, 0x7c0)) S8(0xa2a) = 1;
    if (!P8(global, 0xb6d) && P32(global, 0x7c8)) S8(0xa28) = 1;
    if (!P8(global, 0xb67) && P32(global, 0x7d8)) S8(0xa24) = 1;
}

/* 0x22009f11: a part's (esi) slip angle (+0x16) from its relative wind (+0x48, +0x58) scaled
 * down to 15 bits, the forward part kept at least 100 either way, and its sine and cosine
 * (UTIL sincos into +0x80c, +0x80a). */
void sim1_part_slip(uint8_t *part)
{
    int32_t forward, side;
    uint32_t largest, other, remainder, divisor;
    int16_t y, x;
    uint16_t angle;
    forward = SI32(0x1718) = sim1_fistp32((long double)PF64(part, 0x58) * SF64(0x728) * SF64(0x6d8));   /* parked at +0x1718, as retail */
    largest = forward < 0 ? 0u - (uint32_t)forward : (uint32_t)forward;
    side = SI32(0x1718) = sim1_fistp32((long double)PF64(part, 0x48) * SF64(0x728) * SF64(0x6d8));
    other = side < 0 ? 0u - (uint32_t)side : (uint32_t)side;
    if (!((int32_t)largest > (int32_t)other)) largest = other;
    divisor = (largest >> 15) + 1;
    forward = SI32(0x1718) = sim1_fistp32((long double)PF64(part, 0x58) * SF64(0x728) * SF64(0x6d8));
    y = (int16_t)sim1_idiv32(forward < 0 ? 0xffffffffu : 0, (uint32_t)forward, divisor, &remainder);
    side = SI32(0x1718) = sim1_fistp32((long double)PF64(part, 0x48) * SF64(0x728) * SF64(0x6d8));
    x = (int16_t)sim1_idiv32(side < 0 ? 0xffffffffu : 0, (uint32_t)side, divisor, &remainder);
    if (y >= 0) { if (y <= 0x64) y = 0x64; }
    else if (y >= -0x64) y = -0x64;
    angle = (uint16_t)(P16(part, 0x14) - sim1_bearing(y, x));
    P16(part, 0x16) = angle;
    SIM1_CALL(SIM1_UTIL, 0x2c, void (__stdcall *)(int32_t, void *, void *))(angle, sim1_data + 0x80c, sim1_data + 0x80a);
}

/* 0x2200a66c: the yaw damper's rate (+0x1720, within +-0x7f) from the sideslip and turn, and
 * its filtered value into GLOBAL +0x36e (an eighth with +0x278), the filter's gain switched
 * by whether the rate grows. */
void sim1_yaw_rate(uint8_t *global)
{
    long double value = (long double)SF64(0x1328) * sim1_f64(0x8a6), filtered;
    int32_t rate;
    value -= ((long double)SF64(0x788) * SF64(0x1578) / ((1.0L + SF64(0x1450)) * SF64(0x1570)) + SF64(0x13f8)) * sim1_f64(0x89e);
    rate = (SI32(0x1718) = sim1_fistp32(value * SF64(0x6f8))) >> 1;     /* parked at +0x1718 */
    if (rate >= 0x7f) rate = 0x7f;
    if (rate <= -0x7f) rate = -0x7f;
    SI32(0x1720) = rate;
    if (rate >= 0) sim1_set_f64(0x8ca, sim1_x87(sim1_f64(rate < (int32_t)sim1_u32(0x8b6) ? 0x8ba : 0x8c2)));
    else sim1_set_f64(0x8ca, sim1_x87(sim1_f64(rate < (int32_t)sim1_u32(0x8b6) ? 0x8c2 : 0x8ba)));
    sim1_set_u32(0x8b6, (uint32_t)rate);
    filtered = (value - sim1_f64(0x8ae)) * sim1_f64(0x8ca) * SF64(0x16f0) + sim1_f64(0x8ae);
    sim1_set_f64(0x8ae, (double)filtered);
    rate = (SI32(0x1718) = sim1_fistp32(filtered * SF64(0x6f8))) >> 1;
    if (rate >= 0x7f) rate = 0x7f;
    if (rate <= -0x7f) rate = -0x7f;
    P8(global, 0x36e) = P32(global, 0x278) ? (uint8_t)((int8_t)rate >> 3) : (uint8_t)rate;
}

/* 0x2200a263: a part's (esi) side force (+0x78): its cornering from the slip angle's table
 * (+0x817, unless GLOBAL +0x60c is 2) times its load (+0x80), 48-bit, limited by the force
 * that stops the side velocity (+0x48) when it has no own (+0x14). */
void sim1_part_side_force(uint8_t *global, uint8_t *part)
{
    uint16_t factor = 0x7fff, a_low, a_mid, a_high, b_low, b_mid, b_high;
    int64_t load;
    uint64_t product, limit;
    uint32_t edx;
    if (P32(global, 0x60c) != 2) {
        uint16_t slip = P16(part, 0x16);
        if ((int16_t)slip < 0) slip = (uint16_t)-slip;
        factor = sim1_table32(sim1_data + 0x817, (uint16_t)(slip << 1));
    }
    if ((int16_t)P16(part, 0x16) < 0) factor = (uint16_t)-factor;
    S16(0x7f8) = factor;
    load = sim1_fistp64((long double)PF64(part, 0x80) * SF64(0x6d8));
    memcpy(sim1_data + 0x1718, &load, 8);
    product = sim1_mul48(S16(0x1718), sim1_u32(0x171a), factor);
    sim1_set_u16(0x7ec, (uint16_t)product);
    sim1_set_u32(0x7ee, (uint32_t)(product >> 32));
    limit = sim1_scale48(0u - (uint32_t)sim1_fistp32((long double)PF64(part, 0x48) * SF64(0x728) * SF64(0x6d8)));
    sim1_set_u32(0x7f2, (uint32_t)limit);
    sim1_set_u16(0x7f6, (uint16_t)(limit >> 32));
    a_low = sim1_u16(0x7ec); a_mid = sim1_u16(0x7ee); a_high = sim1_u16(0x7f0);
    b_low = sim1_u16(0x7f2); b_mid = sim1_u16(0x7f4); b_high = sim1_u16(0x7f6);
    if (!P16(part, 0x14)) {
        uint32_t borrow = a_low < b_low, borrow2;
        uint16_t d_mid = (uint16_t)(a_mid - b_mid - borrow), d_high;
        borrow2 = (uint32_t)a_mid < (uint32_t)b_mid + borrow;
        d_high = (uint16_t)(a_high - b_high - borrow2);
        if ((int16_t)a_high >= 0) {
            if ((int16_t)b_high >= 0 && (int16_t)d_high >= 0) { a_low = b_low; a_mid = b_mid; a_high = b_high; }
        } else {
            if ((int16_t)b_high < 0 && !((int16_t)d_high > 0)) { a_low = b_low; a_mid = b_mid; a_high = b_high; }
        }
        (void)d_mid;
    }
    edx = (uint32_t)a_high << 16 | a_mid;
    S16(0x1718) = a_low;
    sim1_set_u32(0x171a, edx);
    S16(0x171e) = (int32_t)edx < 0 ? 0xffff : 0;
    memcpy(&load, sim1_data + 0x1718, 8);
    PF64(part, 0x78) = (double)((long double)load * SF64(0x690));
}

/* 0x22006250: the icing (?) term +0x1760: below the temperature threshold (GLOBAL +0xe8c) it
 * grows with the power (+0xc44), faster with GLOBAL +0xf10/+0xf0e; otherwise it decays; it
 * stays within 0 and 4; +0x1610 from it and +0x30e4. */
void sim1_icing(void)
{
    uint8_t *global = SIM1_GLOBAL;
    long double temperature = (long double)(int32_t)PI16(global, 0xe8c) * SF64(0x6c0);
    int decay = 1;
    if (X87_C0C3(temperature, (long double)RF64(0x15168))) {
        long double power = (long double)(int32_t)(uint32_t)P16(global, 0xc44);
        if (!X87_C0C3(power * RF64(0x15138), (long double)RF64(0x15168))) {
            if (P16(global, 0xf10) && P16(global, 0xf0e) == 1)
                SF64(0x1760) = (double)((long double)SF64(0x1760) - (long double)SF64(0x16f0) * power * RF64(0x15198));
            else SF64(0x1760) = (double)((long double)SF64(0x1760) - (long double)SF64(0x16f0) * RF64(0x151a0));
            decay = 0;
        }
    }
    if (decay) {
        if (!X87_C0C3((long double)SF64(0x1760), (long double)RF64(0x15168)))
            SF64(0x1760) = (double)((long double)SF64(0x1760) - (long double)SF64(0x16f0) * RF64(0x151a8));
        else { S32(0x1760) = 0; S32(0x1764) = 0; }
    }
    if (X87_C0((long double)SF64(0x1760), (long double)RF64(0x15168))) { S32(0x1760) = 0; S32(0x1764) = 0; }
    if (!X87_C0C3((long double)SF64(0x1760), (long double)RF64(0x151b0))) { S32(0x1760) = 0; S32(0x1764) = 0x40100000; }
    SF64(0x1610) = (double)((long double)(int64_t)S32(0x30e4) * SF64(0x1760) * RF64(0x151b8));
}

/* 0x22011e30: the turn rate +0x1528 from the ground velocity and its change, and the climb
 * term +0x1530, each 0 when its divisor is not above the floor. Retail leaves ecx the word
 * +0x151c sign-extended (its saved-ecx slot is the local it loads that through). */
void sim1_turn_climb_rates(void)
{
    long double speed2 = (long double)SF64(0x1388) * SF64(0x1388) + (long double)SF64(0x1398) * SF64(0x1398);
    long double total, cosine;
    if (!X87_C0C3(speed2, (long double)RF64(0x152e8)))
        SF64(0x1528) = (double)(((long double)SF64(0x1398) * SF64(0x1338) - (long double)SF64(0x1388) * SF64(0x1330)) / speed2);
    else { S32(0x1528) = 0; S32(0x152c) = 0; }
    total = ((long double)SF64(0x1390) * SF64(0x1328) + (long double)SF64(0x1388) * SF64(0x1338)) + (long double)SF64(0x1398) * SF64(0x1330);
    cosine = sim1_fcos((long double)(int32_t)SI16(0x151c) * SF64(0x680)) * SF64(0x1450) * SF64(0x1450);
    total /= SF64(0x1450);
    if (!X87_C0C3(cosine, (long double)RF64(0x152e8)))
        SF64(0x1530) = (double)((total * SF64(0x1390) - (long double)SF64(0x1450) * SF64(0x1328)) / cosine);
    else { S32(0x1530) = 0; S32(0x1534) = 0; }
}

/* 0x2200e173: the altitude and pitch hold into the trim (GLOBAL +0xbc0/+0xbc2): engaged, the
 * pitch error (capped at +-0x400) and the vertical speed through two filters (+0x2380,
 * +0x2980), with the bank's lift term, each step clamped, then CONTROLS +0x2c(0); otherwise
 * +0x9e2 follows the pitch within +0x1fe0/+0x1f10. */
void sim1_pitch_hold(uint8_t *global)
{
    uint16_t pitch, trim;
    uint32_t bank, sum;
    if (!P32(global, 0x854) && !S16(0x996) && !S8(0xa22) && !S8(0xa24) && (!P32(global, 0x7fc) || !(S8(0xa26) ^ S8(0xa2b)))) {
        pitch = (uint16_t)(S32(0x1550) >> 16);
        if ((int16_t)pitch >= SI16(0x1fe0)) pitch = S16(0x1fe0);
        else if ((int16_t)pitch <= SI16(0x1f10)) pitch = S16(0x1f10);
        S16(0x9e2) = pitch;
        sim1_set_u16(0xab3, 0);
        return;
    }
    pitch = (uint16_t)((uint16_t)(S32(0x1550) >> 16) - S16(0x9e2));
    if ((int16_t)pitch >= 0x400) pitch = 0x400;
    else if ((int16_t)pitch <= -0x400) pitch = 0xfc00;
    sim1_set_u32(0x9be, (uint32_t)(int32_t)(int16_t)pitch);
    SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x2380, (long double)(int32_t)sim1_u32(0x9be) * SF64(0x930)));
    trim = (uint16_t)(S16(0x950) + P16(global, 0xbc0));
    bank = (uint32_t)(SI32(0x1554) >> 16);
    if ((int32_t)bank < 0) bank = 0u - bank;
    sum = (uint32_t)(((uint64_t)bank * S32(0x1dd0)) >> 16) + (uint32_t)(int32_t)(int16_t)trim;
    P16(global, 0xbc0) = sim1_clamp16((uint16_t)sum);
    SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x2980, (long double)SF64(0x13e8) * SF64(0x688)));
    trim = sim1_clamp16((uint16_t)(S16(0x950) + P16(global, 0xbc0)));
    P16(global, 0xbc0) = trim;
    P16(global, 0xbc2) = trim;
    SIM1_CALL(SIM1_CONTROLS, 0x2c, uint32_t (__stdcall *)(uint32_t))(0);
}

/* 0x2200ec31: the autothrottle (GLOBAL +0x878, the target speed) into all four throttles
 * through the filter at +0x24c0, its integrators cleared at either stop; and the two sounds
 * SOUND +0x24 plays when the flap (+0x87a) and gear (+0x87c) positions change on the ground. */
void sim1_autothrottle(uint8_t *global)
{
    if (P16(global, 0x878) != 0xffff && P32(global, 0x854) && !S16(0x996) && S32(0x1624) != 2) {
        int32_t throttle;
        uint8_t *record = sim1_data + 0x24c0;
        sim1_set_u32(0xa9f, (uint32_t)P16(global, 0x878) - (P32(global, 0x2bc) >> 7));
        SI32(0x950) = sim1_fistp32(sim1_filter(record, (long double)(int32_t)sim1_u32(0xa9f)));
        throttle = (int32_t)PI16(global, 0x88c) + SI32(0x950);
        if (throttle >= 0x3fff) { throttle = 0x3fff; PF64(record, 0x38) = 0.0; PF64(record, 0x40) = 0.0; }
        else if (throttle <= 0) { throttle = 0; PF64(record, 0x38) = 0.0; PF64(record, 0x40) = 0.0; }
        P16(global, 0x88c) = (uint16_t)throttle;
        P16(global, 0x924) = P16(global, 0x88c);
        P16(global, 0x9bc) = P16(global, 0x88c);
        P16(global, 0xa54) = P16(global, 0x88c);
    }
    if (P32(global, 0x854) && P16(global, 0x87a) != 0xffff && (uint32_t)P16(global, 0x87a) != P32(global, 0xbdc)) {
        P32(global, 0xbdc) = P16(global, 0x87a);
        SIM1_CALL(SIM1_SOUND, 0x24, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x16, 0);
    }
    if (P32(global, 0x854) && P32(global, 0x60c) == 1 && (uint32_t)(int32_t)PI16(global, 0x87c) != P32(global, 0xbe8) && !P8(global, 0x366)) {
        P32(global, 0xbe8) = (uint32_t)(int32_t)PI16(global, 0x87c);
        SIM1_CALL(SIM1_SOUND, 0x24, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x15, 0);
    }
}

/* 0x22011cf0: GLOBAL +0x2cc, the heading term (wrapped by +-R370): two products of the sine
 * of the latitude (GLOBAL +0x564:+0x562, rounded as retail keeps it), each clamped to
 * [R368, R360], plus the heading difference (GLOBAL +0x580 less +0x2a0). */
void sim1_heading_term(void)
{
    uint8_t *global = SIM1_GLOBAL;
    long double latitude = (long double)(int32_t)(uint32_t)P16(global, 0x562) * SF64(0x690) + (long double)PI32(global, 0x564);
    long double product = (long double)SF64(0x1580) * SF64(0x1578) * RF64(0x15320);
    long double heading = (long double)PI32(global, 0x580) * SF64(0x690), first, second, sum;
    double sine = (double)sim1_fsin(latitude * RF64(0x15358)), difference;
    first = product * sine;
    difference = (double)((heading - (long double)PI16(global, 0x2a0)) * SF64(0x680));
    if (!X87_C0C3(first, (long double)RF64(0x15360))) first = RF64(0x15360);
    else if (X87_C0(first, (long double)RF64(0x15368))) first = RF64(0x15368);
    second = (long double)SF64(0x1588) * SF64(0x1338) * RF64(0x15340) * sine;
    if (!X87_C0C3(second, (long double)RF64(0x15360))) second = RF64(0x15360);
    else if (X87_C0(second, (long double)RF64(0x15368))) second = RF64(0x15368);
    sum = second + first + difference;
    if (!X87_C0C3(sum, (long double)RF64(0x15370))) { PF64(global, 0x2cc) = (double)((sum - RF64(0x15370)) * SF64(0x688)); return; }
    if (X87_C0(sum, (long double)RF64(0x152e8))) sum -= RF64(0x15378);
    PF64(global, 0x2cc) = (double)(sum * SF64(0x688));
}

/* 0x22009334: the turbulence: in the air, three axes (+0x60c, +0x614, +0x61c) each nudged by
 * UTIL's rand32 times the strength (a weather table word by GLOBAL +0xf10 times +0xf0a, plus
 * +0x1168, at most 0xff) and damped; on the ground all zero. */
void sim1_turbulence(uint8_t *global)
{
    static const uint16_t axes[3] = {0x60c, 0x614, 0x61c};
    static const int32_t gains[3] = {0x14, 0x14, 0x1e};
    uint32_t strength, at;
    if (P32(global, 0x854)) { SF64(0x60c) = 0.0; SF64(0x614) = 0.0; SF64(0x61c) = 0.0; return; }
    strength = (uint32_t)P8(global, 0x1168) + (uint32_t)(((uint32_t)P16(sim1_data + 0x64c, P16(global, 0xf10) & 6) * P16(global, 0xf0a)) >> 16);
    if (strength >= 0xff) strength = 0xff;
    for (at = 0; at < 3; ++at) {
        uint32_t random = SIM1_CALL(SIM1_UTIL, 0x70, uint32_t (*)(void))();
        int32_t value = (int32_t)(uint32_t)((uint64_t)((int64_t)(int32_t)random * (int32_t)strength) >> 7);
        value = (int32_t)(uint32_t)((uint64_t)((int64_t)value * gains[at]) >> 15);
        SF64(axes[at]) = (double)(((long double)value * SF64(0x698) + SF64(axes[at])) * SF64(0x768));
    }
}

/* 0x22004840, stdcall: the seven parts' countdowns (0x22004940) by this owner's step: its
 * +0x8c over +0x88, per pass, times GLOBAL +0xc1a / 0x400 above 0x400; +0x94 cleared first. */
void __stdcall sim1_part_timers(uint8_t *owner)
{
    static const uint16_t parts[7] = {0x23e0, 0x26c0, 0x1d00, 0x1de0, 0x2580, 0x2aa0, 0x2000};
    uint16_t factor = P16(SIM1_GLOBAL, 0xc1a);
    long double step = (long double)PF64(owner, 0x8c) / (long double)(int64_t)P32(owner, 0x88) * SF64(0x16f0) * SF64(0x758);
    double kept;
    uint32_t at;
    if (factor > 0x400) step *= (long double)(int32_t)(factor >> 10);
    kept = (double)step;
    P32(owner, 0x94) = 0;
    for (at = 0; at < 7; ++at) sim1_countdown(sim1_data + parts[at], owner, kept);
}

/* 0x2200f510: the approach coupler (localizer/back course/VOR, +0xa23/+0xa25/+0xa29): the
 * course error from MAIN's nav read-out, limited (0x16c for the VOR, 0x1c7 otherwise) and
 * gated on capture by the climb history's band, through the filter at +0x2920 into the heading
 * hold's target +0xa0c; captured, it takes over from the other holds. Off, the holds clear. */
void sim1_coupler(uint8_t *global)
{
    uint16_t heading, error, limit;
    uint8_t *record = sim1_data + 0x2920;
    int32_t steer;
    uint16_t read;
    if (!S8(0xa23) && !S8(0xa25) && !S8(0xa29)) { S16(0x996) = 0; S16(0x9ae) = 0; return; }
    S16(0x9a2) = S8(0xa29) ? 0x16c : 0x1c7;
    heading = (uint16_t)(P16(global, 0x870) + 0x8000);
    if (S8(0xa25)) heading = (uint16_t)(heading + 0x8000);
    else if (!S8(0xa23)) heading = (uint16_t)(heading - S16(0x9b0));
    S16(0x9a6) = heading;
    read = 0;
    SIM1_CALL(5, 0x78, uint32_t (__stdcall *)(uint32_t, void *))(1, &read);
    error = (uint16_t)-read;
    if (!S16(0x9ae)) {
        uint16_t magnitude;
        sim1_climb_history(global, error);
        magnitude = (int16_t)error < 0 ? (uint16_t)-error : error;
        if ((int16_t)magnitude > (int16_t)sim1_u16(0xa93)) return;
    }
    limit = S16(0x9a2);
    if (!((int16_t)error > (int16_t)limit)) {
        limit = (uint16_t)-limit;
        if ((int16_t)error > (int16_t)limit) goto steer;
    }
    error = limit;
    PF64(record, 0x38) = 0.0;
    PF64(record, 0x40) = 0.0;
    if (!S16(0x9ae)) return;
steer:
    sim1_set_u16(0xa14, error);
    SI32(0x950) = sim1_fistp32(sim1_filter(record, (long double)(int16_t)sim1_u16(0xa14) * SF64(0x930)) * SF64(0x928));
    steer = SI32(0x950);
    if (S8(0xa25)) steer = -steer;
    S16(0x970) = (uint16_t)(S16(0x970) + (uint16_t)steer);
    S16(0xa0c) = (uint16_t)((uint16_t)steer + S16(0x9a6));
    S8(0xa29) = 1;
    S8(0xa28) = 1;
    P32(global, 0x7c8) = 0;
    S16(0x9ae) = 1;
    P32(global, 0x7c0) = 0;
    S8(0xa2a) = 0;
    P32(global, 0x7d8) = 0;
    S8(0xa24) = 0;
}

/* 0x22010a00: the structural limits: in mode 1 or more with GLOBAL +0x830, ten parts' loads
 * (0x22010bb0); then, unless already damaged (+0x832) or crashed (+0x838), the overspeed
 * time (+0x18e8, out of the stall band) against the engines' limit (0x9ffec / (power / 5 + 1))
 * in flight (+0x338 1), or the landing checks on the ground (gear +0x11ec, +0xbec); undamaged,
 * the situation words are refreshed. */
void sim1_structure(void)
{
    static const uint16_t parts[10] = {0x2400, 0x29e0, 0x2200, 0x1f20, 0x22c0, 0x27a0, 0x2020, 0x2140, 0x1e00, 0x26e0};
    uint8_t *global = SIM1_GLOBAL;
    uint32_t at;
    if (S32(0x162c) && P16(global, 0x830)) {
        for (at = 0; at < 10; ++at) sim1_part_limit(sim1_data + parts[at]);
        global = SIM1_GLOBAL;
    }
    if (P16(global, 0x832) || P8(global, 0x838)) return;
    if (P16(global, 0x338) == 1) {
        uint32_t flaps = P32(global, 0xbe0), divisor;
        int16_t level = PI16(global, 0x11ba);
        if ((!flaps && level > SI16(0x2b60)) || level < SI16(0x363c)) S32(0x18e8) += S32(0x16fc);
        else if ((flaps && level > SI16(0x3588)) || level < SI16(0x35f0)) S32(0x18e8) += S32(0x16fc);
        if (S32(0x2510) > 0xbb7e890u) P16(global, 0x832) = 0xc;
        else {
            int32_t fifth = (int32_t)(((int64_t)(uint32_t)P16(global, 0xc44) * 0x66666667) >> 32) >> 1;
            uint32_t remainder;
            divisor = (uint32_t)fifth + ((uint32_t)fifth >> 31) + 1;
            if (S32(0x18e8) > sim1_div32(0, 0x9ffec, divisor, &remainder)) P16(global, 0x832) = 0xc;
        }
        global = SIM1_GLOBAL;
    } else if (S32(0x1624) == 2 || S32(0x1618)) {
        if (P8(global, 0x11ec) == 2) P16(global, 0x832) = 8;
        else if (!P32(global, 0xbec)) P16(global, 0x832) = 0xa;
        global = SIM1_GLOBAL;
    }
    if (!P16(global, 0x832)) sim1_record_words();
}

/* 0x220068e0: GLOBAL +0xb18, the first of the engines' rpm ratios (the first scaled, the rest
 * divided by +0x2b54, as the game divides) above +0x420, mapped linearly; none, 0; and the
 * flag +0x118c says whether it is below +0x418. */
void sim1_engine_ratio(void)
{
    static const uint16_t rpms[3] = {0x930, 0x9c8, 0xa60};
    uint8_t *global = SIM1_GLOBAL;
    uint32_t divisor = S16(0x2b54), at, remainder;
    long double ratio;
    if (P8(global, 0xb73)) { P32(global, 0xb18) = 0; P32(SIM1_GLOBAL, 0xb1c) = 0; goto flag; }
    ratio = (long double)(int32_t)((uint32_t)P16(global, 0x898) * divisor) * SF64(0x690);
    if (!X87_C0C3(ratio, (long double)SF64(0x420))) { PF64(global, 0xb18) = (double)((long double)SF64(0x428) * ratio + SF64(0x430)); goto flag; }
    for (at = 0; at < 3; ++at) {
        ratio = (long double)(int32_t)sim1_idiv32(0, P16(global, rpms[at]), divisor, &remainder);
        if (!X87_C0C3(ratio, (long double)SF64(0x420))) { PF64(global, 0xb18) = (double)((long double)SF64(0x428) * ratio + SF64(0x430)); goto flag; }
    }
    P32(global, 0xb18) = 0;
    P32(SIM1_GLOBAL, 0xb1c) = 0;
flag:
    S32(0x118c) = X87_C0((long double)PF64(SIM1_GLOBAL, 0xb18), (long double)SF64(0x418)) ? 1 : 0;
}

/* 0x2200ea69: the propeller governor: the rpm error (+0x9e6 less GLOBAL +0x898 times +0x439a)
 * through the filter at +0x1ca0, capped at +0xac3, added to each engine's throttle while its
 * rpm is at least +0xac7. RETAIL BUG (kept, suspected): SIM1-GOVERNOR-RPM (docs/retail-bugs.md):
 * below that the throttle becomes the scaled rpm itself, as retail's reused register leaves it.
 * Off, the filter's integrators clear. */
void sim1_governor(uint8_t *global)
{
    static const uint16_t rpms[4] = {0x898, 0x930, 0x9c8, 0xa60}, throttles[4] = {0x88c, 0x924, 0x9bc, 0xa54};
    int32_t correction;
    uint32_t at;
    long double error;
    if (sim1_u32(0xacf)) sim1_set_f64(0x9e6, (double)(long double)(int32_t)sim1_u32(0xacb));
    else if (!(sim1_u32(0xad3) | sim1_u32(0xadb)) && !S8(0xa21)) {
        PF64(sim1_data + 0x1ca0, 0x38) = 0.0;
        PF64(sim1_data + 0x1ca0, 0x40) = 0.0;
        return;
    }
    error = (long double)sim1_f64(0x9e6) - (long double)PI16(global, 0x898) * (long double)SI16(0x439a) * SF64(0x690);
    sim1_set_f64(0xaab, (double)error);
    correction = SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x1ca0, error));
    if (correction >= (int32_t)sim1_u32(0xac3)) correction = (int32_t)sim1_u32(0xac3);
    sim1_set_u32(0xabf, (uint32_t)correction);
    for (at = 0; at < 4; ++at) {
        int32_t value;
        if (at && PI16(global, 0xaec) < (int16_t)(at + 1)) return;
        value = (int32_t)(uint32_t)(((uint64_t)P16(global, rpms[at]) * S16(0x439a)) >> 16);
        if ((uint32_t)value >= sim1_u32(0xac7)) value = (int32_t)PI16(global, throttles[at]) + (int32_t)sim1_u32(0xabf);
        if (value >= 0x3fff) value = 0x3fff;
        if (value <= 0) value = 0;
        P16(global, throttles[at]) = (uint16_t)value;
    }
}

/* 0x22006bb0, stdcall: a steering part's angle (+0x14) and its sine and cosine words (out),
 * from the rudder (and, with GLOBAL +0x278, the ailerons, their combined deflection) when the
 * part is on the ground (+0x23d0) and not fast enough for the fixed angle; scaled by +0x2b68
 * and, slow, by the speed. Off: 0, and the cosine 0x7fff. */
void __stdcall sim1_steering(uint8_t *part, uint32_t off, uint16_t *sine_out, uint16_t *cosine_out)
{
    long double angle;
    if (off || S32(0x23d0) != 1) { P16(part, 0x14) = 0; *sine_out = 0; *cosine_out = 0x7fff; return; }
    if (X87_C0((long double)PF64(part, 0x58), (long double)RF64(0x15200)) && !X87_C0C3((long double)PF64(part, 0x58), (long double)RF64(0x15208)))
        angle = RF64(0x15210);
    else {
        uint8_t *global = SIM1_GLOBAL;
        long double aileron = (long double)(int32_t)PI16(global, 0xbb6) * SF64(0x6a0), rudder = (long double)(int32_t)PI16(global, 0xbba) * SF64(0x6a0);
        long double slow;
        if (P32(global, 0x278) == 1) {
            long double magnitude = sim1_fsqrt((rudder * rudder + aileron * aileron) * SF64(0x768));
            angle = X87_C0(aileron + rudder, (long double)RF64(0x15210)) ? -magnitude : magnitude;
        } else angle = rudder;
        slow = (long double)SF64(0x728) * SF64(0x6d8) * PF64(part, 0x58);
        angle *= SF64(0x2b68);
        if (X87_C0(slow, (long double)RF64(0x15218))) angle = angle * SF64(0x728) * SF64(0x6d8) * PF64(part, 0x58) * RF64(0x15220);
    }
    P16(part, 0x14) = (uint16_t)sim1_ftol(angle * SF64(0x678));
    *sine_out = (uint16_t)sim1_ftol(sim1_fsin(angle) * SF64(0x6e0));
    *cosine_out = (uint16_t)sim1_ftol(sim1_fcos(angle) * SF64(0x6e0));
}

/* 0x22004990: the gear (GLOBAL +0x364 bits 2 and 3, three parts each) moving by a step each
 * pass toward their stops (each part's +8): the first compared before it is rounded, the
 * others after, as retail does. */
void sim1_gear_travel(void)
{
    static const uint16_t groups[2][3] = {{0x23e0, 0x26c0, 0x1d00}, {0x1de0, 0x2580, 0x2aa0}};
    uint8_t *global = SIM1_GLOBAL;
    uint32_t group, at;
    for (group = 0; group < 2; ++group) {
        long double step, first;
        if (!(P8(global, 0x364) & (4u << group))) continue;
        step = (long double)SF64(0x16f0) * RF64(0x150e8);
        first = (long double)SF64(groups[group][0]) + step;
        SF64(groups[group][1]) = (double)((long double)SF64(groups[group][1]) + step);
        SF64(groups[group][2]) = (double)((long double)SF64(groups[group][2]) + step);
        SF64(groups[group][0]) = (double)first;
        if (X87_C0C3(first, (long double)SF64(groups[group][0] + 8))) memcpy(sim1_data + groups[group][0], sim1_data + groups[group][0] + 8, 8);
        for (at = 1; at < 3; ++at)
            if (X87_C0C3((long double)SF64(groups[group][at]), (long double)SF64(groups[group][at] + 8)))
                memcpy(sim1_data + groups[group][at], sim1_data + groups[group][at] + 8, 8);
    }
}

/* 0x2200ed94: the glideslope coupler: captured (+0xa26), the vertical deviation from the
 * station's distance (+0x9f4) and the angles (UTIL sincos of the glideslope angle), its
 * distance +0x9f8, and, within the capture window, the pitch target +0x9e2 through the filter at
 * +0x1d20; not captured, only the height term +0x9f0. */
void sim1_glideslope(uint8_t *global)
{
    uint32_t remainder, value, quotient;
    int32_t deviation;
    if (S8(0xa26) != 1) { sim1_set_u32(0x9f0, 0u - S32(0x1d18) * 0x348u); return; }
    sim1_set_u16(0x9ee, (uint16_t)((uint16_t)-(uint16_t)(S32(0x1550) >> 16) - S16(0x1518)));
    SIM1_CALL(SIM1_UTIL, 0x2c, void (__stdcall *)(int32_t, void *, void *))((uint16_t)-P16(global, 0x872), sim1_data + 0x998, sim1_data + 0x99a);
    quotient = sim1_idiv32((uint32_t)(int32_t)(int16_t)S16(0x998), 0, (uint32_t)(int32_t)(int16_t)S16(0x99a), &remainder);
    value = (uint32_t)((int32_t)quotient >> 17);
    value = (uint32_t)((int64_t)(int32_t)value * (int32_t)sim1_u32(0x9f4)) << 1;
    sim1_set_u32(0x99c, value);
    sim1_set_u32(0x99c, sim1_u32(0x99c) + (P32(global, 0x86c) << 8) * 0x348u);
    {
        int64_t product = (int64_t)(int32_t)(sim1_u32(0x99c) - sim1_u32(0x9c2)) * (int16_t)S16(0x99a);
        sim1_set_u32(0x9f0, (uint32_t)((uint64_t)product >> 15));
    }
    {
        uint32_t height = (uint32_t)(((uint64_t)(S32(0x1d18) - (P32(global, 0x86c) << 16)) * 0x348u) >> 32);
        uint32_t square = height * height + (uint32_t)sim1_u16(0x9f4) * sim1_u16(0x9f4);
        uint16_t distance = (uint16_t)sim1_isqrt64((int64_t)(uint64_t)square);
        if (distance <= 0xa) distance = 0xa;
        sim1_set_u16(0x9f8, distance);
    }
    deviation = (int32_t)sim1_idiv32((uint32_t)((int32_t)sim1_u32(0x9f0) >> 31), sim1_u32(0x9f0), sim1_u32(0x9f4), &remainder);
    if (sim1_u16(0xa2b) && (deviation > 0x19 || deviation < -0x19)) return;
    P32(global, 0x7d0) = 0;
    sim1_set_u16(0xa2b, 0);
    {
        uint16_t target, pitch;
        int32_t steer;
        if (deviation >= 0x38e) target = 0x38e;
        else if (deviation <= -0x38e) target = 0xfc72;
        else target = (uint16_t)deviation;
        sim1_set_u16(0x9fa, (uint16_t)-target);
        steer = SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x1d20, (long double)(int16_t)sim1_u16(0x9fa) * SF64(0x930)) * SF64(0x928));
        if (steer >= SI16(0x1fe0)) pitch = S16(0x1fe0);
        else if (steer <= SI16(0x1f10)) pitch = S16(0x1f10);
        else pitch = (uint16_t)steer;
        S16(0x9e2) = pitch;
    }
}

/* 0x220110a0, stdcall: the 0xc0-byte situation record SIM1 hands MAIN: the first double through
 * the x87, the rest copied as words, the height (+0x1d18) and GLOBAL +0x366 as dwords. */
void __stdcall sim1_situation_record(uint8_t *record)
{
    static const uint16_t first[9] = {0x1540, 0x1548, 0x13e8, 0x13f0, 0x13f8, 0x13a0, 0x13a8, 0x13b0, 0x15a8};
    static const uint16_t second[13] = {0x1358, 0x1360, 0x1368, 0x1328, 0x1330, 0x1338, 0x1418, 0x1420, 0x1428, 0x13b8, 0x13c0, 0x13c8, 0x1760};
    uint32_t at;
    PF64(record, 0) = sim1_x87(SF64(0x1538));
    for (at = 0; at < 9; ++at) memcpy(record + 8 + at * 8, sim1_data + first[at], 8);
    P32(record, 0x50) = S32(0x1d18);
    P32(record, 0x54) = P8(SIM1_GLOBAL, 0x366);
    for (at = 0; at < 13; ++at) memcpy(record + 0x58 + at * 8, sim1_data + second[at], 8);
}

/* 0x22011660, stdcall: a table {count, then (x, y) doubles} read at x: below or at the first x
 * its y, above the last its y, else a binary search and a straight line between the two
 * neighbours (the lower y rounded to a double, as retail keeps it); an empty table answers x.
 * RETAIL BUG (kept): SIM1-AIR-UNCHECKED-TABLE (docs/retail-bugs.md): the last entry, at
 * (count - 1) * 16, is read before the count is checked. Returns st0. */
long double __stdcall sim1_table_lookup(const uint8_t *table, double x)
{
    uint32_t count = P32(table, 0), last = count - 1, middle = last >> 1, low = 0;
#define TABLE_X(i) PF64(table, 4 + (i) * 16)
#define TABLE_Y(i) PF64(table, 0xc + (i) * 16)
    if (X87_C0C3((long double)x, (long double)TABLE_X(0))) return TABLE_Y(0);
    if (!X87_C0C3((long double)x, (long double)TABLE_X(last))) return TABLE_Y(last);
    if (!count) return x;
    for (;;) {
        if (middle == low || middle == last) {
            long double x0 = TABLE_X(middle), fraction = ((long double)x - x0) / ((long double)TABLE_X(middle + 1) - x0);
            double y0 = TABLE_Y(middle);
            return fraction * ((long double)TABLE_Y(middle + 1) - y0) + y0;
        }
        if (!X87_C0C3((long double)x, (long double)TABLE_X(middle))) { low = middle; middle = (middle + last) >> 1; continue; }
        if (!X87_C0((long double)x, (long double)TABLE_X(middle))) return TABLE_Y(middle);
        last = middle;
        middle = (middle + low) >> 1;
    }
#undef TABLE_X
#undef TABLE_Y
}

/* 0x2200ef53: the glideslope's capture: as 0x2200ed94 without the window, its integrators
 * cleared at the deviation's caps, and then the pitch target eased by the speed's excess
 * (GLOBAL +0x2bc over +0x3d04 less 0xa00, times +0xaf7) with the throttles at idle. */
void sim1_glideslope_capture(uint8_t *global)
{
    uint32_t remainder, value, quotient;
    int32_t deviation, steer, excess;
    uint16_t target, pitch;
    S16(0x9dc) = 0;
    sim1_set_u16(0x9ee, (uint16_t)((uint16_t)-(uint16_t)(S32(0x1550) >> 16) - S16(0x1518)));
    SIM1_CALL(SIM1_UTIL, 0x2c, void (__stdcall *)(int32_t, void *, void *))((uint16_t)-P16(global, 0x872), sim1_data + 0x998, sim1_data + 0x99a);
    quotient = sim1_idiv32((uint32_t)(int32_t)(int16_t)S16(0x998), 0, (uint32_t)(int32_t)(int16_t)S16(0x99a), &remainder);
    value = (uint32_t)((int32_t)quotient >> 17);
    value = (uint32_t)((int64_t)(int32_t)value * (int32_t)sim1_u32(0x9f4)) << 1;
    sim1_set_u32(0x99c, value);
    sim1_set_u32(0x99c, sim1_u32(0x99c) + (P32(global, 0x86c) << 8) * 0x348u);
    {
        int64_t product = (int64_t)(int32_t)(sim1_u32(0x99c) - sim1_u32(0x9c2)) * (int16_t)S16(0x99a);
        sim1_set_u32(0x9f0, (uint32_t)((uint64_t)product >> 15));
    }
    {
        uint32_t height = (uint32_t)(((uint64_t)(S32(0x1d18) - (P32(global, 0x86c) << 16)) * 0x348u) >> 32);
        uint32_t square = height * height + (uint32_t)sim1_u16(0x9f4) * sim1_u16(0x9f4);
        uint16_t distance = (uint16_t)sim1_isqrt64((int64_t)(uint64_t)square);
        if (distance <= 0xa) distance = 0xa;
        sim1_set_u16(0x9f8, distance);
    }
    deviation = (int32_t)sim1_idiv32((uint32_t)((int32_t)sim1_u32(0x9f0) >> 31), sim1_u32(0x9f0), sim1_u32(0x9f4), &remainder);
    P32(global, 0x7d0) = 0;
    sim1_set_u16(0xa2b, 0);
    if (deviation >= 0x38e) { S16(0x9de) = 0; S16(0x9e0) = 0; target = 0x38e; }
    else if (deviation <= -0x38e) { S16(0x9de) = 0; S16(0x9e0) = 0; target = 0xfc72; }
    else target = (uint16_t)deviation;
    sim1_set_u16(0x9fa, (uint16_t)-target);
    steer = SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x1d20, (long double)(int16_t)sim1_u16(0x9fa) * SF64(0x930)) * SF64(0x928));
    if (steer >= SI16(0x1fe0)) pitch = S16(0x1fe0);
    else if (steer <= SI16(0x1f10)) pitch = S16(0x1f10);
    else pitch = (uint16_t)steer;
    S16(0x9e2) = pitch;
    excess = (int32_t)(P32(global, 0x2bc) + 0xa00 - S32(0x3d04));
    if (excess < 0 && excess < -0xa00) return;
    if (P16(global, 0x88c)) return;
    {
        int32_t ease = (int32_t)(uint32_t)((uint64_t)((int64_t)excess * (int32_t)sim1_u32(0xaf7)) >> 16);
        S16(0x9e2) = (uint16_t)((uint32_t)-ease + (uint32_t)(int32_t)SI16(0x9e2));
    }
}

/* 0x22002c60, stdcall: a vector (a, b, c) turned through the heading and the bank (+0x1580..)
 * and pitch (+0x1560..) into three outputs; in flight (+0x7c0) one order, on the ground the
 * other; two terms rounded to doubles where retail keeps them in its arguments' slots. */
void __stdcall sim1_rotate_a(double a, double b, double c, double *first, double *second, double *third)
{
    if (S32(0x7c0) == 1) {
        long double t1 = (long double)c * SF64(0x1580) + (long double)a * SF64(0x1588);
        long double t2 = (long double)a * SF64(0x1580) - (long double)c * SF64(0x1588);
        long double w = t1 * SF64(0x1568) + (long double)b * SF64(0x1560);
        double kept1 = (double)t1, kept2 = (double)t2;
        *first = (double)(t2 * SF64(0x1570) + w * SF64(0x1578));
        *second = (double)(w * SF64(0x1570) - (long double)kept2 * SF64(0x1578));
        *third = (double)((long double)kept1 * SF64(0x1560) - (long double)b * SF64(0x1568));
        return;
    }
    {
        long double u1 = (long double)c * SF64(0x1588) + (long double)b * SF64(0x1580);
        long double m = (long double)c * SF64(0x1580) - (long double)b * SF64(0x1588);
        long double u2 = m * SF64(0x1568) + (long double)a * SF64(0x1560);
        double kept1 = (double)u1, kept2 = (double)u2;
        *third = (double)(m * SF64(0x1560) - (long double)a * SF64(0x1568));
        *first = (double)-(u1 * SF64(0x1570) - u2 * SF64(0x1578));
        *second = (double)((long double)kept2 * SF64(0x1570) + (long double)kept1 * SF64(0x1578));
    }
}

/* 0x22002ac0, stdcall: the other way round: (a, b, c) through pitch and bank, then heading; on
 * the ground the components swapped first (b, -a). */
void __stdcall sim1_rotate_b(double a, double b, double c, double *first, double *second, double *third)
{
    if (S32(0x7c0) == 1) {
        long double v1 = (long double)b * SF64(0x1570) + (long double)a * SF64(0x1578);
        long double v2 = (long double)a * SF64(0x1570) - (long double)b * SF64(0x1578);
        long double w = v1 * SF64(0x1568) + (long double)c * SF64(0x1560);
        double kept1 = (double)v1, kept2 = (double)v2;
        *first = (double)(v2 * SF64(0x1580) + w * SF64(0x1588));
        *second = (double)((long double)kept1 * SF64(0x1560) - (long double)c * SF64(0x1568));
        *third = (double)(w * SF64(0x1580) - (long double)kept2 * SF64(0x1588));
        return;
    }
    {
        double swapped_a = sim1_x87(b), swapped_b = (double)-(long double)a;
        long double p = (long double)b * SF64(0x1570) - (long double)swapped_b * SF64(0x1578);
        long double r = (long double)swapped_b * SF64(0x1570) + (long double)swapped_a * SF64(0x1578);
        long double q, pk;
        double kept_p = (double)p, kept_q, kept_r = (double)r;
        pk = kept_p;
        q = p * SF64(0x1568) + (long double)c * SF64(0x1560);
        kept_q = (double)q;
        *first = (double)(pk * SF64(0x1560) - (long double)c * SF64(0x1568));
        *second = (double)(r * SF64(0x1580) - q * SF64(0x1588));
        *third = (double)((long double)kept_q * SF64(0x1580) + (long double)kept_r * SF64(0x1588));
    }
}

/* 0x220052b0: the inertia terms (+0x1110..+0x1138) and the moment arms (+0x15b8..+0x15c8,
 * +0x1708, +0x1710) from the wing span and chord words (+0x3b78, +0x38f2) and the mass
 * (+0x4158); some products rounded where retail parks them on its stack. */
void sim1_inertia(void)
{
    long double span = (long double)(int32_t)(uint32_t)S16(0x3b78), chord = (long double)(int32_t)(uint32_t)S16(0x38f2);
    long double mass = (long double)(int64_t)S32(0x4158);
    long double m1 = (long double)SF64(0x798) * SF64(0x1450) * RF64(0x15108), m2 = (long double)SF64(0x798) * SF64(0x1450) * SF64(0x738);
    long double density;
    double kept1 = (double)m1, kept2 = (double)m2, arm;
    SF64(0x15b8) = (double)((long double)SF64(0x15a8) * mass);
    arm = (double)((long double)SF64(0x15b8) * SF64(0x738));
    SF64(0x1710) = (double)((long double)SF64(0x738) * span * RF64(0x150f8) / SF64(0x1450));
    SF64(0x1708) = (double)((long double)SF64(0x738) * chord * RF64(0x150f8) / SF64(0x1450));
    SF64(0x15c0) = (double)((long double)arm * chord);
    SF64(0x15c8) = (double)((long double)arm * span);
    SF64(0x1110) = (double)(m2 * mass * chord * RF64(0x15100));
    SF64(0x1118) = (double)(m1 * mass * chord * chord * RF64(0x15100));
    SF64(0x1120) = (double)((long double)kept2 * mass * span * RF64(0x15100));
    SF64(0x1128) = (double)((long double)kept1 * mass * span * span * RF64(0x15100));
    density = ((long double)SF64(0x1450) + PF64(SIM1_GLOBAL, 0x8e0)) * SF64(0x798);
    SF64(0x1130) = (double)(density * mass * span * span * RF64(0x15110));
    density = ((long double)SF64(0x1450) + PF64(SIM1_GLOBAL, 0x8e0)) * SF64(0x798);
    SF64(0x1138) = (double)(density * mass * chord * span * RF64(0x15110));
}

/* 0x220040d0: the attitude quaternion (+0x1050..+0x1068) from GLOBAL's bank, pitch and heading
 * (+0x578, +0x57c, +0x580, unsigned) as half angles, the sines and cosines and their products
 * rounded where retail parks them. Retail's last flags are add esp's (ZF clear), which one
 * caller's jne relies on. */
void sim1_quaternion(void)
{
    uint8_t *global = SIM1_GLOBAL;
    long double pitch = -((long double)(int64_t)P32(global, 0x57c) * SF64(0x690) * SF64(0x680));
    long double heading = (long double)(int64_t)P32(global, 0x580) * SF64(0x690) * SF64(0x680);
    long double bank = -((long double)(int64_t)P32(global, 0x578) * SF64(0x690) * SF64(0x680));
    long double half_heading = heading * RF64(0x150d8), half_pitch = pitch * RF64(0x150d8), half_bank = bank * RF64(0x150d8);
    double kept_heading = (double)half_heading, kept_pitch = (double)half_pitch, kept_bank = (double)half_bank;
    long double sh = sim1_fsin(half_heading), sp = sim1_fsin(half_pitch), ch = sim1_fcos(kept_heading);
    long double cp = sim1_fcos(kept_pitch), cb = sim1_fcos(half_bank), sb = sim1_fsin(kept_bank);
    double dsh = (double)sh, dsp = (double)sp, dch = (double)ch, dcp = (double)cp, dsb = (double)sb;
    double spsh = (double)(sp * dsh), t18 = (double)((long double)dsp * dch), t10 = (double)(sh * dcp), cpch = (double)(cp * dch);
    SF64(0x1050) = (double)(sb * spsh + cb * cpch);
    SF64(0x1058) = (double)(((long double)dsp * dch) * cb - (sh * dcp) * dsb);
    SF64(0x1060) = (double)((sp * dsh) * cb + (long double)dsb * cpch);
    SF64(0x1068) = (double)((long double)t10 * cb - (long double)t18 * dsb);
}

/* 0x22003700: parked (mode 3 or more, below a speed, every velocity squared below its floor),
 * the velocities, rates and accelerations all cleared. */
void sim1_park(void)
{
    static const uint16_t cleared[22] = {0x13b8, 0x13c0, 0x13c8, 0x13d0, 0x13d8, 0x13e0, 0x13a0, 0x13a8, 0x13b0, 0x1418, 0x1420,
                                         0x1428, 0x1430, 0x1438, 0x1440, 0x13e8, 0x13f0, 0x13f8, 0x1070, 0x1078, 0x1080, 0x1088};
    uint32_t at;
    if (S32(0x162c) < 3) return;
    if (!X87_C0((long double)(int64_t)P32(SIM1_GLOBAL, 0x2b4) * SF64(0x720) * SF64(0x690), (long double)RF64(0x150a0))) return;
#define SQUARES(a) (((long double)SF64(a) * SF64(a) + (long double)SF64((a) + 8) * SF64((a) + 8)) + (long double)SF64((a) + 0x10) * SF64((a) + 0x10))
    if (!X87_C0(SQUARES(0x1358), (long double)RF64(0x150a8))) return;
    if (!X87_C0(SQUARES(0x1418), (long double)RF64(0x150b0))) return;
    if (!X87_C0(SQUARES(0x1328), (long double)RF64(0x150b8))) return;
#undef SQUARES
    for (at = 0; at < 22; ++at) { S32(cleared[at]) = 0; S32(cleared[at] + 4) = 0; }
}

/* 0x22004f00: the force feedback: with CONTROLS' three axes present, on the ground (GLOBAL
 * +0x604) none; in the air the elevator and aileron loads from the surfaces' deflections and
 * the speed, fed into their weighted inputs' fixed entries (+0x338, +0x2d8) and, smoothed
 * (+0x3a2, +0x3aa), back to CONTROLS. Without the axes both entries are off. */
void sim1_force_feedback(uint8_t *global)
{
    uint32_t axes[3] = {0, 0, 0};
    double forces[2];
    long double elevator, aileron;
    SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, void *))(1, axes);
    if (!axes[0] || !axes[1] || !axes[2]) {
        S32(0x338) = 0xffffffffu; S16(0x33e) = 0; S32(0x2d8) = 0xffffffffu; S16(0x2de) = 0;
        return;
    }
    if (P32(global, 0x604) == 1) {
        forces[0] = 0.0; forces[1] = 0.0;
        SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, void *))(3, forces);
        return;
    }
    S32(0x338) = 1; S16(0x33e) = 0x333; S32(0x2d8) = 1; S16(0x2de) = 0x333;
    elevator = (((long double)SF64(0x1630) * SF64(0x38e8) + (long double)SF64(0x1650) * SF64(0x4030)) + (long double)SF64(0x1490) * SF64(0x35a0));
    elevator = elevator * SF64(0x15b8) * SF64(0x39b8) / (long double)SI32(0x4158) * SF64(0x4058);
    elevator += (long double)SF64(0x1768) * sim1_f64(0x38a);
    sim1_set_f64(0x39a, (double)elevator);
    SI32(0x1718) = sim1_fistp32(-elevator);
    S16(0x33c) = (uint16_t)S32(0x1718);
    P16(global, 0x11c2) = (uint16_t)sim1_weighted(sim1_data + 0x338);
    aileron = ((long double)SF64(0x1638) + SF64(0x1640)) * SF64(0x768) * SF64(0x38a8) * SF64(0x15b8) * SF64(0x34e8) / (long double)SI32(0x4158) * SF64(0x2d80);
    sim1_set_f64(0x392, (double)aileron);
    SI32(0x1718) = sim1_fistp32(-aileron);
    S16(0x2dc) = (uint16_t)S32(0x1718);
    P16(global, 0x11c0) = (uint16_t)sim1_weighted(sim1_data + 0x2d8);
    forces[0] = (double)(((long double)sim1_f64(0x392) - sim1_f64(0x3a2)) * sim1_f64(0x3b2) * SF64(0x16f0) + sim1_f64(0x3a2));
    sim1_set_f64(0x3a2, forces[0]);
    forces[1] = (double)(((long double)sim1_f64(0x39a) - sim1_f64(0x3aa)) * sim1_f64(0x3b2) * SF64(0x16f0) + sim1_f64(0x3aa));
    sim1_set_f64(0x3aa, forces[1]);
    SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, void *))(3, forces);
}

/* 0x22010d50: the situation record at GLOBAL +0x11d8 freed and made again (HOST), and filled. */
void sim1_record_alloc(void)
{
    uint8_t *slot = SIM1_GLOBAL + 0x11d8;
    if (P32(slot, 0)) SIM1_CALL(SIM1_HOST, 0x214, uint32_t (__stdcall *)(void *))(slot);
    SIM1_CALL(SIM1_HOST, 0x20c, uint32_t (__stdcall *)(uint32_t, uint32_t, void *, uint32_t))(1, 0xc0, SIM1_GLOBAL + 0x11d8, 1);
    sim1_situation_record((uint8_t *)(uintptr_t)P32(SIM1_GLOBAL, 0x11d8));
}

/* 0x22011050: the situation record saved with the flight (MAIN tag 0x38e, 0xc0 bytes). */
void sim1_record_save(void)
{
    uint8_t record[0xc0];
    sim1_situation_record(record);
    if (!SIM1_CALL(5, 0x5c, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x38e, 0xc0))
        SIM1_CALL(5, 0x60, uint32_t (__stdcall *)(void *, uint32_t, uint32_t))(record, 0xc0, 0);
}

/* 0x22011850: the four throttles' GLOBAL addresses kept (+0x1fd0..), in the air the prop's
 * efficiency slope from the table at +0x3100 (at 0.1745 and 0), and the engine term +0x1770. */
void sim1_engine_terms(void)
{
    uint8_t *global = SIM1_GLOBAL;
    long double count, a, b;
    S32(0x1fd0) = (uint32_t)(uintptr_t)(global + 0x88c);
    S32(0x1fd4) = (uint32_t)(uintptr_t)(global + 0x924);
    S32(0x1fd8) = (uint32_t)(uintptr_t)(global + 0x9bc);
    S32(0x1fdc) = (uint32_t)(uintptr_t)(global + 0xa54);
    if (!P32(global, 0x604)) {
        uint64_t bits = 0x3fc656ac74291a5cull;
        double angle, kept;
        memcpy(&angle, &bits, 8);
        kept = (double)sim1_table_lookup(sim1_data + 0x3100, angle);
        SF64(0x25f0) = (double)(((long double)kept - sim1_table_lookup(sim1_data + 0x3100, 0.0)) * RF64(0x15310));
    }
    count = (long double)SI32(0x4238);
    a = SF64(0x3708);
    b = SF64(0x3b60);
    SF64(0x1770) = (double)(count / b - (a * count + b * SF64(0x3fd8)) / (b * a));
}

/* 0x22004770, stdcall: an engine's (esi) power +0x8c: with GLOBAL +0x609 0 from its rpm word
 * and +0x3b08; with 1 from the prop table at +0x138 by the rpm, the density and the speed. */
void __stdcall sim1_engine_power(uint8_t *engine)
{
    uint8_t *global = SIM1_GLOBAL;
    if (!P8(global, 0x609))
        PF64(engine, 0x8c) = (double)((long double)(int32_t)(uint32_t)P16(engine, 0xe) * (long double)(int64_t)S32(0x3b08) * RF64(0x150e0));
    if (P8(SIM1_GLOBAL, 0x609) == 1) {
        double rpm = (double)((long double)(int32_t)((uint32_t)P16(engine, 0xa) * (uint32_t)S16(0x439a)) * SF64(0x690));
        long double table = sim1_table_lookup(sim1_data + 0x138, rpm);
        long double root = sim1_fsqrt((long double)SF64(0x7b0) * SF64(0x7a0));
        PF64(engine, 0x8c) = (double)((table * root) * (long double)(int64_t)S32(0x4104) * SF64(0x7b8) * SF64(0x7a8) * SF64(0x6c0));
    }
}

/* 0x22011910, stdcall: the prop's thrust at a speed (x): two lookups in the table at +0xb60 (at
 * +0x7b8 less 1 and 1 less +0x7b8), rounded where retail parks them, one in the argument's slot. */
long double __stdcall sim1_prop_thrust(double x)
{
    long double k = (long double)SF64(0x6e0) / (long double)(int32_t)(uint32_t)S16(0x2c78);
    double advance = (double)(k * x - (long double)(int32_t)SI16(0x2c18));
    double first = (double)sim1_table_lookup(sim1_data + 0xb60, (double)((long double)SF64(0x7b8) - RF64(0x15300)));
    double second = (double)sim1_table_lookup(sim1_data + 0xb60, (double)((long double)RF64(0x15300) - SF64(0x7b8)));
    long double root = sim1_fsqrt((long double)SF64(0x7a0) / SF64(0x7a8));
    return root * ((long double)first * advance / second);
}

/* 0x220119c0, stdcall: the prop's efficiency at a speed (v, floored at R2e8): the same two
 * lookups, the speed's advance ratio, the blade term; at least R2e8. */
long double __stdcall sim1_prop_efficiency(double v)
{
    long double speed = X87_C0C3((long double)v, (long double)RF64(0x152e8)) ? (long double)RF64(0x152e8) : (long double)v;
    double kept_speed = (double)(sim1_fsqrt((long double)SF64(0x7a8) / SF64(0x7a0)) * speed);
    double first = (double)sim1_table_lookup(sim1_data + 0xb60, (double)((long double)SF64(0x7b8) - RF64(0x15300)));
    long double second = sim1_table_lookup(sim1_data + 0xb60, (double)((long double)RF64(0x15300) - SF64(0x7b8)));
    long double value = (second * kept_speed / first + (long double)(int32_t)SI16(0x2c18)) * (long double)(int32_t)(uint32_t)S16(0x2c78) * SF64(0x698);
    if (X87_C0(value, (long double)RF64(0x152e8))) value = RF64(0x152e8);
    return value;
}

/* 0x220054a0: the pitching moment +0x12a0: the wing's lift curve (tables +0x42a0, +0x3100 at
 * the four flap settings, +0x3e80 by the speed) and the tail's, the elevator, trim and rate
 * terms; the partial sums rounded where retail parks them in its locals. */
void sim1_pitch_moment(void)
{
    double lift = (double)sim1_table_lookup(sim1_data + 0x42a0, (double)((long double)SF64(0x1fc8) / ((long double)(int32_t)(uint32_t)S16(0x38f2) * SF64(0x738))));
    double tail = (double)(((long double)SF64(0x1680) + SF64(0x1678)) * SF64(0x768) * SF64(0x3e20));
    double flaps = (double)sim1_table_lookup(sim1_data + 0x3100, SF64(0x14e0));
    long double total, sum;
    flaps = (double)(sim1_table_lookup(sim1_data + 0x3100, SF64(0x14d8)) + flaps);
    flaps = (double)(sim1_table_lookup(sim1_data + 0x3100, SF64(0x14d0)) + flaps);
    tail = (double)((long double)tail - (sim1_table_lookup(sim1_data + 0x3100, SF64(0x14c8)) + flaps) * RF64(0x15118));
    total = sim1_table_lookup(sim1_data + 0x3e80, SF64(0x1458)) * tail * lift;
    total += ((long double)SF64(0x1660) + SF64(0x1658)) * SF64(0x768) * SF64(0x3b70);
    total += (long double)(int32_t)(uint32_t)S16(0x3812) * SF64(0x680) * SF64(0x17c0);
    total += (long double)SF64(0x1630) * SF64(0x17b0);
    total += (long double)SF64(0x3c0) * SF64(0x1760);
    sum = (long double)SF64(0x1528) * SF64(0x17b8) + (long double)SF64(0x1400) * SF64(0x17a8);
    SF64(0x12a0) = (double)(total * SF64(0x15b8) + sum * SF64(0x1120));
}

/* 0x2200a7b0: per-pass setup: the band ratio, the engine terms, the quaternion, and the mask of
 * engines present (GLOBAL +0x888, one bit per engine GLOBAL +0xaec counts). */
void sim1_pass_setup(void)
{
    uint8_t *global = SIM1_GLOBAL;
    uint32_t mask = 1;
    sim1_ratio_968();
    sim1_engine_terms();
    sim1_quaternion();
    if (P16(global, 0xaec) >= 2) { mask |= 2; if (P16(global, 0xaec) >= 3) { mask |= 4; if (P16(global, 0xaec) >= 4) mask |= 8; } }
    P32(global, 0x888) = mask;
}

/* 0x2200fc40, Linkage +0x1c: the aircraft placed: the velocities, rates and forces cleared, the
 * engines' words cleared, the gear down and locked, the altitude raised by GLOBAL +0x120c (48-bit
 * add), the attitude from +0x34f0, and HOST told (control messages 0x10044, 0x1003b, 0x100a4
 * unless +0xbe8 is 0x3fff, 0x100fa). Returns the last message's answer, as retail's eax. */
uint32_t sim1_aircraft_setup(void)
{
    static const uint16_t doubles[26] = {0x1538, 0x1540, 0x1548, 0x13a0, 0x13a8, 0x13b0, 0x13b8, 0x13c0, 0x13c8, 0x13d0, 0x13d8, 0x13e0, 0x13e8,
                                         0x13f0, 0x13f8, 0x1678, 0x1680, 0x1688, 0x1690, 0x1328, 0x1330, 0x1338, 0x1358, 0x1360, 0x1368, 0x15a8};
    static const uint16_t words[8] = {0x896, 0x92e, 0x9c6, 0xa5e, 0x898, 0x930, 0x9c8, 0xa60};
    uint8_t *global;
    uint32_t at, raise, high;
    uint16_t low;
    for (at = 0; at < 26; ++at) SF64(doubles[at]) = 0.0;
    global = SIM1_GLOBAL;
    for (at = 0; at < 8; ++at) P16(global, words[at]) = 0;
    P8(global, 0x365) = 7;
    P32(global, 0x83c) = 6;
    P32(global, 0x368) = 0xfff;
    P32(global, 0xd2c) = 0;
    raise = P32(global, 0x120c);
    low = (uint16_t)(raise + P16(global, 0x572));
    high = (raise >> 16) + P32(global, 0x574) + ((uint32_t)(uint16_t)raise + P16(global, 0x572) > 0xffff);
    P16(global, 0x572) = low;
    P32(global, 0x574) = high;
    P16(global, 0x11fa) = low;
    P32(global, 0x11fc) = high;
    P32(global, 0x11f0) = 1;
    P32(global, 0x578) = (raise & 0xffff0000u) | low;
    SF64(0x1538) = (double)((long double)SI32(0x34f0) * SF64(0x680));
    P32(global, 0x57c) = 0;
    SF64(0x1540) = 0.0;
    P32(global, 0x1200) = P32(global, 0x580);
    P32(global, 0x11f4) = 1;
    SIM1_CALL(SIM1_HOST, 0x40, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x10044, 0);
    SIM1_CALL(SIM1_HOST, 0x40, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x1003b, 0);
    if (P32(global, 0xbe8) != 0x3fff) SIM1_CALL(SIM1_HOST, 0x40, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x100a4, 1);
    return SIM1_CALL(SIM1_HOST, 0x40, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x100fa, 0);
}

/* 0x2200a000: a wheel's (esi) rolling and braking force (+0x88): the friction (0x7fff, or
 * 0xa3d/0xf5c rolling by the gear state, or braking with GLOBAL +0x790) times its load (+0x80)
 * in 48 bits, limited by what stops the wheel (0x22009dfa of its velocities), the braked wheel
 * parked (+0x2200 in mode 2 with the brakes even) held by +0x1fa8 instead. */
void sim1_wheel_force(uint8_t *global, uint8_t *wheel)
{
    uint16_t friction = 0x7fff, scale, a_low, a_mid, a_high, b_low, b_mid, b_high, cx;
    uint32_t edx, eax;
    int64_t load;
    uint64_t product;
    if ((int32_t)sim1_u32(0x812) < 4 && P32(global, 0x60c) != 2) {
        friction = P8(global, 0x11ec) == 1 ? 0xf5c : 0xa3d;
        if (P32(global, 0x790) && !P32(global, 0x79c) && !sim1_u32(0x812)) friction = 0x7fff;
    }
    S16(0x7d4) = friction;
    load = sim1_fistp64((long double)PF64(wheel, 0x80) / SF64(0x788) * SF64(0x6d8));
    memcpy(sim1_data + 0x1718, &load, 8);
    sim1_set_u16(0x7e6, S16(0x1718));
    sim1_set_u32(0x7e8, sim1_u32(0x171a));
    scale = (uint16_t)((((uint32_t)(uint16_t)(S16(0x7fc) << 2) * S16(0x43e2)) >> 16) + S16(0x7d4));
    cx = (uint16_t)(scale >> 1);
    SI32(0x1718) = sim1_fistp32((long double)PF64(wheel, 0x58) * SF64(0x728) * SF64(0x6d8));
    if (SI32(0x1718) < 0) cx = (uint16_t)-cx;
    S16(0x7fa) = cx;
    load = sim1_fistp64((long double)PF64(wheel, 0x80) * SF64(0x6d8));
    memcpy(sim1_data + 0x1718, &load, 8);
    product = sim1_mul48(S16(0x1718), sim1_u32(0x171a), cx);
    sim1_set_u16(0x804, (uint16_t)product);
    sim1_set_u32(0x806, (uint32_t)(product >> 32));
    SI32(0x1718) = sim1_fistp32((long double)PF64(wheel, 0x58) * SF64(0x728) * SF64(0x6d8));
    sim1_set_u32(0x800, (uint32_t)((uint64_t)((int64_t)(int32_t)((uint32_t)S16(0x7d0) << 16) * SI32(0x1718)) >> 32));
    SI32(0x1718) = sim1_fistp32((long double)PF64(wheel, 0x48) * SF64(0x728) * SF64(0x6d8));
    eax = (uint32_t)((uint64_t)((int64_t)(int32_t)((uint32_t)S16(0x7d2) << 16) * SI32(0x1718)) >> 32) + sim1_u32(0x800);
    product = sim1_scale48(eax);
    sim1_set_u32(0x7fe, (uint32_t)product);         /* over +0x800's low word, as retail */
    sim1_set_u16(0x802, (uint16_t)(product >> 32));
    a_low = sim1_u16(0x804); a_mid = sim1_u16(0x806); a_high = sim1_u16(0x808);
    b_low = sim1_u16(0x7fe); b_mid = sim1_u16(0x800); b_high = sim1_u16(0x802);
    {
        uint32_t borrow = a_low < b_low, borrow2 = (uint32_t)a_mid < (uint32_t)b_mid + borrow;
        int16_t d_high = (int16_t)(uint16_t)(a_high - b_high - borrow2);
        int use_b = (int16_t)a_high >= 0 ? d_high >= 0 : d_high < 0;
        if (use_b) { a_low = b_low; a_mid = b_mid; a_high = b_high; }
    }
    edx = (uint32_t)a_high << 16 | a_mid;
    if (S16(0x7fc) && P16(global, 0xbc4) == P16(global, 0xbc6) && S32(0x1624) == 2 && wheel == sim1_data + 0x2200) {
        load = sim1_fistp64((long double)SF64(0x1fa8) * SF64(0x6d8));
        memcpy(sim1_data + 0x1718, &load, 8);
        a_low = S16(0x1718);
        edx = sim1_u32(0x171a);
    }
    S16(0x1718) = a_low;
    sim1_set_u32(0x171a, edx);
    S16(0x171e) = (int32_t)edx < 0 ? 0xffff : 0;
    memcpy(&load, sim1_data + 0x1718, 8);
    PF64(wheel, 0x88) = (double)((long double)load * SF64(0x690));
}

/* 0x2200f153: the autoland's flare: low (under 0x32 in its height units), descending, and on
 * the glideslope or already flaring, the throttles cut toward idle, the trim run up (or, slow on
 * the ground in mode 2, the flare ended), and the pitch target from the height and sink rate
 * (+0x9e2, not below 0xf556); else the flare's terms clear. */
void sim1_flare(uint8_t *global)
{
    int32_t height = (int32_t)(S32(0x1d18) - P32(global, 0x120c));
    uint32_t level;
    if (height < 0) height = 0;
    level = (uint32_t)(((uint64_t)(uint32_t)height * 0x68fe) >> 29);
    if ((int32_t)level >= 0x32 || PI16(global, 0x872) >= 0) goto off;
    if (!S16(0x996)) {
        int32_t sink;
        if (P32(global, 0x874) || !P32(global, 0x854)) goto off;
        SI32(0x1718) = sim1_fistp32((long double)SF64(0x13c0) * SF64(0x728) * SF64(0x6d8));
        sink = -(int32_t)(uint32_t)((uint64_t)((int64_t)SI32(0x1718) * 0x5770a) >> 32);
        if (sink <= (int32_t)level) goto off;
    }
    {
        uint16_t cut = sim1_sub16_saturated(0, P16(global, 0x88c));
        uint16_t step = (uint16_t)(((int32_t)(int16_t)cut * 0x1400) >> 16);
        uint16_t error, scaled, word;
        P16(global, 0x88c) = sim1_add16_saturated(P16(global, 0x88c), step);
        P16(global, 0x924) = P16(global, 0x88c);
        if (S32(0x1624) == 2) {
            uint32_t value;
            uint16_t low;
            int16_t high;
            SI32(0x1718) = sim1_fistp32((long double)SF64(0x1470) * SF64(0x728) * SF64(0x6d8));
            value = S32(0x1718);
            low = (uint16_t)value;
            high = (int16_t)((int32_t)value >> 16);
            if (high > 0 || (!high && low > 0x200) || (high < 0 && low < 0xfe00)) {
                uint16_t trim = (uint16_t)((uint32_t)(((uint64_t)0x7d0 * S32(0x16fc)) >> 15) + (uint32_t)(int32_t)PI16(global, 0xbc4));
                P16(global, 0xbc4) = trim;
                P16(global, 0xbc6) = trim;
            } else {
                P32(global, 0x874) = 1;
                P16(global, 0xbc4) = 0;
                P16(global, 0xbc6) = 0;
            }
        }
        S16(0x9cc) = (uint16_t)(((uint32_t)((int32_t)(int16_t)(uint16_t)(6 - level) * 0x4ccd) << 1) >> 16);
        S16(0x9ca) = (uint16_t)sim1_fistp16(SF64(0x13c0));
        error = (uint16_t)(S16(0x9ca) - S16(0x9cc));
        scaled = (uint16_t)((int32_t)(int16_t)error * 3);
        word = (uint16_t)(((uint32_t)((int32_t)(int16_t)scaled * (int16_t)S16(0x16f8)) << 1) >> 16);
        S16(0xa18) = sim1_add16_saturated(S16(0xa18), word);
        word = sim1_add16_saturated((uint16_t)((int32_t)(int16_t)scaled * 10), S16(0xa18));
        S16(0xa1a) = word;
        word = (uint16_t)((int32_t)(int16_t)word * 8);
        if ((int16_t)word <= (int16_t)0xf556) word = 0xf556;
        S16(0x9e2) = word;
        S16(0x996) = 1;
        return;
    }
off:
    S16(0xa18) = 0;
    S16(0xa1a) = 0;
    S16(0x996) = 0;
}

/* 0x2200f774: the VOR/localizer tracking: the wind correction angle (UTIL sincos of the wind
 * against the heading, then angle-from-sine of the crosswind over the speed), the bearing to
 * the station from the scaled offsets, and the intercept: inside +-0x38e the course error
 * through the filter at +0x2600, outside it the fixed intercept heading, into +0xa0c. */
void sim1_track_course(uint8_t *global)
{
    uint16_t wind_angle = (uint16_t)(P16(global, 0xe92) - P16(global, 0x870) + 0x8000), bearing, error, target;
    uint32_t remainder, divisor;
    int32_t speed, y, x;
    int64_t crosswind;
    uint16_t scale_y, scale_x, magnitude;
    SIM1_CALL(SIM1_UTIL, 0x2c, void (__stdcall *)(int32_t, void *, void *))(wind_angle, sim1_data + 0xa95, sim1_data + 0xa97);
    crosswind = (int64_t)(int32_t)(uint32_t)((int64_t)(int32_t)(uint32_t)P16(global, 0xe90) * 0x83dd) * (int16_t)sim1_u16(0xa95);
    speed = SI32(0x1718) = sim1_fistp32((long double)SF64(0x1388) * SF64(0x728) * SF64(0x6d8));
    {
        uint32_t ratio = sim1_idiv32((uint32_t)((uint64_t)crosswind >> 32), (uint32_t)crosswind, (uint32_t)speed, &remainder);
        sim1_set_u16(0xa99, (uint16_t)SIM1_CALL(SIM1_UTIL, 0x38, uint32_t (__stdcall *)(int32_t, int32_t))((uint16_t)ratio, 0));
    }
    S16(0x9a2) = 0x38e;
    scale_y = sim1_u16(0x980); if ((int16_t)scale_y < 0) scale_y = (uint16_t)-scale_y;
    scale_x = sim1_u16(0x984); if ((int16_t)scale_x < 0) scale_x = (uint16_t)-scale_x;
    magnitude = (int16_t)scale_y > (int16_t)scale_x ? scale_y : scale_x;
    magnitude = (uint16_t)(magnitude + 1);
    divisor = (uint32_t)(int32_t)(int16_t)magnitude;
    y = (int32_t)sim1_idiv32((uint32_t)((int32_t)sim1_u32(0x97e) >> 31), sim1_u32(0x97e), divisor, &remainder) >> 1;
    x = (int32_t)sim1_idiv32((uint32_t)((int32_t)sim1_u32(0x982) >> 31), sim1_u32(0x982), divisor, &remainder) >> 1;
    bearing = sim1_bearing((int16_t)y, (int16_t)x);
    S16(0x9a8) = bearing;
    error = (uint16_t)(bearing - (uint16_t)(P16(global, 0x870) + 0x8000));
    sim1_set_u16(0xa14, error);
    if (S16(0x996)) return;
    S16(0xa16) = error;
    if (!S16(0x9aa)) S16(0x9ac) = (uint16_t)(((int16_t)error >= 0 ? 0x1554 : -0x1554) + S16(0x9a8));
    S16(0x9aa) = 0;
    target = sim1_u16(0xa14);
    if ((int16_t)target >= (int16_t)S16(0x9a2)) { target = S16(0x9a2); S16(0x9aa) = 1; }
    else if (!((int16_t)target > -(int16_t)S16(0x9a2))) { target = (uint16_t)-S16(0x9a2); S16(0x9aa) = 1; }
    else {
        P32(global, 0x7c8) = 0; S8(0xa28) = 0; P32(global, 0x7c0) = 0; S8(0xa2a) = 0; P32(global, 0x7d8) = 0; S8(0xa24) = 0;
        S16(0x9aa) = 0;
        S16(0x9ae) = 1;
    }
    {
        uint16_t heading;
        if (S16(0x9ae) || S16(0x9aa) != 1) {
            SI32(0x920) = (int16_t)target;
            SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x2600, (long double)SI32(0x920) * SF64(0x930)) * SF64(0x928));
            heading = (uint16_t)(S16(0x950) + P16(global, 0x870) + S16(0x9b4) + 0x8000 + sim1_u16(0xa99));
            S16(0x9ae) = 1;
        } else {
            heading = (uint16_t)(S16(0x9ac) + sim1_u16(0xa99));
            PF64(sim1_data + 0x2600, 0x38) = 0.0;
            PF64(sim1_data + 0x2600, 0x40) = 0.0;
        }
        S16(0xa0c) = (uint16_t)(heading + S16(0x9b0));
    }
}

/* 0x2200e2cf: the altitude capture: the selected altitude (GLOBAL +0x7d4) and the current one
 * in 32.16 metres, the vertical speed toward it (the climb rate GLOBAL +0x7f0, or +0x3fd4 once
 * captured), limited near the target, through the filter at +0x28c0 into the pitch target
 * +0x9e2 (not too nose-down high up); off, only the target height. */
void sim1_altitude_capture(uint8_t *global)
{
    uint32_t altitude;
    int32_t rate, climb;
    if (P32(global, 0x768) && P32(global, 0x7d4) != sim1_u32(0xb2f)) {
        sim1_set_u32(0xb2b, 0);
        if (!P32(global, 0x7f0)) {
            uint32_t value = S32(0x3fd4);
            if ((int32_t)((uint32_t)((int32_t)P32(global, 0x7d4) >> 16) - P32(global, 0x574)) < 0) value = 0u - value;
            P32(global, 0x7f0) = value;
        }
    }
    sim1_set_u32(0xb2f, P32(global, 0x7d4));
    sim1_set_u32(0xb33, P32(global, 0x7f0));
    sim1_set_u32(0x9c6, (uint32_t)(((uint64_t)P32(global, 0x7d4) * 0x347f0) >> 32));
    altitude = (P32(global, 0x574) << 16) | P16(global, 0x572);
    altitude += ((uint32_t)P16(global, 0x330) - (uint32_t)P16(global, 0xf48)) * sim1_u32(0xb3f);
    sim1_set_u32(0x9c2, (uint32_t)(((uint64_t)altitude * 0x347f0) >> 16));
    if (!S8(0xa22)) {
        sim1_set_u32(0x9c6, (uint32_t)(((uint64_t)P32(global, 0x7d4) * 0x347f0) >> 32));
        PF64(sim1_data + 0x28c0, 0x38) = 0.0;
        PF64(sim1_data + 0x28c0, 0x40) = 0.0;
        sim1_set_u32(0x9d2, 0);
        return;
    }
    sim1_set_u32(0xa1c, sim1_u32(0x9c6) - (sim1_u32(0x9c2) >> 16));
    rate = (int32_t)(uint32_t)((uint64_t)((int64_t)(int32_t)sim1_u32(0xa1c) * 0x4ccc) >> 16);
    rate = (int32_t)((uint32_t)rate * 0x3cu);
    if (sim1_u32(0xb2b)) { climb = (int32_t)(S32(0x3fd4) >> 16); if (rate < 0) climb = -climb; }
    else climb = (int32_t)P32(global, 0x7f0) >> 16;
    {
        int32_t error = (int32_t)sim1_u32(0xa1c), limit = (int32_t)sim1_u32(0xb37);
        if (error >= 0) { if (!(error > limit)) { int32_t bound = (int32_t)((uint32_t)error + (uint32_t)limit); if (!(climb < bound)) climb = bound; } }
        else if (!((int32_t)(0u - (uint32_t)error) > limit)) { int32_t bound = (int32_t)((uint32_t)error - (uint32_t)limit); if (!(climb > bound)) climb = bound; }
    }
    {
        int32_t magnitude_climb = climb < 0 ? (int32_t)(0u - (uint32_t)climb) : climb, magnitude_rate = rate < 0 ? (int32_t)(0u - (uint32_t)rate) : rate;
        if (!(magnitude_climb < magnitude_rate)) { climb = rate; sim1_set_u32(0xb2b, 1); P32(global, 0x7f0) = 0; }
    }
    sim1_set_u32(0xb3b, (uint32_t)climb);
    sim1_set_u32(0x9d2, (uint32_t)climb);
    sim1_set_u32(0xa2d, (uint32_t)sim1_fistp32((long double)SF64(0x13c0) * SF64(0x948) - (long double)(int32_t)sim1_u32(0x9d2)));
    SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x28c0, (long double)(int32_t)sim1_u32(0xa2d)) * SF64(0x928));
    {
        uint16_t pitch = (uint16_t)(S16(0x950) + S16(0x9e2));
        if ((int16_t)pitch >= SI16(0x1fe0)) pitch = S16(0x1fe0);
        else if ((int16_t)pitch <= SI16(0x1f10)) pitch = S16(0x1f10);
        if ((int32_t)(sim1_u32(0x9c2) >> 16) >= 0x4e20 && (int16_t)pitch <= (int16_t)0xf8e4) pitch = 0xf8e4;
        S16(0x9e2) = pitch;
    }
}

/* 0x22005c00: the yawing moment +0x12d0: the rudder, ailerons, sideslip and rates, plus the
 * propellers' slipstream with each engine's power (unless GLOBAL +0x609 or +0x278) above the
 * threshold, and the gear legs' drag differences, less the asymmetric thrust term. */
void sim1_yaw_moment(void)
{
    uint8_t *global;
    long double moment = ((((((long double)SF64(0x1658) - SF64(0x1660)) * SF64(0x2c88) + (long double)SF64(0x1638) * SF64(0x1850))
                           + (long double)SF64(0x1648) * SF64(0x1868)) + (long double)SF64(0x1640) * SF64(0x1858))
                         + (long double)SF64(0x1498) * SF64(0x1870)) * SF64(0x15c0);
    long double legs;
    moment += ((long double)SF64(0x1410) * SF64(0x1860) + (long double)SF64(0x1408) * SF64(0x1848)) * SF64(0x1118);
    SF64(0x12d0) = (double)moment;
    global = SIM1_GLOBAL;
    if (!P8(global, 0x609) && !P32(global, 0x278)) {
        long double slipstream = ((long double)RF64(0x15178) - (long double)SF64(0x1388) / ((long double)(int32_t)(uint32_t)S16(0x3702) * SF64(0x740) * RF64(0x15170)))
                                 * (long double)(int32_t)(uint32_t)P16(global, 0xc44) * RF64(0x15138);
        if (!X87_C0C3(slipstream, (long double)RF64(0x15168))) {
            moment += (long double)(int32_t)(uint32_t)P16(global, 0x8a2) * slipstream;
            SF64(0x12d0) = (double)moment;
            if (P16(global, 0xaec) >= 2) { moment += (long double)(int32_t)(uint32_t)P16(global, 0x93a) * slipstream; SF64(0x12d0) = (double)moment; }
            if (P16(global, 0xaec) >= 3) { moment += (long double)(int32_t)(uint32_t)P16(global, 0x9d2) * slipstream; SF64(0x12d0) = (double)moment; }
            if (P16(global, 0xaec) >= 4) moment += (long double)(int32_t)(uint32_t)P16(global, 0xa6a) * slipstream;
        }
    }
    legs = (((long double)SF64(0x1d00) - SF64(0x2aa0)) * (long double)SI32(0x3630) + ((long double)SF64(0x26c0) - SF64(0x2580)) * (long double)SI32(0x2b90))
           + ((long double)SF64(0x23e0) - SF64(0x1de0)) * (long double)SI32(0x2c50);
    SF64(0x12d0) = (double)((legs * SF64(0x738) + moment) - (long double)SI32(0x3850) * SF64(0x2000) * SF64(0x738));
}

/* 0x22011a90, stdcall: the engine's carburettor heat and icing (+0x1900 the temperature, +0x18f0
 * the ice, +0x18f8 iced) and the power it lets through (+0x1488, returned in st0): stopped, the
 * demand passes straight; iced, the power decays; else it follows the demand by a lag. */
long double __stdcall sim1_carburettor(double demand)
{
    uint8_t *global = SIM1_GLOBAL;
    long double value;
    if (X87_C0C3((long double)(int32_t)(uint32_t)P16(global, 0xc44) * RF64(0x15318), (long double)RF64(0x152e8))) {
        memcpy(sim1_data + 0x1488, &demand, 8);
        return SF64(0x1488);
    }
    if (P32(global, 0x788) == 1) {
        if (P32(global, 0x29c) == 1) SF64(0x1900) = (double)((long double)SF64(0x1900) + SF64(0x16f0));
        else SF64(0x1900) = (double)((long double)SF64(0x1900) - (long double)SF64(0x16f0) * RF64(0x15320));
    } else memcpy(sim1_data + 0x1900, sim1_data + 0x16e0, 8);
    if (!X87_C0((long double)SF64(0x1900), (long double)RF64(0x15328))) { S32(0x1900) = 0; S32(0x1904) = 0x40590000; }
    if (X87_C0C3((long double)SF64(0x1900), (long double)SF64(0x16e0))) memcpy(sim1_data + 0x1900, sim1_data + 0x16e0, 8);
    if (X87_C0((long double)SF64(0x1900), (long double)RF64(0x15330))) {
        if (P16(global, 0xf10) && P16(global, 0xf0e) == 1 && !X87_C0((long double)SF64(0x1450), (long double)RF64(0x15338))) {
            value = (long double)SF64(0x18f0) - (long double)SF64(0x16f0) * RF64(0x15340);
            SF64(0x18f0) = (double)value;
            if (!X87_C0(value, (long double)RF64(0x15300))) { S32(0x18f0) = 0; S32(0x18f4) = 0x3ff00000; S32(0x18f8) = 1; }
        }
    } else {
        value = (long double)SF64(0x18f0) - (long double)SF64(0x16f0) * RF64(0x15348);
        SF64(0x18f0) = (double)value;
        if (X87_C0C3(value, (long double)RF64(0x152e8))) { S32(0x18f0) = 0; S32(0x18f4) = 0; S32(0x18f8) = 0; }
    }
    if (S32(0x18f8) == 1) {
        value = (long double)SF64(0x1488) - sim1_fchs(SF64(0x1488)) * RF64(0x15350);
        SF64(0x1488) = (double)value;
        if (X87_C0C3(value, (long double)RF64(0x152e8))) { S32(0x1488) = 0; S32(0x148c) = 0; }
        return SF64(0x1488);
    }
    value = (long double)SF64(0x1488) - ((long double)demand - SF64(0x1488)) * RF64(0x15350);
    SF64(0x1488) = (double)value;
    if (!X87_C0(value, (long double)demand)) SF64(0x1488) = sim1_x87(demand);
    return SF64(0x1488);
}

/* 0x220125c0: the rotor's control mixing: the cyclic (GLOBAL +0xbb2/+0xbb6) lagged into
 * +0x1908/+0x1910, the collective (+0x88e, zero on the ground with +0x33c), the pedals (+0xbba)
 * and throttle (+0x88c) into the thrust and torque terms +0x1c40..+0x1c78, the rotor speed
 * (+0xc44) and the coning (+0x1ac0, at most R3d0) blending them. */
void sim1_rotor_mixing(void)
{
    uint8_t *global = SIM1_GLOBAL;
    long double pitch = (long double)(int32_t)PI16(global, 0xbb2) * SF64(0x6a0), roll = (long double)(int32_t)PI16(global, 0xbb6) * SF64(0x6a0);
    double pedals = (double)((long double)(int32_t)PI16(global, 0xbba) * SF64(0x6a0));
    double throttle = (double)((long double)(int32_t)PI16(global, 0x88c) * SF64(0x6a0));
    long double lagged_roll, lagged_pitch, coning, rotor, near, below, sum;
    double kept_coning, kept_roll, kept_pitch;
    SF64(0x1c00) = (double)((long double)(int32_t)(uint32_t)P16(global, 0x88e) * SF64(0x6a0));
    if (!P32(global, 0x920) && P16(global, 0x33c) == 1) { S32(0x1c00) = 0; S32(0x1c04) = 0; }
    lagged_roll = ((long double)SF64(0x1908) + roll) * RF64(0x153b8);
    lagged_pitch = ((long double)SF64(0x1910) + pitch) * RF64(0x153b8);
    coning = fabsl((long double)SF64(0x1ac0)) * RF64(0x153c8);
    kept_roll = (double)lagged_roll;
    kept_pitch = (double)lagged_pitch;
    SF64(0x1908) = kept_roll;
    SF64(0x1910) = kept_pitch;
    kept_coning = (double)coning;
    rotor = (long double)(int32_t)(uint32_t)P16(global, 0xc44) * RF64(0x153c0);
    near = X87_C0(coning, (long double)RF64(0x153d0)) ? (long double)kept_coning : (long double)RF64(0x153d0);
    below = X87_C0((long double)kept_coning, (long double)RF64(0x153d0)) ? (long double)kept_coning : (long double)RF64(0x153d0);
    sum = rotor * ((long double)RF64(0x153d0) - below) + near;
    sum -= RF64(0x153d8);
    SF64(0x1c78) = (double)((long double)throttle * RF64(0x153e8));
    SF64(0x1c40) = (double)(rotor * RF64(0x153f0) - (long double)pedals * RF64(0x153f8));
    SF64(0x1c58) = (double)(sum * kept_pitch * RF64(0x153e0));
    SF64(0x1c48) = (double)(sum * kept_roll * RF64(0x153e0));
}

/* 0x2200e9d3: the propeller governor's rpm and pitch: set (GLOBAL +0x7e4) from the commanded
 * rpm (+0x7e8) the pitch +0x7e0, through the efficiency curve for a variable-pitch prop
 * (+0x612); else the rpm from the pitch, through the thrust curve. */
void sim1_prop_pitch(uint8_t *global)
{
    long double value;
    if (P32(global, 0x7e4)) {
        value = (long double)PI32(global, 0x7e8) * SF64(0x690) * SF64(0x1468) * SF64(0x748);
        if (P16(global, 0x612)) { SF64(0x1718) = (double)value; value = sim1_prop_efficiency(SF64(0x1718)); }
        PI32(global, 0x7e0) = sim1_fistp32(value * SF64(0x6d8));
        return;
    }
    value = (long double)PI32(global, 0x7e0) * SF64(0x690);
    if (P16(global, 0x612)) { SF64(0x1718) = (double)value; value = sim1_prop_thrust(SF64(0x1718)); }
    PI32(global, 0x7e8) = sim1_fistp32(value * SF64(0x740) / SF64(0x1468) * SF64(0x6d8));
}

/* 0x22009cd9: one contact part (esi) this pass, the part counter (+0x812) advancing: unless
 * it is off (+4, +8), its damage mark, relative wind, damping, steering, slip, wheel and side
 * forces, axes, rotation into the body and its share of the totals. The steering's index word
 * (ax) kept at +0x7fc. */
void sim1_part_pass(uint8_t *global, uint8_t *part, uint16_t index)
{
    S16(0x7fc) = index;
    if (P32(part, 4) && P32(part, 8)) {
        if (!P32(part, 0)) sim1_mark_part(global, part);
        sim1_part_wind(part);
        sim1_part_damping(part);
        sim1_steering(part, sim1_u32(0x812), (uint16_t *)(sim1_data + 0x7d2), (uint16_t *)(sim1_data + 0x7d0));
        sim1_part_slip(part);
        sim1_wheel_force(global, part);
        sim1_part_side_force(global, part);
        sim1_part_axes(part);
        sim1_part_rotate(part);
        sim1_part_add(part);
    }
    sim1_set_u32(0x812, sim1_u32(0x812) + 1);
}

/* 0x220010a0, Linkage +0x08, deinit: SIM1's three CHAIN callbacks removed (lists 1, 5, 0x14),
 * the situation record made, on the ground GLOBAL +0x278 kept from +0x1c80; then (0x2200a7fe,
 * its tail) a jet whose engines +0x1778 left unset gets its starters back to 3. Returns GLOBAL,
 * as retail's eax. */
uint32_t sim1_deinit_core(void)
{
    uint8_t *global;
    SIM1_CALL(SIM1_CHAIN, 0x1c, uint32_t (__stdcall *)(uint32_t, uint32_t))(1, sim1_callbacks[0]);
    SIM1_CALL(SIM1_CHAIN, 0x1c, uint32_t (__stdcall *)(uint32_t, uint32_t))(5, sim1_callbacks[1]);
    SIM1_CALL(SIM1_CHAIN, 0x1c, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x14, sim1_callbacks[2]);
    sim1_record_alloc();
    global = SIM1_GLOBAL;
    if (P32(global, 0x604) == 1) P32(global, 0x278) = S32(0x1c80);
    global = SIM1_GLOBAL;
    if (P8(global, 0x609) == 1 && !S32(0x1778)) {
        P16(global, 0x892) = 3; P16(global, 0x92a) = 3; P16(global, 0x9c2) = 3; P16(global, 0xa5a) = 3;
        S32(0x1778) = 0;
    }
    return (uint32_t)(uintptr_t)SIM1_GLOBAL;
}

/* 16-bit fixed point as 0x220024e0 does it: a sum of two signed 16x16 products in 32 bits,
 * doubled, its high word rounded by the bit below (shl/rcl, then add ax,ax / adc dx,0). */
static uint16_t sim1_fixed_round(uint32_t sum)
{
    sum <<= 1;
    return (uint16_t)((sum >> 16) + ((sum >> 15) & 1));
}
#define FIXED_PRODUCT(a, b) ((uint32_t)((int32_t)(int16_t)(a) * (int16_t)(b)))

/* 0x220024e0: a 16-bit vector (cx, di, si) turned through the attitude's fixed-point sines and
 * cosines (+0x1590..+0x15a4), the order by +0x7c0. Retail's callers use only di (the height
 * component); the other two are returned through the pointers. */
uint16_t sim1_rotate16(uint16_t cx, uint16_t di, uint16_t si, uint16_t *cx_out, uint16_t *si_out)
{
    uint16_t first, second, third, out_cx, out_di, out_si;
    if (S32(0x7c0) != 1) {
        uint16_t c = di, d = (uint16_t)-cx;
        first = sim1_fixed_round(FIXED_PRODUCT(S16(0x1598), c) + FIXED_PRODUCT(S16(0x159c), d));
        second = sim1_fixed_round(FIXED_PRODUCT(S16(0x159c), c) - FIXED_PRODUCT(S16(0x1598), d));
        third = sim1_fixed_round(FIXED_PRODUCT(S16(0x1590), second) + FIXED_PRODUCT(S16(0x1594), si));
        out_cx = sim1_fixed_round(FIXED_PRODUCT(S16(0x1594), second) - FIXED_PRODUCT(S16(0x1590), si));
        out_di = sim1_fixed_round(FIXED_PRODUCT(S16(0x15a4), first) - FIXED_PRODUCT(S16(0x15a0), third));
        out_si = sim1_fixed_round(FIXED_PRODUCT(S16(0x15a0), first) + FIXED_PRODUCT(S16(0x15a4), third));
    } else {
        first = sim1_fixed_round(FIXED_PRODUCT(S16(0x159c), cx) - FIXED_PRODUCT(S16(0x1598), di));
        second = sim1_fixed_round(FIXED_PRODUCT(S16(0x159c), di) + FIXED_PRODUCT(S16(0x1598), cx));
        third = sim1_fixed_round(FIXED_PRODUCT(S16(0x1590), second) + FIXED_PRODUCT(S16(0x1594), si));
        out_cx = sim1_fixed_round(FIXED_PRODUCT(S16(0x15a4), first) + FIXED_PRODUCT(S16(0x15a0), third));
        out_di = sim1_fixed_round(FIXED_PRODUCT(S16(0x1594), second) - FIXED_PRODUCT(S16(0x1590), si));
        out_si = sim1_fixed_round(FIXED_PRODUCT(S16(0x15a4), third) - FIXED_PRODUCT(S16(0x15a0), first));
    }
    if (cx_out) *cx_out = out_cx;
    if (si_out) *si_out = out_si;
    return out_di;
}

/* 0x22010df0, stdcall: the situation restored from a saved record (the reverse of 0x220110a0):
 * the attitude as 16.16 angles, the rates and body velocities, the mass and height; the other
 * velocities cleared; then the trigonometry, the earth velocity and the track. */
void __stdcall sim1_situation_restore(const uint8_t *record)
{
    static const uint16_t cleared[10] = {0x1358, 0x1360, 0x1368, 0x1328, 0x1330, 0x1338, 0x1418, 0x1420, 0x1428, 0x1760};
    long double scale = (long double)SF64(0x690) * SF64(0x680);
    uint32_t angle, at;
    angle = ((uint32_t)P16(record, 0) << 16) + P16(record, 0x32);
    S32(0x1550) = angle;
    SF64(0x1538) = (double)((long double)(int64_t)angle * scale);
    angle = ((uint32_t)P16(record, 2) << 16) + P16(record, 0x30);
    S32(0x1554) = angle;
    SF64(0x1540) = (double)((long double)(int64_t)angle * scale);
    angle = ((uint32_t)P16(record, 4) << 16) + P16(record, 0x34);
    S32(0x1558) = angle;
    SF64(0x1548) = (double)((long double)(int64_t)angle * scale);
    SF64(0x13e8) = (double)((long double)(int32_t)PI16(record, 6) * SF64(0x680));
    SF64(0x13f0) = (double)((long double)(int32_t)PI16(record, 0xa) * SF64(0x680));
    SF64(0x13f8) = (double)((long double)(int32_t)PI16(record, 8) * SF64(0x680));
    SF64(0x13a0) = (double)((long double)(int32_t)(((uint32_t)P16(record, 0xc) << 16) + P16(record, 0x12)) * SF64(0x720) * SF64(0x690));
    SF64(0x13a8) = (double)((long double)(int32_t)(((uint32_t)P16(record, 0xe) << 16) + P16(record, 0x14)) * SF64(0x720) * SF64(0x690));
    SF64(0x13b0) = (double)((long double)(int32_t)(((uint32_t)P16(record, 0x10) << 16) + P16(record, 0x16)) * SF64(0x720) * SF64(0x690));
    SF64(0x15a8) = (double)((long double)(int64_t)P32(record, 0x3c) * SF64(0x690));
    S32(0x1d18) = P32(record, 0x18);
    for (at = 0; at < 10; ++at) { S32(cleared[at]) = 0; S32(cleared[at] + 4) = 0; }
    P8(SIM1_GLOBAL, 0x366) = 0;
    sim1_attitude_trig();
    sim1_rotate_b(SF64(0x13a0), SF64(0x13a8), SF64(0x13b0), &SF64(0x13b8), &SF64(0x13c0), &SF64(0x13c8));
    sim1_track();
}

/* 0x22011290, stdcall: the situation set from a saved record: the attitude (as doubles, then
 * their 16.16 angles truncated), rates, velocities, mass, height and GLOBAL +0x366; a full
 * record (0xc0 bytes) carries the earth velocities and the rest too, a short one leaves them to
 * be made from the body velocity. Then the track. */
void __stdcall sim1_situation_set(const uint8_t *record, uint32_t size)
{
    static const uint16_t rates[7] = {0x13e8, 0x13f0, 0x13f8, 0x13a0, 0x13a8, 0x13b0, 0x15a8};
    static const uint16_t rest[13] = {0x1358, 0x1360, 0x1368, 0x1328, 0x1330, 0x1338, 0x1418, 0x1420, 0x1428, 0x13b8, 0x13c0, 0x13c8, 0x1760};
    uint32_t at;
    long double pitch;
    memcpy(sim1_data + 0x1538, record, 8);
    pitch = (long double)SF64(0x1538) * SF64(0x6d8);
    SF64(0x1540) = sim1_x87(PF64(record, 8));
    SF64(0x1548) = sim1_x87(PF64(record, 0x10));
    S32(0x1550) = (uint32_t)sim1_ftol(pitch * SF64(0x678));
    S32(0x1554) = (uint32_t)sim1_ftol((long double)SF64(0x1540) * SF64(0x6d8) * SF64(0x678));
    S32(0x1558) = (uint32_t)sim1_ftol((long double)SF64(0x1548) * SF64(0x6d8) * SF64(0x678));
    for (at = 0; at < 7; ++at) memcpy(sim1_data + rates[at], record + 0x18 + at * 8, 8);
    S32(0x1d18) = P32(record, 0x50);
    P8(SIM1_GLOBAL, 0x366) = P8(record, 0x54);
    if (size == 0xc0) {
        for (at = 0; at < 13; ++at) memcpy(sim1_data + rest[at], record + 0x58 + at * 8, 8);
        sim1_track();
        return;
    }
    for (at = 0; at < 13; ++at) { S32(rest[at]) = 0; S32(rest[at] + 4) = 0; }
    sim1_attitude_trig();
    sim1_rotate_b(SF64(0x13a0), SF64(0x13a8), SF64(0x13b0), &SF64(0x13b8), &SF64(0x13c0), &SF64(0x13c8));
    sim1_track();
}

/* 0x22005e20: the rolling moment +0x12d8: the surfaces, sideslip and rates; less the
 * propellers' torque (by rpm, unless GLOBAL +0x609 or +0x278, with the engines running); less
 * each engine's torque through its throttle record (+0x1fd0..); plus the stall-region term
 * between two angles of attack. */
void sim1_roll_moment(void)
{
    uint8_t *global;
    long double moment;
    uint32_t engine;
    moment = ((((((long double)SF64(0x1640) + SF64(0x1638)) * SF64(0x1890)
                 - (((long double)SF64(0x1808) - SF64(0x17f0)) - ((long double)SF64(0x1800) - SF64(0x17f8)) * RF64(0x15180)) * SF64(0x18a8) * RF64(0x15188))
                + (((long double)SF64(0x1490) - (long double)(int32_t)SI16(0x3900) * SF64(0x680)) - RF64(0x15190)) * SF64(0x1898))
               + (long double)SF64(0x1648) * SF64(0x1888)) + (long double)SF64(0x1498) * SF64(0x1880)) * SF64(0x15c0);
    moment += ((long double)SF64(0x1410) * SF64(0x18a0) + (long double)SF64(0x1408) * SF64(0x1878)) * SF64(0x1118);
    SF64(0x12d8) = (double)moment;
    global = SIM1_GLOBAL;
    if (!P8(global, 0x609) && !P32(global, 0x278) && P16(global, 0xc44)) {
        static const uint16_t rpms[3] = {0x930, 0x9c8, 0xa60};
        long double torque = (long double)(int32_t)(uint32_t)P16(global, 0x8c8) * SF64(0x13e8) * SF64(0x760) * SF64(0x690) * SF64(0x3710);
        torque = torque * (long double)(int32_t)(uint32_t)P16(global, 0xc44) * RF64(0x15138);
        SF64(0x12d8) = (double)((long double)SF64(0x12d8) - (long double)(int32_t)(uint32_t)P16(global, 0x898) * torque);
        for (engine = 0; engine < 3; ++engine) {
            if (P16(global, 0xaec) < engine + 2) break;
            SF64(0x12d8) = (double)((long double)SF64(0x12d8) - (long double)(int32_t)(uint32_t)P16(global, rpms[engine]) * torque);
        }
    }
    for (engine = 0; engine < P16(global, 0xaec); ++engine) {
        const uint8_t *throttle = (const uint8_t *)(uintptr_t)S32(0x1fd0 + engine * 4);
        SF64(0x12d8) = (double)((long double)SF64(0x12d8) - (long double)PI32(throttle, 0x1c) * (long double)(int32_t)PI16(throttle, 0x36) * SF64(0x738));
    }
    if (!X87_C0C3((long double)SF64(0x768) * RF64(0x15140), (long double)SF64(0x1490)) && X87_C0((long double)SF64(0x768) * RF64(0x15148), (long double)SF64(0x1490))) {
        long double stall = (long double)SF64(0x1138) * SF64(0x14a8) * SF64(0x14a8) * SF64(0x1410) * SF64(0x30b8);
        long double mass = (long double)(int64_t)((uint32_t)S16(0x38f2) * S32(0x4158)) * SF64(0x1648) * SF64(0x15d0) * SF64(0x38f8);
        SF64(0x12d8) = (double)((stall + mass) + SF64(0x12d8));
    }
}

/* 0x22010da0, stdcall, CHAIN list 0x14's callback (SIM1_ROUTINE pops 8): message 0x22 loads
 * a situation (marking +0x1778, so deinit leaves the starters alone): a tag 0x5a record is
 * restored from, a 0x38e one set from with its size; 0x23 saves one. */
void __stdcall sim1_situation_message(uint32_t message, const uint8_t *event)
{
    if (message == 0x22) {
        S32(0x1778) = 1;
        if (P32(event, 0) == 0x5a) { sim1_situation_restore((const uint8_t *)(uintptr_t)P32(event, 0xc)); return; }
        if (P32(event, 0) == 0x38e) sim1_situation_set((const uint8_t *)(uintptr_t)P32(event, 0xc), P32(event, 8));
        return;
    }
    if (message == 0x23) sim1_record_save();
}

/* 0x22010cc0: the situation at startup: the one GLOBAL +0x11d8 keeps (a full record); else
 * MAIN's saved record, the 0x38e kind or else the older 0x5a kind, freed after. */
void sim1_situation_start(void)
{
    uint32_t record[2];     /* retail's two locals: the record, its size */
    const uint8_t *kept = (const uint8_t *)(uintptr_t)P32(SIM1_GLOBAL, 0x11d8);
    if (kept) { sim1_situation_set(kept, 0xc0); return; }
    if (!SIM1_CALL(SIM1_MAIN, 0x6c, uint32_t (__stdcall *)(uint32_t, uint32_t *, uint32_t *))(0x38e, &record[0], &record[1]))
        sim1_situation_set((const uint8_t *)(uintptr_t)record[0], record[1]);
    else if (!SIM1_CALL(SIM1_MAIN, 0x6c, uint32_t (__stdcall *)(uint32_t, uint32_t *, uint32_t *))(0x5a, &record[0], &record[1]))
        sim1_situation_restore((const uint8_t *)(uintptr_t)record[0]);
    else return;
    SIM1_CALL(SIM1_HOST, 0x214, uint32_t (__stdcall *)(void *))(&record[0]);
}

/* 0x2200e744 (edi GLOBAL): the propeller governor's pass, unless +0x996 (or with +0x1624 2, the
 * filter left alone): the prop's pitch and rpm (0x2200e9d3); the rpm, commanded (+0x7e4: from
 * +0x7e8, through the efficiency curve) or the pitch's high word (+0x7dc; through the curve for
 * a fixed prop), less GLOBAL +0x2bc / 128, through the filter at +0x24c0, capped at +0xac3,
 * as +0xabf. Positive: any engine over the rpm limit (+0xacb * 65536 / +0x439a; the fourth at
 * it) hands over to the governor (0x2200ea69, +0xacf set), this filter cleared; else each
 * engine's throttle takes the step. RETAIL BUG (kept, suspected): SIM1-GOVERNOR-RPM (docs/retail-bugs.md), the
 * governor's loop again: below +0xac7 the throttle becomes the scaled rpm itself. */
void sim1_governor_pass(uint8_t *global)
{
    static const uint16_t rpms[4] = {0x898, 0x930, 0x9c8, 0xa60}, throttles[4] = {0x88c, 0x924, 0x9bc, 0xa54};
    uint32_t rpm, limit, at, remainder;
    int32_t step;
    if (S16(0x996)) goto off;
    if (S32(0x1624) == 2) return;
    sim1_prop_pitch(global);
    if (P32(global, 0x7e4)) {
        rpm = (uint32_t)(SI32(0x1718) = sim1_fistp32((long double)PI32(global, 0x7e8) * SF64(0x690) * SF64(0x1468) * SF64(0x748)));
        goto curve;
    }
    if (!P32(global, 0x7dc)) goto off;
    rpm = P32(global, 0x7e0) >> 16;
    if (!P16(global, 0x612)) {
curve:
        S32(0x1718) = rpm;
        SF64(0x1718) = (double)(long double)(int32_t)S32(0x1718);
        rpm = (uint32_t)(SI32(0x1718) = sim1_fistp32(sim1_prop_efficiency(SF64(0x1718))));
    }
    sim1_set_u32(0xa9f, rpm - (P32(global, 0x2bc) >> 7));
    step = SI32(0x950) = sim1_fistp32(sim1_filter(sim1_data + 0x24c0, (long double)(int32_t)sim1_u32(0xa9f)));
    if (step >= (int32_t)sim1_u32(0xac3)) step = (int32_t)sim1_u32(0xac3);
    sim1_set_u32(0xabf, (uint32_t)step);
    sim1_set_u32(0xacf, 0);
    if ((int32_t)sim1_u32(0xabf) >= 0) {
        limit = sim1_div32(0, sim1_u32(0xacb) << 16, S16(0x439a), &remainder);
        if (P16(global, 0x898) > limit || P16(global, 0x930) > limit || P16(global, 0x9c8) > limit || P16(global, 0xa60) >= limit) {
            sim1_set_u32(0xacf, 1);
            sim1_governor(global);
            goto off;
        }
    }
    for (at = 0; at < 4; ++at) {
        int32_t value;
        if (at && PI16(global, 0xaec) < (int16_t)(at + 1)) return;
        value = (int32_t)(uint32_t)(((uint64_t)P16(global, rpms[at]) * S16(0x439a)) >> 16);
        if ((uint32_t)value >= sim1_u32(0xac7)) value = (int32_t)PI16(global, throttles[at]) + (int32_t)sim1_u32(0xabf);
        if (value >= 0x3fff) value = 0x3fff;
        if (value <= 0) value = 0;
        P16(global, throttles[at]) = (uint16_t)value;
    }
    return;
off:
    PF64(sim1_data + 0x24c0, 0x38) = 0.0;
    PF64(sim1_data + 0x24c0, 0x40) = 0.0;
}

/* One contact point's height (for 0x220097e5): its offset (+0x24, +0x28, +0x2c, times 6.5)
 * turned into the earth frame, the height word times 256 plus the height +0x1d18 (a gear that
 * is up, 0x10000 instead); a wheel adds its compression (+0xa0, parked at +0x1718 as retail
 * does). The height kept at +0x10, below ground at +8, the last pass's at +0xc. */
static uint32_t sim1_contact_below(uint32_t point, int down, int wheel)
{
    uint32_t height = 0x10000, below;
    if (down)
        height = (uint32_t)(int32_t)(int16_t)sim1_rotate16((uint16_t)((int32_t)(S32(point + 0x24) * 0xd) >> 1), (uint16_t)((int32_t)(S32(point + 0x28) * 0xd) >> 1),
                                                           (uint16_t)((int32_t)(S32(point + 0x2c) * 0xd) >> 1), 0, 0) * 0x100u + S32(0x1d18);
    if (wheel) height += (uint32_t)(SI32(0x1718) = sim1_fistp32((long double)SF64(point + 0xa0) * SF64(0x6d8) * SF64(0x728)));
    S32(point + 0x10) = height;
    below = (int32_t)height < 0;
    S32(point + 0xc) = S32(point + 8);
    S32(point + 8) = below;
    return below;
}

/* 0x220097e5 (edi GLOBAL): which contact points are below the ground: the wheels (the first
 * three by their gear's down flags, GLOBAL +0xbf4, +0xbf0, +0xbec, the first through edi; the
 * fourth always down) count into +0x1624 (the first two) and +0x1628 (all four); the other
 * points into +0x1618, +0x161c and +0x1620; +0x162c all of them; GLOBAL +0x366 whether any
 * wheel touches. Retail leaves edi GLOBAL, reloaded. */
void sim1_contacts_below(uint8_t *global)
{
    uint32_t count = S32(0x2204) ? sim1_contact_below(0x2200, P32(global, 0xbf4) != 0, 1) : 0;
    S32(0x1624) = count; S32(0x1628) = count;
    if (S32(0x1f24)) { count = sim1_contact_below(0x1f20, P32(SIM1_GLOBAL, 0xbf0) != 0, 1); S32(0x1624) += count; S32(0x1628) += count; }
    if (S32(0x2404)) S32(0x1628) += sim1_contact_below(0x2400, P32(SIM1_GLOBAL, 0xbec) != 0, 1);
    if (S32(0x29e4)) S32(0x1628) += sim1_contact_below(0x29e0, 1, 1);
    S32(0x1618) = S32(0x2024) ? sim1_contact_below(0x2020, 1, 0) : 0;
    if (S32(0x2144)) S32(0x1618) += sim1_contact_below(0x2140, 1, 0);
    S32(0x161c) = S32(0x27a4) ? sim1_contact_below(0x27a0, 1, 0) : 0;
    if (S32(0x22c4)) S32(0x161c) += sim1_contact_below(0x22c0, 1, 0);
    S32(0x1620) = 0;
    if (S32(0x1e04)) S32(0x1620) += sim1_contact_below(0x1e00, 1, 0);
    if (S32(0x26e4)) S32(0x1620) += sim1_contact_below(0x26e0, 1, 0);
    S32(0x1620) = S32(0x1620) + S32(0x1618) + S32(0x161c);
    S32(0x162c) = S32(0x1620) + S32(0x1628);
    P8(SIM1_GLOBAL, 0x366) = S32(0x1628) != 0;
}

/* 0x2200e4fa (edi GLOBAL): the autothrottle's modes, with it or its holds on (+0x774, +0x76c,
 * +0x770). Armed (+0x774): off without +0x810; with +0x80c newly set, takeoff (+0xad3) on the
 * ground, else approach (+0xadb) at or below +0xae3, else neither and +0x80c off; +0x80c just
 * cleared ends both. An rpm hold (+0x7dc or +0x7e4) runs the governor's pass (0x2200e744);
 * else, armed, the governor runs (after 0x2200e6d6) unless on the ground outside takeoff with
 * the highest rpm under +0xaeb. The last pass's +0x80c, +0xad3 and +0xadb kept.
 * RETAIL BUG (kept, suspected): SIM1-AUTOTHROTTLE-ENGINES (docs/retail-bugs.md): the highest
 * rpm's engine-count tests are inverted, so a twin or more looks at the first engine only and
 * a single at all four words. */
void sim1_autothrottle_modes(uint8_t *global)
{
    uint32_t rpm;
    if (!P32(global, 0x774) && !P32(global, 0x76c) && !P32(global, 0x770)) goto done;
    if (P32(global, 0x774)) {
        if (!P32(global, 0x810)) {
            sim1_set_u32(0xad3, 0); sim1_set_u32(0xadb, 0);
            P32(global, 0x80c) = 0; P32(global, 0x7dc) = 0; P32(global, 0x7e4) = 0;
            goto done;
        }
        if (P32(global, 0x80c)) {
            if (!sim1_u32(0xae7)) {
                if (P8(global, 0x366)) {
                    sim1_set_u32(0xad3, sim1_u32(0xad3) | 1); sim1_set_u32(0xadb, 0);
                    P32(global, 0x7dc) = 0; P32(global, 0x7e4) = 0; S8(0xa21) = 0;
                } else if (!(SI32(0x1d18) > (int32_t)sim1_u32(0xae3))) {
                    sim1_set_u32(0xadb, sim1_u32(0xadb) | 1); sim1_set_u32(0xad3, 0);
                    P32(global, 0x7dc) = 0; P32(global, 0x7e4) = 0; S8(0xa21) = 0;
                } else {
                    sim1_set_u32(0xad3, 0); sim1_set_u32(0xadb, 0); P32(global, 0x80c) = 0;
                }
            }
        } else if (sim1_u32(0xae7)) { sim1_set_u32(0xad3, 0); sim1_set_u32(0xadb, 0); }
    }
    if (P32(global, 0x7dc) || P32(global, 0x7e4)) {
        sim1_set_u32(0xad3, 0); sim1_set_u32(0xadb, 0); P32(global, 0x80c) = 0;
        sim1_governor_pass(global);
        goto done;
    }
    if (!P32(global, 0x774)) goto done;
    if (P8(global, 0x366) && !sim1_u32(0xad3)) {
        rpm = P16(global, 0x898);
        if (PI16(global, 0xaec) < 2) {
            if (!((int32_t)rpm > (int32_t)P16(global, 0x930))) rpm = P16(global, 0x930);
            if (PI16(global, 0xaec) < 3) {
                if (!((int32_t)rpm > (int32_t)P16(global, 0x9c8))) rpm = P16(global, 0x9c8);
                if (PI16(global, 0xaec) < 4 && !((int32_t)rpm > (int32_t)P16(global, 0xa60))) rpm = P16(global, 0xa60);
            }
        }
        if ((uint32_t)(((uint64_t)rpm * S16(0x439a)) >> 16) < sim1_u32(0xaeb)) goto done;
    }
    sim1_heading_9e6(global);
    sim1_governor(global);
done:
    sim1_set_u32(0xae7, P32(global, 0x80c));
    sim1_set_u32(0xad7, sim1_u32(0xad3));
    sim1_set_u32(0xadf, sim1_u32(0xadb));
}

/* 0x22004c10: seven contact points' stiffness terms (+0x2000, +0x23e0, +0x26c0, +0x1d00,
 * +0x1de0, +0x2580, +0x2aa0): GLOBAL's pairs of words (+0xb74.. +0xba8) multiplied as unsigned
 * 64-bit integers, times the scale word +0xaf4 * +0x698 * +0x690. (0x22004d80, the tail of
 * 0x22004380, turns them back.) */
void sim1_contact_terms(void)
{
    static const uint16_t pairs[7][3] = {{0xb78, 0xb74, 0x2000}, {0xb7c, 0xb80, 0x23e0}, {0xb88, 0xb84, 0x26c0}, {0xb8c, 0xb90, 0x1d00},
                                         {0xb98, 0xb94, 0x1de0}, {0xba0, 0xb9c, 0x2580}, {0xba8, 0xba4, 0x2aa0}};
    const uint8_t *global = SIM1_GLOBAL;
    long double scale = (long double)(int32_t)PI16(global, 0xaf4) * SF64(0x698) * SF64(0x690);
    uint32_t at;
    for (at = 0; at < 7; ++at)
        SF64(pairs[at][2]) = (double)((long double)(int64_t)(uint64_t)P32(global, pairs[at][0]) * (long double)(int64_t)(uint64_t)P32(global, pairs[at][1]) * scale);
}

/* 0x22002200, stdcall: x moved toward the target by rate * +0x16f0 / divisor, stopping at the
 * target; at the target (or unordered), x. Returns st0, unrounded. (0x220022a0 and
 * 0x22002468 after it are unreferenced, left out; 0x22002260 is init's tail.) */
long double __stdcall sim1_rate_limit(double x, double target, double divisor, double rate)
{
    long double value = x, step;
    int below;
    if (X87_C3(value, (long double)target)) return value;
    below = X87_C0(value, (long double)target);
    step = (long double)rate * SF64(0x16f0) / divisor;
    if (below) {
        value += step;
        return X87_C0(value, (long double)target) ? value : (long double)target;
    }
    value -= step;
    return X87_C0C3(value, (long double)target) ? (long double)target : value;
}

/* Retail's fld/fchs/fstp of a double: negated through the x87 (a signalling NaN quieted). */
static void sim1_negate(uint32_t offset)
{
    SF64(offset) = -sim1_x87(SF64(offset));
}

/* 0x22007050: the ground's roughness: for the surface type GLOBAL +0x11ec (on a change from
 * +0x11b8) the three wheels' bump rates (+0x11d8, +0x11c8, +0x11c0) and the bound +0x11d0;
 * each wheel's bump height (+0x24a0, +0x1fc0, +0x22a0) stepped by a random amount (UTIL's
 * generator) of the speed GLOBAL +0x2b4, turning back at the bound and at the floor. Types 0, 2
 * and 4 (and none of 1..7) have no bumps. */
void sim1_ground_bumps(void)
{
    static const uint32_t rates[7][4] = {   /* the rate's two words, the bound's, by type 1..7 */
        {0x0a3d70a4, 0x3fc0a3d7, 0x9999999a, 0x3fc99999}, {0, 0, 0, 0}, {0xae147ae1, 0x3fcae147, 0x33333333, 0x3fd33333},
        {0, 0, 0, 0}, {0x70a3d70a, 0x3fb70a3d, 0x33333333, 0x3fc33333}, {0x5c28f5c3, 0x3fc5c28f, 0, 0x3fd00000},
        {0x9999999a, 0x3fa99999, 0x9999999a, 0x3fb99999}};
    static const uint16_t heights[3] = {0x24a0, 0x1fc0, 0x22a0}, steps[3] = {0x11d8, 0x11c8, 0x11c0};
    static const uint16_t checks[3] = {0x22a0, 0x1fc0, 0x24a0}, checked_steps[3] = {0x11c0, 0x11c8, 0x11d8};
    uint8_t type = P8(SIM1_GLOBAL, 0x11ec);
    uint32_t at;
    if (!type || type == 4 || type == 2) {
        S32(0x24a0) = 0; S32(0x24a4) = 0; S32(0x1fc0) = 0; S32(0x1fc4) = 0; S32(0x22a0) = 0; S32(0x22a4) = 0;
        S8(0x11b8) = P8(SIM1_GLOBAL, 0x11ec);
        return;
    }
    if (type != S8(0x11b8)) {
        const uint32_t *rate = type <= 7 ? rates[type - 1] : rates[1];
        S32(0x11d8) = rate[0]; S32(0x11dc) = rate[1];
        S32(0x11c0) = rate[0]; S32(0x11c4) = rate[1];
        S32(0x11c8) = rate[0]; S32(0x11cc) = rate[1];
        S32(0x11d0) = rate[2]; S32(0x11d4) = rate[3];
    }
    for (at = 0; at < 3; ++at) {
        long double random = SIM1_CALL(SIM1_UTIL, 0xb8, double (__stdcall *)(void))();
        long double speed = (long double)(int64_t)(uint64_t)P32(SIM1_GLOBAL, 0x2b4) * SF64(0x720) * SF64(0x690);
        SF64(heights[at]) = (double)(random * speed * SF64(steps[at]) * SF64(0x16f0) + SF64(heights[at]));
    }
    for (at = 0; at < 3; ++at) {
        if (!X87_C0C3((long double)SF64(checks[at]), (long double)SF64(0x11d0))) {
            sim1_negate(checked_steps[at]);
            S32(checks[at]) = S32(0x11d0); S32(checks[at] + 4) = S32(0x11d4);
        }
        if (X87_C0((long double)SF64(checks[at]), (long double)RF64(0x15210))) {
            sim1_negate(checked_steps[at]);
            S32(checks[at]) = 0; S32(checks[at] + 4) = 0;
        }
    }
    S8(0x11b8) = P8(SIM1_GLOBAL, 0x11ec);
}

/* Retail's clamp of a stored double to its bounds' bit patterns (high words; low words 0 for
 * 325, 0xd70a3d71 for 10.47): above the upper bound (not below or equal), the top; then below
 * the lower, the bottom. */
static void sim1_clamp_stored(uint32_t offset, uint32_t upper, uint32_t lower, uint32_t low, uint32_t high)
{
    if (!X87_C0C3((long double)SF64(offset), (long double)RF64(upper))) { S32(offset) = low; S32(offset + 4) = high; }
    if (X87_C0((long double)SF64(offset), (long double)RF64(lower))) { S32(offset) = low; S32(offset + 4) = high | 0x80000000u; }
}

/* 0x22002eb0: the body accelerations: linear (+0x1328, +0x1330, +0x1338: force over the mass
 * +0x1728, plus the rates' cross products with the velocities), within +-325; angular
 * (+0x1358, +0x1360, +0x1368: Euler's equations with the inertias +0x15d8..+0x15e8 and the
 * cross inertia +0x15f0), within +-10.47; the last pass's kept (+0x1340.., +0x1370..). The
 * first linear one's upper bound is tested before it is rounded to a double. */
void sim1_body_accelerations(void)
{
    double mass = (double)(long double)(int64_t)(uint64_t)S32(0x1728);   /* retail's local */
    double old_x = SF64(0x1328), old_y = SF64(0x1330), old_z = SF64(0x1338), old_p = SF64(0x1358), old_q = SF64(0x1360), old_r = SF64(0x1368);
    long double x, y, z, p, q, r;
    x = ((long double)SF64(0x1270) / mass + (long double)SF64(0x13f0) * SF64(0x13a8)) - (long double)SF64(0x13f8) * SF64(0x13b0);
    z = ((long double)SF64(0x1280) / mass + (long double)SF64(0x13f8) * SF64(0x13a0)) - (long double)SF64(0x13e8) * SF64(0x13a8);
    y = ((long double)SF64(0x1278) / mass + (long double)SF64(0x13e8) * SF64(0x13b0)) - (long double)SF64(0x13f0) * SF64(0x13a0);
    SF64(0x1328) = (double)x;
    SF64(0x1350) = sim1_x87(old_z); SF64(0x1340) = sim1_x87(old_x); SF64(0x1348) = sim1_x87(old_y);
    SF64(0x1338) = (double)z;
    SF64(0x1330) = (double)y;
    if (!X87_C0C3(x, (long double)RF64(0x15038))) { S32(0x1328) = 0; S32(0x132c) = 0x40745000; }
    if (X87_C0((long double)SF64(0x1328), (long double)RF64(0x15040))) { S32(0x1328) = 0; S32(0x132c) = 0xc0745000; }
    sim1_clamp_stored(0x1330, 0x15038, 0x15040, 0, 0x40745000);
    sim1_clamp_stored(0x1338, 0x15038, 0x15040, 0, 0x40745000);
    SF64(0x1378) = sim1_x87(old_q);
    p = (((((long double)SF64(0x13f0) * SF64(0x13f0) - (long double)SF64(0x13f8) * SF64(0x13f8)) * SF64(0x15f0))
          + (((long double)SF64(0x15e8) - SF64(0x15d8)) * SF64(0x13f8)) * SF64(0x13f0)) + SF64(0x1310)) / SF64(0x15e0);
    SF64(0x1358) = (double)p;
    q = (((((long double)SF64(0x15e0) - SF64(0x15e8)) * SF64(0x13f8)) * SF64(0x13e8) + SF64(0x1318))
         - ((long double)SF64(0x13f0) * SF64(0x13e8) + old_r) * SF64(0x15f0)) / SF64(0x15d8);
    r = (((((long double)SF64(0x15d8) - SF64(0x15e0)) * SF64(0x13f0)) * SF64(0x13e8)
          + ((long double)SF64(0x13f8) * SF64(0x13e8) - SF64(0x1378)) * SF64(0x15f0)) + SF64(0x1320)) / SF64(0x15e8);
    SF64(0x1370) = sim1_x87(old_p); SF64(0x1380) = sim1_x87(old_r);
    SF64(0x1360) = (double)q;
    SF64(0x1368) = (double)r;
    sim1_clamp_stored(0x1358, 0x15048, 0x15050, 0xd70a3d71, 0x4024f0a3);
    sim1_clamp_stored(0x1360, 0x15048, 0x15050, 0xd70a3d71, 0x4024f0a3);
    sim1_clamp_stored(0x1368, 0x15048, 0x15050, 0xd70a3d71, 0x4024f0a3);
}

/* 0x22006460: a pair of wrapped angles following GLOBAL +0x578 and +0x57c (as angles, past
 * +0x151d0 taken down by +0x770 turns; the last pair kept at +0x11a0, +0x11a8): with
 * GLOBAL +0xb67 or +0x118c, tracking: the gain +0x3d0 easing to 0 and the offsets +0x1170,
 * +0x1178 stepping, each output +0x1160, +0x1168 stepped against its bound (+0x400, +0x408)
 * or turned back toward the angle by the wrapped difference; else, started (+0x1188) and for
 * +0x410 seconds (+0x1180), the offsets decaying and the gain rising to 1; else reset to the
 * angles. Outputs scaled by +0x151f0 into +0x1150, +0x1158. */
void sim1_angle_follow(void)
{
    uint8_t *global;
    long double scale, value;
    SF64(0x11a0) = sim1_x87(SF64(0x1190));
    SF64(0x11a8) = sim1_x87(SF64(0x1198));
    global = SIM1_GLOBAL;
    scale = (long double)SF64(0x690) * SF64(0x680);
    value = scale * (long double)(int64_t)(uint64_t)P32(global, 0x578);
    SF64(0x1190) = (double)value;
    if (!X87_C0C3(value, (long double)RF64(0x151d0))) SF64(0x1190) = (double)((long double)SF64(0x1190) - (long double)SF64(0x770) * RF64(0x151d0));
    value = (long double)SF64(0x690) * SF64(0x680) * (long double)(int64_t)(uint64_t)P32(global, 0x57c);
    SF64(0x1198) = (double)value;
    if (!X87_C0C3(value, (long double)RF64(0x151d0))) SF64(0x1198) = (double)((long double)SF64(0x1198) - (long double)SF64(0x770) * RF64(0x151d0));
    if (!P8(global, 0xb67) && !S32(0x118c)) {
        if (S32(0x1188) && X87_C0((long double)SF64(0x1180), (long double)SF64(0x410))) {
            SF64(0x1170) = (double)(((long double)SF64(0x3f0) * SF64(0x16f0) - RF64(0x151d8)) * SF64(0x1170));
            SF64(0x1178) = (double)(((long double)SF64(0x3f8) * SF64(0x16f0) - RF64(0x151d8)) * SF64(0x1178));
            SF64(0x1160) = (double)sim1_wrap(SF64(0x1190), SF64(0x1170));
            SF64(0x1168) = (double)sim1_wrap(SF64(0x1198), SF64(0x1178));
            if (X87_C0((long double)SF64(0x3d0), (long double)RF64(0x151e0))) SF64(0x3d0) = (double)((long double)SF64(0x3d8) * SF64(0x16f0) + SF64(0x3d0));
            else { S32(0x3d0) = 0; S32(0x3d4) = 0x3ff00000; }
            SF64(0x1180) = (double)((long double)SF64(0x1180) + SF64(0x16f0));
        } else {
            SF64(0x1160) = (double)((long double)SF64(0x690) * SF64(0x680) * (long double)PI32(global, 0x578));
            SF64(0x3d0) = RF64(0x151e0);
            S32(0x1188) = 0;
            value = (long double)SF64(0x690) * SF64(0x680) * (long double)PI32(global, 0x57c);
            SF64(0x1170) = RF64(0x151e8);
            SF64(0x1178) = RF64(0x151e8);
            SF64(0x1168) = (double)value;
        }
    } else {
        long double first, second;
        if (!X87_C0C3((long double)SF64(0x3d0), (long double)RF64(0x151e8))) SF64(0x3d0) = (double)((long double)SF64(0x3d0) - (long double)SF64(0x3d8) * SF64(0x16f0));
        else { S32(0x3d0) = 0; S32(0x3d4) = 0; }
        first = (long double)SF64(0x3e0) * SF64(0x16f0) + SF64(0x1170);
        second = (long double)SF64(0x3e8) * SF64(0x16f0) + SF64(0x1178);
        SF64(0x1170) = (double)first;
        SF64(0x1178) = (double)second;
        if (!X87_C0C3(first, (long double)SF64(0x400)))
            SF64(0x1160) = (double)(sim1_wrap(SF64(0x1190), -sim1_x87(SF64(0x11a0))) * SF64(0x3d0) + SF64(0x1190) + SF64(0x1170));
        else {
            value = (long double)SF64(0x3e0) + SF64(0x1160);
            SF64(0x1160) = (double)value;
            if (X87_C0C3(value, (long double)SF64(0x400))) { S32(0x1160) = S32(0x400); S32(0x1164) = S32(0x404); }
        }
        if (X87_C0((long double)SF64(0x1178), (long double)SF64(0x408)))
            SF64(0x1168) = (double)(sim1_wrap(SF64(0x1198), -sim1_x87(SF64(0x11a8))) * SF64(0x3d0) + SF64(0x1198) + SF64(0x1178));
        else {
            value = (long double)SF64(0x3e8) + SF64(0x1168);
            SF64(0x1168) = (double)value;
            if (!X87_C0(value, (long double)SF64(0x408))) { S32(0x1168) = S32(0x408); S32(0x116c) = S32(0x40c); }
        }
        S32(0x1188) = 1;
        S32(0x1180) = 0; S32(0x1184) = 0;
    }
    SF64(0x1150) = (double)((long double)SF64(0x1160) * RF64(0x151f0));
    SF64(0x1158) = (double)((long double)SF64(0x1168) * RF64(0x151f0));
}

/* 0x22013030: the rotor's forces and moments (+0x1c60, +0x1c68, +0x1be0, +0x1c20, +0x1c50,
 * +0x1c10): the rotor speed GLOBAL +0xc44, the disc's tilt angles (+0x1c48, +0x1c58 plus the
 * speed's share) through their sines and cosines, the thrust from the table at +0xd90 (by
 * +0x1ac0), the inflow over the ground (+0x1d18) and its cap; each plus the fuselage's terms.
 * Retail takes each tilt's sine unrounded and its cosine from the double it parks, and parks
 * its terms as doubles in locals; both kept. */
void sim1_rotor_forces(void)
{
    double speed = (double)((long double)(int32_t)(uint32_t)P16(SIM1_GLOBAL, 0xc44) * RF64(0x154d0));   /* retail's locals, as it parks them */
    double power = (double)((long double)SF64(0x738) * SF64(0x3e08));
    long double thrust = sim1_table_lookup(sim1_data + 0xd90, SF64(0x1ac0));
    long double pitch = (long double)speed * SF64(0x43e8) + SF64(0x1c48);
    long double roll = (long double)speed * SF64(0x4048) + SF64(0x1c58);
    long double sin_pitch = sim1_fsin(pitch), sin_roll = sim1_fsin(roll);
    long double cos_roll = sim1_fcos((double)roll), cos_pitch = sim1_fcos((double)pitch);
    double lift = (double)(thrust * SF64(0x1c30));
    double drag = (double)(thrust * SF64(0x1c30) + SF64(0x12b8));
    double sin_pitch_d = (double)sin_pitch, sin_roll_d = (double)sin_roll, cos_roll_d = (double)cos_roll, cos_pitch_d = (double)cos_pitch;
    double inflow = (double)((((long double)SF64(0x738) * SF64(0x3e08) + ((long double)SI32(0x1d18) * RF64(0x154d8)) * RF64(0x154e0)) - RF64(0x154e8))
                             / ((long double)SF64(0x3d48) + SF64(0x3d48)));
    double side = (double)((long double)cos_roll_d * SF64(0x1bf0));
    double up = (double)((long double)side - ((long double)sin_roll_d * cos_pitch_d) * drag);
    double forward = (double)((sin_pitch * drag) * cos_roll_d - (long double)cos_pitch_d * SF64(0x12c0));
    double vertical = (double)((sin_roll * SF64(0x1bf0) + (long double)sin_pitch_d * SF64(0x12c0)) - (cos_roll * cos_pitch_d) * drag);
    long double cap = ((long double)RF64(0x15500) - (long double)inflow * RF64(0x154f8)) - (long double)SF64(0x1ab8) * RF64(0x15508);
    double lateral;
    if (!X87_C0C3((long double)RF64(0x15510), cap)) cap = RF64(0x15510);
    lateral = (double)((long double)lift * sin_roll_d + side);
    SF64(0x1c60) = (double)(((sim1_fchs(sim1_fsin(SF64(0x1ad8)) * SF64(0x1c18)) + up) + SF64(0x1a40)) + SF64(0x1a00) + SF64(0x19c8));
    SF64(0x1c68) = (double)((((sim1_fcos(SF64(0x1ad8)) * speed) * SF64(0x1c18) + forward) + SF64(0x1a48)) + SF64(0x19f8) + SF64(0x19d0));
    SF64(0x1be0) = (double)(((cap * vertical + SF64(0x1a50)) + SF64(0x1a08)) + SF64(0x19d8));
    SF64(0x1c20) = (double)(((long double)forward * power) * RF64(0x15518) + SF64(0x1a38));
    SF64(0x1c50) = (double)((((long double)lateral * power + SF64(0x1a28)) + SF64(0x1a18)) + SF64(0x19e8));
    SF64(0x1c10) = (double)(((sim1_fchs(SF64(0x1c08)) * speed - (((long double)SF64(0x738) * SF64(0x3de8)) * SF64(0x1c18)) * RF64(0x15520)) + SF64(0x1a30)) + SF64(0x1a20));
}
