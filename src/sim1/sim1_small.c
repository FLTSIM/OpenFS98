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
 * 0x22004260, turns them back.) */
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

/* 0x220032a0: one integration step: the body velocities (+0x13a0, +0x13a8, +0x13b0) less the
 * linear accelerations' trapezoid (this pass's and the last's, times dt and +0x15058), within
 * +-4500; the rates (+0x13e8..) the same from the angular ones, within +-8.73; and the rates in
 * the stability axes (+0x1400.. +0x1410, turned through +0x14a8/+0x14b0, negated outside the
 * angle of attack's band +0x768 * [+0x15078, +0x15070]), within +-8.73. The first velocity's
 * upper bound is tested before it is rounded, as retail does. */
void sim1_integrate(void)
{
    static const uint16_t rates[6] = {0x13e8, 0x13f0, 0x13f8, 0x1400, 0x1408, 0x1410};
    long double x, cosine, sine;
    uint32_t at;
    x = (long double)SF64(0x13a0) - (((long double)SF64(0x1340) + SF64(0x1328)) * SF64(0x16f0)) * RF64(0x15058);
    SF64(0x13a0) = (double)x;
    SF64(0x13b0) = (double)((long double)SF64(0x13b0) - (((long double)SF64(0x1350) + SF64(0x1338)) * SF64(0x16f0)) * RF64(0x15058));
    SF64(0x13a8) = (double)((long double)SF64(0x13a8) - (((long double)SF64(0x1348) + SF64(0x1330)) * SF64(0x16f0)) * RF64(0x15058));
    if (!X87_C0C3(x, (long double)RF64(0x15060))) { S32(0x13a0) = 0; S32(0x13a4) = 0x40b19400; }
    if (X87_C0((long double)SF64(0x13a0), (long double)RF64(0x15068))) { S32(0x13a0) = 0; S32(0x13a4) = 0xc0b19400; }
    sim1_clamp_stored(0x13a8, 0x15060, 0x15068, 0, 0x40b19400);
    sim1_clamp_stored(0x13b0, 0x15060, 0x15068, 0, 0x40b19400);
    SF64(0x13e8) = (double)((long double)SF64(0x13e8) - (((long double)SF64(0x1370) + SF64(0x1358)) * SF64(0x16f0)) * RF64(0x15058));
    SF64(0x13f0) = (double)((long double)SF64(0x13f0) - (((long double)SF64(0x1378) + SF64(0x1360)) * SF64(0x16f0)) * RF64(0x15058));
    SF64(0x13f8) = (double)((long double)SF64(0x13f8) - (((long double)SF64(0x1380) + SF64(0x1368)) * SF64(0x16f0)) * RF64(0x15058));
    if (X87_C0((long double)SF64(0x1490), (long double)SF64(0x768) * RF64(0x15070))
        || X87_C0((long double)SF64(0x768) * RF64(0x15078), (long double)SF64(0x1490))) {
        cosine = sim1_fchs(SF64(0x14a8)); sine = sim1_fchs(SF64(0x14b0));
    } else {
        cosine = SF64(0x14a8); sine = SF64(0x14b0);
    }
    SF64(0x1400) = sim1_x87(SF64(0x13e8));
    SF64(0x1408) = (double)(cosine * SF64(0x13f0) - sine * SF64(0x13f8));
    SF64(0x1410) = (double)(cosine * SF64(0x13f8) + sine * SF64(0x13f0));
    for (at = 0; at < 6; ++at) sim1_clamp_stored(rates[at], 0x15080, 0x15088, 0x8f5c28f6, 0x402175c2);
}

/* A contact point (for 0x22004380) whose compression (+0) is past its limit (+8). */
static int sim1_point_over(uint32_t point)
{
    return !X87_C0C3((long double)SF64(point), (long double)SF64(point + 8));
}

/* A point taken into an engine record's mask (+0x84, the point's bit +0x14) and count (+0x88). */
static void sim1_point_add(uint8_t *record, uint32_t point)
{
    P32(record, 0x84) |= S32(point + 0x14);
    P32(record, 0x88) += 1;
}

/* 0x22004380, stdcall: which contact points an engine (its record) strikes: the layout
 * (+0x80: GLOBAL +0xaf8 for the gear type +0x33a 1, else 1) picks the points to test; the
 * first that is past its limit sets the mask (+0x84) and count (+0x88), and for layouts 1 and
 * 7 more are added. Layout 7 goes by which engine the record is. */
void __stdcall sim1_engine_strike(uint8_t *record)
{
    uint8_t *global;
    P32(record, 0x84) = 0;
    P32(record, 0x88) = 0;
    global = SIM1_GLOBAL;
    P32(record, 0x80) = P16(global, 0x33a) == 1 ? P32(global, 0xaf8) : 1;
    switch (P32(record, 0x80)) {
    case 0:
        P32(record, 0x84) = 0;
        return;
    case 2:
        if (sim1_point_over(0x26c0)) goto point_26c0;
        if (sim1_point_over(0x23e0)) { P32(record, 0x88) = 1; P32(record, 0x84) = S32(0x23f4); }
        return;
    case 3:
        if (sim1_point_over(0x2580)) { P32(record, 0x88) = 1; P32(record, 0x84) = S32(0x2594); return; }
        if (sim1_point_over(0x1de0)) { P32(record, 0x88) = 1; P32(record, 0x84) = S32(0x1df4); }
        return;
    case 4:
        if (!sim1_point_over(0x26c0)) return;
point_26c0:
        P32(record, 0x88) = 1; P32(record, 0x84) = S32(0x26d4);
        return;
    case 5:
        if (sim1_point_over(0x2580)) { P32(record, 0x88) = 1; P32(record, 0x84) = S32(0x2594); }
        return;
    case 6:
        if (sim1_point_over(0x2000)) { P32(record, 0x88) = 1; P32(record, 0x84) = S32(0x2014); }
        return;
    case 7:
        global = SIM1_GLOBAL;
        if (record == global + 0x88c && sim1_point_over(0x1de0)) { P32(record, 0x88) = 1; P32(record, 0x84) = S32(0x1df4); global = SIM1_GLOBAL; }
        if (P16(global, 0xaec) == 2 && record == global + 0x924 && sim1_point_over(0x23e0)) { sim1_point_add(record, 0x23e0); global = SIM1_GLOBAL; }
        if (P16(global, 0xaec) == 3 && record == global + 0x9bc && sim1_point_over(0x23e0)) { sim1_point_add(record, 0x23e0); global = SIM1_GLOBAL; }
        if (P16(global, 0xaec) != 4) return;
        if (record == global + 0x924 && sim1_point_over(0x1de0)) { sim1_point_add(record, 0x1de0); global = SIM1_GLOBAL; }
        if ((record == global + 0x9bc || record == global + 0xa54) && sim1_point_over(0x23e0)) sim1_point_add(record, 0x23e0);
        return;
    default:    /* 1, and past 7 */
        if (sim1_point_over(0x2000)) { P32(record, 0x88) = 1; P32(record, 0x84) = S32(0x2014); }
        if (sim1_point_over(0x26c0)) sim1_point_add(record, 0x26c0);
        else if (sim1_point_over(0x23e0)) sim1_point_add(record, 0x23e0);
        if (sim1_point_over(0x2580)) { sim1_point_add(record, 0x2580); return; }
        if (sim1_point_over(0x1de0)) sim1_point_add(record, 0x1de0);
        return;
    }
}

/* 0x220057b0: the pitching moment's sum +0x12c8: the table at +0x2da0 (by the angle of attack
 * +0x1490), the surfaces, flaps, rates and trim; the propellers' torque (by rpm, as in the
 * rolling moment, but from the pitch rate); the gear points' compressions times their
 * stiffness; each engine's throttle term (+0x1fd0..); the stall region's term; and the buffet
 * (+0x3c8, reversing each +0x15160 seconds) above +0x15150 between the angles. Retail keeps
 * the sum unrounded on the x87 from the table through the gear points, storing a rounded copy
 * after each engine but the fourth. */
void sim1_pitch_sum_moment(void)
{
    uint8_t *global;
    long double moment;
    uint32_t engine;
    moment = sim1_table_lookup(sim1_data + 0x2da0, SF64(0x1490));
    {
        long double surfaces = (((long double)SF64(0x1680) + SF64(0x1678)) * SF64(0x2ab8) + ((long double)SF64(0x1660) + SF64(0x1658)) * SF64(0x37f8)) * SF64(0x768)
                               - ((((long double)SF64(0x16a8) + SF64(0x16a0)) + SF64(0x1698)) * SF64(0x2b70)) * RF64(0x15130);
        moment = (((((moment + surfaces) + ((long double)(int32_t)(uint32_t)S16(0x3812) * SF64(0x680)) * SF64(0x1840))
                    + (long double)SF64(0x1630) * SF64(0x1818)) + (long double)SF64(0x1650) * SF64(0x1820)) + SF64(0x1830)) * SF64(0x15c8)
                 + ((long double)SF64(0x1528) * SF64(0x1838) + (long double)SF64(0x1400) * SF64(0x1828)) * SF64(0x1128);
    }
    SF64(0x12c8) = (double)moment;
    global = SIM1_GLOBAL;
    if (!P8(global, 0x609) && !P32(global, 0x278) && P16(global, 0xc44)) {
        static const uint16_t rpms[3] = {0x930, 0x9c8, 0xa60};
        long double torque = (long double)(int32_t)(uint32_t)P16(global, 0x8c8) * SF64(0x13f8) * SF64(0x760) * SF64(0x690) * SF64(0x3710);
        torque = torque * (long double)(int32_t)(uint32_t)P16(global, 0xc44) * RF64(0x15138);
        moment += (long double)(int32_t)(uint32_t)P16(global, 0x898) * torque;
        SF64(0x12c8) = (double)moment;
        for (engine = 0; engine < 3; ++engine) {
            if (P16(global, 0xaec) < engine + 2) break;
            moment += (long double)(int32_t)(uint32_t)P16(global, rpms[engine]) * torque;
            if (engine < 2) SF64(0x12c8) = (double)moment;
        }
    }
    SF64(0x12c8) = (double)((((((long double)SF64(0x1de0) + SF64(0x23e0)) * (long double)SI32(0x2c58) + ((long double)SF64(0x2580) + SF64(0x26c0)) * (long double)SI32(0x2b98))
                              + ((long double)SF64(0x2aa0) + SF64(0x1d00)) * (long double)SI32(0x3638)) + (long double)SI32(0x3858) * SF64(0x2000)) * SF64(0x738) + moment);
    for (engine = 0; engine < P16(global, 0xaec); ++engine) {
        const uint8_t *throttle = (const uint8_t *)(uintptr_t)S32(0x1fd0 + engine * 4);
        SF64(0x12c8) = (double)((long double)PI32(throttle, 0x1c) * (long double)(int32_t)PI16(throttle, 0x38) * SF64(0x738) + SF64(0x12c8));
    }
    if (!X87_C0C3((long double)SF64(0x768) * RF64(0x15140), (long double)SF64(0x1490)) && X87_C0((long double)SF64(0x768) * RF64(0x15148), (long double)SF64(0x1490))) {
        long double mass = (long double)(int64_t)(uint64_t)((uint32_t)S16(0x3b78) * S32(0x4158)) * SF64(0x1630) * SF64(0x15d0) * SF64(0x2b58);
        SF64(0x12c8) = (double)(((((long double)SF64(0x1130) * SF64(0x1400)) * SF64(0x2b78) + mass) * SF64(0x14a8)) * SF64(0x14a8) + SF64(0x12c8));
    }
    if (X87_C0((long double)SF64(0x1388), (long double)RF64(0x15150))) { S32(0x1768) = 0; S32(0x176c) = 0; return; }
    if (X87_C0((long double)SF64(0x1490), (long double)(int32_t)SI16(0x3802) * SF64(0x680) - RF64(0x15158))) {
        S32(0x1768) = 0; S32(0x176c) = 0; S32(0x1148) = 0; S32(0x114c) = 0;
        return;
    }
    if (!X87_C0((long double)SF64(0x1148), (long double)RF64(0x15160))) { sim1_negate(0x3c8); S32(0x1148) = 0; S32(0x114c) = 0; }
    {
        long double buffet = (long double)SF64(0x3c8) * SF64(0x16f0) + SF64(0x1768);
        SF64(0x1148) = (double)((long double)SF64(0x1148) + SF64(0x16f0));
        SF64(0x1768) = (double)buffet;
        SF64(0x12c8) = (double)(buffet + SF64(0x12c8));
    }
}

/* 0x220016d0: the flaps: the target (GLOBAL +0xbdc times +0x4028 and +0x6a0) for both sides
 * (+0x1688, +0x1690); unless GLOBAL +0xb6a is 1, each side (+0x1678, +0x1680) moves toward it
 * at +0x4028 over +0x43d8 a second (0x22002200); the sides back into GLOBAL +0xbe0 and +0xbe4. */
void sim1_flaps(void)
{
    long double target = (long double)(int64_t)(uint64_t)P32(SIM1_GLOBAL, 0xbdc) * SF64(0x4028) * SF64(0x6a0);
    SF64(0x1688) = (double)target;
    SF64(0x1690) = (double)target;
    if (P8(SIM1_GLOBAL, 0xb6a) != 1) {
        SF64(0x1678) = (double)sim1_rate_limit(SF64(0x1678), SF64(0x1688), SF64(0x43d8), SF64(0x4028));
        SF64(0x1680) = (double)sim1_rate_limit(SF64(0x1680), SF64(0x1690), SF64(0x43d8), SF64(0x4028));
    }
    P32(SIM1_GLOBAL, 0xbe0) = (uint32_t)sim1_ftol((long double)SF64(0x1678) * SF64(0x6e8) / SF64(0x4028));
    P32(SIM1_GLOBAL, 0xbe4) = (uint32_t)sim1_ftol((long double)SF64(0x1680) * SF64(0x6e8) / SF64(0x4028));
}

/* 0x2200daf1 (edi GLOBAL): the autopilot's pass: the station's distance and the annunciators;
 * with GLOBAL +0x854 (on the ground) the limits, course, glideslope capture, flare, rudder and
 * heading holds, ground stop and autothrottle; else the flare throttle, coupler, glideslope and
 * the autothrottle's modes; then the altitude capture, turn hold, ground attitude, wing
 * leveller, pitch hold and yaw damper. The rudder and aileron inputs (0x220051dc, 0x220051b4)
 * leave edi GLOBAL where they run, so the holds after them get GLOBAL; returns retail's edi. */
uint8_t *sim1_autopilot_pass(uint8_t *global)
{
    sim1_station_distance(global);
    sim1_annunciators(global);
    if (P32(global, 0x854)) {
        int held;
        sim1_set_limits(global);
        sim1_track_course(global);
        sim1_glideslope_capture(global);
        sim1_flare(global);
        held = S16(0x996) != 0;
        sim1_rudder_hold(global);
        if (held) global = SIM1_GLOBAL;     /* the rudder input left edi GLOBAL */
        sim1_heading_hold();
        sim1_ground_stop(global);
        sim1_autothrottle(global);
    } else {
        sim1_flare_throttle(global);
        sim1_coupler(global);
        sim1_glideslope(global);
        sim1_autothrottle_modes(global);
    }
    sim1_altitude_capture(global);
    sim1_turn_hold(global);
    sim1_ground_attitude(global);
    if (sim1_wing_leveller(global)) global = SIM1_GLOBAL;     /* the aileron input did */
    sim1_pitch_hold(global);
    sim1_yaw_damper(global);
    if (S32(0x318) == 1) global = SIM1_GLOBAL;                /* the rudder input did */
    return global;
}

/* 0x220096d0 (edi GLOBAL): the ground pass: the part counter +0x812 from 0; the bumps, the
 * points below the ground (which leaves edi GLOBAL), the attitude matrix, the cross term and
 * the pairs cleared; the steering words (GLOBAL +0xbc4..) cleared without +0x79c; the wheels
 * (+0x2400, +0x29e0, then +0x1f20 and +0x2200 with their steering words), then (+0x816 set) the
 * other points; mode 3's zeroing; with GLOBAL +0x120a, SOUND's 0x13 with +0x869; GLOBAL +0x368
 * halved; the crash count. Retail leaves edi GLOBAL. */
void sim1_ground_pass(uint8_t *global)
{
    static const uint16_t others[6] = {0x2020, 0x2140, 0x22c0, 0x27a0, 0x1e00, 0x26e0};
    uint32_t at;
    sim1_set_u32(0x812, 0);
    sim1_ground_bumps();
    sim1_contacts_below(global);
    global = SIM1_GLOBAL;
    sim1_attitude_matrix();
    sim1_cross_term();
    sim1_clear_pairs();
    if (!P32(global, 0x79c)) { P16(global, 0xbc4) = 0; P16(global, 0xbc6) = 0; P16(global, 0xbc8) = 0; }
    S8(0x816) = 0;
    sim1_part_pass(global, sim1_data + 0x2400, 0);
    sim1_part_pass(global, sim1_data + 0x29e0, 0);
    sim1_part_pass(global, sim1_data + 0x1f20, P16(global, 0xbc4));
    sim1_part_pass(global, sim1_data + 0x2200, P16(global, 0xbc6));
    S8(0x816) = 1;
    for (at = 0; at < 6; ++at) sim1_part_pass(global, sim1_data + others[at], 0);
    sim1_mode3_zero();
    if (P16(global, 0x120a)) SIM1_CALL(SIM1_SOUND, 0x24, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x13, sim1_u32(0x869));
    P32(global, 0x368) >>= 1;
    sim1_crash_count(global);
}

/* 0x22004260: the engines' pass (not for GLOBAL +0x609 2): the parts' bits and clearing, the
 * contact points' stiffness terms (0x22004c10); for each engine fitted its power, the points it
 * strikes and its timers; the gear's travel; then (0x22004d80, its tail) the stiffness terms
 * turned back into GLOBAL's words: each term over its partner word, times +0x6e0 * +0x6d8
 * over the scale word +0xaf4, where the partner is not 0. */
void sim1_engines_pass(void)
{
    static const uint16_t engines[4] = {0x88c, 0x924, 0x9bc, 0xa54};
    static const uint16_t pairs[7][3] = {{0xb78, 0x2000, 0xb74}, {0xb80, 0x23e0, 0xb7c}, {0xb88, 0x26c0, 0xb84}, {0xb90, 0x1d00, 0xb8c},
                                         {0xb98, 0x1de0, 0xb94}, {0xba0, 0x2580, 0xb9c}, {0xba8, 0x2aa0, 0xba4}};
    long double scale;
    uint32_t at;
    if (P8(SIM1_GLOBAL, 0x609) == 2) return;
    sim1_part_bits();
    sim1_part_clear();
    sim1_contact_terms();
    for (at = 0; at < 4; ++at) {
        if (!(P16(SIM1_GLOBAL, 0xaec) > at)) continue;
        sim1_engine_power(SIM1_GLOBAL + engines[at]);
        sim1_engine_strike(SIM1_GLOBAL + engines[at]);
        sim1_part_timers(SIM1_GLOBAL + engines[at]);
    }
    sim1_gear_travel();
    scale = (long double)SF64(0x6e0) * SF64(0x6d8) / (long double)(int32_t)PI16(SIM1_GLOBAL, 0xaf4);
    for (at = 0; at < 7; ++at) {
        uint32_t partner = P32(SIM1_GLOBAL, pairs[at][0]);
        if (partner) P32(SIM1_GLOBAL, pairs[at][2]) = (uint32_t)sim1_ftol((long double)SF64(pairs[at][1]) / (long double)(int64_t)(uint64_t)partner * scale);
    }
}

/* A filter record's hold reset (for 0x2200d8c7): its limit word 0x2000, its last word -1,
 * its count 0. */
static void sim1_hold_reset(uint32_t record)
{
    S16(record + 0xe) = 0x2000;
    S32(record + 0x10) = 0xffffffffu;
    S16(record + 0x14) = 0;
}

/* 0x2200d8c7: the autopilot's entry: the hold record at +0x2d8 reset, the limit check and the
 * over-limit alert; the full pass (0x2200daf1) when on the ground with GLOBAL +0x84c, or with
 * the autopilot on (+0x7bc, not +0xb6a) and any mode; else, without +0xb6a, the yaw damper and
 * the autothrottle's modes and the attitude's high words kept (+0x9e2, +0x9e4). Turned off
 * (+0x84c or +0x7bc cleared since the last pass), both hold records reset and CONTROLS'
 * +0x30 and +0x28 told 0. The last pass's +0x7bc and +0x84c kept. */
void sim1_autopilot(void)
{
    uint8_t *global = SIM1_GLOBAL;
    sim1_hold_reset(0x2d8);
    sim1_check_874(global);
    sim1_overlimit_alert(global);
    if ((P32(global, 0x854) && P32(global, 0x84c))
        || (!P8(global, 0xb6a) && P32(global, 0x7bc)
            && (P32(global, 0x7c8) | P32(global, 0x800) | P32(global, 0x7c4) | P32(global, 0x804) | P32(global, 0x7c0) | P32(global, 0x7d8) | P32(global, 0x7d0) | P32(global, 0x7fc))))
        global = sim1_autopilot_pass(global);
    else {
        if (!P8(global, 0xb6a)) {
            sim1_yaw_damper(global);
            sim1_autothrottle_modes(global);
        }
        S16(0x9e2) = (uint16_t)(S32(0x1550) >> 16);
        S16(0x9e4) = (uint16_t)(S32(0x1554) >> 16);
    }
    if ((sim1_u32(0xaef) && !P32(global, 0x84c)) || (sim1_u32(0xaf3) && !P32(global, 0x7bc))) {
        sim1_hold_reset(0x2d8);
        sim1_hold_reset(0x308);
        SIM1_CALL(SIM1_CONTROLS, 0x30, uint32_t (__stdcall *)(uint32_t))(0);
        SIM1_CALL(SIM1_CONTROLS, 0x28, uint32_t (__stdcall *)(uint32_t))(0);
    }
    sim1_set_u32(0xaf3, P32(global, 0x7bc));
    sim1_set_u32(0xaef, P32(global, 0x84c));
}

/* 0x22001360: the spoilers. Armed (+0x2c04 1): GLOBAL +0xbcc's arming deploys the lever
 * (+0xbd0) to 0x12c0 and disarming stows it from there; on the ground (+0x1624 2) armed, no
 * engine's throttle above 0 deploys it fully (+0x1038 marking that), and a throttle opened again
 * stows and disarms it; the lever past 0x12c0 then maps onto 0..0x3fff (retail's magic-number
 * divide by 11583). The lever's position (+0x1668, +0x1670 each side) plus, with +0x4054 and
 * the flaps not both below +0x3860, each aileron's share past +0x4160, what passes +0x3718
 * moved to the other side; each side rate-limited (0x22002200) and back into GLOBAL +0xbd4
 * and +0xbd8. */
void sim1_spoilers(void)
{
    uint8_t *global = SIM1_GLOBAL;
    uint32_t lever;
    long double value;
    if (S32(0x2c04) == 1) {
        if (P32(global, 0xbcc) == 1 && !S32(0x103c)) { P32(global, 0xbd0) = 0x12c0; global = SIM1_GLOBAL; }
        if (!P32(global, 0xbcc) && S32(0x103c) == 1 && P32(global, 0xbd0) == 0x12c0) { P32(global, 0xbd0) = 0; global = SIM1_GLOBAL; }
        S32(0x103c) = P32(global, 0xbcc);
        if (P32(global, 0xbcc) == 1 && S32(0x1624) == 2) {
            if (PI16(global, 0x88c) <= 0 && PI16(global, 0x924) <= 0 && PI16(global, 0x9bc) <= 0 && PI16(global, 0xa54) <= 0) {
                P32(global, 0xbd0) = 0x3fff;
                S32(0x1038) = 1;
                global = SIM1_GLOBAL;
            } else if (S32(0x1038) == 1) {
                P32(global, 0xbd0) = 0;
                P32(SIM1_GLOBAL, 0xbcc) = 0;
                S32(0x1038) = 0;
                global = SIM1_GLOBAL;
            }
        }
        lever = P32(global, 0xbd0);
        if (lever > 0x12c0) {
            uint32_t n = lever * 0x3fff - 0x4afed40, h = (uint32_t)(((uint64_t)n * 0x6a1bcdb3u) >> 32);
            lever = (((n - h) >> 1) + h) >> 13;
        } else lever = 0;
    } else lever = P32(global, 0xbd0);
    SF64(0x1668) = (double)((long double)(int64_t)(uint64_t)lever * SF64(0x3718) * SF64(0x6a0));
    S32(0x1670) = S32(0x1668); S32(0x1674) = S32(0x166c);
    if (S32(0x4054) && (!X87_C0((long double)SF64(0x1678), (long double)SF64(0x3860)) || !X87_C0((long double)SF64(0x1680), (long double)SF64(0x3860)))) {
        if (!X87_C0C3((long double)SF64(0x1640), (long double)SF64(0x4160))) {
            value = ((long double)SF64(0x1640) - SF64(0x4160)) * SF64(0x3fa8) + SF64(0x1668);
            SF64(0x1670) = (double)value;
            if (!X87_C0(value, (long double)SF64(0x3718))) {
                long double over = (long double)SF64(0x1670) - SF64(0x3718);
                SF64(0x1670) = sim1_x87(SF64(0x3718));
                value = (long double)SF64(0x1668) - over;
                SF64(0x1668) = (double)value;
                if (X87_C0C3(value, (long double)RF64(0x15000))) { S32(0x1668) = 0; S32(0x166c) = 0; }
            }
        }
        if (X87_C0((long double)SF64(0x1638), sim1_fchs(SF64(0x4160)))) {
            value = (sim1_fchs(SF64(0x1638)) - SF64(0x4160)) * SF64(0x3fa8) + SF64(0x1668);
            SF64(0x1668) = (double)value;
            if (!X87_C0(value, (long double)SF64(0x3718))) {
                long double over = (long double)SF64(0x1668) - SF64(0x3718);
                SF64(0x1668) = sim1_x87(SF64(0x3718));
                value = (long double)SF64(0x1670) - over;
                SF64(0x1670) = (double)value;
                if (X87_C0C3(value, (long double)RF64(0x15000))) { S32(0x1670) = 0; S32(0x1674) = 0; }
            }
        }
    }
    SF64(0x1658) = (double)sim1_rate_limit(SF64(0x1658), SF64(0x1668), SF64(0x4240), SF64(0x3718));
    SF64(0x1660) = (double)sim1_rate_limit(SF64(0x1660), SF64(0x1670), SF64(0x4240), SF64(0x3718));
    P32(SIM1_GLOBAL, 0xbd4) = (uint32_t)sim1_ftol((long double)SF64(0x1658) * SF64(0x6e8) / SF64(0x3718));
    P32(SIM1_GLOBAL, 0xbd8) = (uint32_t)sim1_ftol((long double)SF64(0x1660) * SF64(0x6e8) / SF64(0x3718));
}

/* One gear leg's events (for 0x220017f0): arrived at its target, CONTROLS +0x5c's event once
 * (the flag +stopped); moving away from it, the stopped flag cleared; short of it, the event
 * once more (the flag +moving). */
static void sim1_leg_events(uint32_t position, uint32_t target, uint32_t moving, uint32_t stopped, uint32_t event)
{
    if (X87_C3((long double)SF64(position), (long double)SF64(target))) {
        uint32_t was = S32(stopped);
        S32(moving) = 0;
        if (!was) { S32(stopped) = 1; SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, uint32_t))(event, 0); }
    } else if (!X87_C0C3((long double)SF64(position), (long double)SF64(target))) S32(stopped) = 0;
    else if (!S32(moving)) { S32(moving) = 1; SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, uint32_t))(event, 0); }
}

/* One working leg's target moved by the handle's step (for 0x220017f0): position and target
 * both, within 1.0 (tested before rounding). */
static long double sim1_leg_step(long double step, uint32_t position, uint32_t target)
{
    long double value = step + SF64(position);
    SF64(position) = (double)value;
    SF64(target) = (double)value;
    if (!X87_C0C3(value, (long double)RF64(0x15030))) { S32(target) = 0; S32(target + 4) = 0x3ff00000; S32(position) = 0; S32(position + 4) = 0x3ff00000; }
    return step;
}

/* 0x220017f0: the landing gear: a new failure (GLOBAL +0xbfc set) draws which legs work
 * (+0x1040, +0x1044, +0x1048, by UTIL's generator against 0.28, 0.52, 0.76 and 0.88); cleared,
 * all work. The handle (+0xbe8) down: each working leg's position (+0x1698, +0x16a0, +0x16a8)
 * steps by +0xbf8 (taken, times +0x15028) toward 1, a stuck one's target 1; up: on the ground
 * with the gear not all down, the targets 1, else the working legs' targets their positions and
 * the stuck ones' 0. Each leg moves to its target at its rate (+0x4148, +0x2d70, +0x3c08) and
 * into GLOBAL +0xbec, +0xbf0, +0xbf4; CONTROLS +0x5c's events 5, 6, 7 as they arrive or set
 * out. With +0x3584, the gear horn (SOUND 5, on a change): on the ground the handle not up;
 * in the air a leg not down and the flaps past +0x3f98, or flaps past +0x3e68 and a throttle
 * below +0x3e10. RETAIL BUG (kept, suspected): SIM1-GEAR-HORN-ENGINES (docs/retail-bugs.md):
 * the throttle test looks at the first engine and (two or more fitted) the second only. */
void sim1_landing_gear(void)
{
    uint8_t *global = SIM1_GLOBAL;
    uint32_t handle_step, horn = 0;
    if (P32(global, 0xbfc) == 1 && !S32(0x104c)) {
        long double draw;
        S32(0x104c) = 1;
        draw = SIM1_CALL(SIM1_UTIL, 0xb8, double (__stdcall *)(void))();
        if (X87_C0(draw, (long double)RF64(0x15008))) { S32(0x1040) = 1; S32(0x1044) = 1; S32(0x1048) = 1; }
        else if (X87_C0(draw, (long double)RF64(0x15010))) { S32(0x1040) = 1; S32(0x1044) = 0; S32(0x1048) = 0; }
        else if (X87_C0(draw, (long double)RF64(0x15018))) { S32(0x1040) = 0; S32(0x1044) = 1; S32(0x1048) = 1; }
        else {
            S32(0x1040) = 0;
            if (X87_C0(draw, (long double)RF64(0x15020))) { S32(0x1044) = 1; S32(0x1048) = 0; }
            else { S32(0x1044) = 0; S32(0x1048) = 1; }
        }
    } else if (!P32(global, 0xbfc)) { S32(0x1040) = 0; S32(0x1044) = 0; S32(0x1048) = 0; S32(0x104c) = 0; }
    global = SIM1_GLOBAL;
    handle_step = P32(global, 0xbf8);
    P32(global, 0xbf8) = 0;
    global = SIM1_GLOBAL;
    if (!P32(global, 0xbe8)) {
        if (P8(global, 0x366) == 1 && !X87_C3((long double)SF64(0x16a8) * SF64(0x16a0) * SF64(0x1698), (long double)RF64(0x15030))) {
            S32(0x16b0) = 0; S32(0x16b4) = 0x3ff00000; S32(0x16b8) = 0; S32(0x16bc) = 0x3ff00000;
            S32(0x16c0) = 0; S32(0x16c4) = 0x3ff00000;
        } else {
            if (S32(0x1040)) { S32(0x16b0) = S32(0x1698); S32(0x16b4) = S32(0x169c); } else { S32(0x16b0) = 0; S32(0x16b4) = 0; }
            if (S32(0x1044)) { S32(0x16b8) = S32(0x16a0); S32(0x16bc) = S32(0x16a4); } else { S32(0x16b8) = 0; S32(0x16bc) = 0; }
            if (S32(0x1048)) { S32(0x16c0) = S32(0x16a8); S32(0x16c4) = S32(0x16ac); } else { S32(0x16c0) = 0; S32(0x16c4) = 0; }
        }
    } else {
        long double step = (long double)(int64_t)(uint64_t)handle_step * RF64(0x15028);
        if (S32(0x1040) == 1) sim1_leg_step(step, 0x1698, 0x16b0); else { S32(0x16b0) = 0; S32(0x16b4) = 0x3ff00000; }
        if (S32(0x1044) == 1) sim1_leg_step(step, 0x16a0, 0x16b8); else { S32(0x16b8) = 0; S32(0x16bc) = 0x3ff00000; }
        if (S32(0x1048) == 1) sim1_leg_step(step, 0x16a8, 0x16c0); else { S32(0x16c0) = 0; S32(0x16c4) = 0x3ff00000; }
    }
    {   /* the rate 1.0 kept opaque: inlined, GCC would drop the limiter's multiply by it, which
         * rounds at the game's 24-bit precision (Direct3D sets it) */
        double one = sim1_x87(1.0);
        SF64(0x1698) = (double)sim1_rate_limit(SF64(0x1698), SF64(0x16b0), SF64(0x4148), one);
        SF64(0x16a0) = (double)sim1_rate_limit(SF64(0x16a0), SF64(0x16b8), SF64(0x2d70), one);
        SF64(0x16a8) = (double)sim1_rate_limit(SF64(0x16a8), SF64(0x16c0), SF64(0x3c08), one);
    }
    P32(SIM1_GLOBAL, 0xbec) = (uint32_t)sim1_ftol((long double)SF64(0x1698) * SF64(0x6e8)) & 0xffff;
    P32(SIM1_GLOBAL, 0xbf0) = (uint32_t)sim1_ftol((long double)SF64(0x16a0) * SF64(0x6e8)) & 0xffff;
    P32(SIM1_GLOBAL, 0xbf4) = (uint32_t)sim1_ftol((long double)SF64(0x16a8) * SF64(0x6e8)) & 0xffff;
    sim1_leg_events(0x1698, 0x16b0, 0xbc, 0xc0, 5);
    sim1_leg_events(0x16a0, 0x16b8, 0xc4, 0xc8, 6);
    sim1_leg_events(0x16a8, 0x16c0, 0xcc, 0xd0, 7);
    if (S32(0x3584) != 1) return;
    if (S32(0x1624) == 2) horn = P32(SIM1_GLOBAL, 0xbe8) != 0x3fff;
    else if (X87_C0((long double)SF64(0x1698), (long double)RF64(0x15030)) || X87_C0((long double)SF64(0x16a0), (long double)RF64(0x15030))
             || X87_C0((long double)SF64(0x16a8), (long double)RF64(0x15030))) {
        if (!X87_C0C3((long double)SF64(0x1678), (long double)SF64(0x3f98))) horn = 1;
        else if (!X87_C0((long double)SF64(0x1678), (long double)SF64(0x3e68))) {
            int32_t closed = SI32(0x3e10);
            global = SIM1_GLOBAL;
            if ((int32_t)PI16(global, 0x88c) < closed) horn = 1;
            else if (P16(global, 0xaec) > 1) horn = (int32_t)PI16(global, 0x924) < closed;
        }
    }
    if (horn == 1) {
        if (!S32(0x1014)) SIM1_CALL(SIM1_SOUND, 0x24, uint32_t (__stdcall *)(uint32_t, uint32_t))(5, 1);
    } else if (S32(0x1014) == 1) SIM1_CALL(SIM1_SOUND, 0x24, uint32_t (__stdcall *)(uint32_t, uint32_t))(5, horn);
    S32(0x1014) = horn;
}

/* An engine-load term (for 0x22008bd4): GLOBAL's word a times the scale word +0xaf4 (its low
 * 32 bits), times word b / 256, shifted down 23. */
static uint32_t sim1_load_term(const uint8_t *global, uint32_t a, uint32_t b)
{
    uint32_t low = P32(global, a) * (uint32_t)P16(global, 0xaf4);
    return (uint32_t)(((uint64_t)low * (P32(global, b) >> 8)) >> 23);
}

/* 0x22008bd4 (edi GLOBAL): the air-relative motion: the wind (GLOBAL +0xe94.. against
 * +0x60c..) turned into the body axes and added to the velocities (+0x1388, +0x1390,
 * +0x1398); the true airspeed's square, value and ratio (+0x1448, +0x1450, +0x1458) and its
 * word (+0x1460, GLOBAL +0x11c6, at most 0xffff); the propeller's rpm words (GLOBAL +0x2b8,
 * +0x2bc, +0x2c4: through the efficiency curve and carburettor, or the fixed prop's limit) and
 * the power-on flag (+0x36d, SOUND 3); the angle of attack (+0x1518, +0x1490 with its sine and
 * cosine, and words) and sideslip (+0x151c, +0x1498, ...), the flight path and the elevator's
 * angle (+0x1510, +0x14a0, +0x1508); the turn and climb rates; the stall horn's word (GLOBAL
 * +0x11be) and flag (+0x36c, the trim hold, SOUND 8 on a change); dynamic pressure
 * (+0x15a8, +0x15b0), the pitch rate's word, the climb word; the load factor word (GLOBAL
 * +0x11d0); the engine-load sums (+0x15f8.., +0x1724, +0x1728); the drag sums and height;
 * and gravity turned into the body axes (+0x1288..). */
void sim1_air_motion(uint8_t *global)
{
    uint32_t speed, limit, rpm, power, flag, old, product, remainder;
    uint16_t word, alpha;
    long double value, angle;
    SF64(0x5fc) = (double)(sim1_fchs((long double)(int32_t)PI16(global, 0xe94) * SF64(0x720) * SF64(0x6c0)) + SF64(0x614));
    SF64(0x5f4) = (double)(sim1_fchs((long double)(int32_t)PI16(global, 0xe96) * SF64(0x720) * SF64(0x6c0)) + SF64(0x60c));
    SF64(0x604) = (double)(sim1_fchs((long double)(int32_t)PI16(global, 0xe98) * SF64(0x720) * SF64(0x6c0)) + SF64(0x61c));
    sim1_rotate_a(SF64(0x5fc), SF64(0x604), SF64(0x5f4), &SF64(0x62c), &SF64(0x634), &SF64(0x624));
    SF64(0x1388) = (double)((long double)SF64(0x624) + SF64(0x13b0));
    SF64(0x1390) = (double)((long double)SF64(0x62c) + SF64(0x13a0));
    SF64(0x1398) = (double)((long double)SF64(0x634) + SF64(0x13a8));
    value = (long double)SF64(0x1388) * SF64(0x1388) + (long double)SF64(0x1390) * SF64(0x1390) + (long double)SF64(0x1398) * SF64(0x1398);
    SF64(0x1448) = (double)value;
    value = sim1_fsqrt(value);
    SF64(0x1450) = (double)value;
    value /= SF64(0x1468);
    SF64(0x1458) = (double)value;
    speed = (uint32_t)(SI32(0x1718) = sim1_fistp32(value * sim1_f64(0x65e)));
    word = speed >= 0xffff ? 0xffff : (uint16_t)speed;
    S16(0x1460) = word;
    P16(global, 0x11c6) = word;
    PI32(global, 0x2b8) = sim1_fistp32((long double)SF64(0x1388) * SF64(0x748) * SF64(0x700));
    if (PI32(global, 0x2b8) < 0) P32(global, 0x2b8) = 0;
    SF64(0x1718) = (double)((long double)SF64(0x1388) * SF64(0x748));
    value = sim1_prop_efficiency(SF64(0x1718));
    SF64(0x1718) = (double)value;
    SF64(0x640) = (double)value;
    PI32(global, 0x2bc) = sim1_fistp32(sim1_carburettor(SF64(0x1718)) * SF64(0x700));
    if (P16(global, 0x612)) {
        SF64(0x1718) = (double)((long double)PI32(global, 0xb0c) * sim1_f64(0x666) * SF64(0x1468) * SF64(0x748));
        limit = (uint32_t)(SI32(0x1718) = sim1_fistp32(sim1_prop_efficiency(SF64(0x1718)) * SF64(0x700)));
        if (!((int32_t)limit < SI32(0x3d04))) limit = S32(0x3d04);
    } else {
        uint32_t root = sim1_isqrt64((int64_t)(uint64_t)S32(0x1700));
        uint64_t load = (uint64_t)S32(0x63c) * P32(global, 0xb0c);
        uint32_t cap = sim1_div32((uint32_t)(load >> 32), (uint32_t)load, 0x52524c, &remainder);
        limit = sim1_div32(S32(0x3d04) >> 24, S32(0x3d04) << 8, root, &remainder);
        if (!(limit < cap)) limit = cap;
    }
    P32(global, 0x2c4) = limit;
    rpm = P16(global, 0x612) ? P32(global, 0x2bc) : P32(global, 0x2b8);
    power = 0;
    if (!((int32_t)rpm < (int32_t)limit)) {
        power = 1;
        if (P16(global, 0x338) && P16(global, 0xc44)) S32(0x2510) += (uint32_t)P16(global, 0xc44) * S32(0x16fc);
    }
    SIM1_CALL(SIM1_SOUND, 0x24, uint32_t (__stdcall *)(uint32_t, uint32_t))(3, power);
    P8(global, 0x36d) = (uint8_t)power;
    SF64(0x1718) = (double)((long double)PI32(global, 0x2c4) * SF64(0x6c8));
    S16(0x1518) = (uint16_t)(S16(0x3900) - sim1_bearing_of(SF64(0x1388), SF64(0x1398)));
    angle = (long double)(int32_t)SI16(0x1518) * SF64(0x680);
    SF64(0x1490) = (double)angle;
    SF64(0x14a8) = (double)sim1_fcos(angle);
    SF64(0x14b0) = (double)sim1_fsin(angle);
    SI16(0x1524) = sim1_fistp16((long double)SF64(0x14a8) * SF64(0x6e0));
    SI16(0x1520) = sim1_fistp16((long double)SF64(0x14b0) * SF64(0x6e0));
    S16(0x151c) = sim1_bearing_of(SF64(0x1388), SF64(0x1390));
    angle = (long double)(int32_t)SI16(0x151c) * SF64(0x680);
    SF64(0x1498) = (double)angle;
    SF64(0x14b8) = (double)sim1_fcos(angle);
    SF64(0x14c0) = (double)sim1_fsin(angle);
    SI16(0x64a) = sim1_fistp16((long double)SF64(0x14b8) * SF64(0x6e0));
    SI16(0x648) = sim1_fistp16((long double)SF64(0x14c0) * SF64(0x6e0));
    word = (uint16_t)(SI32(0x1718) = sim1_fistp32(sim1_fchs(SF64(0x1398)) / SF64(0x1450) * SF64(0x6e0)));
    word = (uint16_t)SIM1_CALL(SIM1_UTIL, 0x38, uint32_t (__stdcall *)(int32_t, int32_t))(word, 0);
    S16(0x1718) = word;
    SF64(0x1510) = (double)((long double)(int32_t)(int16_t)word * SF64(0x680));
    SF64(0x1718) = (double)((long double)(int32_t)SI16(0x4150) * SF64(0x13f8) + SF64(0x1390));
    S16(0x1718) = sim1_bearing_of(SF64(0x1388), SF64(0x1718));
    angle = (long double)(int32_t)SI16(0x1718) * SF64(0x680);
    SF64(0x14a0) = (double)angle;
    SF64(0x1508) = (double)sim1_fcos(angle);
    sim1_turn_climb_rates();
    alpha = S16(0x1518);
    if (!((SI32(0x1718) = sim1_fistp32((long double)SF64(0x1388) * SF64(0x728))) > 0xc)) alpha = 0xc001;
    word = 0;
    if (!(alpha & 0x8000)) {
        uint32_t wide = 0x5333u * alpha;
        uint16_t rest;
        word = sim1_div16((uint16_t)(wide >> 16), (uint16_t)wide, S16(0x3802), &rest);
        if (word > 0x7fff) word = 0x7fff;
    }
    P16(global, 0x11be) = (uint16_t)(0x7fff - word);
    flag = 0;
    if (P32(global, 0x604) != 1) {
        alpha = S16(0x1518);
        if (!((SI32(0x1718) = sim1_fistp32((long double)SF64(0x1388) * SF64(0x728))) < 0xc) && !((int16_t)alpha < (int16_t)S16(0x3802))) flag = 1;
    }
    if (S32(0x423c)) sim1_trim_hold(global, flag, 0);
    old = P8(global, 0x36c);
    P8(global, 0x36c) = (uint8_t)flag;
    if (flag != old && P32(global, 0x77c)) SIM1_CALL(SIM1_SOUND, 0x24, uint32_t (__stdcall *)(uint32_t, uint32_t))(8, flag);
    SF64(0x15a8) = (double)((long double)SF64(0x798) * SF64(0x1448) * SF64(0x768));
    SF64(0x15b0) = (double)((long double)SF64(0x798) * SF64(0x1398) * SF64(0x1398) * SF64(0x768));
    SI16(0x1718) = sim1_fistp16((long double)SF64(0x13f8) * SF64(0x678));
    P16(global, 0x37c) = S16(0x1718);
    PI32(global, 0x2c8) = SI32(0x1718) = sim1_fistp32((long double)SF64(0x13c0) * SF64(0x728) * SF64(0x6f8));
    {
        uint32_t base = (uint32_t)(int32_t)PI16(global, 0xe8c) + S32(0x590), squared = (uint32_t)P16(global, 0x11c6) * P16(global, 0x11c6);
        uint32_t factor = (uint32_t)(((uint64_t)squared * 0x20c4a) >> 32) + 0x10000;
        P32(global, 0x11d0) = (uint32_t)(((uint64_t)factor * base) >> 16) - S32(0x590);
    }
    S32(0x15f8) = sim1_load_term(global, 0xb78, 0xb74);
    S32(0x15fc) = sim1_load_term(global, 0xb98, 0xb94);
    S32(0x1600) = sim1_load_term(global, 0xb80, 0xb7c);
    S32(0x1604) = sim1_load_term(global, 0xba0, 0xb9c);
    S32(0x1608) = sim1_load_term(global, 0xb88, 0xb84);
    product = S32(0x30e4) + S32(0x15f8) + S32(0x15fc) + S32(0x1600) + S32(0x1604) + S32(0x1608);
    product += (uint32_t)(SI32(0x1718) = sim1_fistp32(SF64(0x1610)));
    S32(0x1724) = product;
    sim1_drag_sums();
    S32(0x1728) = (uint32_t)(((uint64_t)S32(0x1724) * 0x7f5) >> 16);
    sim1_height(global);
    SF64(0x1718) = (double)sim1_fchs((long double)SI32(0x1724));
    sim1_rotate_a(sim1_f64(0x66e), SF64(0x1718), sim1_f64(0x66e), &SF64(0x1288), &SF64(0x1290), &SF64(0x1298));
}

/* 0x22011f50: the helicopter's rotor and tail rotor terms: the main rotor's thrust (by the
 * collective +0x4390 and the rotor speed +0x1af0) and its tilt (+0x1ae0) into the forces
 * +0x19c8.. +0x19e8; the torque's yaw term +0x12b8 (turning, the rotor above +0x15398); the
 * tail rotor's thrust (+0x3b80, the angle +0x2c70 less the pedals +0x1ad8) into +0x19f8.. +0x1a20
 * and its moments; the fuselage's damping by the tables at +0xc08.. +0xd50 (by +0x1ac0) into
 * +0x1a28.. +0x1a50. Retail parks most terms as doubles in its frame and keeps some unrounded
 * on the x87 (the angles to fsin and fcos among them); the C does the same, the locals named
 * by frame offset. */
void sim1_rotor_terms(void)
{
    long double lift, base, rotor_sin, rotor_cos, theta, tail, tail_lift, tail_base, theta_cos, theta_sin;
    double l8_lift, l18_base, l0_span, l38_sin, l20_drag, l8_cos, l10_moment, l20_side, l10_cos, l18_sin, l28_pedal_sin, l30_pedal_cos;
    double l20_tail, l0_tail_span, l0_theta_cos, l20_tail_moment, l18_tail_base, l10_theta_sin;
    double l18_table, l20_fin, l8_density, l38_damp, l30_roll, l0_roll2, l38_yaw, l0_yaw2, l0_cross;
    long double k = (long double)SF64(0x4390) * RF64(0x15380) / ((long double)SF64(0x4390) - RF64(0x15388));
    lift = (k * ((long double)SF64(0x2c70) + SF64(0x1af0))) * SF64(0x3fe0) + SF64(0x2b80);
    l8_lift = (double)lift;
    base = fabsl(lift + SF64(0x2b80)) / ((long double)SF64(0x4390) * RF64(0x15390));
    l18_base = (double)base;
    l0_span = (double)((long double)SF64(0x4170) * SF64(0x1af8));
    rotor_sin = sim1_fsin(SF64(0x1ae0));
    l38_sin = (double)rotor_sin;
    l20_drag = (double)((base + SF64(0x2c90)) * l0_span);
    rotor_cos = sim1_fcos(SF64(0x1ae0));
    l8_cos = (double)rotor_cos;
    l10_moment = (double)((long double)l8_lift * l0_span);
    l20_side = (double)(rotor_sin * l20_drag);
    l10_cos = (double)(rotor_cos * l10_moment);
    l18_sin = (double)(rotor_sin * l10_moment);
    l28_pedal_sin = (double)sim1_fsin(SF64(0x1ad8));
    l30_pedal_cos = (double)sim1_fcos(SF64(0x1ad8));
    SF64(0x19c8) = (double)sim1_fchs((((long double)l18_base + SF64(0x3b48)) * l8_cos) * l0_span - l18_sin);
    SF64(0x19d0) = RF64(0x15398);
    SF64(0x19d8) = (double)sim1_fchs((long double)l20_side + l10_cos);
    SF64(0x19e8) = (double)(((((long double)l8_cos * l20_drag) - l18_sin) * SF64(0x738)) * SF64(0x3d08)
                            + ((sim1_fchs(rotor_sin * l20_drag) - l10_cos) * SF64(0x738)) * SF64(0x3d08));
    SF64(0x19e0) = RF64(0x15398);
    SF64(0x19f0) = RF64(0x15398);
    if (!X87_C0C3((long double)SF64(0x1af0), (long double)RF64(0x15398)) && !X87_C0((long double)SF64(0x1ac0), (long double)RF64(0x15398)))
        SF64(0x12b8) = (double)((((sim1_fsin(SF64(0x1af0)) * ((long double)SF64(0xe40) / SF64(0x2c08))) * SF64(0x36c8)) * SF64(0x1af8)) * RF64(0x153a0));
    else { S32(0x12b8) = 0; S32(0x12bc) = 0; }
    k = (long double)SF64(0x3b80) * RF64(0x15380) / ((long double)SF64(0x3b80) - RF64(0x15388));
    theta = (long double)SF64(0x2c70) - SF64(0x1ad8);
    tail = k * theta;
    l20_tail = (double)tail;
    tail_lift = tail + SF64(0x3808);
    l20_tail = (double)tail_lift;
    l0_tail_span = (double)((long double)SF64(0x36b8) * SF64(0x1af8));
    theta_cos = sim1_fcos(theta);
    l0_theta_cos = (double)theta_cos;
    l20_tail_moment = (double)(((long double)SF64(0x36b8) * SF64(0x1af8)) * l20_tail);
    tail_base = fabsl(tail_lift) / (((long double)SF64(0x3fc8) * SF64(0x3b80)) * RF64(0x15390));
    l18_tail_base = (double)((tail_base + SF64(0x3b48)) * l0_tail_span);
    theta_sin = sim1_fsin(theta);
    l10_theta_sin = (double)theta_sin;
    SF64(0x19f8) = (double)((((long double)SF64(0x36b8) * SF64(0x1af8)) * l20_tail) * l0_theta_cos + ((tail_base + SF64(0x3b48)) * l0_tail_span) * l10_theta_sin);
    SF64(0x1a00) = (double)sim1_fchs((long double)l18_tail_base * l0_theta_cos - (long double)l20_tail_moment * l10_theta_sin);
    SF64(0x1a08) = RF64(0x15398);
    SF64(0x1a18) = RF64(0x15398);
    SF64(0x1a20) = (double)sim1_fchs(((long double)SF64(0x738) * SF64(0x2c80)) * SF64(0x19f8));
    SF64(0x1a10) = (double)(((long double)SF64(0x738) * SF64(0x2ac0)) * SF64(0x19f8));
    l18_table = (double)sim1_table_lookup(sim1_data + 0xc08, SF64(0x1ac0));
    l20_fin = (double)((((long double)l28_pedal_sin * SF64(0x3cb8)) * SF64(0x3fb8)) * SF64(0x1af8));
    l8_density = (double)((long double)SF64(0x3d48) / SF64(0x1ab8));
    l38_damp = (double)((long double)l38_sin * SF64(0x3730));
    l30_roll = (double)(((long double)SF64(0x13e8) * SF64(0x738)) * SF64(0x3e08));
    SF64(0x1a40) = (double)((sim1_fchs(((((long double)l18_table + SF64(0x30a8)) * l30_pedal_cos) * l8_cos) * SF64(0x3e28) * SF64(0x1af8)) - fabsl((long double)l20_fin))
                            - ((long double)SF64(0x3e18) * SF64(0x1af8)) * l38_damp);
    SF64(0x1a48) = (double)(sim1_fchs(l20_fin) - ((((long double)l28_pedal_sin * l8_cos) * SF64(0x30a8)) * SF64(0x3e28)) * SF64(0x1af8));
    SF64(0x1a50) = (double)(sim1_fchs(((long double)SF64(0x3730) * SF64(0x1af8)) * l38_damp) - ((((long double)l30_pedal_cos * l38_sin) * SF64(0x30a8)) * SF64(0x3e28)) * SF64(0x1af8));
    l0_roll2 = (double)((long double)l30_roll * l30_roll);
    {
        long double table = sim1_table_lookup(sim1_data + 0xce0, SF64(0x1ac0));
        l38_yaw = (double)(((long double)SF64(0x13f8) * SF64(0x738)) * SF64(0x3de8));
        SF64(0x1a28) = (double)((((((table * l8_density) * l0_roll2) * SF64(0x798)) * SF64(0x13e8)) * SF64(0x768)) * SF64(0x36c8));
        l0_yaw2 = (double)((long double)l38_yaw * l38_yaw);
        SF64(0x1a28) = (double)((long double)SF64(0x1a28) - ((((long double)l8_density * SF64(0x13e8)) * SF64(0x36c8)) * SF64(0x1af8)) * RF64(0x153a8));
    }
    SF64(0x1a30) = (double)((((((sim1_table_lookup(sim1_data + 0xd18, SF64(0x1ac0)) * l8_density) * l0_yaw2) * SF64(0x798)) * SF64(0x13f8)) * SF64(0x768)) * SF64(0x36b8));
    SF64(0x1a30) = (double)((((sim1_table_lookup(sim1_data + 0xd50, SF64(0x1ac0)) * l8_density) * SF64(0x13f8)) * SF64(0x36b8)) * SF64(0x1af8));
    l0_cross = (double)((((long double)SF64(0x13f0) * SF64(0x738)) * SF64(0x3e08)) * (((long double)SF64(0x13f8) * SF64(0x738)) * SF64(0x3e08)));
    SF64(0x1a38) = (double)((((((sim1_table_lookup(sim1_data + 0xc70, SF64(0x1ac0)) * l8_density) * l0_cross) * SF64(0x798)) * SF64(0x13f0)) * SF64(0x768)) * SF64(0x36c8));
    SF64(0x1a38) = (double)((((sim1_table_lookup(sim1_data + 0xca8, SF64(0x1ac0)) * l8_density) * SF64(0x13f0)) * SF64(0x36c8)) * SF64(0x1af8));
    (void)l20_tail;
}

/* 0x2200ac8a (edi GLOBAL): the ground systems: the fuel rate +0x16fc to 0x1555, the gear,
 * flaps and spoilers, the height (through the caller's edi, which they keep). */
void sim1_ground_systems(uint8_t *global)
{
    S32(0x16fc) = 0x1555;
    sim1_landing_gear();
    sim1_flaps();
    sim1_spoilers();
    sim1_height(global);
}

/* 0x22001320 (edi GLOBAL): the controls' pass: the autopilot; the elevator, aileron and rudder
 * controls (the elevator's input leaves edi GLOBAL, so the rest get it); the surfaces, force
 * feedback, spoilers, flaps, gear and the brakes' hold check; then (0x22002260, init's tail)
 * GLOBAL +0x11c0, +0x11c2, +0x11c4 into +0xbb8, +0xbb4, +0xbbc. Returns retail's edi. */
uint8_t *sim1_controls_pass(uint8_t *global)
{
    sim1_autopilot();
    sim1_elevator_control(global);
    global = SIM1_GLOBAL;
    sim1_aileron_control(global);
    sim1_rudder_control(global);
    sim1_surfaces();
    sim1_force_feedback(global);
    sim1_spoilers();
    sim1_flaps();
    sim1_landing_gear();
    sim1_hold_check();
    P16(SIM1_GLOBAL, 0xbb8) = P16(SIM1_GLOBAL, 0x11c0);
    P16(SIM1_GLOBAL, 0xbb4) = P16(SIM1_GLOBAL, 0x11c2);
    P16(SIM1_GLOBAL, 0xbbc) = P16(SIM1_GLOBAL, 0x11c4);
    return global;
}

/* A 16-bit spool rate clamped to +-limit (for 0x2200769d): the signed product's high word. */
static uint16_t sim1_spool_step(uint16_t value, uint16_t gain, int16_t low, int16_t high)
{
    int16_t step = (int16_t)(((int32_t)(int16_t)value * (int16_t)gain) >> 16);
    if (!(step > low)) return (uint16_t)low;
    if (!(step < high)) return (uint16_t)high;
    return (uint16_t)step;
}

/* The high word of a 16x16 unsigned product (mul WORD). */
#define MUL16_HIGH(a, b) ((uint16_t)(((uint32_t)(uint16_t)(a) * (uint16_t)(b)) >> 16))

/* One thrust term of 0x2200769d: the pressure ratio from the spool word through the inlet
 * (0x8440000 / +0x476, times +0x3d50 twice), less the bleed 0x844, its root times 0x1d, less
 * the temperature's share (0xdf4 times +0x468). */
static uint32_t sim1_thrust_pressure(uint16_t spool)
{
    uint32_t remainder, value = sim1_div32(0, 0x8440000, S16(0x476), &remainder);
    uint64_t product;
    value = (value * S32(0x3d50)) << 1;
    product = (uint64_t)value * spool;
    value = sim1_div32((uint32_t)(product >> 48), (uint32_t)(product >> 16), 0x2800, &remainder);
    value = sim1_isqrt(value - 0x844) * 0x1d;
    return value - (uint32_t)(((uint64_t)0xdf4 * S16(0x468)) >> 16);
}

/* The other thrust term of 0x2200769d: the jet's velocity from the spool word (times +0x478,
 * +0x3d50 doubled, 0xcccd, less one, times 0x1b3384), rooted, less the temperature's share,
 * at least 0; times the flow word, 6 / 0xc1 and +0x490. */
static uint16_t sim1_thrust_velocity(uint16_t spool, uint16_t flow)
{
    uint32_t value, remainder;
    uint64_t product = (uint64_t)spool * S32(0x478);
    value = (uint32_t)(product >> 4);
    product = (uint64_t)value * (S32(0x3d50) << 1);
    value = (uint32_t)(product >> 16);
    value = (uint32_t)(((uint64_t)value * 0xcccd) >> 25) - 0x10000;
    value = sim1_isqrt((uint32_t)(((uint64_t)value * 0x1b3384) >> 16));
    value -= (uint32_t)(((uint64_t)0xdf4 * S16(0x468)) >> 16);
    if ((int32_t)value < 0) value = 0;
    product = (uint64_t)(flow * value) * 6;
    value = sim1_div32((uint32_t)(product >> 32), (uint32_t)product, 0xc1, &remainder);
    return (uint16_t)(((uint64_t)value * S32(0x490)) >> 16);
}

/* 0x2200769d (ebx the engine record, edi GLOBAL): a jet engine's pass: running (+8) when the
 * starter (+6) or running with N2 (+4) past 0x2000, unless high on the ground with GLOBAL
 * +0x33e or starved (+0x94, GLOBAL +0x33c); the fuel flow from the throttle (+0, its reverse
 * scaled) into the N1 target and flow (+0xe, +0x10, the fuel word +0x450); N1 (+0xa) and N2
 * (+0xc) spooling at clamped rates through the tables at +0x4260, +0x3ba0, +0x3640, +0x4060,
 * +0x35c0, +0x4000; the starter off at +0x3540; EGT (+0x32), thrust (+0x1c, both spools' terms,
 * negated in reverse), the oil pressure and temperature lags (+0x2c, +0x2e, out of fuel ends
 * running), the fuel left (+0x44, GLOBAL +0x364 bit 2 draining), the gauges' words (+0x49c..)
 * and the temperatures (+0x4c, +0x48). 16-bit sums saturate at 0x7fff/0x8001 as retail's jno
 * does where retail tests it. */
void sim1_jet_engine(uint8_t *global, uint8_t *engine)
{
    uint16_t ax, dx, cx;
    uint32_t eax, ecx, remainder;
    uint64_t product;
    int32_t signed_value;
    P16(engine, 0x3c) = 0xffff;
    ax = 0;
    if (!(P16(global, 0x33e) && P32(global, 0x574) > S32(0x3fa0)) && !(!P32(engine, 0x94) && P16(global, 0x33c))
        && P16(engine, 4) >= 0x2000 && (P16(engine, 6) == 1 || P16(engine, 8) == 1)) ax = 1;
    P16(engine, 8) = ax;
    signed_value = (int32_t)PI16(engine, 0);
    eax = (uint32_t)signed_value;
    if (signed_value < 0) eax = ((uint32_t)-signed_value * S32(0x2d64)) >> 13;
    ax = sim1_sub16_saturated((uint16_t)(S16(0x3622) + MUL16_HIGH(eax, S16(0x43e4))), P16(engine, 0xa));
    dx = sim1_spool_step(ax, S16(0x3df0), (int16_t)0xe890, 0x1770);
    ax = sim1_div16((uint16_t)(P16(engine, 0xe) >> 8), (uint16_t)(P16(engine, 0xe) << 8), S16(0x2d7a), &cx);
    ax = sim1_add16_saturated(ax, dx);
    if ((int16_t)ax < 0) ax = 0;
    if (P16(engine, 8) != 1) ax = 0;
    S16(0x450) = ax;
    eax = (uint32_t)ax * S16(0x2d7a);
    ax = (uint16_t)(eax >> 8);
    if ((uint16_t)(eax >> 24)) ax = 0xffff;
    P16(engine, 0xe) = ax;
    product = (uint64_t)ax * S32(0x16cc);
    eax = (uint32_t)(product >> 26);
    P32(engine, 0x10) = (uint32_t)(((uint64_t)S32(0x4104) * eax) >> 8);
    ecx = (uint32_t)(((uint64_t)((int64_t)SI32(0x490) * SI32(0x48c))) >> 16);
    eax = sim1_div32(S16(0x450), 0, ecx, &remainder);
    dx = (uint16_t)(eax >> 16);
    if (dx >= 0x8000) dx = 0x7fff;
    P16(engine, 0x14) = dx;
    S16(0x452) = dx;
    ax = sim1_table16(sim1_data + 0x4260, (uint16_t)(dx << 1));
    S16(0x44e) = ax;
    ax = (uint16_t)((uint32_t)(int32_t)(int16_t)ax * S32(0x48c) >> 16);
    if (ax >= 0x8000) ax = 0x7fff;
    ax = sim1_sub16_saturated(ax, P16(engine, 0xa));
    if (P16(engine, 6) == 1) ax = sim1_add16_saturated(ax, S16(0x2c68));
    dx = sim1_spool_step(ax, S16(0x3e02), (int16_t)0xe890, 0x1770);
    ax = sim1_add16_saturated(P16(engine, 0xa), dx);
    if ((int16_t)ax < 0) ax = 0;
    P16(engine, 0xa) = ax;
    S16(0x44c) = ax;
    if (P16(engine, 6) == 1 && !((int16_t)P16(engine, 0xa) < (int16_t)S16(0x3540))) P16(engine, 6) = 0;
    dx = MUL16_HIGH(sim1_table16(sim1_data + 0x3ba0, (uint16_t)(S16(0x44e) << 1)), 0x280);
    S16(0x454) = dx;
    product = (uint64_t)(uint32_t)(int32_t)(int16_t)dx * S32(0x490);
    ax = (uint16_t)sim1_idiv32((uint32_t)(product >> 32), (uint32_t)product, S32(0x48c), &remainder);
    if (ax >= 0x8000) ax = 0x7fff;
    S16(0x456) = ax;
    dx = MUL16_HIGH(S16(0x454), S16(0x4154));
    S16(0x494) = dx;
    S16(0x496) = (uint16_t)(S16(0x454) - dx);
    ax = sim1_table16(sim1_data + 0x3640, (uint16_t)(S16(0x44e) << 1));
    P16(engine, 0x30) = ax;
    S16(0x458) = ax;
    ax = P16(engine, 8) ? S16(0x44e) : 0;
    S16(0x460) = sim1_table16(sim1_data + 0x4060, (uint16_t)(ax << 1));
    S16(0x45a) = sim1_table16(sim1_data + 0x35c0, (uint16_t)(S16(0x460) << 1));
    ax = (uint16_t)(((uint32_t)S16(0x460) * S32(0x48c)) >> 16);
    S16(0x45e) = ax;
    dx = sim1_spool_step((uint16_t)(ax - P16(engine, 0xc)), S16(0x3a50), (int16_t)0xfa24, 0x5dc);
    ax = (uint16_t)(P16(engine, 0xc) + dx);
    P16(engine, 0xc) = ax;
    S16(0x45c) = ax;
    ax = 0;
    if (P16(engine, 8)) {
        ax = sim1_table16(sim1_data + 0x4000, P16(engine, 0x30));
        S16(0x464) = ax;
        ax = (uint16_t)((uint32_t)(int32_t)(int16_t)ax * S32(0x488) >> 16);
        S16(0x466) = ax;
        cx = sim1_div16(0x111, 0, S16(0x3582), &dx);
        ax = (uint16_t)(S16(0x466) - cx);
        if (!((int16_t)ax > 0)) ax = 0;
    }
    eax = (uint32_t)ax * S16(0x30e2);
    dx = (uint16_t)(eax >> 16);
    ax = (uint16_t)(eax >> 8);
    if (dx > 0x7f || (dx == 0x7f && ax > 0x7fff)) ax = 0x7fff;
    P16(engine, 0x32) = ax;
    ecx = sim1_thrust_pressure(P16(engine, 0x30));
    if ((int32_t)ecx < 0) ecx = 0;
    S16(0x46e) = (uint16_t)ecx;
    S16(0x498) = sim1_thrust_velocity(P16(engine, 0x30), S16(0x494));
    S16(0x46c) = (uint16_t)sim1_thrust_pressure(S16(0x45a));
    S16(0x49a) = sim1_thrust_velocity(S16(0x45a), S16(0x496));
    eax = (uint32_t)(((uint64_t)((uint32_t)S16(0x49a) + S16(0x498)) * S32(0x4104)) >> 8);
    if (PI16(engine, 0) < 0) eax = 0u - eax;
    P32(engine, 0x1c) = eax;
    ax = P16(engine, 0xa);
    if (!((int16_t)ax > 0)) ax = 0;
    S16(0x16d0) = MUL16_HIGH(ax, S16(0x439a));
    ax = S16(0x16d0);
    if (P16(engine, 8)) {
        int32_t sum = (int16_t)ax + (int16_t)S16(0x2d68);
        ax = (sum > 0x7fff || sum < -0x8000) ? 0x7fff : (uint16_t)sum;
    }
    eax = (uint32_t)ax * S16(0x3fb0);
    ax = (eax >> 16) || (uint16_t)eax > 0x7fff ? 0x7fff : (uint16_t)eax;
    S16(0x442) = ax;
    if (P8(global, 0x364) & 1) { ax = (uint16_t)(S16(0x44a) + 0x64); if (ax > 0x6000) ax = 0x6000; S16(0x44a) = ax; }
    else S16(0x44a) = 0;
    ax = (uint16_t)(S16(0x442) + S16(0x44a));
    if (ax > 0x7fff) ax = 0x7fff;
    P16(engine, 0x2c) = (uint16_t)(P16(engine, 0x2c) + (uint16_t)(((int32_t)(int16_t)(uint16_t)(ax - P16(engine, 0x2c)) * 0x40) >> 16));
    ax = S16(0x16d0);
    if (P16(engine, 8)) ax = sim1_add16_saturated(ax, S16(0x3728));
    eax = (uint32_t)ax * S16(0x4050);
    ax = (eax >> 16) || ((uint16_t)eax & 0x8000) ? 0x7fff : (uint16_t)eax;
    S16(0x444) = ax;
    if (P8(global, 0x364) & 2) S16(0x446) = sim1_sub16_saturated(S16(0x446), S16(0x448));
    else S16(0x446) = 0;
    ax = sim1_add16_saturated(S16(0x444), S16(0x446));
    if ((int16_t)ax < 0) ax = 0;
    P16(engine, 0x2e) = (uint16_t)(P16(engine, 0x2e) + (uint16_t)(((int32_t)(int16_t)(uint16_t)(ax - P16(engine, 0x2e)) * 0x1400) >> 16));
    if (!P16(engine, 0x2e)) P16(engine, 8) = 0;
    ecx = 0x3fff;
    if (P8(global, 0x364) & 2) {
        eax = ((uint32_t)S16(0x448) * S32(0x16fc)) >> 15;
        ecx = P32(engine, 0x44) - eax;
        if (!((int32_t)ecx > 0)) ecx = 0;
    }
    P32(engine, 0x44) = ecx;
    S16(0x4a4) = S16(0x456);
    S16(0x49c) = MUL16_HIGH(S16(0x44c), 0x190);
    S16(0x49e) = MUL16_HIGH(S16(0x45c), 0x190);
    dx = (uint16_t)(MUL16_HIGH(S16(0x462), 0xfa0) - 0x111);
    if (!((int16_t)dx > 0)) dx = 0;
    S16(0x4a2) = dx;
    S16(0x4a0) = MUL16_HIGH(S16(0x450), 0x1d10);
    P32(engine, 0x50) = 0x3fff;
    eax = (uint32_t)(((uint64_t)P16(engine, 0xc) * sim1_u32(0x4a6)) >> 16) + sim1_u32(0x4aa) - P32(engine, 0x4c);
    eax = (uint32_t)((uint64_t)((int64_t)(int32_t)eax * (int32_t)sim1_u32(0x4ae)) >> 16);
    eax = (uint32_t)((uint64_t)((int64_t)(int32_t)eax * SI32(0x16fc)) >> 15);
    P32(engine, 0x4c) += eax;
    P32(engine, 0x48) = (uint32_t)(((uint64_t)P16(engine, 0xa) * sim1_u32(0x4b2)) >> 16) + sim1_u32(0x4b6);
}

/* 0x22007530 (edi GLOBAL): the jets' atmosphere and engines: the airspeed word (+0x1460, its
 * part past 0x6aaa quartered) into +0x440 and +0x468 and the inlet tables at +0x4de and +0x4bc;
 * the pressure factor +0x47c; the density (+0x488) by the temperature GLOBAL +0xe8c and its
 * root (+0x48c); the velocity factor (+0x478), and the pressure GLOBAL +0xe8e into +0x490 and
 * +0x480, the temperature into +0x484; then each fitted engine (0x2200769d). (Retail leaves
 * +0x374a / 4 in ax; no caller reads it.) */
void sim1_jets(uint8_t *global)
{
    static const uint16_t engines[4] = {0x88c, 0x924, 0x9bc, 0xa54};
    uint16_t ax = S16(0x1460);
    uint32_t eax, remainder, at;
    int64_t wide;
    if (ax > 0x6aaa) ax = (uint16_t)(((int16_t)(uint16_t)(ax - 0x6aaa) >> 2) + 0x6aaa);
    S16(0x440) = ax;
    S16(0x468) = ax;
    S16(0x474) = sim1_table16(sim1_data + 0x4de, S16(0x468));
    S16(0x476) = sim1_table16(sim1_data + 0x4bc, S16(0x468));
    eax = (uint32_t)S16(0x440) * S16(0x440);
    S32(0x47c) = (uint32_t)(((uint64_t)eax * 0x20c4a) >> 32) + 0x10000;
    wide = (int64_t)((int32_t)PI16(global, 0xe8c) + 0x11126) * SI32(0x47c);
    S32(0x488) = sim1_idiv32((uint32_t)((uint64_t)wide >> 32), (uint32_t)wide, 0x12026, &remainder);
    S32(0x48c) = sim1_isqrt(S32(0x488)) << 8;
    eax = sim1_isqrt(S32(0x47c)) << 8;
    for (at = 0; at < 3; ++at) eax = (uint32_t)(((uint64_t)eax * S32(0x47c)) >> 16);
    S32(0x478) = eax;
    {
        uint64_t product = (uint64_t)eax * P16(global, 0xe8e);
        S32(0x490) = sim1_idiv32((uint32_t)(product >> 32), (uint32_t)product, 0x3f5b, &remainder);
    }
    eax = (uint32_t)P16(global, 0xe8e) << 16;
    S32(0x480) = sim1_idiv32((int32_t)eax < 0 ? 0xffffffffu : 0, eax, 0x3f5b, &remainder);
    wide = (int64_t)((int32_t)PI16(global, 0xe8c) + 0x11126) * 0x10000;
    S32(0x484) = sim1_idiv32((uint32_t)((uint64_t)wide >> 32), (uint32_t)wide, 0x12026, &remainder);
    sim1_jet_engine(global, global + engines[0]);
    for (at = 1; at < 4; ++at) {
        if (P16(global, 0xaec) < at + 1) break;
        sim1_jet_engine(global, global + engines[at]);
    }
}

/* 0x2200ac4a (edi GLOBAL): on the ground (GLOBAL +0x270), the forward speed from the rpm
 * word (+0x2b8) into +0x1388 and the ground systems (0x2200ac8a); else the turbulence every
 * sixth pass (+0x8e0 counting down). */
void sim1_ground_or_turbulence(uint8_t *global)
{
    if (P32(global, 0x270)) {
        SF64(0x1388) = (double)((long double)PI32(global, 0x2b8) * SF64(0x6c8) * SF64(0x740));
        sim1_ground_systems(global);
        return;
    }
    if (--S32(0x8e0)) return;
    S32(0x8e0) = 6;
    sim1_turbulence(global);
}

/* A clamp of a cosine ratio to [-1, 1] as 0x220039d0 does it, on the double it parks. */
static double sim1_unit_clamp(double value)
{
    if (!X87_C0C3((long double)value, (long double)RF64(0x150c8))) value = 1.0;
    if (X87_C0((long double)value, (long double)RF64(0x150d0))) value = -1.0;
    return value;
}

/* An angle from its cosine (for 0x220039d0): 0 where the numerator is 0, else _CIacos of the
 * clamped ratio, negated where the sign term is negative. */
static void sim1_angle_from_cosine(uint32_t offset, double numerator, double sign, double ratio)
{
    if (X87_C3((long double)numerator, (long double)RF64(0x150c0))) { S32(offset) = 0; S32(offset + 4) = 0; return; }
    if (X87_C0((long double)sign, (long double)RF64(0x150c0))) SF64(offset) = (double)sim1_fchs(sim1_ciacos(ratio));
    else SF64(offset) = (double)sim1_ciacos(ratio);
}

/* 0x220039d0: the attitude's step: the quaternion's rates (+0x1070.., from the body rates
 * +0x13e8..), a trapezoid step of the quaternion (+0x1050..) normalised; its direction
 * cosines (parked as doubles in retail's frame) turning the body velocities into the earth's
 * (+0x13b8, +0x13c0, +0x13c8); pitch (+0x1090, by _CIasin within its limits), bank and
 * heading (+0x1098, +0x10a0, by _CIacos) and the pitch's cosine (+0x10a8); the angles negated
 * into +0x1538.. and as 16.16 words into +0x1550.. and GLOBAL +0x578... */
void sim1_attitude_step(void)
{
    long double norm, x;
    double old_70 = SF64(0x1070), old_78 = SF64(0x1078), old_80 = SF64(0x1080), old_88 = SF64(0x1088);
    double l1c_first, l24, l2c, l44, l54, l3c, l4c, l1c, l34, l4, l14, l5c, l64, lc_sine, l6c, l24_cos, l14_w, l4_u, l1c_v, l4_ratio, lc_ratio;
    int32_t word;
    SF64(0x1070) = (double)sim1_fchs(((long double)SF64(0x1068) * SF64(0x13f8) - ((long double)SF64(0x1060) * SF64(0x13e8) + (long double)SF64(0x1058) * SF64(0x13f0))) * SF64(0x768));
    SF64(0x1078) = (double)((((long double)SF64(0x1060) * SF64(0x13f8) - (long double)SF64(0x1050) * SF64(0x13f0)) + (long double)SF64(0x1068) * SF64(0x13e8)) * SF64(0x768));
    SF64(0x1080) = (double)(((sim1_fchs((long double)SF64(0x1050) * SF64(0x13e8)) - (long double)SF64(0x1068) * SF64(0x13f0)) - (long double)SF64(0x1058) * SF64(0x13f8)) * SF64(0x768));
    SF64(0x1088) = (double)((((long double)SF64(0x1050) * SF64(0x13f8) - (long double)SF64(0x1058) * SF64(0x13e8)) + (long double)SF64(0x1060) * SF64(0x13f0)) * SF64(0x768));
    SF64(0x1050) = (double)((((long double)old_70 + SF64(0x1070)) * SF64(0x16f0)) * SF64(0x768) + SF64(0x1050));
    SF64(0x1058) = (double)((((long double)old_78 + SF64(0x1078)) * SF64(0x16f0)) * SF64(0x768) + SF64(0x1058));
    SF64(0x1060) = (double)((((long double)old_80 + SF64(0x1080)) * SF64(0x16f0)) * SF64(0x768) + SF64(0x1060));
    SF64(0x1068) = (double)((((long double)old_88 + SF64(0x1088)) * SF64(0x16f0)) * SF64(0x768) + SF64(0x1068));
    norm = (long double)RF64(0x150c8) / sim1_fsqrt((long double)SF64(0x1050) * SF64(0x1050) + (long double)SF64(0x1058) * SF64(0x1058)
                                                    + (long double)SF64(0x1060) * SF64(0x1060) + (long double)SF64(0x1068) * SF64(0x1068));
    {
        double scale = (double)norm, q50 = SF64(0x1050), q60 = SF64(0x1060), q68 = SF64(0x1068);
        x = norm * q50;
        SF64(0x1050) = (double)x;
        SF64(0x1058) = (double)((long double)scale * SF64(0x1058));
        SF64(0x1060) = (double)((long double)scale * q60);
        SF64(0x1068) = (double)((long double)scale * q68);
        l1c_first = (double)(x * SF64(0x1050));
        l24 = (double)((long double)SF64(0x1058) * SF64(0x1058));
        l2c = (double)(((long double)scale * q60) * SF64(0x1058));
        l44 = (double)(((long double)scale * q68) * SF64(0x1058));
        l54 = (double)((long double)SF64(0x1058) * SF64(0x1050));
        l3c = (double)(x * SF64(0x1050) - l24);
    }
    l4c = (double)((long double)SF64(0x1068) * SF64(0x1050));
    l1c = (double)((long double)SF64(0x1060) * SF64(0x1050));
    l34 = (double)((long double)SF64(0x1068) * SF64(0x1060));
    l4 = (double)((long double)SF64(0x1060) * SF64(0x1060));
    l14 = (double)((long double)SF64(0x1068) * SF64(0x1068));
    l5c = (double)((((long double)l24 + l1c_first) - l4) - l14);
    l64 = (double)(((long double)l2c + l4c) * SF64(0x770));
    lc_sine = (double)(((long double)l44 - l1c) * SF64(0x770));
    l6c = (double)(((long double)l54 + l34) * SF64(0x770));
    l24_cos = (double)(((long double)l3c - l4) + l14);
    l14_w = sim1_x87(SF64(0x13b0));
    l4_u = sim1_x87(SF64(0x13a0));
    l1c_v = (double)sim1_fchs(SF64(0x13a8));
    SF64(0x13b8) = (double)(((((long double)l3c + l4) - l14) * l4_u + (((long double)l34 - l54) * SF64(0x770)) * l1c_v) + (long double)l14_w * l64);
    SF64(0x13c0) = (double)sim1_fchs(((long double)SF64(0x13a0) * l6c + (long double)SF64(0x13b0) * lc_sine) + (long double)l1c_v * l24_cos);
    SF64(0x13c8) = (double)((((((long double)l1c + l44) * SF64(0x770)) * l1c_v) + (((long double)l2c - l4c) * SF64(0x770)) * l4_u) + (long double)l14_w * l5c);
    if (!X87_C0C3((long double)lc_sine, (long double)RF64(0x150d0))) {
        if (!X87_C0((long double)lc_sine, (long double)RF64(0x150c8))) SF64(0x1090) = (double)((long double)SF64(0x768) * RF64(0x15070));
        else SF64(0x1090) = (double)sim1_ciasin(sim1_fchs(lc_sine));
    } else SF64(0x1090) = (double)((long double)SF64(0x768) * RF64(0x15078));
    SF64(0x10a8) = (double)sim1_fcos(SF64(0x1090));
    l4_ratio = sim1_unit_clamp((double)((long double)l5c / SF64(0x10a8)));
    x = (long double)l24_cos / SF64(0x10a8);
    lc_ratio = (double)x;
    if (!X87_C0C3(x, (long double)RF64(0x150c8))) { x = RF64(0x150c8); lc_ratio = (double)x; }
    if (X87_C0(x, (long double)RF64(0x150d0))) lc_ratio = -1.0;
    sim1_angle_from_cosine(0x10a0, l5c, l64, l4_ratio);
    sim1_angle_from_cosine(0x1098, l24_cos, l6c, lc_ratio);
    SF64(0x1538) = (double)sim1_fchs(SF64(0x1090));
    SF64(0x1540) = (double)sim1_fchs(SF64(0x1098));
    SF64(0x1548) = sim1_x87(SF64(0x10a0));
    word = -sim1_ftol((long double)SF64(0x1090) * SF64(0x6d8) * SF64(0x678));
    S32(0x1550) = (uint32_t)word;
    S32(0x1554) = (uint32_t)-sim1_ftol((long double)SF64(0x1098) * SF64(0x6d8) * SF64(0x678));
    S32(0x1558) = (uint32_t)sim1_ftol((long double)SF64(0x10a0) * SF64(0x6d8) * SF64(0x678));
    P32(SIM1_GLOBAL, 0x578) = (uint32_t)word;
    P32(SIM1_GLOBAL, 0x57c) = (uint32_t)-sim1_ftol((long double)SF64(0x1098) * SF64(0x6d8) * SF64(0x678));
    P32(SIM1_GLOBAL, 0x580) = (uint32_t)sim1_ftol((long double)SF64(0x10a0) * SF64(0x6d8) * SF64(0x678));
}

/* 0x22007ef1 (edi GLOBAL, ebx the engine record, esi its second record): a piston engine's
 * pass: rpm (+0xc, through the prop's gear +0x2b54) into +0x16d0; running (+8) by the
 * magnetos and starter (+6: its count +2 of the second record), the fuel (+0x94, GLOBAL
 * +0x33c), the mixture's flow (+0x14), the oil (GLOBAL +0x364); the fuel flow (+0xe) and the
 * manifold pressure (+0x34, by the throttle +0, the altitude GLOBAL +0xe8e); the carburettor's
 * icing (the second record's +4, +6) and the heat; the power (+0x512, the table at +0x3560),
 * the cylinder head (+0x32) and oil temperatures (+0x2c, +0x2e) lagging; the torque word
 * (+0x16), the rpm stepping (+0x16d0, within 0..9999); the prop's pitch (+0x20, GLOBAL +0xaf0
 * the governor); the EGT (+0x5c); the thrust (+0x1c, through the tables at +0x4180 and
 * +0x3440 and the power 0x3cc00 of the density +0x4024), negated once after a stall (+0x516);
 * then the rpm back into +0xc and +0xa and the starter's spring (0x22008a77). */
void sim1_piston_engine(uint8_t *global, uint8_t *engine, uint8_t *extra)
{
    uint16_t ax, dx, cx, rest;
    uint32_t eax, ecx, edx, remainder;
    uint64_t product;
    int64_t signed_product;
    uint8_t stalled;
    if (!P32(global, 0x784)) P16(engine, 0x26) = 0;
    ax = (uint16_t)((uint16_t)((S16(0x3cc0) >> 2) + S16(0x38a2)) << 2);
    P16(engine, 0x3c) = ax;
    S16(0x2b54) = ax;
    ax = P16(engine, 0xc);
    if (ax >= 0xea60) ax = 0xea60;
    eax = (uint32_t)ax * S16(0x2b54);
    S16(0x16d0) = (uint16_t)((eax >> 16) + ((eax >> 15) & 1));
    S16(0x502) = 0xa;
    if ((P8(global, 0x364) & 1) && P16(engine, 0x2c) > 0x3e80) goto stop;
    if ((P8(global, 0x364) & 2) && (int16_t)P16(engine, 0x2e) <= 0x5dc) goto stop;
    if (!P16(engine, 8)) {
        if (!P16(global, 0x340)) { if (!P16(engine, 6)) goto next; }
        else if (P8(global, 0x608) != 0x33 && P16(engine, 6) != 4 && !S16(0x16d0)) goto next;
        if (!P32(engine, 0x94) && P16(global, 0x33c)) goto next;
        if (!P16(engine, 0x14)) goto next;
        P16(engine, 8) = 1;
    }
    if (P16(engine, 6) == 4 || P16(engine, 6) == 3) { P16(engine, 0x22) = 1; P16(engine, 0x24) = 1; }
    else if (!P16(engine, 6)) { P16(engine, 0x22) = 0; P16(engine, 0x24) = 0; P16(engine, 8) = 0; goto next; }
    else if (P16(engine, 6) == 2) { P16(engine, 0x22) = 1; P16(engine, 0x24) = 0; }
    else { P16(engine, 0x24) = 1; P16(engine, 0x22) = 0; }
    if (!P32(engine, 0x94) && P16(global, 0x33c)) goto stop;
    if (P16(engine, 0x14)) goto next;
stop:
    P16(engine, 8) = 0;
next:
    if (P16(engine, 6) == 4) {
        ax = (uint16_t)(P16(extra, 2) + 1);
        if (!(ax < 7)) { P16(engine, 6) = 3; ax = 0; }
        P16(extra, 2) = ax;
    }
    cx = (uint16_t)(MUL16_HIGH(P16(engine, 0) << 2, S16(0x3530)) + S16(0x3dd4));
    ax = P16(global, 0x342) == 1 ? P16(engine, 4) : 0x3400;
    S16(0x504) = MUL16_HIGH((uint16_t)(ax << 2), S16(0x3fe8));
    ax = P16(global, 0x342) == 1 ? P16(engine, 4) : 0x3400;
    dx = MUL16_HIGH(ax, cx);
    P16(engine, 0x14) = dx;
    if (!P16(engine, 8)) dx = 0;
    P16(engine, 0xe) = MUL16_HIGH(dx, S16(0x514));
    dx = (uint16_t)(MUL16_HIGH(P16(engine, 0) << 2, S16(0x3530)) + S16(0x3dd4));
    S16(0x512) = MUL16_HIGH((uint16_t)S32(0x3b08), dx);
    if ((uint16_t)(P16(engine, 0x22) + P16(engine, 0x24)) != 2) S16(0x512) = MUL16_HIGH(S16(0x512), S16(0x4398));
    if (P16(global, 0x342)) {
        dx = MUL16_HIGH(P16(engine, 0), 0x1bdc);
        if ((int16_t)P16(engine, 4) < 0x708) goto starved;
        if ((int16_t)P16(engine, 4) > (int16_t)dx) goto fed;
        dx = MUL16_HIGH(S16(0x3fe8), 0x1bdc);
        if ((int16_t)P16(extra, 0xc) > (int16_t)dx) goto fed;
        S16(0x512) = 0;
        if (S16(0x16d0)) goto fed;
starved:
        P16(engine, 8) = 0;
        S16(0x512) = 0;
    }
fed:
    if (!(S16(0x500) < 0x510)) {
        if (!P32(global, 0x784)) goto heated;
        if (!P16(global, 0x5dc)) {
            if ((P16(global, 0xf10) & 1) && (P16(global, 0xf0e) & 1) && !(PI16(global, 0xe8c) > 0x500) && !P16(engine, 0x26)) {
                eax = (uint32_t)(uint16_t)(7 * P16(global, 0xc44)) * S16(0x16f8);
                P32(extra, 4) -= eax;
                if (PI16(extra, 6) < 0) P32(extra, 4) = 0;
            } else {
                int64_t sum;
                ax = 0x834;
                if (P16(engine, 0x26)) ax = (uint16_t)(ax + 0xea60);
                eax = (uint32_t)ax * S16(0x16f8);
                sum = (int64_t)PI32(extra, 4) + (int32_t)eax;
                P32(extra, 4) = sum > 0x7fffffff || sum < -0x7fffffff - 1 ? 0x7fffffffu : (uint32_t)sum;
            }
        }
        S16(0x512) = MUL16_HIGH((uint16_t)(P16(extra, 6) << 1), S16(0x512));
    }
    if (P16(engine, 0x26)) S16(0x512) = MUL16_HIGH(S16(0x512), S16(0x2d3a));
heated:
    eax = (uint32_t)P16(global, 0xe8e) * 0xf1ec;
    dx = (uint16_t)(eax >> 15);
    S16(0x508) = dx;
    if (!P16(engine, 8)) {
        if (S16(0x16d0) >= 0xc8) {
            ecx = (uint32_t)(int32_t)(int16_t)dx;
            product = (uint64_t)(0x4000u - (uint32_t)(int32_t)PI16(engine, 0)) * ecx;
            ecx -= (uint32_t)(product >> 14);
            dx = (uint16_t)(((uint64_t)0xe666 * ecx) >> 16);
        }
    } else {
        uint16_t saved;
        cx = (uint16_t)(0x4000 - P16(engine, 0xc));
        if ((int16_t)cx < 0) cx = 0;
        ax = (uint16_t)((S16(0x3cc0) >> 2) + S16(0x38a2));
        eax = (uint32_t)ax * cx;
        saved = (uint16_t)((uint32_t)((uint64_t)eax * 0x14) >> 16);
        eax = sim1_div32(0, (uint32_t)S16(0x512) << 16, S32(0x3b08), &remainder);
        edx = (uint32_t)(((uint64_t)eax * S16(0x508)) >> 16) + saved;
        if (!(edx < S16(0x508))) edx = S16(0x508);
        dx = (uint16_t)edx;
        if (!(dx > 0xc00)) dx = 0xc00;
    }
    dx = (uint16_t)(((int32_t)(int16_t)(uint16_t)(dx - P16(engine, 0x34)) * 0x2800) >> 16);
    if (dx) P16(engine, 0x34) = (uint16_t)(P16(engine, 0x34) + dx);
    eax = sim1_div32(0, (uint32_t)S16(0x504) << 16, S16(0x514), &remainder);
    P16(extra, 0xc) = (uint16_t)eax;
    ax = (uint16_t)eax;
    if ((int16_t)ax > (int16_t)S16(0x3fe8)) ax = S16(0x3fe8);
    ax = sim1_table16(sim1_data + 0x3560, (uint16_t)((uint32_t)ax * S16(0x374e)));
    S16(0x512) = MUL16_HIGH(ax, S16(0x512));
    dx = MUL16_HIGH(S16(0x512), S16(0x1704));
    if (dx < 1) dx = 1;
    if (!P16(engine, 8)) dx = 0;
    S16(0x506) = dx;
    eax = sim1_div32(0, (uint32_t)S16(0x512) * 0xeb, S32(0x3b08), &remainder) << 16;
    if (!P16(engine, 8)) eax = 0;
    ecx = P16(global, 0x342) ? P16(engine, 4) : 0x3400;
    if (!((int32_t)ecx > 0x32)) ecx = 0x32;
    eax = sim1_div32(0, eax, ecx, &remainder) >> 1;
    eax = (uint32_t)(((uint64_t)eax * S16(0x1704)) >> 16);
    eax = (eax * S32(0x2c14)) >> 8;
    if (!(eax < 0x4000)) eax = 0x4000;
    ax = (uint16_t)((uint32_t)((int64_t)(int32_t)(eax - (uint32_t)(int32_t)PI16(engine, 0x32)) * 0x1400) >> 16);
    if (ax) P16(engine, 0x32) = (uint16_t)(P16(engine, 0x32) + ax);
    ecx = S16(0x16d0);
    if (!((int32_t)ecx > 0x64)) ecx = 0x64;
    eax = sim1_div32(0, (uint32_t)S16(0x506) * 0x1484, ecx, &remainder);
    if (!(eax < 0x7fff)) eax = 0x7fff;
    P16(engine, 0x16) = (uint16_t)eax;
    ax = (uint16_t)eax;
    if (!((int16_t)ax > 0) && S16(0x16d0)) {
        S8(0x516) = (uint8_t)((uint8_t)ax + 1);
        SI16(0x1718) = sim1_fistp16((long double)SF64(0x1388) * SF64(0x728));
        ax = (uint16_t)(S16(0x1718) << 1);
    }
    ax = sim1_sub16_saturated(ax, P16(engine, 0x18));
    if (!P16(engine, 8)) ax = sim1_sub16_saturated(ax, 0x4b);
    if (P16(engine, 6) == 4) {
        cx = (uint16_t)(0xc8 - S16(0x16d0));
        if (!((int16_t)cx < 0)) { ax = (uint16_t)(ax + cx); if (!P16(engine, 8)) ax = (uint16_t)(ax + 0x4b); }
    }
    dx = (uint16_t)((uint32_t)((int32_t)(int16_t)ax * (int16_t)S16(0x2c7a)) >> 15);
    if (!((int16_t)dx > (int16_t)0xfe37)) dx = 0xfe37;
    else if (!((int16_t)dx < 0x1c9)) dx = 0x1c9;
    if ((P16(global, 0xc46) & 0x10) && (int16_t)(uint16_t)(P16(engine, 0) - P16(extra, 0)) > 0x1000 && !((int16_t)dx < 0x14)) dx = 0x14;
    else P16(extra, 0) = P16(engine, 0);
    S16(0x16d0) = (uint16_t)(S16(0x16d0) + dx);
    if (!((int16_t)S16(0x16d0) < 0x270f)) S16(0x16d0) = 0x270f;
    if (!((int16_t)S16(0x16d0) > 0)) S16(0x16d0) = 0;
    ax = (uint16_t)(((uint32_t)S16(0x16d0) * S16(0x36b2)) >> 12);
    if (P16(engine, 8)) ax = sim1_add16_saturated(ax, S16(0x3b88));
    eax = (uint32_t)ax * S16(0x4082);
    S16(0x510) = (eax >> 16) || (uint16_t)eax > 0x3fff ? 0x3fff : (uint16_t)eax;
    if (P8(global, 0x364) & 2) P16(extra, 8) = sim1_sub16_saturated(P16(extra, 8), 0x200);
    else {
        P16(extra, 8) = 0;
        if (!(S16(0x510) < 0x3c8c)) {
            P16(extra, 0xe) = (uint16_t)(P16(extra, 0xe) + 0x40);
            if (!(P16(extra, 0xe) < 0x4000)) P16(extra, 0xe) = 0;
        } else {
            P16(extra, 0xe) = (uint16_t)(P16(extra, 0xe) - 8);
            if (PI16(extra, 0xe) < 0) P16(extra, 0xe) = 0;
        }
    }
    ax = sim1_add16_saturated(S16(0x510), P16(extra, 8));
    if ((int16_t)ax < 0) ax = 0;
    ax = (uint16_t)(ax - P16(engine, 0x2e));
    if (ax) {
        dx = (uint16_t)(((int32_t)(int16_t)ax * 0x1400) >> 16);
        if (!dx) dx = 1;
        P16(engine, 0x2e) = (uint16_t)(P16(engine, 0x2e) + dx);
    }
    ax = 0;
    if (P16(engine, 8)) {
        ax = sim1_div16(S16(0x512), 0, (uint16_t)S32(0x3b08), &rest);
        ax = sim1_add16_saturated(MUL16_HIGH(ax, S16(0x3fd0)), S16(0x4354));
        ax = (uint16_t)(((uint32_t)ax * S16(0x3800)) >> 12);
        if ((int16_t)ax < 0) ax = 0x7fff;
    }
    SI16(0x1718) = sim1_fistp16((long double)SF64(0x1388) * SF64(0x728));
    ax = (uint16_t)(ax - (uint16_t)(S16(0x1718) << 4));
    if ((int16_t)ax < 0) ax = 0;
    S16(0x50e) = ax;
    ax = (uint16_t)(((int16_t)(uint16_t)(P16(global, 0xe8c) - 0xf00) >> 1) + S16(0x50e));
    if ((int16_t)ax < 0) ax = 0;
    if (ax > 0x3fff) ax = 0x3fff;
    S16(0x50e) = ax;
    if (P8(global, 0x364) & 1) { cx = (uint16_t)(P16(extra, 0xa) + 0x64); if (cx > 0x3fff) cx = 0x3fff; P16(extra, 0xa) = cx; }
    else P16(extra, 0xa) = 0;
    ax = (uint16_t)(S16(0x50e) + P16(extra, 0xa));
    if (ax > 0x3fff) ax = 0x3fff;
    if ((int16_t)ax < 0) ax = 0;
    ax = (uint16_t)(ax - P16(engine, 0x2c));
    if (ax) {
        dx = (uint16_t)(((int32_t)(int16_t)ax * 0x40) >> 16);
        if (!dx) dx = 1;
        P16(engine, 0x2c) = (uint16_t)(P16(engine, 0x2c) + dx);
    }
    PF64(engine, 0x5c) = (double)((((long double)(int32_t)SI16(0x506) / (long double)SI32(0x3b08)) * sim1_f64(0x56b) + (long double)(int32_t)PI16(engine, 0x2c) * sim1_f64(0x573)
                                   + SF64(0x16e0) - PF64(engine, 0x5c)) * sim1_f64(0x57b) + PF64(engine, 0x5c));
    cx = S16(0x16d0);
    if (!((int16_t)cx > 0x64)) cx = 0x64;
    eax = (uint32_t)(SI32(0x1718) = sim1_fistp32((long double)SF64(0x1388) * SF64(0x728) * SF64(0x6d8)));
    dx = (uint16_t)(eax >> 16);
    ax = (uint16_t)eax;
    if ((int16_t)dx < 0) { dx = 0; ax = 0; }
    ax = sim1_div16(dx, ax, cx, &rest);
    eax = (uint32_t)ax * 0x2e8;
    ax = sim1_div16((uint16_t)(eax >> 16), (uint16_t)eax, (uint16_t)S32(0x4024), &rest);
    S16(0x16c8) = ax;
    sim1_set_u16(0x529, (uint16_t)(ax << 1));
    if (P32(global, 0xaf0) == 1) {
        P16(engine, 2) = (uint16_t)(P16(engine, 2) + 0x400);
        ax = P16(engine, 2);
        if (!((int16_t)ax > 0)) ax = 0;
        else if ((int16_t)ax > 0x3fff) ax = 0x3fff;
        P16(engine, 2) = ax;
    }
    if (P32(global, 0xaf0)) {
        dx = (uint16_t)(MUL16_HIGH(P16(engine, 2), S16(0x3cc0)) + S16(0x38a2));
        ax = (uint16_t)((uint16_t)(S16(0x16d0) - dx) << 1);
        if (!((int16_t)ax > (int16_t)0xff06)) ax = 0xff06;
        else if (!((int16_t)ax < 0xfa)) ax = 0xfa;
        ax = (uint16_t)(ax + P16(engine, 0x20));
        if (!((int16_t)ax > (int16_t)S16(0x43e0))) ax = S16(0x43e0);
        else if (!((int16_t)ax < (int16_t)S16(0x3d10))) ax = S16(0x3d10);
        P16(engine, 0x20) = ax;
    } else {
        ax = S16(0x35b4);
        P16(engine, 0x20) = ax;
    }
    sim1_set_u16(0x527, (uint16_t)((uint32_t)(uint16_t)(ax - S16(0x43e0)) * S16(0x3e30)));
    S16(0x50a) = sim1_table2d(sim1_data + 0x4180);
    S16(0x50c) = sim1_table2d(sim1_data + 0x3440);
    eax = (uint32_t)S16(0x16d0) * S16(0x16d0);
    ax = sim1_div16((uint16_t)(eax >> 16), (uint16_t)eax, 0xe10, &rest);
    {
        uint32_t density = S32(0x4024), low = density * density * density;
        uint64_t power = (uint64_t)low * density * density;
        eax = sim1_div32((uint32_t)(power >> 32), (uint32_t)power, 0x3cc00, &remainder);
        eax = (uint32_t)(((uint64_t)eax * ax) >> 8);
    }
    eax = (uint32_t)(((uint64_t)eax * S16(0x50a)) >> 8);
    product = (uint64_t)eax * S16(0x1704);
    eax = sim1_div32((uint32_t)(product >> 48), (uint32_t)(product >> 16), 0x1a5, &remainder);
    ax = (uint16_t)eax;
    if (!(eax < 0x7fff)) ax = 0x7fff;
    if (!((int16_t)ax > 0xa)) ax = 0xa;
    P16(engine, 0x18) = ax;
    SI32(0x1718) = sim1_fistp32((long double)SF64(0x1388) * SF64(0x728) * SF64(0x6d8));
    ax = (uint16_t)((uint64_t)((int64_t)SI32(0x1718) * 0x1f) >> 20);
    if (!((int16_t)ax > 0xa)) ax = 0xa;
    cx = ax;
    ax = sim1_div16((int16_t)S16(0x16d0) < 0 ? 0xffff : 0, S16(0x16d0), cx, &rest);
    eax = (uint32_t)ax * P16(engine, 0x18);
    signed_product = (int64_t)(int32_t)eax * (int32_t)(uint32_t)S16(0x50c);
    eax = (uint32_t)((uint64_t)signed_product >> 20);
    stalled = S8(0x516);
    S8(0x516) = 0;
    if (stalled) eax = 0u - eax;
    P32(engine, 0x1c) = eax;
    ax = sim1_div16(S16(0x16d0), 0, S16(0x2b54), &rest);
    if (ax != P16(engine, 0xc)) { P16(engine, 0xc) = ax; P16(engine, 0xa) = ax; }
    sim1_contact_rise(engine);
}

/* 0x220127b0: the rotor blades: the collective's lift slope (+0x19a8, +0x19b0); the air's
 * density ratio to a power (+0x1960, _CIpow); the inflow and thrust coefficients (+0x1950,
 * +0x1990, +0x1958 the induced velocity, +0x1970 its inverse) and the tip-loss factor (+0x1948,
 * within 0.2..40); the flapping (+0x19c0 the azimuth): the cyclic's flap angles (+0x1938,
 * +0x1940, +0x1980, +0x1918, +0x1978), the coning (+0x1bf8), the blade's drag (+0x1998,
 * +0x1c30), the advance ratios (+0x1920, +0x1928, +0x1930) and their powers; the rotor's
 * torque and thrust terms (+0x1c70, +0x1988, +0x1c28, +0x1c08, +0x1bf0) and the moment +0x12c0.
 * Retail keeps some terms unrounded on the x87 and parks others in its frame; the C does the
 * same, the frame's locals by offset. */
void sim1_rotor_blades(void)
{
    long double lift, value;
    double l0_ratio, l18_inflow, l8_cos, l10_sin, l20_density, l10_advance, l8_thrust, l28_power, l18_power, l10_power, l18_area, l20_cone;
    double old_cone;
    lift = (long double)SF64(0x36a8) * RF64(0x15400) / ((long double)SF64(0x36a8) - RF64(0x15408));
    SF64(0x19a8) = (double)lift;
    SF64(0x19b0) = (double)(lift * SF64(0x1c78) + SF64(0x3720));
    value = sim1_cipow(SF64(0x3d48), SF64(0xd88));
    {
        long double inflow = sim1_fsin(SF64(0x1af0)) * SF64(0x1ab8) + SF64(0x1948);
        SF64(0x1968) = (double)((long double)SF64(0x3d48) * SF64(0xe40));
        inflow /= SF64(0x1968);
        SF64(0x1950) = (double)inflow;
        SF64(0x1960) = (double)(value * SF64(0x798) * SF64(0x3720) / SF64(0x2c20));
        SF64(0x1990) = (double)((long double)SF64(0x1ac8) / SF64(0x1968));
        l18_inflow = (double)sim1_cipow(inflow, RF64(0x15410));
    }
    value = sim1_cipow(SF64(0x1990), RF64(0x15410));
    l0_ratio = (double)value;
    value = sim1_cipow(value + l18_inflow, RF64(0x15418));
    SF64(0x1958) = (double)value;
    if (X87_C3(value, (long double)RF64(0x15420))) { S32(0x1970) = 0; S32(0x1974) = 0; }
    else SF64(0x1970) = (double)((long double)RF64(0x15428) / SF64(0x1958));
    if (X87_C0((long double)SF64(0x1ac8), (long double)RF64(0x15430))) SF64(0x1948) = (double)(sim1_cipow((long double)SF64(0x1bf8) * RF64(0x15418), RF64(0x15418)) * SF64(0x1968));
    else SF64(0x1948) = (double)((((long double)SF64(0x1968) * SF64(0x1bf8)) * RF64(0x15418)) * SF64(0x1970));
    if (X87_C0C3((long double)SF64(0x1948), (long double)RF64(0x15438))) { S32(0x1948) = 0x9999999a; S32(0x194c) = 0x3fc99999; }
    else if (!X87_C0((long double)SF64(0x1948), (long double)RF64(0x15440))) { S32(0x1948) = 0; S32(0x194c) = 0x40440000; }
    old_cone = SF64(0x1bf8);
    l8_cos = (double)sim1_fcos(SF64(0x19c0));
    l10_sin = (double)sim1_fsin(SF64(0x19c0));
    SF64(0x1938) = (double)(((long double)SF64(0x1920) - (long double)SF64(0x1928) * l8_cos) - (long double)SF64(0x1930) * l10_sin);
    l20_density = (double)((long double)SF64(0x19b0) * SF64(0x798));
    SF64(0x1bf8) = (double)((((long double)SF64(0x1c78) * l0_ratio) * RF64(0x15418) + (long double)SF64(0x1c78) * RF64(0x15448)) - (long double)SF64(0x1950) * RF64(0x15450));
    SF64(0x1940) = (double)(((long double)SF64(0x1928) * l10_sin - (long double)SF64(0x1930) * l8_cos) * SF64(0xe40));
    SF64(0x1998) = (double)((long double)SF64(0x43f0) * SF64(0x1bf8));
    {
        long double moment = (long double)SF64(0x1968) * SF64(0x1950);
        SF64(0x1980) = (double)(moment - (moment * SF64(0x1938)) * l8_cos);
    }
    SF64(0x1918) = (double)((((long double)old_cone * RF64(0x15418)) * SF64(0x1970) + SF64(0x1950)) / SF64(0x1990));
    SF64(0x1978) = (double)(((long double)SF64(0x1990) * SF64(0x1968)) * l10_sin + SF64(0x1968));
    SF64(0x1980) = (double)((long double)SF64(0x1980) - (long double)SF64(0x1940) * SF64(0x3d48));
    SF64(0x1bf8) = (double)((((long double)SF64(0x19b0) * SF64(0x3598)) * RF64(0x15418)) * SF64(0x1bf8));
    SF64(0x1998) = (double)(((((long double)SF64(0x19b0) * SF64(0x798)) * RF64(0x15418)) * SF64(0x1998)) * ((long double)SF64(0x3b00) * SF64(0x3d48)));
    l10_advance = (double)((long double)l0_ratio - RF64(0x15458));
    SF64(0x1920) = (double)(((((long double)SF64(0x1c78) * l10_advance) * RF64(0x15460) + (long double)SF64(0x1950) * RF64(0x15468)) * SF64(0x1960)) * RF64(0x15418));
    l8_thrust = (double)((long double)SF64(0x1990) * SF64(0x1920));
    SF64(0x1c30) = (double)((((((long double)SF64(0x1998) * SF64(0x1968)) * SF64(0x1968)) * SF64(0x2c30)) * SF64(0x3b00)) * SF64(0xe40) / SF64(0x2c08));
    SF64(0x1928) = (double)((((long double)SF64(0x1950) + SF64(0x1950)) + (long double)SF64(0x1c78) * RF64(0x15470)) * SF64(0x1990) / ((long double)RF64(0x15428) - (long double)l0_ratio * RF64(0x15418)));
    SF64(0x1930) = (double)((((long double)SF64(0x1990) * SF64(0x1920)) * RF64(0x15478)) / (((long double)RF64(0x15428) - (long double)l0_ratio * RF64(0x15450)) * RF64(0x15480)));
    SF64(0x1c70) = (double)((((((long double)SF64(0x30a0) * RF64(0x15460)) / SF64(0x19b0)) * l10_advance) - ((long double)SF64(0x1950) * SF64(0x1c78)) * RF64(0x15468)) - (long double)l18_inflow * RF64(0x15418));
    l28_power = (double)sim1_cipow(SF64(0x1920), RF64(0x15410));
    l18_power = (double)sim1_cipow(SF64(0x1930), RF64(0x15410));
    l10_power = (double)sim1_cipow(SF64(0x1928), RF64(0x15410));
    value = (long double)SF64(0x19b0) / ((long double)SF64(0x36a8) * RF64(0x154a0));
    SF64(0x19b8) = (double)value;
    l18_area = (double)((long double)SF64(0x1990) * SF64(0x1950));
    SF64(0x1988) = (double)(((((long double)l10_power * RF64(0x15488) + (long double)l18_power * RF64(0x15490)) + (long double)l28_power * RF64(0x15418)) * RF64(0x15418)) * l0_ratio);
    SF64(0x1c70) = (double)(((((long double)SF64(0x1c70) - ((long double)l18_power + l10_power) * RF64(0x15490)) - SF64(0x1988)) - ((long double)SF64(0x1928) * l18_area) * RF64(0x15418))
                            + (((long double)SF64(0x1990) * SF64(0x1930)) * SF64(0x1920)) * RF64(0x15498));
    SF64(0x1c28) = (double)((((value + SF64(0x30a0)) * SF64(0x1990)) * RF64(0x15418)) / SF64(0x19b0) + ((long double)SF64(0x1928) * SF64(0x1c78)) * RF64(0x15498));
    l20_cone = (double)((long double)SF64(0x1968) * SF64(0x1968));
    SF64(0x1c08) = (double)((((((long double)SF64(0x3d48) * SF64(0x1c70)) * l20_density) * RF64(0x15418)) * SF64(0x3d48)) * l20_cone);
    SF64(0x1c28) = (double)((((long double)SF64(0x1c28) - ((long double)SF64(0x1c78) * l18_area) * RF64(0x15418)) + ((long double)SF64(0x1990) * l10_power) * RF64(0x15460))
                            + ((long double)SF64(0x1950) * SF64(0x1928)) * RF64(0x154a8));
    {
        long double square = (long double)SF64(0x1990) * SF64(0x1990);
        SF64(0x12c0) = (double)((((((long double)SF64(0x1930) * square) * RF64(0x15418)) - ((long double)l8_thrust * RF64(0x154a8) - (long double)SF64(0x1930) * RF64(0x154b8))) * SF64(0x1c78))
                                - (((((((long double)SF64(0x1930) * SF64(0x1928)) * RF64(0x15460)) - ((long double)SF64(0x1950) * SF64(0x1920)) * RF64(0x154c0)) * SF64(0x1990))
                                    - ((long double)SF64(0x1928) * SF64(0x1920)) * RF64(0x154c8)) + (long double)SF64(0x1950) * SF64(0x1930)));
        SF64(0x1c28) = (double)(((long double)SF64(0x1c28) - ((long double)SF64(0x1930) * SF64(0x1920)) * RF64(0x154b0)) + ((long double)SF64(0x1990) * l28_power) * RF64(0x15460));
        SF64(0x1bf0) = (double)((((((((long double)SF64(0x798) * SF64(0x3720)) * SF64(0x3d48)) * SF64(0x1c28)) * RF64(0x15418)) * SF64(0x2c30)) * SF64(0x3b00)) * l20_cone);
        SF64(0x12c0) = (double)((((((((((long double)SF64(0x12c0) - ((long double)SF64(0x1928) * SF64(0x1920)) * square) * SF64(0x19b0)) * SF64(0x1968)) * SF64(0x1968)) * SF64(0x798))
                                   * SF64(0x2c30)) * SF64(0x3b00)) * SF64(0x3d48)) * RF64(0x15418));
    }
}

/* A table term of 0x2200fe80: the table's value (0x2200a786) plus a word times +0x6b0. */
static long double sim1_table_term(uint32_t table, int32_t word)
{
    return sim1_table_scaled(sim1_data + table) + (long double)word * SF64(0x6b0);
}

/* A deflection's fold of 0x2200fe80 (|angle| past +0xb50: what is left of pi at the top,
 * else +0xb50), on the unrounded value, a NaN to +0xb50. */
static long double sim1_fold_deflection(long double value)
{
    if (X87_C0(value, (long double)SF64(0xb50))) return value;
    if (X87_C0C3(value, (long double)RF64(0x152a0) - SF64(0xb50))) return SF64(0xb50);
    return (long double)RF64(0x152a0) - value;
}

/* 0x2200fe80: the aerodynamic coefficients: the limits' fraction (0x22010920); the lift, drag,
 * side and moment curves' tables (0x2200a786) plus their trim words (+0x17a8.. +0x17c8,
 * +0x18b0.. +0x18c8, +0x1828.. +0x1840, ...); the angle of attack less the trim (+0x4178),
 * folded about +0xb50, and the flaps' and slats' deflections (+0x14d0, +0x14e0, +0x14d8,
 * +0x14c8 less it, folded) into +0x17e8.. +0x1808 by the density +0x2d90; the ground effect
 * (+0x1810, the table at +0x3100, with +0x2c98); the rate terms by the tables at +0x2cc0,
 * +0x2ae0, +0x3a60, +0x3760, +0x3920, +0x3c20 (by the angle +0x1490 * +0x688); the tail's
 * downwash (+0x1890); and the propellers' slipstream over the tail (+0x1898: the engines'
 * thrust over the dynamic pressure, faded out with speed past +0x3702 * +0x740), none for a
 * turbine, on the ground or slow. Retail keeps several terms unrounded on the x87 and parks
 * others in its frame; the C does the same. */
void sim1_aero_coefficients(void)
{
    static const uint16_t engines[3][2] = {{0x940, 0x944}, {0x9d8, 0x9dc}, {0xa70, 0xa74}};
    uint8_t *global;
    long double value, flap, speed, sum;
    double trim, folded, density, parked;
    uint32_t at;
    sim1_limit_fraction();
    SF64(0x17a8) = (double)sim1_table_term(0x2ba0, SI32(0x3b58));
    SF64(0x17b0) = (double)(sim1_table_scaled(sim1_data + 0x3e40) * SF64(0x14a8) + (long double)(int32_t)SI16(0x2c28) * SF64(0x6b0));
    SF64(0x17b8) = (double)sim1_table_term(0x3d20, SI32(0x3dd8));
    SF64(0x17c0) = (double)sim1_table_term(0x3600, (int32_t)SI16(0x4152));
    value = sim1_table_scaled(sim1_data + 0x3500);
    trim = (double)((long double)(int32_t)SI16(0x4178) * SF64(0x680));
    folded = (double)fabsl((long double)SF64(0x1490) - trim);
    SF64(0x17c8) = (double)(value + (long double)(int32_t)(uint32_t)S16(0x3810) * SF64(0x6b0));
    SF64(0x17d0) = (double)((long double)(int32_t)(uint32_t)S16(0x3e00) * SF64(0x6b0));
    SF64(0x17d8) = (double)((long double)(int32_t)(uint32_t)S16(0x4044) * SF64(0x6b0));
    SF64(0x17e0) = (double)((long double)(int32_t)(uint32_t)S16(0x3bc2) * SF64(0x6b0));
    if (X87_C0((long double)folded, (long double)SF64(0xb50))) value = folded;
    else if (X87_C0((long double)RF64(0x152a0) - SF64(0xb50), (long double)folded)) value = (long double)RF64(0x152a0) - folded;
    else value = SF64(0xb50);
    {
        long double lift = ((long double)SF64(0x1680) + SF64(0x1678)) * SF64(0x3e20) * SF64(0x768) + value * SF64(0x25f0);
        double lift_d = (double)lift;
        density = (double)(long double)(int64_t)(uint64_t)S32(0x2d90);
        SF64(0x17e8) = (double)(((lift * lift_d) * SF64(0x690)) * density);
    }
    flap = sim1_fold_deflection(fabsl((long double)SF64(0x14d0) - trim));
    parked = (double)(flap * SF64(0x25f0));
    SF64(0x17f0) = (double)((((long double)SF64(0x690) * parked) * parked) * density);
    flap = sim1_fold_deflection(fabsl((long double)SF64(0x14e0) - trim));
    parked = (double)(flap * SF64(0x25f0));
    SF64(0x17f8) = (double)((((long double)SF64(0x690) * parked) * parked) * density);
    flap = sim1_fold_deflection(fabsl((long double)SF64(0x14d8) - trim));
    parked = (double)(flap * SF64(0x25f0));
    SF64(0x1800) = (double)((((long double)SF64(0x690) * parked) * parked) * density);
    value = fabsl((long double)SF64(0x14c8) - trim);
    if (X87_C0(value, (long double)SF64(0xb50))) SF64(0x18e0) = (double)value;
    else if (X87_C0C3(value, (long double)RF64(0x152a0) - SF64(0xb50))) { S32(0x18e0) = S32(0xb50); S32(0x18e4) = S32(0xb54); }
    else SF64(0x18e0) = (double)((long double)RF64(0x152a0) - value);
    value = (long double)SF64(0x18e0) * SF64(0x25f0);
    SF64(0x1808) = (double)((((long double)SF64(0x690) * value) * value) * density);
    if (S16(0x2c98) == 1) {
        parked = (double)sim1_table_lookup(sim1_data + 0x3100, SF64(0x1490));
        SF64(0x1810) = (double)((long double)RF64(0x152b0) - (sim1_table_lookup(sim1_data + 0x3100, SF64(0x1490)) * parked) * RF64(0x152a8));
    } else { S32(0x1810) = 0; S32(0x1814) = 0; }
    SF64(0x18b8) = (double)sim1_table_term(0x2be0, SI32(0x3094));
    SF64(0x18b0) = (double)sim1_table_term(0x39e0, (int32_t)SI16(0x3dd6));
    SF64(0x18c0) = (double)sim1_table_term(0x3ce0, SI32(0x3098));
    SF64(0x18c8) = (double)sim1_table_term(0x3b20, (int32_t)SI16(0x4084));
    value = sim1_table_scaled(sim1_data + 0x3be0);
    parked = (double)(value + ((long double)SI32(0x4238) * SF64(0x6b0) - fabsl((long double)SF64(0x1650)) * SF64(0x1770)));
    SF64(0x1818) = (double)((sim1_table_lookup(sim1_data + 0x2cc0, SF64(0x1630)) * parked) * SF64(0x14a8));
    SF64(0x1828) = (double)sim1_table_term(0x3680, SI32(0x2cb0));
    SF64(0x1830) = (double)sim1_table_term(0x30c0, SI32(0x2c38));
    SF64(0x1838) = (double)sim1_table_term(0x40a0, SI32(0x3f94));
    SF64(0x1840) = (double)sim1_table_term(0x3400, SI32(0x3b54));
    if (S32(0x3b68)) { S32(0x1820) = S32(0x3fd8); S32(0x1824) = S32(0x3fdc); }
    else { S32(0x1820) = 0; S32(0x1824) = 0; }
    value = sim1_table_scaled(sim1_data + 0x3a20);
    parked = (double)(value + ((long double)(int32_t)SI16(0x3748) * SF64(0x768)) * SF64(0x6b0));
    SF64(0x1850) = (double)((sim1_table_lookup(sim1_data + 0x2ae0, SF64(0x1638)) * parked) * SF64(0x14f0));
    value = sim1_table_scaled(sim1_data + 0x3a20);
    parked = (double)(value + ((long double)(int32_t)SI16(0x3748) * SF64(0x768)) * SF64(0x6b0));
    value = (sim1_table_lookup(sim1_data + 0x2ae0, SF64(0x1640)) * parked) * SF64(0x1500);
    SF64(0x1858) = (double)value;
    parked = (double)sim1_table_lookup(sim1_data + 0x3a60, (double)((long double)SF64(0x1490) * SF64(0x688)));
    SF64(0x1848) = (double)(sim1_table_term(0x40e0, SI32(0x2ca8)) * parked);
    SF64(0x1860) = (double)sim1_table_term(0x3820, SI32(0x2cac));
    value = sim1_table_term(0x43a0, SI32(0x3738));
    {
        double angle = (double)((long double)SF64(0x1490) * SF64(0x688));
        SF64(0x1868) = (double)(value * SF64(0x14a8));
        parked = (double)sim1_table_lookup(sim1_data + 0x3760, angle);
    }
    SF64(0x1870) = (double)(sim1_table_term(0x2d40, (int32_t)SI16(0x3fc0)) * parked);
    value = sim1_table_term(0x4120, SI32(0x39c0));
    {
        double angle = (double)((long double)SF64(0x1490) * SF64(0x688));
        SF64(0x1888) = (double)(value * SF64(0x1508));
        parked = (double)sim1_table_lookup(sim1_data + 0x3920, angle);
    }
    SF64(0x18a0) = (double)(sim1_table_term(0x4360, (int32_t)SI16(0x2d38)) * parked);
    value = sim1_table_term(0x3880, SI32(0x2c2c));
    {
        double angle = (double)((long double)SF64(0x1490) * SF64(0x688));
        SF64(0x1880) = (double)value;
        SF64(0x1880) = (double)(sim1_table_lookup(sim1_data + 0x3c20, angle) * SF64(0x1880));
    }
    SF64(0x1878) = (double)sim1_table_term(0x38c0, SI32(0x2d34));
    value = sim1_table_scaled(sim1_data + 0x36e0);
    SF64(0x18a8) = (double)((long double)SI32(0x3b0c) * SF64(0x690));
    SF64(0x1890) = (double)(((value + ((long double)(int32_t)SI16(0x3842) * SF64(0x768)) * SF64(0x6b0)) * SF64(0x14a8)) * RF64(0x152c0));
    global = SIM1_GLOBAL;
    if (P8(global, 0x609) || P32(global, 0x278) || X87_C0C3((long double)SF64(0x1448), (long double)RF64(0x152b8))) { S32(0x1898) = 0; S32(0x189c) = 0; return; }
    {
        long double mass = (long double)SF64(0x738) * SF64(0x738), area = (long double)(int64_t)(uint64_t)S32(0x4024);
        SF64(0x18d8) = (double)((mass * area) * area);
    }
    value = (long double)SF64(0x18d8) * SF64(0x798) * SF64(0x1448);
    SF64(0x1898) = (double)(((long double)(int32_t)(0u - P32(global, 0x8a8)) / value) * ((long double)(int32_t)(uint32_t)P16(global, 0x8ac) * SF64(0xb58) * SF64(0x680)));
    for (at = 0; at < 3; ++at) {
        if (P16(global, 0xaec) < at + 2) break;
        sum = ((long double)PI32(global, engines[at][0]) / value) * ((long double)(int32_t)(uint32_t)P16(global, engines[at][1]) * SF64(0xb58) * SF64(0x680));
        SF64(0x1898) = (double)((long double)SF64(0x1898) - sum);
    }
    speed = (long double)(int32_t)(uint32_t)S16(0x3702) * SF64(0x740);
    SF64(0x1898) = (double)(((long double)(int32_t)(uint32_t)P16(global, 0xc44) * SF64(0x1898)) * RF64(0x152c8));
    if (!X87_C0C3((long double)SF64(0x1388), speed + speed)) { S32(0x1898) = 0; S32(0x189c) = 0; return; }
    if (X87_C0C3((long double)SF64(0x1388), speed)) return;
    SF64(0x1898) = (double)(((long double)RF64(0x152d0) - (long double)SF64(0x1388) / speed) * SF64(0x1898));
}

/* 0x22007e80 (edi GLOBAL): the piston engines: the fuel-flow scale +0x514 from +0x1704; each
 * fitted engine (0x22007ef1, with its second record at +0x52b + 16 per engine). (Retail leaves
 * +0x3fe8 in ax; no caller reads it.) */
void sim1_pistons(uint8_t *global)
{
    uint32_t at;
    S16(0x514) = S16(0x1704);
    sim1_piston_engine(global, global + 0x88c, sim1_data + 0x52b);
    for (at = 1; at < 4; ++at) {
        if (P16(global, 0xaec) < at + 1) break;
        sim1_piston_engine(global, global + 0x88c + at * 0x98, sim1_data + 0x52b + at * 0x10);
    }
}

/* 0x22001100 (edi GLOBAL): the local flow: the dynamic pressure's term +0x15d0 (by GLOBAL
 * +0x8e0); the wing tips' and the tail's angles of attack (+0x0a2, +0x0a6, +0x0a0, +0x0a4: the
 * velocities plus and less the rates times the arm +0x38f2 * +0x738, through 0x2200a599, plus
 * the trim +0x3900 and for the tips +0x2c10) into +0x14d0.., the sines and cosines of the
 * first and last; then the coefficients and the forces and moments (0x2200fe80, 0x220052b0,
 * 0x220054a0, 0x22005640, 0x22005740, 0x220056e0, 0x22006100, 0x220057b0, 0x22005c00,
 * 0x22005e20). The flow's components pass through retail's frame as doubles. */
void sim1_local_flow(uint8_t *global)
{
    long double x = PF64(global, 0x8e0);
    double roll_arm, yaw_arm, across, along;
    SF64(0x15d0) = (double)(((x * x + (x * SF64(0x1450) + x * SF64(0x1450))) * SF64(0x768)) * SF64(0x798));
    roll_arm = (double)((long double)(int32_t)SI16(0x38f2) * SF64(0x738) * SF64(0x13f0));
    yaw_arm = (double)((long double)(int32_t)SI16(0x38f2) * SF64(0x738) * SF64(0x13f8));
    across = (double)((long double)roll_arm - SF64(0x1398));
    along = (double)((long double)yaw_arm + SF64(0x1388));
    S16(0xa2) = (uint16_t)(sim1_bearing_of(along, across) + S16(0x3900) + S16(0x2c10));
    across = (double)((long double)roll_arm * SF64(0x768) - SF64(0x1398));
    along = (double)((long double)yaw_arm * SF64(0x768) + SF64(0x1388));
    S16(0xa6) = (uint16_t)(sim1_bearing_of(along, across) + S16(0x3900));
    across = (double)(sim1_fchs(roll_arm) - SF64(0x1398));
    along = (double)(sim1_fchs(yaw_arm) + SF64(0x1388));
    S16(0xa0) = (uint16_t)(sim1_bearing_of(along, across) + S16(0x3900) + S16(0x2c10));
    across = (double)(sim1_fchs((long double)roll_arm * SF64(0x768)) - SF64(0x1398));
    along = (double)(sim1_fchs((long double)yaw_arm * SF64(0x768)) + SF64(0x1388));
    S16(0xa4) = (uint16_t)(sim1_bearing_of(along, across) + S16(0x3900));
    SF64(0x14d0) = (double)((long double)(int32_t)SI16(0xa2) * SF64(0x680));
    SF64(0x14e0) = (double)((long double)(int32_t)SI16(0xa6) * SF64(0x680));
    SF64(0x14d8) = (double)((long double)(int32_t)SI16(0xa4) * SF64(0x680));
    SF64(0x14c8) = (double)((long double)(int32_t)SI16(0xa0) * SF64(0x680));
    SF64(0x14e8) = (double)sim1_fsin(SF64(0x14d0));
    SF64(0x14f0) = (double)sim1_fcos(SF64(0x14d0));
    SF64(0x14f8) = (double)sim1_fsin(SF64(0x14c8));
    SF64(0x1500) = (double)sim1_fcos(SF64(0x14c8));
    sim1_aero_coefficients();
    sim1_inertia();
    sim1_pitch_moment();
    sim1_pitch_sum();
    sim1_thrust_sum();
    sim1_side_sum();
    sim1_turn_pair();
    sim1_pitch_sum_moment();
    sim1_yaw_moment();
    sim1_roll_moment();
}

/* The AIR decoder's plain records: a record copied whole into GLOBAL (1) or .data (0). */
static const struct { uint16_t id; uint8_t global; uint16_t offset; } sim1_air_copies[] = {
        {0x310, 1, 0x609},
        {0x311, 1, 0xaec},
        {0x312, 1, 0xaf4},
        {0x314, 1, 0x850},
        {0x316, 1, 0xb0c},
        {0x319, 1, 0x77c},
        {0x322, 1, 0x780},
        {0x323, 1, 0x784},
        {0x328, 1, 0x790},
        {0x331, 1, 0x794},
        {0x332, 1, 0x79c},
        {0x334, 1, 0x7b4},
        {0x338, 0, 0x36c0},
        {0x341, 0, 0x2cc0},
        {0x342, 0, 0x2ae0},
        {0x343, 0, 0x3d60},
        {0x400, 0, 0x42a0},
        {0x401, 0, 0x3e80},
        {0x404, 0, 0x3100},
        {0x410, 0, 0x3e40},
        {0x411, 0, 0x3d20},
        {0x412, 0, 0x2ba0},
        {0x413, 0, 0x3600},
        {0x420, 0, 0x3be0},
        {0x421, 0, 0x40a0},
        {0x422, 0, 0x3680},
        {0x423, 0, 0x3400},
        {0x430, 0, 0x3500},
        {0x433, 0, 0x30c0},
        {0x440, 0, 0x3b20},
        {0x441, 0, 0x39e0},
        {0x442, 0, 0x3ce0},
        {0x443, 0, 0x2be0},
        {0x450, 0, 0x2d40},
        {0x451, 0, 0x3760},
        {0x452, 0, 0x43a0},
        {0x453, 0, 0x3a20},
        {0x454, 0, 0x3820},
        {0x455, 0, 0x40e0},
        {0x456, 0, 0x3a60},
        {0x459, 0, 0x3880},
        {0x460, 0, 0x3c20},
        {0x461, 0, 0x4120},
        {0x462, 0, 0x36e0},
        {0x463, 0, 0x4360},
        {0x464, 0, 0x3920},
        {0x465, 0, 0x38c0},
        {0x473, 0, 0x2da0},
        {0x501, 0, 0x3560},
        {0x503, 0, 0x4180},
        {0x504, 0, 0x3440},
        {0x601, 0, 0x4260},
        {0x602, 0, 0x3ba0},
        {0x603, 0, 0x3640},
        {0x604, 0, 0x4060},
        {0x605, 0, 0x35c0},
        {0x606, 0, 0x4000}
};

/* A point's contact ratio (for 0x2200af90): the stiffness over its radius (the two doubles at
 * +0x30, +0x40 of its record), into +0x18 and a quarter of it into +0x1c, where it is on. */
static void sim1_contact_ratio(uint32_t point, long double stiffness, int both, uint32_t other)
{
    int32_t ratio;
    if (S32(point + 4) != 1) return;
    ratio = sim1_ftol(stiffness / sim1_fsqrt((long double)SF64(point + 0x30) * SF64(point + 0x30) + (long double)SF64(point + 0x40) * SF64(point + 0x40)));
    S32(point + 0x18) = (uint32_t)ratio;
    if (both) S32(other + 0x18) = (uint32_t)ratio;
    ratio = (ratio + (int32_t)((uint32_t)(ratio >> 31) & 3)) >> 2;
    S32(point + 0x1c) = (uint32_t)ratio;
    if (both) S32(other + 0x1c) = (uint32_t)ratio;
}

#define RB(p, o) P8(p, o)
#define RW(p, o) P16(p, o)
#define RS16(p, o) PI16(p, o)
#define RD(p, o) P32(p, o)
#define RF(p, o) PF64(p, o)
#define SIM1_MEMMOVE SIM1_CALL(SIM1_HOST, 0x21c, uint32_t (__stdcall *)(void *, const void *, uint32_t))

/* 0x2200af90, stdcall: the aircraft (.AIR) file's records decoded: each a 4-byte id, a 4-byte
 * length and the data (read through HOST's memmove, +0x21c, as retail does). Most records are
 * copied whole into GLOBAL or a table in .data (sim1_air_copies); the rest set fields, flags
 * and the contact points (0x1004 the gear's, 0x1005 the others', through 0x2200ad20 and their
 * ratios). Afterwards a larger engine count copies engine 1's record to the new engines
 * (0x2200ada0), the starters are capped by the engine type, empty lists cleared, GLOBAL +0x368
 * set, the stall angle and the pass's setup made (0x2200d750, 0x2200a7b0). Returns 1.
 * Retail's frame buffers: an id 0x302 record longer than its 0x1c bytes overruns retail's
 * stack; the C copies at most 0x1c (SIM1-AIR-FRAME-OVERRUN, fixed, docs/retail-bugs.md), and a
 * shorter one leaves stale frame bytes in retail where the C has zeros. */
uint32_t __stdcall sim1_air_decode(const uint8_t *data, int32_t size)
{
    uint8_t *global = SIM1_GLOBAL;
    uint16_t engines = P16(global, 0xaec);
    uint32_t offset = 0, id, length, at;
    S32(0x3cc4) = 1;
    P32(global, 0x82c) = 0;
    P32(SIM1_GLOBAL, 0x290) = 0;
    P32(SIM1_GLOBAL, 0x294) = 0;
    P32(SIM1_GLOBAL, 0x298) = 0;
    S32(0x2510) = 0;
    if (size > 0) do {
        uint8_t *record;
        SIM1_MEMMOVE(&id, data + offset, 4);
        offset += 4;
        SIM1_MEMMOVE(&length, data + offset, 4);
        offset += 4;
        record = (uint8_t *)data + offset;
        global = SIM1_GLOBAL;
        for (at = 0; at < sizeof sim1_air_copies / sizeof sim1_air_copies[0]; ++at)
            if (sim1_air_copies[at].id == id) break;
        if (at < sizeof sim1_air_copies / sizeof sim1_air_copies[0]) {
            SIM1_MEMMOVE((sim1_air_copies[at].global ? SIM1_GLOBAL : sim1_data) + sim1_air_copies[at].offset, record, length);
            goto next;
        }
        switch (id) {
        case 0x103:
            P32(SIM1_GLOBAL, 0x82c) = 1;
            break;
        case 0x105:
            SIM1_MEMMOVE(SIM1_GLOBAL + 0x604, record, length);
            if (P32(SIM1_GLOBAL, 0x604) == 1) { S32(0x1c80) = P32(SIM1_GLOBAL, 0x278); P32(SIM1_GLOBAL, 0x278) = 0; }
            break;
        case 0x300: {
            uint16_t words[6];
            for (at = 0; at < 6; ++at) words[at] = RW(record, at * 2);
            SIM1_CALL(SIM1_CONTROLS, 0x18, uint32_t (__stdcall *)(void *))(words);
            break;
        }
        case 0x301: {
            uint32_t word = 0;
            SIM1_MEMMOVE(SIM1_GLOBAL + 0xdb0, record, 2);
            SIM1_MEMMOVE(SIM1_GLOBAL + 0xdb2, record + 2, 2);
            SIM1_MEMMOVE(SIM1_GLOBAL + 0xdb4, record + 4, 2);
            SIM1_MEMMOVE(SIM1_GLOBAL + 0x120c, record + 6, 4);
            SIM1_MEMMOVE(&word, record + 0xa, 2);
            S32(0x34f0) = word << 16;
            break;
        }
        case 0x302: {
            uint32_t pairs[7] = {0};
            static const uint16_t into[7] = {0xb78, 0xb80, 0xb98, 0xb88, 0xba0, 0xb90, 0xba8};
            SIM1_MEMMOVE(pairs, record, length < sizeof pairs ? length : sizeof pairs);
            for (at = 0; at < 7; ++at) P32(SIM1_GLOBAL, into[at]) = pairs[at];
            break;
        }
        case 0x313: {
            static const uint16_t throttles[4] = {0x88c, 0x924, 0x9bc, 0xa54};
            SIM1_MEMMOVE(SIM1_GLOBAL + 0xb00, record, length);
            for (at = 0; at < 4; ++at)
                if (PI16(SIM1_GLOBAL, throttles[at]) < PI16(SIM1_GLOBAL, 0xb00)) P16(SIM1_GLOBAL, throttles[at]) = P16(SIM1_GLOBAL, 0xb00);
            break;
        }
        case 0x315: {
            uint16_t word = 0;
            SIM1_MEMMOVE(&word, record, 2);
            SIM1_CALL(SIM1_HOST, 0x3c, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x100e0, (uint32_t)(int32_t)(int16_t)word);
            break;
        }
        case 0x317:
            P32(SIM1_GLOBAL, 0x764) = RD(record, 0);
            P32(SIM1_GLOBAL, 0x768) = RD(record, 4);
            S32(0x3fd4) = RD(record, 8);
            S32(0x4040) = RD(record, 0xc);
            if (!P32(SIM1_GLOBAL, 0x764)) { P32(SIM1_GLOBAL, 0x7bc) = 0; P32(SIM1_GLOBAL, 0x808) = 0; }
            if (!P32(SIM1_GLOBAL, 0x768)) P32(SIM1_GLOBAL, 0x7ec) = 0;
            break;
        case 0x318:
            SIM1_MEMMOVE(SIM1_GLOBAL + 0x778, record, length);
            if (!P32(SIM1_GLOBAL, 0x778)) { P32(SIM1_GLOBAL, 0xbdc) = 0; P32(SIM1_GLOBAL, 0xbe0) = 0; P32(SIM1_GLOBAL, 0xbe4) = 0; }
            break;
        case 0x324:
            SIM1_MEMMOVE(SIM1_GLOBAL + 0x788, record, length);
            if (!P32(SIM1_GLOBAL, 0x788)) P32(SIM1_GLOBAL, 0x29c) = 0;
            break;
        case 0x327:
            SIM1_MEMMOVE(SIM1_GLOBAL + 0x78c, record, length);
            if (!P32(SIM1_GLOBAL, 0x78c)) { P32(SIM1_GLOBAL, 0xbd0) = 0; P32(SIM1_GLOBAL, 0xbd4) = 0; P32(SIM1_GLOBAL, 0xbd8) = 0; }
            break;
        case 0x329:
            P32(SIM1_GLOBAL, 0x76c) = RD(record, 0);
            P32(SIM1_GLOBAL, 0x770) = RD(record, 4);
            P32(SIM1_GLOBAL, 0x774) = RD(record, 8);
            if (!P32(SIM1_GLOBAL, 0x774)) { P32(SIM1_GLOBAL, 0x810) = 0; P32(SIM1_GLOBAL, 0x80c) = 0; }
            if (!P32(SIM1_GLOBAL, 0x76c)) P32(SIM1_GLOBAL, 0x7dc) = 0;
            break;
        case 0x330:
            SIM1_MEMMOVE(SIM1_GLOBAL + 0x798, record, length);
            if (P32(SIM1_GLOBAL, 0x798) == 1) P32(SIM1_GLOBAL, 0xaf0) = 0;
            else if (P32(SIM1_GLOBAL, 0x798) == 2) P32(SIM1_GLOBAL, 0xaf0) = 2;
            break;
        case 0x333:
            SIM1_MEMMOVE(SIM1_GLOBAL + 0x7ac, record, 4);
            SIM1_MEMMOVE(SIM1_GLOBAL + 0x7b0, record + 4, 4);
            if (!P32(SIM1_GLOBAL, 0x7ac) && !P32(SIM1_GLOBAL, 0x7b0)) P16(SIM1_GLOBAL, 0xe5a) = 0;
            break;
        case 0x335:
            P32(SIM1_GLOBAL, 0x7a0) = RD(record, 0);
            P32(SIM1_GLOBAL, 0x7a4) = RD(record, 4);
            P32(SIM1_GLOBAL, 0x7a8) = RD(record, 8);
            if (!P32(SIM1_GLOBAL, 0x7a0) && !P32(SIM1_GLOBAL, 0x7a4)) P16(SIM1_GLOBAL, 0xe5a) = 0;
            break;
        case 0x337:
            SIM1_MEMMOVE(sim1_data + 0x2c04, record, length);
            if (!S32(0x2c04)) P32(SIM1_GLOBAL, 0xbcc) = 0;
            break;
        case 0x344:
            SIM1_MEMMOVE(SIM1_GLOBAL + 0x5d4, record, length);
            if (!P32(SIM1_GLOBAL, 0x5d4) && P32(SIM1_GLOBAL, 0x5d8)) SIM1_CALL(SIM1_HOST, 0x3c, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x10017, 0);
            break;
        case 0x1004: {
            uint32_t points;
            P32(global, 0x60c) = RD(record, 0);
            P8(SIM1_GLOBAL, 0x610) = P32(SIM1_GLOBAL, 0x60c) == 1;
            S32(0x23d0) = RD(record, 4);
            S32(0x2224) = RD(record, 8);
            S32(0x2228) = RD(record, 0xc);
            S32(0x222c) = RD(record, 0x10);
            S32(0x1f4c) = RD(record, 0x10);
            S32(0x2424) = RD(record, 0x14);
            S32(0x2428) = RD(record, 0x18);
            S32(0x242c) = RD(record, 0x1c);
            S32(0x4148) = RD(record, 0x20); S32(0x414c) = RD(record, 0x24);
            S32(0x2d70) = RD(record, 0x28); S32(0x2d74) = RD(record, 0x2c);
            S32(0x3c08) = RD(record, 0x30); S32(0x3c0c) = RD(record, 0x34);
            S32(0x2218) = RD(record, 0x38);
            S32(0x221c) = RD(record, 0x3c);
            S32(0x2220) = RD(record, 0x40);
            S32(0x2418) = RD(record, 0x44);
            S32(0x241c) = RD(record, 0x48);
            S32(0x2420) = RD(record, 0x4c);
            if (RD(record, 8)) {   /* the left leg mirrored */
                S32(0x1f48) = RD(record, 0xc);
                S32(0x1f44) = 0u - RD(record, 8);
                S32(0x1f4c) = S32(0x222c);
                S32(0x1f3c) = S32(0x221c);
                S32(0x1f38) = RD(record, 0x38);
                S32(0x1f40) = S32(0x2220);
            } else { S32(0x1f44) = 0; S32(0x1f48) = 0; S32(0x1f4c) = 0; }
            if (RD(record, 0x14)) {   /* the right one */
                S32(0x29fc) = RD(record, 0x48);
                S32(0x2a04) = 0u - RD(record, 0x14);
                S32(0x2a08) = S32(0x2428);
                S32(0x2a0c) = S32(0x242c);
                S32(0x29f8) = S32(0x2418);
                S32(0x2a00) = RD(record, 0x4c);
            } else { S32(0x2a04) = 0; S32(0x2a08) = 0; S32(0x2a0c) = 0; }
            points = sim1_contact_offsets(sim1_data + 0x2400, 0);
            points += sim1_contact_offsets(sim1_data + 0x1f20, 0);
            points += sim1_contact_offsets(sim1_data + 0x2200, 0);
            points += sim1_contact_offsets(sim1_data + 0x29e0, 0);
            if (P32(SIM1_GLOBAL, 0x60c) != 1) {
                P32(SIM1_GLOBAL, 0xbec) = 0x3fff; P32(SIM1_GLOBAL, 0xbf0) = 0x3fff; P32(SIM1_GLOBAL, 0xbf4) = 0x3fff; P32(SIM1_GLOBAL, 0xbe8) = 0x3fff;
            }
            if (points == 1 && !P32(SIM1_GLOBAL, 0x57c))
                P32(SIM1_GLOBAL, 0x57c) = SIM1_CALL(SIM1_UTIL, 0x70, uint32_t (__stdcall *)(void))() >> 31 != 1 ? 0xfff50000u : 0xb0000u;
            break;
        }
        case 0x1005: {
            long double stiffness;
            S32(0x27c4) = RD(record, 0x20); S32(0x22e4) = 0u - RD(record, 0x20);
            S32(0x27c8) = RD(record, 0x24); S32(0x22e8) = RD(record, 0x24);
            S32(0x27cc) = RD(record, 0x28); S32(0x22ec) = RD(record, 0x28);
            S32(0x27c0) = RD(record, 0x2c); S32(0x22e0) = RD(record, 0x2c);
            S32(0x2164) = RD(record, 0); S32(0x2168) = RD(record, 4); S32(0x216c) = RD(record, 8); S32(0x2160) = RD(record, 0xc);
            S32(0x2044) = RD(record, 0x10); S32(0x2048) = RD(record, 0x14); S32(0x204c) = RD(record, 0x18); S32(0x2040) = RD(record, 0x1c);
            S32(0x1e24) = RD(record, 0x30); S32(0x1e28) = RD(record, 0x34); S32(0x1e2c) = RD(record, 0x38); S32(0x1e20) = RD(record, 0x3c);
            S32(0x2704) = RD(record, 0x40); S32(0x2708) = RD(record, 0x44); S32(0x270c) = RD(record, 0x48); S32(0x2700) = RD(record, 0x4c);
            sim1_contact_offsets(sim1_data + 0x27a0, 1);
            sim1_contact_offsets(sim1_data + 0x22c0, 1);
            sim1_contact_offsets(sim1_data + 0x2140, 1);
            sim1_contact_offsets(sim1_data + 0x2020, 1);
            sim1_contact_offsets(sim1_data + 0x1e00, 1);
            sim1_contact_offsets(sim1_data + 0x26e0, 1);
            global = SIM1_GLOBAL;
            stiffness = (((long double)(int64_t)(uint64_t)(P32(global, 0xb78) + P32(global, 0xb98) + P32(global, 0xb90) + P32(global, 0xb88) + P32(global, 0xb80) + P32(global, 0xba8) + P32(global, 0xba0))
                          * (long double)(int32_t)PI16(global, 0xaf4)) * SF64(0x6c0) + (long double)(int64_t)(uint64_t)S32(0x30e4)) * RF64(0x15280);
            sim1_contact_ratio(0x2140, stiffness, 0, 0);
            sim1_contact_ratio(0x27a0, stiffness, 1, 0x22c0);
            sim1_contact_ratio(0x2020, stiffness, 0, 0);
            sim1_contact_ratio(0x1e00, stiffness, 0, 0);
            sim1_contact_ratio(0x26e0, stiffness, 0, 0);
            break;
        }
        case 0x340:
            S32(0x4054) = RD(record, 0x0);
            SF64(0x3860) = sim1_x87(RF(record, 0x4));
            SF64(0x4160) = sim1_x87(RF(record, 0xc));
            SF64(0x3fa8) = sim1_x87(RF(record, 0x14));
            break;
        case 0x325:
            S32(0x3584) = RD(record, 0x0);
            S32(0x3e10) = RD(record, 0x4);
            SF64(0x3f98) = sim1_x87(RF(record, 0x8));
            SF64(0x3e68) = sim1_x87(RF(record, 0x10));
            break;
        case 0x339:
            S32(0x423c) = RD(record, 0x0);
            S32(0x3e04) = RD(record, 0x4);
            break;
        case 0x320:
            S32(0x39b8) = RD(record, 0x0);
            S32(0x39bc) = RD(record, 0x4);
            SF64(0x34e8) = sim1_x87(RF(record, 0x8));
            SF64(0x4038) = sim1_x87(RF(record, 0x10));
            SF64(0x3708) = sim1_x87(RF(record, 0x18));
            SF64(0x3df8) = sim1_x87(RF(record, 0x20));
            SF64(0x4228) = sim1_x87(RF(record, 0x28));
            SF64(0x3de0) = sim1_x87(RF(record, 0x30));
            SF64(0x35a8) = sim1_x87(RF(record, 0x38));
            SF64(0x3b60) = sim1_x87(RF(record, 0x40));
            S32(0x3b68) = RD(record, 0x48);
            SF64(0x35a0) = sim1_x87(RF(record, 0x4c));
            SF64(0x38e8) = sim1_x87(RF(record, 0x54));
            SF64(0x4030) = sim1_x87(RF(record, 0x5c));
            SF64(0x4058) = sim1_x87(RF(record, 0x64));
            SF64(0x38a8) = sim1_x87(RF(record, 0x6c));
            SF64(0x2d80) = sim1_x87(RF(record, 0x74));
            SF64(0x4028) = sim1_x87(RF(record, 0x7c));
            SF64(0x3718) = sim1_x87(RF(record, 0x84));
            break;
        case 0x500:
            S16(0x4082) = RW(record, 0x0);
            S16(0x3800) = RW(record, 0x2);
            S16(0x3b88) = RW(record, 0x4);
            S16(0x4354) = RW(record, 0x6);
            S16(0x3e30) = RW(record, 0xa);
            S16(0x35b4) = RW(record, 0xc);
            S16(0x3cc0) = RW(record, 0xe);
            S16(0x38a2) = RW(record, 0x14);
            S16(0x3d10) = RW(record, 0x10);
            S16(0x43e0) = RW(record, 0x12);
            S16(0x4398) = RW(record, 0x16);
            S16(0x374e) = RW(record, 0x1c);
            S32(0x3b08) = (uint32_t)RW(record, 0x1e);
            S16(0x3fe8) = RW(record, 0x20);
            S32(0x4024) = (uint32_t)RW(record, 0x22);
            S32(0x2c14) = (uint32_t)RW(record, 0x2c);
            S16(0x2c7a) = RW(record, 0x2e);
            S16(0x3dd4) = RW(record, 0x30);
            S16(0x3530) = RW(record, 0x32);
            S16(0x2b54) = RW(record, 0x38);
            S16(0x2d3a) = RW(record, 0x3a);
            S16(0x36b2) = RW(record, 0x3e);
            S16(0x3fd0) = RW(record, 0x40);
            SF64(0x3710) = sim1_x87(RF(record, 0x24));
            P16(global, 0x8c8) = RW(record, 0x38);
            break;
        case 0x600:
            S16(0x4050) = RW(record, 0x0);
            S16(0x3fb0) = RW(record, 0x4);
            S16(0x3728) = RW(record, 0x8);
            S16(0x2d68) = RW(record, 0xc);
            S16(0x4154) = RW(record, 0x10);
            S16(0x3df0) = RW(record, 0x14);
            S16(0x3e02) = RW(record, 0x18);
            S16(0x3a50) = RW(record, 0x1c);
            S16(0x439a) = RW(record, 0x20);
            S16(0x374a) = RW(record, 0x24);
            S16(0x3582) = RW(record, 0x28);
            S32(0x3d50) = RD(record, 0x2c);
            S16(0x2c68) = RW(record, 0x30);
            S16(0x3540) = RW(record, 0x34);
            S16(0x3622) = RW(record, 0x38);
            S16(0x43e4) = RW(record, 0x3c);
            S32(0x3fa0) = RD(record, 0x40);
            S32(0x4104) = RD(record, 0x44);
            S16(0x30e2) = RW(record, 0x48);
            S16(0x2d7a) = RW(record, 0x4c);
            S32(0x2d64) = RD(record, 0x50);
            break;
        case 0x1001:
            S32(0x35b0) = RD(record, 0x0);
            S32(0x35bc) = RD(record, 0x4);
            S32(0x35e4) = RD(record, 0x8);
            S32(0x35b8) = RD(record, 0xc);
            break;
        case 0x1003:
            S32(0x3850) = RD(record, 0x0);
            S32(0x3854) = RD(record, 0x4);
            S32(0x3858) = RD(record, 0x8);
            S32(0x2c50) = RD(record, 0xc);
            S32(0x2c54) = RD(record, 0x10);
            S32(0x2c58) = RD(record, 0x14);
            S32(0x2b90) = RD(record, 0x18);
            S32(0x2b94) = RD(record, 0x1c);
            S32(0x2b98) = RD(record, 0x20);
            S32(0x3630) = RD(record, 0x24);
            S32(0x3634) = RD(record, 0x28);
            S32(0x3638) = RD(record, 0x2c);
            break;
        case 0x1002:
            P16(global, 0x8c2) = RW(record, 0x0);
            P16(global, 0x8c4) = RW(record, 0x4);
            P16(global, 0x8c6) = RW(record, 0x8);
            P16(global, 0x95a) = RW(record, 0xc);
            P16(global, 0x95c) = RW(record, 0x10);
            P16(global, 0x95e) = RW(record, 0x14);
            P16(global, 0x9f2) = RW(record, 0x18);
            P16(global, 0x9f4) = RW(record, 0x1c);
            P16(global, 0x9f6) = RW(record, 0x20);
            P16(global, 0xa8a) = RW(record, 0x24);
            P16(global, 0xa8c) = RW(record, 0x28);
            P16(global, 0xa8e) = RW(record, 0x2c);
            break;
        case 0x1101:
            S16(0x43d0) = (uint16_t)RB(record, 0x0);
            S16(0x3702) = RW(record, 0x2);
            S32(0x3d04) = RD(record, 0x4);
            S16(0x2c18) = RW(record, 0x8);
            S16(0x2c78) = RW(record, 0xa);
            SF64(0x43d8) = sim1_x87(RF(record, 0xc));
            SF64(0x4240) = sim1_x87(RF(record, 0x14));
            S32(0x30e4) = RD(record, 0x1c);
            S16(0x3802) = RW(record, 0x20);
            S32(0x3b50) = RD(record, 0x22);
            S16(0x43e2) = RW(record, 0x26);
            S16(0x2b60) = RW(record, 0x28);
            S16(0x363c) = RW(record, 0x2a);
            S16(0x3588) = RW(record, 0x2c);
            S16(0x35f0) = RW(record, 0x2e);
            S16(0x3810) = RW(record, 0x30);
            S16(0x3e00) = RW(record, 0x32);
            S16(0x4044) = RW(record, 0x34);
            S16(0x3bc2) = RW(record, 0x36);
            SF64(0x3818) = sim1_x87(RF(record, 0x38));
            SF64(0x2c60) = sim1_x87(RF(record, 0x40));
            SF64(0x3668) = sim1_x87(RF(record, 0x48));
            S16(0x4178) = RW(record, 0x50);
            S32(0x3dd8) = RD(record, 0x52);
            S32(0x3b58) = RD(record, 0x56);
            S16(0x2c28) = RW(record, 0x5a);
            S16(0x4152) = RW(record, 0x5c);
            SF64(0x3e20) = sim1_x87(RF(record, 0x5e));
            SF64(0x3b70) = sim1_x87(RF(record, 0x66));
            S16(0x3fc0) = RW(record, 0x6e);
            S16(0x3748) = RW(record, 0x70);
            S32(0x3738) = RD(record, 0x72);
            S32(0x2ca8) = RD(record, 0x76);
            S32(0x2cac) = RD(record, 0x7a);
            SF64(0x2c88) = sim1_x87(RF(record, 0x7e));
            S32(0x2c38) = RD(record, 0x86);
            S32(0x3f94) = RD(record, 0x8a);
            S32(0x4238) = RD(record, 0x8e);
            S32(0x3b54) = RD(record, 0x92);
            S32(0x2cb0) = RD(record, 0x96);
            SF64(0x2b58) = sim1_x87(RF(record, 0x9a));
            SF64(0x2b78) = sim1_x87(RF(record, 0xa2));
            SF64(0x3fd8) = sim1_x87(RF(record, 0xaa));
            SF64(0x2ab8) = sim1_x87(RF(record, 0xb2));
            SF64(0x2b70) = sim1_x87(RF(record, 0xba));
            SF64(0x37f8) = sim1_x87(RF(record, 0xc2));
            S32(0x2c2c) = RD(record, 0xca);
            S16(0x3842) = RW(record, 0xce);
            S32(0x39c0) = RD(record, 0xd0);
            SF64(0x38f8) = sim1_x87(RF(record, 0xd4));
            SF64(0x30b8) = sim1_x87(RF(record, 0xdc));
            S32(0x2d34) = RD(record, 0xe4);
            S16(0x2d38) = RW(record, 0xe8);
            S32(0x3b0c) = RD(record, 0xea);
            S16(0x4084) = RW(record, 0xee);
            S16(0x3dd6) = RW(record, 0xf0);
            S32(0x3094) = RD(record, 0xf2);
            S32(0x3098) = RD(record, 0xf6);
            P8(global, 0x608) = RB(record, 0x0);
            break;
        case 0x1199:
            S32(0x2520) = RD(record, 0x0);
            S32(0x2524) = RD(record, 0x4);
            SF64(0x2528) = sim1_x87(RF(record, 0x8));
            SF64(0x2530) = sim1_x87(RF(record, 0x10));
            SF64(0x2538) = sim1_x87(RF(record, 0x18));
            SF64(0x2540) = sim1_x87(RF(record, 0x20));
            SF64(0x2548) = sim1_x87(RF(record, 0x28));
            SF64(0x2550) = sim1_x87(RF(record, 0x30));
            SF64(0x2860) = sim1_x87(RF(record, 0x38));
            SF64(0x2868) = sim1_x87(RF(record, 0x40));
            SF64(0x2870) = sim1_x87(RF(record, 0x48));
            SF64(0x2878) = sim1_x87(RF(record, 0x50));
            SF64(0x2880) = sim1_x87(RF(record, 0x58));
            SF64(0x2888) = sim1_x87(RF(record, 0x60));
            SF64(0x2890) = sim1_x87(RF(record, 0x68));
            SF64(0x28c0) = sim1_x87(RF(record, 0x70));
            SF64(0x28c8) = sim1_x87(RF(record, 0x78));
            SF64(0x28d0) = sim1_x87(RF(record, 0x80));
            SF64(0x28d8) = sim1_x87(RF(record, 0x88));
            SF64(0x28e0) = sim1_x87(RF(record, 0x90));
            SF64(0x28e8) = sim1_x87(RF(record, 0x98));
            SF64(0x28f0) = sim1_x87(RF(record, 0xa0));
            SF64(0x2380) = sim1_x87(RF(record, 0xa8));
            SF64(0x2388) = sim1_x87(RF(record, 0xb0));
            SF64(0x2390) = sim1_x87(RF(record, 0xb8));
            SF64(0x2398) = sim1_x87(RF(record, 0xc0));
            SF64(0x23a0) = sim1_x87(RF(record, 0xc8));
            SF64(0x23a8) = sim1_x87(RF(record, 0xd0));
            SF64(0x23b0) = sim1_x87(RF(record, 0xd8));
            SF64(0x2920) = sim1_x87(RF(record, 0xe0));
            SF64(0x2928) = sim1_x87(RF(record, 0xe8));
            SF64(0x2930) = sim1_x87(RF(record, 0xf0));
            SF64(0x2938) = sim1_x87(RF(record, 0xf8));
            SF64(0x2940) = sim1_x87(RF(record, 0x100));
            SF64(0x2948) = sim1_x87(RF(record, 0x108));
            SF64(0x2950) = sim1_x87(RF(record, 0x110));
            SF64(0x2600) = sim1_x87(RF(record, 0x118));
            SF64(0x2608) = sim1_x87(RF(record, 0x120));
            SF64(0x2610) = sim1_x87(RF(record, 0x128));
            SF64(0x2618) = sim1_x87(RF(record, 0x130));
            SF64(0x2620) = sim1_x87(RF(record, 0x138));
            SF64(0x2628) = sim1_x87(RF(record, 0x140));
            SF64(0x2630) = sim1_x87(RF(record, 0x148));
            SF64(0x1d20) = sim1_x87(RF(record, 0x150));
            SF64(0x1d28) = sim1_x87(RF(record, 0x158));
            SF64(0x1d30) = sim1_x87(RF(record, 0x160));
            SF64(0x1d38) = sim1_x87(RF(record, 0x168));
            SF64(0x1d40) = sim1_x87(RF(record, 0x170));
            SF64(0x1d48) = sim1_x87(RF(record, 0x178));
            SF64(0x1d50) = sim1_x87(RF(record, 0x180));
            SF64(0x2980) = sim1_x87(RF(record, 0x188));
            SF64(0x2988) = sim1_x87(RF(record, 0x190));
            SF64(0x2990) = sim1_x87(RF(record, 0x198));
            SF64(0x2998) = sim1_x87(RF(record, 0x1a0));
            SF64(0x29a0) = sim1_x87(RF(record, 0x1a8));
            SF64(0x29a8) = sim1_x87(RF(record, 0x1b0));
            SF64(0x29b0) = sim1_x87(RF(record, 0x1b8));
            SF64(0x20e0) = sim1_x87(RF(record, 0x1c0));
            SF64(0x20e8) = sim1_x87(RF(record, 0x1c8));
            SF64(0x20f0) = sim1_x87(RF(record, 0x1d0));
            SF64(0x20f8) = sim1_x87(RF(record, 0x1d8));
            SF64(0x2100) = sim1_x87(RF(record, 0x1e0));
            SF64(0x2108) = sim1_x87(RF(record, 0x1e8));
            SF64(0x2110) = sim1_x87(RF(record, 0x1f0));
            SF64(0x1ec0) = sim1_x87(RF(record, 0x1f8));
            SF64(0x1ec8) = sim1_x87(RF(record, 0x200));
            SF64(0x1ed0) = sim1_x87(RF(record, 0x208));
            SF64(0x1ed8) = sim1_x87(RF(record, 0x210));
            SF64(0x1ee0) = sim1_x87(RF(record, 0x218));
            SF64(0x1ee8) = sim1_x87(RF(record, 0x220));
            SF64(0x1ef0) = sim1_x87(RF(record, 0x228));
            SF64(0x2660) = sim1_x87(RF(record, 0x230));
            SF64(0x2668) = sim1_x87(RF(record, 0x238));
            SF64(0x2670) = sim1_x87(RF(record, 0x240));
            SF64(0x2678) = sim1_x87(RF(record, 0x248));
            SF64(0x2680) = sim1_x87(RF(record, 0x250));
            SF64(0x2688) = sim1_x87(RF(record, 0x258));
            SF64(0x2690) = sim1_x87(RF(record, 0x260));
            SF64(0x25a0) = sim1_x87(RF(record, 0x268));
            SF64(0x25a8) = sim1_x87(RF(record, 0x270));
            SF64(0x25b0) = sim1_x87(RF(record, 0x278));
            SF64(0x25b8) = sim1_x87(RF(record, 0x280));
            SF64(0x25c0) = sim1_x87(RF(record, 0x288));
            SF64(0x25c8) = sim1_x87(RF(record, 0x290));
            SF64(0x25d0) = sim1_x87(RF(record, 0x298));
            SF64(0x24c0) = sim1_x87(RF(record, 0x2a0));
            SF64(0x24c8) = sim1_x87(RF(record, 0x2a8));
            SF64(0x24d0) = sim1_x87(RF(record, 0x2b0));
            SF64(0x24d8) = sim1_x87(RF(record, 0x2b8));
            SF64(0x24e0) = sim1_x87(RF(record, 0x2c0));
            SF64(0x24e8) = sim1_x87(RF(record, 0x2c8));
            SF64(0x24f0) = sim1_x87(RF(record, 0x2d0));
            SF64(0x1d80) = sim1_x87(RF(record, 0x2d8));
            SF64(0x1d88) = sim1_x87(RF(record, 0x2e0));
            SF64(0x1d90) = sim1_x87(RF(record, 0x2e8));
            SF64(0x1d98) = sim1_x87(RF(record, 0x2f0));
            SF64(0x1da0) = sim1_x87(RF(record, 0x2f8));
            SF64(0x1da8) = sim1_x87(RF(record, 0x300));
            SF64(0x1db0) = sim1_x87(RF(record, 0x308));
            SF64(0x1ca0) = sim1_x87(RF(record, 0x310));
            SF64(0x1ca8) = sim1_x87(RF(record, 0x318));
            SF64(0x1cb0) = sim1_x87(RF(record, 0x320));
            SF64(0x1cb8) = sim1_x87(RF(record, 0x328));
            SF64(0x1cc0) = sim1_x87(RF(record, 0x330));
            SF64(0x1cc8) = sim1_x87(RF(record, 0x338));
            SF64(0x1cd0) = sim1_x87(RF(record, 0x340));
            break;
        case 0x1202:
            S32(0x1dd0) = RD(record, 0x0);
            break;
        case 0x1204:
            S32(0x4158) = RD(record, 0x0);
            S16(0x38f2) = RW(record, 0x4);
            S16(0x3b78) = RW(record, 0x8);
            S16(0x374c) = RW(record, 0xc);
            S16(0x3900) = RW(record, 0xe);
            S16(0x2c10) = RW(record, 0x10);
            S32(0x2d90) = RD(record, 0x12);
            S16(0x3ddc) = RW(record, 0x16);
            S16(0x2c98) = RW(record, 0x1a);
            break;
        case 0x1205:
            S16(0x38f0) = RW(record, 0x0);
            S16(0x36b0) = RW(record, 0x4);
            S16(0x36d0) = RW(record, 0x8);
            S16(0x2d78) = RW(record, 0xc);
            S16(0x3812) = RW(record, 0x10);
            break;
        case 0x1206:
            S16(0x3a52) = RW(record, 0x0);
            S16(0x2c6a) = RW(record, 0x4);
            S16(0x4150) = RW(record, 0x8);
            S16(0x3750) = RW(record, 0xc);
            break;
        case 0x1203:
            S32(0x1780) = RD(record, 0x0);
            S32(0x1784) = RD(record, 0x4);
            SF64(0x1788) = sim1_x87(RF(record, 0x8));
            SF64(0x1790) = sim1_x87(RF(record, 0x10));
            SF64(0x1798) = sim1_x87(RF(record, 0x18));
            S16(0x1fe0) = RW(record, 0x20);
            S16(0x1f10) = RW(record, 0x22);
            break;
        case 0x1400:
            S32(0x36b8) = RD(record, 0x0);
            S32(0x36bc) = RD(record, 0x4);
            S32(0x3b80) = RD(record, 0x8);
            S32(0x3b84) = RD(record, 0xc);
            S32(0x2c70) = RD(record, 0x10);
            S32(0x2c74) = RD(record, 0x14);
            S32(0x2c80) = RD(record, 0x18);
            S32(0x2c84) = RD(record, 0x1c);
            S32(0x2ac0) = RD(record, 0x20);
            S32(0x2ac4) = RD(record, 0x24);
            S32(0x3808) = RD(record, 0x28);
            S32(0x380c) = RD(record, 0x2c);
            S32(0x3b48) = RD(record, 0x30);
            S32(0x3b4c) = RD(record, 0x34);
            S32(0x3fc8) = RD(record, 0x38);
            S32(0x3fcc) = RD(record, 0x3c);
            break;
        case 0x1401:
            S32(0x4170) = RD(record, 0x0);
            S32(0x4174) = RD(record, 0x4);
            S32(0x4390) = RD(record, 0x8);
            S32(0x4394) = RD(record, 0xc);
            S32(0x3af8) = RD(record, 0x10);
            S32(0x3afc) = RD(record, 0x14);
            S32(0x3d08) = RD(record, 0x18);
            S32(0x3d0c) = RD(record, 0x1c);
            S32(0x3538) = RD(record, 0x20);
            S32(0x353c) = RD(record, 0x24);
            S32(0x2b80) = RD(record, 0x28);
            S32(0x2b84) = RD(record, 0x2c);
            S32(0x2c90) = RD(record, 0x30);
            S32(0x2c94) = RD(record, 0x34);
            S32(0x3fe0) = RD(record, 0x38);
            S32(0x3fe4) = RD(record, 0x3c);
            break;
        case 0x1402:
            S32(0x4388) = RD(record, 0x0);
            S32(0x438c) = RD(record, 0x4);
            S32(0x3c10) = RD(record, 0x8);
            S32(0x3c14) = RD(record, 0xc);
            S32(0x3e08) = RD(record, 0x10);
            S32(0x3e0c) = RD(record, 0x14);
            S32(0x43e8) = RD(record, 0x18);
            S32(0x43ec) = RD(record, 0x1c);
            S32(0x4048) = RD(record, 0x20);
            S32(0x404c) = RD(record, 0x24);
            S32(0x3d48) = RD(record, 0x28);
            S32(0x3d4c) = RD(record, 0x2c);
            S32(0x3b00) = RD(record, 0x30);
            S32(0x3b04) = RD(record, 0x34);
            S32(0x2c30) = RD(record, 0x38);
            S32(0x2c34) = RD(record, 0x3c);
            S32(0x2c20) = RD(record, 0x40);
            S32(0x2c24) = RD(record, 0x44);
            S32(0x43f0) = RD(record, 0x48);
            S32(0x43f4) = RD(record, 0x4c);
            S32(0x3720) = RD(record, 0x50);
            S32(0x3724) = RD(record, 0x54);
            S32(0x30a0) = RD(record, 0x58);
            S32(0x30a4) = RD(record, 0x5c);
            S32(0x3598) = RD(record, 0x60);
            S32(0x359c) = RD(record, 0x64);
            S32(0x36c8) = RD(record, 0x68);
            S32(0x36cc) = RD(record, 0x6c);
            S32(0x36a8) = RD(record, 0x70);
            S32(0x36ac) = RD(record, 0x74);
            S32(0x2c08) = RD(record, 0x78);
            S32(0x2c0c) = RD(record, 0x7c);
            break;
        case 0x1403:
            S32(0x3528) = RD(record, 0x0);
            S32(0x352c) = RD(record, 0x4);
            S32(0x3de8) = RD(record, 0x8);
            S32(0x3dec) = RD(record, 0xc);
            S32(0x3a48) = RD(record, 0x10);
            S32(0x3a4c) = RD(record, 0x14);
            S32(0x4168) = RD(record, 0x18);
            S32(0x416c) = RD(record, 0x1c);
            S32(0x30b0) = RD(record, 0x20);
            S32(0x30b4) = RD(record, 0x24);
            S32(0x3590) = RD(record, 0x28);
            S32(0x3594) = RD(record, 0x2c);
            S32(0x3740) = RD(record, 0x30);
            S32(0x3744) = RD(record, 0x34);
            S32(0x43c8) = RD(record, 0x38);
            S32(0x43cc) = RD(record, 0x3c);
            S32(0x4230) = RD(record, 0x40);
            S32(0x4234) = RD(record, 0x44);
            S32(0x2c40) = RD(record, 0x48);
            S32(0x2c44) = RD(record, 0x4c);
            S32(0x35e8) = RD(record, 0x50);
            S32(0x35ec) = RD(record, 0x54);
            S32(0x2ca0) = RD(record, 0x58);
            S32(0x2ca4) = RD(record, 0x5c);
            S32(0x2d88) = RD(record, 0x60);
            S32(0x2d8c) = RD(record, 0x64);
            break;
        case 0x1404:
            S32(0x3e28) = RD(record, 0x0);
            S32(0x3e2c) = RD(record, 0x4);
            S32(0x3fb8) = RD(record, 0x8);
            S32(0x3fbc) = RD(record, 0xc);
            S32(0x3e18) = RD(record, 0x10);
            S32(0x3e1c) = RD(record, 0x14);
            S32(0x30a8) = RD(record, 0x18);
            S32(0x30ac) = RD(record, 0x1c);
            S32(0x3cb8) = RD(record, 0x20);
            S32(0x3cbc) = RD(record, 0x24);
            S32(0x3730) = RD(record, 0x28);
            S32(0x3734) = RD(record, 0x2c);
            break;
        default:
            break;
        }
next:
        offset += length;
    } while ((int32_t)offset < size);
    global = SIM1_GLOBAL;
    if (P16(global, 0xaec) > engines) {
        if (P16(global, 0xaec) > 1) sim1_copy_contact(global + 0x924, global + 0x88c);
        if (P16(SIM1_GLOBAL, 0xaec) > 2) sim1_copy_contact(SIM1_GLOBAL + 0x9bc, SIM1_GLOBAL + 0x88c);
        if (P16(SIM1_GLOBAL, 0xaec) > 3) sim1_copy_contact(SIM1_GLOBAL + 0xa54, SIM1_GLOBAL + 0x88c);
    }
    {
        static const uint16_t starters[4] = {0x892, 0x92a, 0x9c2, 0xa5a};
        global = SIM1_GLOBAL;
        if (!P8(global, 0x609)) { for (at = 0; at < 4; ++at) if (P16(SIM1_GLOBAL, starters[at]) > 4) P16(SIM1_GLOBAL, starters[at]) = 3; }
        else if (P8(global, 0x609) == 1) { for (at = 0; at < 4; ++at) if (P16(SIM1_GLOBAL, starters[at]) > 2) P16(SIM1_GLOBAL, starters[at]) = 2; }
    }
    if (!P32(SIM1_GLOBAL, 0x7a0)) P32(SIM1_GLOBAL, 0x7ac) = 0;
    if (!P32(SIM1_GLOBAL, 0x7a4)) { P32(SIM1_GLOBAL, 0x7b0) = 0; P32(SIM1_GLOBAL, 0x7b4) = 0; }
    P32(SIM1_GLOBAL, 0x368) = 0xfff;
    sim1_angle_2b68();
    sim1_pass_setup();
    return 1;
}
#undef RB
#undef RW
#undef RS16
#undef RD
#undef RF
#undef SIM1_MEMMOVE

/* 0x2200d790, stdcall (two arguments, unread): the engines' frame: the pitch schedule
 * (0x2200d7f0); unless parked (GLOBAL +0x365) or slewing (+0x268), the ground systems or the
 * turbulence (0x2200ac4a) and the engines by type (+0x609: 0 pistons, 1 jets). */
void __stdcall sim1_engines_frame(uint32_t first, uint32_t second)
{
    uint8_t *global = SIM1_GLOBAL;
    (void)first; (void)second;
    sim1_pitch_schedule();
    if (P8(global, 0x365) || P32(global, 0x268)) return;
    sim1_ground_or_turbulence(global);
    if (!P8(global, 0x609)) sim1_pistons(global);
    else if (P8(global, 0x609) == 1) sim1_jets(global);
}

/* 0x2200ae90: the aircraft's .AIR file loaded: its path (ACONTAIN +0x30 from GLOBAL +0x11e0),
 * opened (HOST +0x17c), sized, read whole into an allocation and decoded (0x2200af90), freed
 * and closed. Returns the decoder's 1, else 0. RETAIL BUG (kept): SIM1-AIR-SIZE-HANDLE
 * (docs/retail-bugs.md): when sizing fails the file is left open. */
uint32_t sim1_load_air(void)
{
    char path[0x100];
    uint32_t file, size, got, result = 0;
    uint8_t *buffer;
    SIM1_CALL(SIM1_ACONTAIN, 0x30, uint32_t (__stdcall *)(void *, uint32_t, char *, uint32_t))(SIM1_GLOBAL + 0x11e0, 2, path, 0x100);
    if (SIM1_CALL(SIM1_HOST, 0x17c, uint32_t (__stdcall *)(uint32_t *, char *, uint32_t, uint32_t))(&file, path, 0xf, 0)) return 0;
    if (SIM1_CALL(SIM1_HOST, 0x1a4, uint32_t (__stdcall *)(uint32_t, uint32_t *))(file, &size)) return 0;
    SIM1_CALL(SIM1_HOST, 0x20c, uint32_t (__stdcall *)(uint32_t, uint32_t, uint8_t **, uint32_t))(1, size, &buffer, 0);
    if (!SIM1_CALL(SIM1_HOST, 0x19c, uint32_t (__stdcall *)(uint32_t, uint8_t *, uint32_t, uint32_t *))(file, buffer, size, &got) && got == size)
        result = sim1_air_decode(buffer, (int32_t)size);
    SIM1_CALL(SIM1_HOST, 0x214, uint32_t (__stdcall *)(void *))(&buffer);
    SIM1_CALL(SIM1_HOST, 0x13c, uint32_t (__stdcall *)(uint32_t))(file);
    return result;
}

/* 0x22013b30: the tail rotor's blades, as 0x220127b0 does the main rotor's: the density ratio
 * to a power (+0x1b70), the inflow (+0x1b60, +0x1ba0, +0x1b68 and its inverse +0x1b80), the
 * flapping by the azimuth +0x19c0 (+0x1b48, +0x1b50, +0x1b90, +0x1b88, +0x1b20, +0x1b18), the
 * coning (+0x1bb8, +0x1bc0), the advance ratios (+0x1b28, +0x1b30, +0x1b38) and their powers,
 * the torque and thrust terms (+0x1bb0, +0x1b98, +0x1bd0) and the rotor's forces +0x1c18,
 * +0x1be8, +0x1c38. Retail keeps some terms unrounded on the x87 and parks others in its frame;
 * the C does the same. */
void sim1_tail_rotor_blades(void)
{
    long double value, inflow, base, work, power28;
    double l10_inflow, l0_cos, l8_sin, l0_ratio, l18_density, l8_advance, l10_power, l8_power, l10_area, l18_scale;
    value = sim1_cipow(SF64(0x3590), SF64(0x1008));
    SF64(0x1b78) = (double)((long double)SF64(0x3590) * SF64(0xe38));
    inflow = (sim1_fsin(SF64(0x1ad8)) * SF64(0x1ab8) - SF64(0x1b58)) / SF64(0x1b78);
    SF64(0x1b60) = (double)inflow;
    SF64(0x1b70) = (double)(value * SF64(0x798) * SF64(0x35e8) / SF64(0x4230));
    SF64(0x1ba0) = (double)((long double)SF64(0x1ac8) / SF64(0x1b78));
    l10_inflow = (double)sim1_cipow(inflow, RF64(0x15670));
    value = sim1_cipow(sim1_cipow(SF64(0x1b58), RF64(0x15670)) + l10_inflow, RF64(0x15678));
    SF64(0x1b68) = (double)value;
    if (X87_C3(value, (long double)RF64(0x15680))) { S32(0x1b80) = 0; S32(0x1b84) = 0; }
    else SF64(0x1b80) = (double)((long double)RF64(0x15688) / SF64(0x1b68));
    l0_cos = (double)sim1_fcos(SF64(0x19c0));
    l8_sin = (double)sim1_fsin(SF64(0x19c0));
    SF64(0x1b48) = (double)(((long double)SF64(0x1b28) - (long double)SF64(0x1b30) * l0_cos) - (long double)SF64(0x1b38) * l8_sin);
    SF64(0x1b50) = (double)(((long double)SF64(0x1b30) * l8_sin - (long double)SF64(0x1b38) * l0_cos) * SF64(0xe38));
    {
        long double moment = (long double)SF64(0x1b78) * SF64(0x1b60);
        SF64(0x1b90) = (double)(moment - (moment * SF64(0x1b48)) * l0_cos);
    }
    work = (long double)SF64(0x1b90) - (long double)SF64(0x1b50) * SF64(0x3590);
    SF64(0x1b90) = (double)work;
    SF64(0x1b88) = (double)(((long double)SF64(0x1ba0) * SF64(0x1b78)) * l8_sin + SF64(0x1b78));
    SF64(0x1b58) = RF64(0x15690);
    SF64(0x1b20) = (double)((((long double)SF64(0x1bb8) * RF64(0x15678)) * SF64(0x1b80) + SF64(0x1b60)) / SF64(0x1ba0));
    SF64(0x1b18) = (double)(work / SF64(0x1b88) + SF64(0x1c40));
    l0_ratio = (double)sim1_cipow(SF64(0x1ba0), RF64(0x15670));
    l18_density = (double)((long double)SF64(0x798) * SF64(0x35e8));
    base = (((long double)SF64(0x1c40) * l0_ratio) * RF64(0x15678) + (long double)SF64(0x1c40) * RF64(0x15698)) - (long double)SF64(0x1b60) * RF64(0x156a0);
    SF64(0x1bb8) = (double)base;
    SF64(0x1bc0) = (double)((base * SF64(0x2c40)) * (((long double)SF64(0x798) * SF64(0x35e8)) * RF64(0x15678)));
    l8_advance = (double)((long double)l0_ratio - RF64(0x156a8));
    SF64(0x1b28) = (double)(((((long double)SF64(0x1c40) * l8_advance) * RF64(0x156b0) + (long double)SF64(0x1b60) * RF64(0x156b8)) * SF64(0x1b70)) * RF64(0x15678));
    value = ((long double)SF64(0x3740) * SF64(0x3590)) * SF64(0x1bc0);
    SF64(0x1bc0) = (double)value;
    SF64(0x1b38) = (double)((((long double)SF64(0x1ba0) * SF64(0x1b28)) * RF64(0x156c8)) / (((long double)RF64(0x15688) - (long double)l0_ratio * RF64(0x156a0)) * RF64(0x156d0)));
    SF64(0x1b30) = (double)(((((long double)SF64(0x1b60) + SF64(0x1b60)) + (long double)SF64(0x1c40) * RF64(0x156c0)) * SF64(0x1ba0)) / ((long double)RF64(0x15688) - (long double)l0_ratio * RF64(0x15678)));
    SF64(0x1bb0) = (double)((((((long double)SF64(0x2ca0) * RF64(0x156b0)) / SF64(0x35e8)) * l8_advance) - ((long double)SF64(0x1b60) * SF64(0x1c40)) * RF64(0x156b8)) - (long double)l10_inflow * RF64(0x15678));
    SF64(0x1c18) = (double)(((((value * SF64(0x1b78)) * SF64(0x1b78)) * SF64(0x43c8)) * SF64(0x3740)));
    l10_power = (double)sim1_cipow(SF64(0x1b38), RF64(0x15670));
    l8_power = (double)sim1_cipow(SF64(0x1b30), RF64(0x15670));
    power28 = sim1_cipow(SF64(0x1b28), RF64(0x15670));
    l10_area = (double)((long double)SF64(0x1ba0) * SF64(0x1b60));
    SF64(0x1b98) = (double)(((((long double)l8_power * RF64(0x156d8) + (long double)l10_power * RF64(0x156e0)) + power28 * RF64(0x15678)) * RF64(0x15678)) * l0_ratio);
    SF64(0x1bb0) = (double)(((long double)SF64(0x1bb0) - ((long double)l10_power + l8_power) * RF64(0x156e0)) - SF64(0x1b98));
    work = ((long double)SF64(0x1bb0) - ((long double)SF64(0x1b30) * l10_area) * RF64(0x15678)) + (((long double)SF64(0x1ba0) * SF64(0x1b38)) * SF64(0x1b28)) * RF64(0x156e8);
    SF64(0x1bb0) = (double)work;
    l18_scale = (double)((long double)SF64(0x3590) * l18_density);
    SF64(0x1bd0) = (double)((((long double)SF64(0x1ba0) * SF64(0x2ca0)) * RF64(0x15678)) / SF64(0x35e8) + ((long double)SF64(0x1b30) * SF64(0x1c40)) * RF64(0x156e8));
    SF64(0x1be8) = (double)((((work * l18_scale) * RF64(0x15678)) * SF64(0x3590)) * ((long double)SF64(0x1b78) * SF64(0x1b78)));
    SF64(0x1bd0) = (double)((((long double)SF64(0x1bd0) - ((long double)SF64(0x1c40) * l10_area) * RF64(0x15678)) + ((long double)SF64(0x1ba0) * l8_power) * RF64(0x156b0))
                            + ((long double)SF64(0x1b60) * SF64(0x1b30)) * RF64(0x156f0));
    work = ((long double)SF64(0x1bd0) - ((long double)SF64(0x1b38) * SF64(0x1b28)) * RF64(0x156f8)) + ((long double)SF64(0x1ba0) * power28) * RF64(0x156b0);
    SF64(0x1bd0) = (double)work;
    SF64(0x1c38) = (double)((((((work * l18_scale) * RF64(0x15678)) * SF64(0x43c8)) * SF64(0x3740)) * ((long double)SF64(0x1b78) * SF64(0x1b78))));
}

/* 0x220135f0: the helicopter's power and torque. With collective (+0x1c00) at or above
 * R15550 the governed engine: a lag on the table value of the rotor's load (+0x1b00), the
 * power table (0xe50) against it, and the rotor speed (+0xe40) falling by the shortfall,
 * capped at its nominal (+0x2c08). Below, the idle branch: +0x1b00 follows collective,
 * the power is the lesser of the table and a collective line (plus a forward-speed term
 * when +0x1ad0 is positive), accumulated through +0x1b08, and the rotor speed held
 * between 15.64 and 41.36. Then the torque (+0x1aa8), its six coefficients
 * (+0xe38, +0x1a58..+0x1a80) and three tables (0xeb8, 0xf20, 0xf88) for the tail rotor,
 * the yaw lag (+0x1b10, +0x1ab0) and GLOBAL+0x918. */
void sim1_heli_power(void)
{
    uint8_t *global;
    long double root, power, load, value, near0, near1, t;
    double l0, l8, l10, l18, l20, l28, w;
    root = sim1_fsqrt(SF64(0x7a0));
    power = (long double)SF64(0x688) * SF64(0x1c78);
    load = (((long double)SF64(0x1c08) + SF64(0x1be8)) * RF64(0x15540)) / SF64(0x7a8);
    l10 = (double)power;
    l0 = (double)load;
    l20 = (double)root;
    l18 = (double)(power * power);
    l8 = (double)((long double)SF64(0x3d48) * SF64(0xe40));
    if (X87_C0(load, (long double)RF64(0x15548))) l0 = 0.0;
    if (!X87_C0((long double)SF64(0x1c00), (long double)RF64(0x15550))) {
        l28 = (double)sim1_table_lookup(sim1_data + 0xeb8, l0);
        SF64(0x1b00) = (double)((long double)SF64(0x1b00) - ((long double)SF64(0x1b00) - l28) * RF64(0x15558));
        t = sim1_table_lookup(sim1_data + 0xe50, SF64(0x1b00));
        SF64(0xff8) = (double)t;
        value = (long double)SF64(0x2c08) - ((t - l0) * RF64(0x15560)) / ((long double)SF64(0x2c20) * SF64(0x2c30));
        SF64(0xe40) = (double)value;
        if (!X87_C0C3(value, (long double)SF64(0x2c08))) SF64(0xe40) = SF64(0x2c08);
        near0 = (((((long double)l8 * l8) * SF64(0x798)) * SF64(0x3d48)) * RF64(0x155a0)) - RF64(0x155a8);
        near1 = (((((long double)l10 * RF64(0x15570)) - (((long double)l18 * l10) * RF64(0x15578))) - (long double)l18 * RF64(0x15580)) - RF64(0x15588))
                - ((long double)RF64(0x15590) - SF64(0x1c00)) * RF64(0x15598);
        SF64(0x1a98) = (double)((long double)SF64(0xe40) * RF64(0x15568));
    } else {
        long double follow, bound;
        follow = (long double)SF64(0x1c00) * RF64(0x155b0) - RF64(0x155b8);
        l10 = (double)follow;
        l0 = (double)(((fabsl((long double)SF64(0x1c08)) + fabsl((long double)SF64(0x1be8))) * RF64(0x15540)) / SF64(0x7a8));
        bound = (long double)RF64(0x155d0) - (long double)SF64(0x1c00) * RF64(0x155c8);
        value = (follow - SF64(0xe40)) * RF64(0x155c0) + SF64(0x1b00);
        SF64(0x1b00) = (double)value;
        if (!X87_C0C3(value, bound)) SF64(0x1b00) = (double)bound;
        if (X87_C0((long double)SF64(0x1b00), (long double)RF64(0x15548))) SF64(0x1b00) = 0.0;
        t = sim1_table_lookup(sim1_data + 0xe50, SF64(0x1b00));
        SF64(0xff8) = (double)t;
        l18 = (double)((long double)RF64(0x155e0) - (long double)SF64(0x1c00) * RF64(0x155d8));
        w = X87_C0C3(t, (long double)l18) ? SF64(0xff8) : l18;
        value = w;
        if (!X87_C0C3((long double)SF64(0x1ad0), (long double)RF64(0x15548)))
            value = value + (((long double)SF64(0x1ab8) * SF64(0x1c30)) * RF64(0x155e8)) * ((long double)SF64(0xe40) / SF64(0x2c08));
        value = ((long double)SF64(0x1b08) + value) * RF64(0x155f0);
        SF64(0xff8) = (double)value;
        SF64(0x1b08) = SF64(0xff8);
        SF64(0xe40) = (double)((long double)SF64(0xe40) - ((value - l0) * RF64(0x15560)) / ((long double)SF64(0x2c20) * SF64(0x2c30)));
        if (!X87_C0C3((long double)SF64(0xe40), (long double)RF64(0x155f8))) { S32(0xe40) = 0x7ae147ae; S32(0xe44) = 0x4044ae14; }
        else if (X87_C0C3((long double)SF64(0xe40), (long double)RF64(0x15600))) { S32(0xe40) = 0x147ae148; S32(0xe44) = 0x402f47ae; }
        near0 = (((((long double)l8 * l8) * SF64(0x798)) * SF64(0x3d48)) * RF64(0x155a0)) - RF64(0x15618);
        near1 = (long double)RF64(0x15610) - (long double)SF64(0x1c00) * RF64(0x15608);
        SF64(0x1a98) = (double)((long double)l10 * RF64(0x15568));
    }
    SF64(0x1aa8) = (double)(near1 * near0);
    {
        long double rotor = SF64(0xe40);
        double torque_arg = (double)((long double)SF64(0x1aa8) * RF64(0x15658));
        double t3, t4;
        long double t5, old;
        SF64(0xe38) = (double)(rotor * RF64(0x15620));
        SF64(0x1a60) = (double)(rotor * RF64(0x15628));
        SF64(0x1a58) = RF64(0x15630);
        SF64(0x1a70) = (double)(rotor * RF64(0x15638));
        SF64(0x1a68) = RF64(0x15640);
        SF64(0x1a78) = RF64(0x15648);
        SF64(0x1a80) = RF64(0x15650);
        t3 = (double)sim1_table_lookup(sim1_data + 0xeb8, torque_arg);
        t4 = (double)sim1_table_lookup(sim1_data + 0xf20, t3);
        t5 = sim1_table_lookup(sim1_data + 0xf88, t4) * SF64(0x7a0);
        old = SF64(0x1b10);
        l18 = (double)t5;
        SF64(0x1aa0) = (double)((long double)SF64(0xe40) * RF64(0x15568));
        SF64(0x1a90) = (double)(((long double)t4 * l20) * RF64(0x15660));
        SF64(0x1ab0) = (double)(old - (old - l18) * RF64(0x15668));
        SF64(0x1b10) = l18;
        global = SIM1_GLOBAL;
        PF64(global, 0x918) = (double)(((long double)t3 * l20) * SF64(0x7a8));
    }
}

/* 0x22013340: the helicopter's frame. The forward speed (+0x1450, at least 0.01) and the
 * body's rates and speeds into the rotor's inputs (+0x1ab8..+0x1af8, +0x1ae8 from GLOBAL
 * +0x57c), the rotor's routines in their order, the rotor's forces and moments back to the
 * body (+0x1228..+0x1238, +0x12c8..+0x12d8), and its outputs to GLOBAL as integers (scaled
 * by +0x6e8; the four words at +0x896..+0x8ba and seven dwords at +0x8f0..+0x908). */
void sim1_rotor_frame(void)
{
    uint8_t *global;
    if (!X87_C0C3((long double)SF64(0x1450), (long double)RF64(0x15528))) SF64(0x1ab8) = SF64(0x1450);
    else { S32(0x1ab8) = 0x47ae147b; S32(0x1abc) = 0x3f847ae1; }
    SF64(0x1ac0) = SF64(0x1388);
    SF64(0x1ac8) = SF64(0x1390);
    SF64(0x1ad0) = (double)sim1_fchs(SF64(0x1398));
    SF64(0x1ad8) = SF64(0x1498);
    SF64(0x1af0) = SF64(0x1510);
    SF64(0x1af8) = SF64(0x15a8);
    SF64(0x1ae0) = SF64(0x1490);
    global = SIM1_GLOBAL;
    SF64(0x1ae8) = (double)sim1_fchs(((long double)P32(global, 0x57c) * SF64(0x690)) * SF64(0x680));
    sim1_rotor_mixing();
    sim1_rotor_terms();
    sim1_rotor_blades();
    sim1_tail_rotor_blades();
    sim1_rotor_forces();
    sim1_heli_power();
    SF64(0x1228) = SF64(0x1c68);
    SF64(0x1230) = (double)sim1_fchs(SF64(0x1be0));
    SF64(0x1238) = SF64(0x1c60);
    SF64(0x12d0) = (double)sim1_fchs(SF64(0x1c20));
    SF64(0x12c8) = (double)sim1_fchs(SF64(0x1c50));
    SF64(0x12d8) = SF64(0x1c10);
    P16(SIM1_GLOBAL, 0x898) = (uint16_t)sim1_ftol((long double)SF64(0x6e8) * SF64(0x1a98));
    P16(SIM1_GLOBAL, 0x896) = (uint16_t)sim1_ftol((long double)SF64(0x6e8) * SF64(0x1a90));
    P16(SIM1_GLOBAL, 0x8b8) = (uint16_t)sim1_ftol(((long double)SF64(0x6e8) * SF64(0x1a58)) * RF64(0x15530));
    P16(SIM1_GLOBAL, 0x8ba) = (uint16_t)sim1_ftol(((long double)SF64(0x6e8) * SF64(0x1a60)) * RF64(0x15538));
    P32(SIM1_GLOBAL, 0x908) = (uint32_t)sim1_ftol((long double)SF64(0x6e8) * SF64(0x1aa0));
    P32(SIM1_GLOBAL, 0x8f0) = (uint32_t)sim1_ftol((long double)SF64(0x6e8) * SF64(0x1ab0));
    P32(SIM1_GLOBAL, 0x8f4) = (uint32_t)sim1_ftol((long double)SF64(0x6e8) * SF64(0x1aa8));
    P32(SIM1_GLOBAL, 0x8f8) = (uint32_t)sim1_ftol((long double)SF64(0x6e8) * SF64(0x1a80));
    P32(SIM1_GLOBAL, 0x904) = (uint32_t)sim1_ftol((long double)SF64(0x6e8) * SF64(0x1a68));
    P32(SIM1_GLOBAL, 0x900) = (uint32_t)sim1_ftol((long double)SF64(0x6e8) * SF64(0x1a70));
    P32(SIM1_GLOBAL, 0x8fc) = (uint32_t)sim1_ftol((long double)SF64(0x6e8) * SF64(0x1a78));
}

/* A 48-bit position (a word of fraction at +0, the whole part's dword at +2) moved by a
 * 64-bit step in the same units: fistp QWORD, then shld edx,eax,16 and add WORD / adc DWORD. */
static void sim1_position_add(uint8_t *position, int64_t step)
{
    uint32_t low = (uint32_t)P16(position, 0) + (uint16_t)step;
    P16(position, 0) = (uint16_t)low;
    P32(position, 2) += (uint32_t)((uint64_t)step >> 16) + (low >> 16);
}

/* 0x22002752: the simulation's step (edi GLOBAL). The attitude's trigonometry, and when the
 * attitude's high words (+0x578..+0x580) moved since the last step (+0x122..+0x126) the
 * quaternion, the angles as doubles (+0x1538..+0x1548) and the trigonometry again; the flow
 * (or, with +0x604 1, the helicopter's frame), the stall warning, the ground, the gear,
 * the structure, the sums, the accelerations and the integration; the velocities kept
 * (+0x13d0..+0x13e0) and the attitude stepped; the ground speed (+0x2b4); and unless paused
 * (+0x830 with +0x832 and not +0x83c), the position moved by the mean of the old and new
 * velocities over the frame's time (+0x16f0) times the rate (+0xc1a / 0x400, or 1 before
 * +0x1d18 reaches 0xf0000; east-west over the cosine of the latitude), the attitude's high
 * words kept and WINDOW's matrix rebuilt (+0xd18). Returns edi, GLOBAL. */
uint8_t *sim1_simulation_step(uint8_t *edi)
{
    uint8_t *global;
    uint16_t rate;
    long double step;
    int64_t whole;
    sim1_attitude_trig();
    if ((uint16_t)(P32(edi, 0x578) >> 16) != sim1_u16(0x122) || (uint16_t)(P32(edi, 0x57c) >> 16) != sim1_u16(0x124)
        || (uint16_t)(P32(edi, 0x580) >> 16) != sim1_u16(0x126)) {
        sim1_quaternion();
        S32(0x1550) = P32(edi, 0x578);
        SF64(0x1538) = (double)(((long double)SI32(0x1550) * SF64(0x680)) * SF64(0x690));
        S32(0x1554) = P32(edi, 0x57c);
        SF64(0x1540) = (double)(((long double)SI32(0x1554) * SF64(0x680)) * SF64(0x690));
        S32(0x1558) = P32(edi, 0x580);
        SF64(0x1548) = (double)(((long double)SI32(0x1558) * SF64(0x680)) * SF64(0x690));
        sim1_set_7c0();
        sim1_attitude_trig();
    }
    global = SIM1_GLOBAL;
    if (P32(global, 0x604) == 1) sim1_rotor_frame();
    else sim1_local_flow(global);
    sim1_stall_warning();
    sim1_ground_pass(global);
    sim1_gear_force();
    sim1_structure();
    sim1_vector_sums();
    sim1_body_accelerations();
    sim1_integrate();
    sim1_attitude_trig();
    SF64(0x13d0) = SF64(0x13b8);
    SF64(0x13d8) = SF64(0x13c0);
    SF64(0x13e0) = SF64(0x13c8);
    sim1_park();
    sim1_attitude_step();
    P32(global, 0x2b4) = (uint32_t)sim1_fistp32((sim1_fsqrt((long double)SF64(0x13b8) * SF64(0x13b8) + (long double)SF64(0x13c8) * SF64(0x13c8)) * SF64(0x728)) * SF64(0x6d8));
    if (P16(global, 0x830) && !P32(global, 0x83c) && P16(global, 0x832)) return global;
    if (SI32(0x1d18) < 0xf0000) rate = 1;
    else {
        rate = (uint16_t)(P16(global, 0xc1a) / 0x400);
        if (!rate) rate = 1;
    }
    sim1_set_u16(0xe0, rate);
    sim1_set_f64(0xe2, (double)(int16_t)rate);
    step = (((((long double)SF64(0x13c8) + SF64(0x13e0)) * SF64(0x768)) * SF64(0x16f0)) * sim1_f64(0xe2)) * SF64(0x728);
    whole = sim1_fistp64(step * SF64(0x6d8));
    memcpy(sim1_data + 0x1718, &whole, 8);
    sim1_position_add(global + 0x562, whole);
    step = (((((long double)SF64(0x13b8) + SF64(0x13d0)) * SF64(0x768)) * SF64(0x16f0)) * sim1_f64(0xe2)) * SF64(0x130);
    step = step / sim1_fcos(fabsl(((long double)PI32(global, 0x564) + (long double)(int16_t)P16(global, 0x562) * SF64(0x690)) * SF64(0x128)));
    whole = sim1_fistp64(((step * SF64(0x678)) * SF64(0x6d8)) * SF64(0x6d8));
    memcpy(sim1_data + 0x1718, &whole, 8);
    sim1_position_add(global + 0x56a, whole);
    step = (((((long double)SF64(0x13c0) + SF64(0x13d8)) * SF64(0x768)) * SF64(0x16f0)) * sim1_f64(0xe2)) * SF64(0x728);
    whole = sim1_fistp64(step * SF64(0x6d8));
    memcpy(sim1_data + 0x1718, &whole, 8);
    sim1_position_add(global + 0x572, whole);
    S32(0x7c0) = 1;
    sim1_set_u16(0x122, (uint16_t)(P32(global, 0x578) >> 16)); sim1_set_u16(0x11a, (uint16_t)(P32(global, 0x578) >> 16));
    sim1_set_u16(0x124, (uint16_t)(P32(global, 0x57c) >> 16)); sim1_set_u16(0x11c, (uint16_t)(P32(global, 0x57c) >> 16));
    sim1_set_u16(0x126, (uint16_t)(P32(global, 0x580) >> 16)); sim1_set_u16(0x11e, (uint16_t)(P32(global, 0x580) >> 16));
    global = SIM1_GLOBAL;
    SIM1_CALL(9, 0x2c, void (__stdcall *)(uint32_t, uint32_t, uint32_t, void *))
        (P32(global, 0x578) >> 16, P32(global, 0x57c) >> 16, P32(global, 0x580) >> 16, global + 0xd18);
    P32(global, 0xd2c) = 1;
    return global;
}

/* 0x2200ab16 (edi GLOBAL): one step of the simulation over +0x16fc ticks: the ticks counted
 * (+0x17a0) and kept as a time (+0x16f0, +0x16f8); the air, icing and its motion, the controls,
 * the step itself, the yaw rate, the engines' ratio, the followers, the heading term and the
 * engines. Returns retail's edi (the controls' and the step's, GLOBAL). */
uint8_t *sim1_tick_step(uint8_t *edi)
{
    S32(0x17a0) += S32(0x16fc);
    SF64(0x16f0) = (double)((long double)SI32(0x16fc) * SF64(0x698));
    S16(0x16f8) = (uint16_t)S32(0x16fc);
    sim1_air_data(edi);
    sim1_icing();
    sim1_air_motion(edi);
    edi = sim1_controls_pass(edi);
    edi = sim1_simulation_step(edi);
    sim1_yaw_rate(edi);
    sim1_engine_ratio();
    sim1_angle_follow();
    sim1_flag_follow();
    sim1_heading_term();
    sim1_engines_pass();
    return edi;
}

/* 0x2200a84b, stdcall (and its CHAIN list 1 wrapper 0x22001020): the frame's tick. Paused
 * (+0x5dc) clears +0x3cc4 and the jet's +0x1778. Slewing (+0x365) or once after +0x3cc4: the
 * record's words, and in the slew's altitude mode (+0x366) the height (+0x572, +0x574) from
 * GLOBAL +0x20 and +0x120c and the bank from +0x34f0. Otherwise, with the crash, the replay or
 * +0x838: the time from +0x2aa, the air, gear, flaps, followers and heading term (a
 * helicopter's frame too when +0x758). Otherwise the time (+0x2aa ticks times the rate
 * +0xc1a, at most 0x400, over 0x100, with the carry +0x8ec) cut into steps: under 0x420 carried
 * to the next tick, up to 0x800 one step, to 0x1000 two halves, past it 0x800 at a time.
 * Then the ticks counted (+0x17a4), the gear model, SOUND's wind from the speed and the
 * angles, and CONTROLS' force feedback block 9 (+0x8f4..+0x900) when flying.
 * RETAIL BUG (kept): SIM1-SLEW-HEIGHT-EAX (docs/retail-bugs.md): the height's shrd takes its
 * low byte from eax, the second argument (or the record words' result), not (presumably) GLOBAL +0x1c. */
void __stdcall sim1_chain_tick(uint32_t first, uint32_t second)
{
    uint8_t *edi = SIM1_GLOBAL;
    uint32_t eax = second, high, ecx, low;
    (void)first;
    P16(edi, 0x120a) = 0;
    if (P16(edi, 0x5dc)) { S32(0x3cc4) = 0; S32(0x1778) = 0; }
    if (!P8(edi, 0x365)) {
        if (!S32(0x3cc4)) goto run;
        S32(0x3cc4) = 0;
    } else eax = sim1_record_words();
    if (P8(edi, 0x366)) {
        high = P32(edi, 0x20);
        eax = (eax >> 24) | (high << 8);
        high = (uint32_t)((int32_t)high >> 8);
        ecx = P32(edi, 0x120c);
        low = (uint32_t)(uint16_t)eax + (uint16_t)ecx;
        P16(edi, 0x572) = (uint16_t)low;
        P32(edi, 0x574) = high + (ecx >> 16) + (low >> 16);
        P32(edi, 0x578) = S32(0x34f0);
    }
    if (P8(edi, 0x365)) goto tail;
run:
    S32(0x17a0) = 0;
    if (P8(edi, 0x838) || P32(edi, 0x268) || P32(edi, 0x270)) {
        S32(0x16fc) = (uint32_t)(int32_t)PI16(edi, 0x2aa);
        SF64(0x16f0) = (double)((long double)SI32(0x16fc) * SF64(0x698));
        S32(0x8f0) = 1;
        sim1_air_data(edi);
        sim1_landing_gear();
        sim1_flaps();
        sim1_angle_follow();
        sim1_heading_term();
        if (P32(edi, 0x604) == 1 && P32(edi, 0x758)) sim1_rotor_frame();
        goto tail;
    }
    {
        uint32_t was = S32(0x8f0), ticks = P16(edi, 0x2aa), rate = P16(edi, 0xc1a);
        S32(0x8f0) = 0;
        if (was) { sim1_quaternion(); goto tail; }     /* its flags leave ZF clear: the jne is taken */
        if (rate >= 0x400) rate = 0x400;
        P32(edi, 0x274) = S32(0x8ec) + ticks;
        eax = (uint32_t)(((uint64_t)rate * ticks) >> 8) + S32(0x8ec);
        S32(0x16fc) = eax;
        S32(0x8ec) = 0;
        for (;;) {
            if (eax <= 0x800) {
                if (eax < 0x420) { S32(0x8ec) = eax; P32(edi, 0x274) = 0; }
                else edi = sim1_tick_step(edi);
                break;
            }
            if (eax <= 0x1000) {
                S32(0x16fc) = eax >> 1;
                edi = sim1_tick_step(edi);
                edi = sim1_tick_step(edi);
                break;
            }
            S32(0x16fc) = 0x800;
            edi = sim1_tick_step(edi);
            eax -= 0x800;
        }
    }
tail:
    S32(0x17a4) = S32(0x17a0);
    if (P8(edi, 0x365)) { S32(0x8e4) = 0; S32(0x8e8) = 0; }
    sim1_gear_model(edi);
    S32(0x1718) = (uint32_t)sim1_fistp32(((long double)SF64(0x1450) * SF64(0x728)) * SF64(0x6d8));
    SIM1_CALL(SIM1_SOUND, 0x28, void (__stdcall *)(uint32_t, uint32_t, uint32_t))(S32(0x1718), S16(0x1518), S16(0x151c));
    edi = SIM1_GLOBAL;
    if (!(P8(edi, 0x838) || P32(edi, 0x268) || P32(edi, 0x270))) {
        S32(0x8f4) = S32(0x1628);
        S32(0x8f8) = P8(edi, 0x11ec);
        S32(0x8fc) = P32(edi, 0x2b4);
        S32(0x900) = 0;
        SIM1_CALL(SIM1_CONTROLS, 0x5c, uint32_t (__stdcall *)(uint32_t, void *))(9, sim1_data + 0x8f4);
    }
}

/* 0x22001060, Linkage +0x04, init: the situation's start, SIM1's three CHAIN callbacks added
 * (the tick to list 1, the engines' frame to list 5, the situation messages to list 0x14), the
 * aircraft's AIR file loaded; then (a jmp) the record's words. Returns their eax. */
uint32_t sim1_init_core(void)
{
    sim1_situation_start();
    SIM1_CALL(SIM1_CHAIN, 0x18, uint32_t (__stdcall *)(uint32_t, uint32_t))(1, sim1_callbacks[0]);
    SIM1_CALL(SIM1_CHAIN, 0x18, uint32_t (__stdcall *)(uint32_t, uint32_t))(5, sim1_callbacks[1]);
    SIM1_CALL(SIM1_CHAIN, 0x18, uint32_t (__stdcall *)(uint32_t, uint32_t))(0x14, sim1_callbacks[2]);
    sim1_load_air();
    return sim1_record_words();
}

/* The six Linkage services (sim1.c has the other agent's versions of these, in inline asm
 * over its own image; these are the same in C over sim1_data, for the native build). */

/* 0x2200ac0d, Linkage +0x18, stdcall: the situation record's 0x2c bytes (+0x1730) copied
 * out a byte at a time (rep movsb, so an overlapping destination sees its own copy). eax is
 * the caller's. */
void __stdcall sim1_service_descriptor(uint8_t *out)
{
    volatile const uint8_t *source = sim1_data + 0x1730;
    volatile uint8_t *destination = out;
    uint32_t at;
    for (at = 0; at < 0x2c; ++at) destination[at] = source[at];
}

/* 0x2200ac2f, Linkage +0x28, stdcall: the word +0x1518 written out; it is left in ax (the
 * rest of eax is the caller's). */
uint16_t __stdcall sim1_service_word(uint8_t *out)
{
    uint16_t value = sim1_u16(0x1518);
    memcpy(out, &value, 2);
    return value;
}

/* 0x220117c0, Linkage +0x2c, stdcall: the ticks counted (+0x17a4) as a double, times R152f0
 * (1/2). Returns 0. */
uint32_t __stdcall sim1_service_scaled(double *out)
{
    *out = (double)((long double)SI32(0x17a4) * RF64(0x152f0));
    return 0;
}

/* 0x220117e0, Linkage +0x30, stdcall: the three velocities (+0x13c8, +0x13b8, +0x13c0) out,
 * each through the x87 (a signalling NaN comes out quiet). Returns 0. */
uint32_t __stdcall sim1_service_vector(double *a, double *b, double *c)
{
    *a = sim1_x87(SF64(0x13c8));
    *b = sim1_x87(SF64(0x13b8));
    *c = sim1_x87(SF64(0x13c0));
    return 0;
}

/* 0x22011750, Linkage +0x34, stdcall: a reset to a speed: +0x13b0 the value (unsigned) times
 * +0x740, the other velocities and rates (+0x13a0, +0x13a8, +0x13e8..+0x13f8) R152e8 (0).
 * Returns the value. */
uint32_t __stdcall sim1_service_reset(uint32_t value)
{
    SF64(0x13b0) = (double)((long double)value * SF64(0x740));
    SF64(0x13a0) = sim1_x87(RF64(0x152e8));
    SF64(0x13a8) = sim1_x87(RF64(0x152e8));
    SF64(0x13e8) = sim1_x87(RF64(0x152e8));
    SF64(0x13f0) = sim1_x87(RF64(0x152e8));
    SF64(0x13f8) = sim1_x87(RF64(0x152e8));
    return value;
}

/* 0x220096b8, Linkage +0x38: the wind's double (+0x640) in st0. */
long double sim1_service_wind(void)
{
    return sim1_x87(SF64(0x640));
}
