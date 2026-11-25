#ifndef FS98_DIVIDE_H
#define FS98_DIVIDE_H

#include <stdint.h>

/* Integer divides that never trap (ARCHITECTURE-SPINE AD-18).
 *
 * Each helper mirrors one x86 divide's true operands as unsigned bit patterns, as the registers
 * hold them; callers cast. In range it returns the instruction's quotient and remainder
 * (truncation toward zero, the remainder takes the dividend's sign). Where the instruction would
 * fault (divisor 0, or the quotient does not fit, including the most negative dividend by -1) it
 * returns what retail's host filter 0x4119a0 writes:
 *   div:  quotient and remainder all ones at the width;
 *   idiv: the quotient is 0x80000001 / 0x8001 / 0x81 when the sign bit of the dividend's top half
 *         (edx, dx, ah) differs from the divisor's sign bit at the operand width, else
 *         0x7fffffff / 0x7fff / 0x7f (the true quotient's sign is not used); the remainder is
 *         the largest positive value at the width.
 * `remainder` may be NULL. The filter's decode-time quirks (DIVIDE-BYTE-REGISTER,
 * DIVIDE-DISP8-UNSIGNED, DIVIDE-SIB-REGISTER-STEP; docs/retail-bugs.md) belong to whatever
 * executes guest code, not to these helpers.
 *
 *   edx:eax by r32:   fs98_div32(edx, eax, d, &r)
 *   cltd; idiv:       fs98_idiv32(-(x >> 31), x, d, &r)   (x the bit pattern of eax)
 *   xor edx,edx; div: fs98_div32(0, x, d, &r)
 *   dx:ax by r16:     fs98_div16(dx, ax, d, &r)
 *   ax by r8:         fs98_div8(ax, d, &r); the quotient is al, the remainder ah */

/* shortcut: 64-bit arithmetic for every width, one rule instead of three; upgrade when a 64-bit divide is needed */
static inline int fs98_divide_unsigned(uint64_t n, uint32_t d, uint64_t max, uint64_t *q, uint64_t *r)
{
    if (d == 0 || n / d > max)
        return 0;
    *q = n / d;
    *r = n % d;
    return 1;
}

/* n by d, quotient within [low, high]; d == -1 is apart, since INT64_MIN / -1 overflows in C */
static inline int fs98_divide_signed(int64_t n, int32_t d, int64_t low, int64_t high, int64_t *q, int64_t *r)
{
    if (d == 0)
        return 0;
    if (d == -1) {
        if (n < -high || n > -low)
            return 0;
        *q = -n;
        *r = 0;
        return 1;
    }
    *q = n / d;
    *r = n % d;
    return *q >= low && *q <= high;
}

static inline uint32_t fs98_div32(uint32_t high, uint32_t low, uint32_t divisor, uint32_t *remainder)
{
    uint64_t q, r;
    if (!fs98_divide_unsigned(((uint64_t)high << 32) | low, divisor, 0xffffffffu, &q, &r))
        q = r = 0xffffffffu;
    if (remainder)
        *remainder = (uint32_t)r;
    return (uint32_t)q;
}

static inline uint32_t fs98_idiv32(uint32_t high, uint32_t low, uint32_t divisor, uint32_t *remainder)
{
    int64_t q, r;
    uint64_t n = ((uint64_t)high << 32) | low;
    /* the bit patterns as signed values, without an out-of-range conversion */
    int64_t value = (n >> 63) ? -(int64_t)(~n) - 1 : (int64_t)n;
    int32_t d = (divisor >> 31) ? -(int32_t)(~divisor) - 1 : (int32_t)divisor;
    if (!fs98_divide_signed(value, d, -2147483647LL - 1, 2147483647LL, &q, &r)) {
        q = ((high ^ divisor) >> 31) ? 0x80000001u : 0x7fffffffu;
        r = 0x7fffffff;
    }
    if (remainder)
        *remainder = (uint32_t)r;
    return (uint32_t)q;
}

static inline uint16_t fs98_div16(uint16_t high, uint16_t low, uint16_t divisor, uint16_t *remainder)
{
    uint64_t q, r;
    if (!fs98_divide_unsigned(((uint32_t)high << 16) | low, divisor, 0xffff, &q, &r))
        q = r = 0xffff;
    if (remainder)
        *remainder = (uint16_t)r;
    return (uint16_t)q;
}

static inline uint16_t fs98_idiv16(uint16_t high, uint16_t low, uint16_t divisor, uint16_t *remainder)
{
    int64_t q, r;
    uint32_t n = ((uint32_t)high << 16) | low;
    int64_t value = (n >> 31) ? (int64_t)n - 0x100000000LL : (int64_t)n;
    int32_t d = (divisor >> 15) ? (int32_t)divisor - 0x10000 : (int32_t)divisor;
    if (!fs98_divide_signed(value, d, -32768, 32767, &q, &r)) {
        q = ((high ^ divisor) >> 15) ? 0x8001 : 0x7fff;
        r = 0x7fff;
    }
    if (remainder)
        *remainder = (uint16_t)r;
    return (uint16_t)q;
}

static inline uint8_t fs98_div8(uint16_t ax, uint8_t divisor, uint8_t *remainder)
{
    uint64_t q, r;
    if (!fs98_divide_unsigned(ax, divisor, 0xff, &q, &r))
        q = r = 0xff;
    if (remainder)
        *remainder = (uint8_t)r;
    return (uint8_t)q;
}

static inline uint8_t fs98_idiv8(uint16_t ax, uint8_t divisor, uint8_t *remainder)
{
    int64_t q, r;
    int64_t value = (ax >> 15) ? (int64_t)ax - 0x10000 : (int64_t)ax;
    int32_t d = (divisor >> 7) ? (int32_t)divisor - 0x100 : (int32_t)divisor;
    if (!fs98_divide_signed(value, d, -128, 127, &q, &r)) {
        q = ((((ax >> 8) ^ divisor) >> 7) & 1) ? 0x81 : 0x7f;
        r = 0x7f;
    }
    if (remainder)
        *remainder = (uint8_t)r;
    return (uint8_t)q;
}

#endif /* FS98_DIVIDE_H */
