/* SIM1.sim, decompiled: what every hand-written routine shares.
 *
 * SIM1's state is its .data (retail 0x22016000, 0x43f8 bytes with the zero-filled tail). The C
 * reads and writes it through sim1_data, which points at that image: the module's own in the
 * DLL (sim1_image in sim1.c), retail's mapped .data in tools/difftest/sim1rdiff.c. Offsets are
 * .data offsets, as in docs/modules/sim1.md (0x1600c is GLOBAL's link, so .data +0xc).
 *
 * Retail's hand-written assembly passes values in registers and leaves results in them; the C
 * takes them as arguments and returns them. Which registers a retail caller reads after a
 * routine is listed in sim1_tables.inc (tools/gen_sim1_tables.py), and is what the harness
 * compares. */
#ifndef FS98_SIM1_INTERNAL_H
#define FS98_SIM1_INTERNAL_H
#include <stdint.h>
#include <string.h>
#include <math.h>

extern uint8_t *sim1_data;
extern uint8_t *sim1_base;      /* the image: .rdata constants by rva (0x15000..) */

/* .data by offset */
#define S8(offset) (*(uint8_t *)(sim1_data + (offset)))
#define S16(offset) (*(uint16_t *)(sim1_data + (offset)))
#define S32(offset) (*(uint32_t *)(sim1_data + (offset)))
#define SI16(offset) (*(int16_t *)(sim1_data + (offset)))
#define SI32(offset) (*(int32_t *)(sim1_data + (offset)))
#define SF64(offset) (*(double *)(sim1_data + (offset)))
/* the GLOBAL block and memory by pointer */
#define SIM1_GLOBAL (*(uint8_t **)(sim1_data + 0xc))
#define P8(p, n) (*(uint8_t *)((uint8_t *)(p) + (n)))
#define P16(p, n) (*(uint16_t *)((uint8_t *)(p) + (n)))
#define P32(p, n) (*(uint32_t *)((uint8_t *)(p) + (n)))
#define PI16(p, n) (*(int16_t *)((uint8_t *)(p) + (n)))
#define PI32(p, n) (*(int32_t *)((uint8_t *)(p) + (n)))
#define PF64(p, n) (*(double *)((uint8_t *)(p) + (n)))
#define RF64(rva) (*(const double *)(sim1_base + (rva)))
#define RF32(rva) (*(const float *)(sim1_base + (rva)))

/* a double through the x87, as retail's fld/fstp copies do: a signalling NaN comes out quiet
 * (and raises invalid). A plain assignment may be compiled as an integer move. */
static inline double sim1_x87(double value)
{
    __asm__ volatile("" : "+t"(value));
    return value;
}
/* x87 comparisons as retail tests the status word after fcom: C0 is "below or unordered",
 * C3 "equal or unordered", C0|C3 "not above". Relational C operators compile to fcomi, which
 * raises invalid on any NaN as fcom does. */
#define X87_C0(a, b) (!((a) >= (b)))
#define X87_C3(a, b) (!((a) < (b)) && !((a) > (b)))
#define X87_C0C3(a, b) (!((a) > (b)))

/* Integer divides as the game runs them: retail divides into overflow and by zero, and the
 * host's filter (0x4119a0, docs/modules/host.md) repairs the fault: div gives quotient and
 * remainder all ones; idiv the largest quotient, or the most negative but one when divisor
 * and the dividend's top half differ in sign, and the largest remainder. */
static inline uint32_t sim1_div32(uint32_t high, uint32_t low, uint32_t divisor, uint32_t *remainder)
{
    uint64_t dividend = (uint64_t)high << 32 | low;
    if (!divisor || dividend / divisor > 0xffffffffu) { *remainder = 0xffffffffu; return 0xffffffffu; }
    *remainder = (uint32_t)(dividend % divisor);
    return (uint32_t)(dividend / divisor);
}
static inline uint32_t sim1_idiv32(uint32_t high, uint32_t low, uint32_t divisor, uint32_t *remainder)
{
    int64_t dividend = (int64_t)((uint64_t)high << 32 | low), quotient;
    if (divisor && !(dividend == INT64_MIN && (int32_t)divisor == -1)) {
        quotient = dividend / (int32_t)divisor;
        if (quotient >= INT32_MIN && quotient <= INT32_MAX) {
            *remainder = (uint32_t)(int32_t)(dividend % (int32_t)divisor);
            return (uint32_t)(int32_t)quotient;
        }
    }
    *remainder = 0x7fffffffu;
    return ((int32_t)divisor < 0) != ((int32_t)high < 0) ? 0x80000001u : 0x7fffffffu;
}
static inline uint16_t sim1_div16(uint16_t high, uint16_t low, uint16_t divisor, uint16_t *remainder)
{
    uint32_t dividend = (uint32_t)high << 16 | low;
    if (!divisor || dividend / divisor > 0xffffu) { *remainder = 0xffff; return 0xffff; }
    *remainder = (uint16_t)(dividend % divisor);
    return (uint16_t)(dividend / divisor);
}
static inline uint16_t sim1_idiv16(uint16_t high, uint16_t low, uint16_t divisor, uint16_t *remainder)
{
    int32_t dividend = (int32_t)((uint32_t)high << 16 | low), quotient;
    if (divisor) {
        quotient = dividend / (int16_t)divisor;
        if (quotient >= -0x8000 && quotient <= 0x7fff) {
            *remainder = (uint16_t)(int16_t)(dividend % (int16_t)divisor);
            return (uint16_t)(int16_t)quotient;
        }
    }
    *remainder = 0x7fff;
    return ((int16_t)divisor < 0) != ((int16_t)high < 0) ? 0x8001 : 0x7fff;
}

/* .data words at odd offsets (retail's hand-written code packs them) */
static inline uint32_t sim1_u32(uint32_t offset) { uint32_t value; memcpy(&value, sim1_data + offset, 4); return value; }
static inline void sim1_set_u32(uint32_t offset, uint32_t value) { memcpy(sim1_data + offset, &value, 4); }

static inline uint16_t sim1_u16(uint32_t offset) { uint16_t value; memcpy(&value, sim1_data + offset, 2); return value; }
static inline void sim1_set_u16(uint32_t offset, uint16_t value) { memcpy(sim1_data + offset, &value, 2); }
static inline double sim1_f64(uint32_t offset) { double value; memcpy(&value, sim1_data + offset, 8); return value; }
static inline void sim1_set_f64(uint32_t offset, double value) { memcpy(sim1_data + offset, &value, 8); }

/* the modules' services, through SIM1's ImportTable links (.data +4 + 8 * import): 0 HOST,
 * 1 GLOBAL, 2 ACONTAIN, 3 CHAIN, 4 CONTROLS, 5 MAIN, 6 REALITY, 7 SOUND, 8 UTIL, 9 WINDOW */
#define SIM1_LINK(import) (*(uint8_t **)(sim1_data + 4 + 8 * (import)))
#define SIM1_CALL(import, offset, type) ((type)(uintptr_t)P32(SIM1_LINK(import), (offset)))
#define SIM1_UTIL 8

/* the x87's own transcendental and rounding instructions (a C library's sinl may reduce the
 * argument differently, and out of range fsin leaves it alone) */
static inline long double sim1_fsin(long double value) { __asm__("fsin" : "+t"(value)); return value; }
static inline long double sim1_fcos(long double value) { __asm__("fcos" : "+t"(value)); return value; }
static inline long double sim1_fsqrt(long double value) { __asm__("fsqrt" : "+t"(value)); return value; }
/* fchs as an operation of its own: GCC would fold -x + y into y - x, which keeps a NaN x's sign */
static inline long double sim1_fchs(long double value) { __asm__("fchs" : "+t"(value)); return value; }
/* fistp qword: rounded as the control word says; out of range the integer indefinite */
static inline int64_t sim1_fistp64(long double value)
{
    int64_t result;
    __asm__("fistpll %0" : "=m"(result) : "t"(value) : "st");
    return result;
}

/* fistp dword: rounded as the control word says; out of range 0x80000000 */
static inline int32_t sim1_fistp32(long double value)
{
    int32_t result;
    __asm__("fistpl %0" : "=m"(result) : "t"(value) : "st");
    return result;
}
/* fistp word: out of range 0x8000 */
static inline int16_t sim1_fistp16(long double value)
{
    int16_t result;
    __asm__("fistps %0" : "=m"(result) : "t"(value) : "st");
    return result;
}
/* MSVCRT's _ftol: truncated (fistp qword under round-to-zero), the low 32 bits */
static inline int32_t sim1_ftol(long double value)
{
    uint16_t control, chop;
    int64_t result;
    /* the value made first: GCC could otherwise compute it after the fldcw, rounding it by chop */
    __asm__ volatile("" : "+t"(value));
    __asm__ volatile("fnstcw %0" : "=m"(control));
    chop = (uint16_t)(control | 0x0c00);
    __asm__ volatile("fldcw %0" : : "m"(chop));
    __asm__ volatile("fistpll %0" : "=m"(result) : "t"(value) : "st");
    __asm__ volatile("fldcw %0" : : "m"(control));
    return (int32_t)result;
}
/* MSVCRT's x87 helpers, the same routines retail imports (set at load: sim1_crt_*) */
extern void *sim1_crt_cipow, *sim1_crt_ciacos, *sim1_crt_ciasin;
/* _CIpow: st1 to the power st0, both popped, the result in st0 */
static inline long double sim1_cipow(long double base, long double exponent)
{
    long double result;
    __asm__ volatile("call *%3" : "=t"(result) : "0"(exponent), "u"(base), "m"(sim1_crt_cipow)
                     : "st(1)", "eax", "ecx", "edx", "memory");
    return result;
}
/* _CIasin: the arcsine of st0, in st0 */
static inline long double sim1_ciasin(long double value)
{
    long double result;
    __asm__ volatile("call *%2" : "=t"(result) : "0"(value), "m"(sim1_crt_ciasin) : "eax", "ecx", "edx", "memory");
    return result;
}
/* _CIacos: the arccosine of st0, in st0 */
static inline long double sim1_ciacos(long double value)
{
    long double result;
    __asm__ volatile("call *%2" : "=t"(result) : "0"(value), "m"(sim1_crt_ciacos) : "eax", "ecx", "edx", "memory");
    return result;
}
/* SIM1's own callbacks as CHAIN knows them (tick, frame, situation): the DLL's entries, or in
 * the harness retail's, so both sides register and remove the same addresses */
extern uint32_t sim1_callbacks[3];
#define SIM1_CHAIN 3
#define SIM1_HOST 0
#define SIM1_MAIN 5
#define SIM1_REALITY 6
#define SIM1_ACONTAIN 2
#define SIM1_CONTROLS 4
#define SIM1_SOUND 7

/* 16-bit add and subtract as retail saturates them after jno: overflowed, 0x7fff when the
 * wrapped result is negative (the true sum positive), else 0x8001 */
static inline uint16_t sim1_add16_saturated(uint16_t a, uint16_t b)
{
    int32_t sum = (int16_t)a + (int16_t)b;
    if (sum > 0x7fff || sum < -0x8000) return (int16_t)(uint16_t)sum < 0 ? 0x7fff : 0x8001;
    return (uint16_t)sum;
}
static inline uint16_t sim1_sub16_saturated(uint16_t a, uint16_t b)
{
    int32_t difference = (int16_t)a - (int16_t)b;
    if (difference > 0x7fff || difference < -0x8000) return (int16_t)(uint16_t)difference < 0 ? 0x7fff : 0x8001;
    return (uint16_t)difference;
}

/* a .data address as retail holds it (a relocated word) */
#define SIM1_DATA_VA(offset) ((uint32_t)(uintptr_t)(sim1_data + (offset)))

#endif
