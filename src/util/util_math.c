/* UTIL.DLL's pure arithmetic services.
 *
 * UTIL (module id 20) publishes 47 services in 47 slots from Linkage+0x18 to
 * +0xc4.  The ones here are the self-contained arithmetic: they touch neither
 * GLOBAL nor NUMERIC, so they can be reimplemented and verified one at a time
 * without the rest of the module existing yet.  That matters, because unlike
 * SLEW a partial UTIL cannot be dropped into the game -- the host binds every
 * slot -- so these are checked against retail with tools/difftest/utildiff.c
 * rather than by running the sim.
 *
 * Retail rounds with the x87 in its default mode (nearest, ties to even), so
 * the square roots here go through the same instructions rather than through
 * the C library, which would otherwise disagree on halfway cases.
 */

#include <stdint.h>

#include <fs98/host.h>

#include "util_imports.h"

#include "cos_table.inc"
#include "util_data.inc"

#ifndef FS98_STDCALL
#  define FS98_STDCALL __attribute__((stdcall))
#endif

/* Retail rounds on the x87, nearest with ties to even.  On x86 the same
 * instructions are used, which settles the question of what "the same" means.
 * Elsewhere -- ARM, for a native Mac or Android build -- there is no x87, so a
 * portable path computes the same thing through the C library: `sqrt` is
 * correctly rounded, and `nearbyint` rounds in the current mode, which is
 * nearest-even unless something has changed it.
 *
 * Define FS98_PORTABLE_FP to force the portable path on x86 as well, which is
 * how it gets checked against retail rather than merely believed.
 */
#if (defined(__i386__) || defined(__x86_64__)) && !defined(FS98_PORTABLE_FP)
#  define FS98_FP_X87 1
#else
#  define FS98_FP_X87 0
#  include <math.h>
#endif

static int64_t fs98_fsqrt_round(int64_t v)
{
#if FS98_FP_X87
    int64_t r;
    __asm__ __volatile__ ("fildll %1\n\tfsqrt\n\tfistpll %0"
                          : "=m" (r) : "m" (v) : "st");
    return r;
#else
    return (int64_t)nearbyint(sqrt((double)v));
#endif
}

/* Slot 0x1c.  Square root of an unsigned 32 bit value.
 *
 * The trailing `sub eax, eax>>16` only bites for roots of 65536 or more, which
 * a 32 bit input cannot produce; it is kept because it is what the module does.
 */
int32_t FS98_STDCALL fs98_util_sqrt32(uint32_t v)
{
    int32_t r = (int32_t)fs98_fsqrt_round((int64_t)(uint64_t)v);
    return r - (int32_t)((uint32_t)r >> 16);
}

/* Slot 0x18.  Square root of a 64 bit value held at a pointer.
 *
 * When the high half is negative the value is shifted right by two first and
 * the root doubled afterwards, which keeps it inside what fild will accept as
 * a signed quantity.
 */
int32_t FS98_STDCALL fs98_util_sqrt64(const void *p)
{
    uint32_t lo = ((const uint32_t *)p)[0];
    int32_t  hi = ((const int32_t  *)p)[1];
    uint64_t v  = ((uint64_t)(uint32_t)hi << 32) | lo;

    if (hi < 0)
        return (int32_t)(fs98_fsqrt_round((int64_t)(v >> 2)) * 2);
    return (int32_t)fs98_fsqrt_round((int64_t)v);
}

/* Slot 0x28.  Length of a 2D vector, to 16 bit components.
 *
 * The sum is kept to 32 bits -- the retail code takes only the low half of
 * each `imul` -- and a zero sum short-circuits before the root. */
int32_t FS98_STDCALL fs98_util_hypot(int16_t a, int16_t b)
{
    int32_t s = (int32_t)((int32_t)a * (int32_t)a)
              + (int32_t)((int32_t)b * (int32_t)b);
    if (s == 0)
        return 0;
    return fs98_util_sqrt32((uint32_t)s);
}

/* Quarter-wave cosine, linearly interpolated, for angles 0..0x4000 where
 * 0x4000 is ninety degrees.  Internal: retail passes the angle in EBX and
 * returns in EAX, so it is reachable only from inside the module.
 *
 * The table holds unsigned Q16 but is read with a sign-extending load, so
 * entry 0 arrives as -1 and becomes 0x7fff after the masking and halving at
 * the end.  That sequence is reproduced rather than simplified. */
static int32_t fs98_util_quarter_cos(uint16_t angle)
{
    uint32_t a    = angle;
    uint32_t idx  = ((a * 2u) & 0xffffu) >> 8;
    uint32_t frac = a & 0x7fu;
    int32_t  cur  = (int16_t)fs98_util_cos_q16[idx];
    int32_t  nxt  = (int16_t)fs98_util_cos_q16[idx + 1];
    int32_t  d    = (int16_t)(nxt - cur);
    int32_t  t    = (int32_t)(((int32_t)frac * d) * 2) >> 8;

    return (int32_t)((uint32_t)((cur + t) & 0xffff) >> 1);
}

/* Slot 0x2c.  Sine and cosine of a 16 bit angle, as Q15.
 *
 * Either output pointer may be null.  Note the order: the second argument
 * receives the sine and the third the cosine.
 */
void FS98_STDCALL fs98_util_sincos(int16_t angle, int16_t *out_sin,
                                   int16_t *out_cos)
{
    int32_t a = angle;
    int32_t s, c;

    if (a < 0) {
        int32_t m = -a;
        int16_t ps, pc;
        if (m == 0x8000)        /* -32768 has no positive counterpart */
            m = 0x7fff;
        fs98_util_sincos((int16_t)m, &ps, &pc);
        s = -(int32_t)ps;
        c = pc;
    } else if (a > 0x4000) {
        c = -fs98_util_quarter_cos((uint16_t)(0x8000 - a));
        s =  fs98_util_quarter_cos((uint16_t)(a - 0x4000));
    } else {
        c = fs98_util_quarter_cos((uint16_t)a);
        s = fs98_util_quarter_cos((uint16_t)(0x4000 - a));
    }

    if (out_sin)
        *out_sin = (int16_t)s;
    if (out_cos)
        *out_cos = (int16_t)c;
}

/* ---- slot 0x44: metres to longitude ---------------------------------------
 *
 * Converts a 48-bit north/south-independent step into a longitude step, given
 * the latitude it happens at.  All three arguments are 48-bit quantities in
 * FS98's usual layout: a 16-bit low half at +2 and a 32-bit high half at +4.
 *
 * The divisor is the circumference of the latitude circle in metres, read from
 * a table indexed by the top bits of the latitude and interpolated with what
 * is left.  40075000 at the equator falling to nothing at the pole, which is
 * why a metre of easting buys more longitude the further north you go.
 *
 * This is transliterated from the instruction sequence rather than rewritten
 * as arithmetic.  The routine mixes a 64-bit negate, an unsigned multiply, a
 * two-step 64/32 division and several `shld`/`shrd` pairs, and at least one of
 * those steps is wrong in a way that matters (see QUIRK below) -- expressing
 * the "intent" would quietly correct it.
 */
/* The latitude circle's circumference, interpolated from the table.  Shared by
 * `+0x44` and `+0x48`, which are inverses of each other and derive it the same
 * way. */
static uint32_t fs98_util_lat_scale(const void *pos48)
{
    const unsigned char *pos = (const unsigned char *)pos48;
    uint32_t p_hi   = *(const uint32_t *)(pos + 4);
    uint32_t lat_lo = ((p_hi & 0xffffu) << 16) | *(const uint16_t *)(pos + 2);
    int32_t  lat_hi = (int32_t)p_hi >> 16;
    uint32_t off, frac;

    if (lat_hi < 0) {                       /* 64-bit negate of lat_hi:lat_lo */
        uint32_t old = lat_lo;
        lat_lo = (uint32_t)(-(int32_t)lat_lo);
        lat_hi = (int32_t)(~(uint32_t)lat_hi + (old != 0 ? 0u : 1u));
    }

    off  = (uint32_t)(((uint32_t)lat_hi & ~1u) * 2u);
    frac = ((uint32_t)(lat_hi & 1) << 31) | (lat_lo >> 1);

    if (off + FS98_UTIL_LATTABLE + 8 > FS98_UTIL_DATA_SIZE)
        off = 0;                            /* see the note on range, below */

    {
        const unsigned char *t = fs98_util_data + FS98_UTIL_LATTABLE + off;
        uint32_t cur   = *(const uint32_t *)t;
        uint32_t next  = *(const uint32_t *)(t + 4);
        uint64_t prod  = (uint64_t)frac * (cur - next);

        /* QUIRK.  `add eax,eax / adc edx,0` doubles only the low half and
         * folds its carry upward; doubling a 64-bit value wants `adc edx,edx`.
         * So the high half is not doubled, and the scale comes out
         * fractionally different from the interpolation the code reads as if
         * it were doing.  Reproduced -- it moves results by a unit or two. */
        {
            uint32_t p_lo  = (uint32_t)prod;
            uint32_t p_hi2 = (uint32_t)(prod >> 32);
            return cur - (p_hi2 + (p_lo >> 31));
        }
    }
}

void FS98_STDCALL fs98_util_metres_to_longitude(const void *in48,
                                                const void *pos48,
                                                void *out48)
{
    const unsigned char *in  = (const unsigned char *)in48;
    unsigned char *out = (unsigned char *)out48;

    /* numerator = input << 16, held as edx:eax */
    uint32_t n_hi = *(const uint32_t *)(in + 4);
    uint32_t n_lo = (uint32_t)(*(const uint16_t *)(in + 2)) << 16;
    uint32_t divisor = fs98_util_lat_scale(pos48);
    uint32_t q_hi, q_lo;
    uint64_t num;
    int negate = 0;

    num = ((uint64_t)n_hi << 32) | n_lo;
    if ((int32_t)n_hi < 0) {
        num = (uint64_t)(-(int64_t)num);
        negate = 1;
    }

    if (divisor == 0)                       /* retail would fault; do not */
        divisor = 1;

    /* two-step 64/32 division, giving a 64-bit quotient q_hi:q_lo */
    q_hi = (uint32_t)(num / divisor);
    q_lo = (uint32_t)(((num % divisor) << 32) / divisor);

    if (negate) {
        /* QUIRK.  The low 16 bits of the fractional half are cleared, and then
         * the 64-bit negate is written as `not ecx / neg eax / sbb edx,-1` --
         * the borrow correction lands on EDX, the leftover remainder, instead
         * of on ECX where the high half of the quotient lives.  So the high
         * half comes back as ~q_hi and never gets its +1, which is right only
         * when the low half is non-zero.  Reproduced as written. */
        q_lo &= 0xffff0000u;
        q_hi = ~q_hi;
        q_lo = (uint32_t)(-(int32_t)q_lo);
    }

    *(uint16_t *)(out + 2) = (uint16_t)((q_lo >> 16) | (q_hi << 16));
    *(uint32_t *)(out + 4) = q_hi;
}

/* ---- slots 0x50, 0x54, 0x58, 0x5c: stubs ----------------------------------
 *
 * These four shipped unfinished.  The first three are byte-for-byte identical
 * to each other:
 *
 *     mov eax, [ebp+0x8]      ; load the first argument
 *     mov eax, [ebp+0xc]      ; ...throw it away and load the second
 *     ret 8
 *
 * -- a two-argument function that ignores the first and returns the second,
 * which is what a compiler emits for a body of `a; return b;`.  The fourth
 * saves EBX, ESI and EDI, restores them, and returns, leaving EAX at whatever
 * the caller happened to have in it.
 *
 * There is nothing to recover here: no name, no callers inside UTIL, and no
 * behaviour beyond the above.  They are reimplemented as what they are rather
 * than guessed at, and the fourth returns void because its result is not a
 * value the retail code ever defines.
 */
int32_t FS98_STDCALL fs98_util_stub_50(int32_t ignored, int32_t value)
{
    (void)ignored;
    return value;
}

int32_t FS98_STDCALL fs98_util_stub_54(int32_t ignored, int32_t value)
{
    (void)ignored;
    return value;
}

int32_t FS98_STDCALL fs98_util_stub_58(int32_t ignored, int32_t value)
{
    (void)ignored;
    return value;
}

void FS98_STDCALL fs98_util_stub_5c(void)
{
}

/* ---- slot 0x20: square root of a 16-bit value, clamped to a byte ----------
 *
 * The same x87 round as the other roots, then `r - (r >> 8)`.  That tail looks
 * redundant until the top of the range: sqrt(65535) rounds to 256, and the
 * subtraction pulls it back to 255 so the answer always fits in a byte.
 */
int32_t FS98_STDCALL fs98_util_sqrt16(uint16_t v)
{
    int32_t r;
#if FS98_FP_X87
    int32_t t = (int32_t)(uint32_t)v;
    __asm__ __volatile__ ("fildl %1\n\tfsqrt\n\tfistpl %0"
                          : "=m" (r) : "m" (t) : "st");
#else
    r = (int32_t)nearbyint(sqrt((double)v));
#endif
    return r - (int32_t)((uint32_t)r >> 8);
}

/* ---- slot 0x6c: pseudo-random number -------------------------------------
 *
 * A Lehmer generator, seed held in UTIL's own .data and starting at 12345:
 *
 *     seed = (seed * 0x970 + 0x5b6a9) mod 0x1b0963
 *
 * and the new seed is the result.  The multiply and the division are both
 * unsigned and go through a 64-bit intermediate, so the modulus is exact.
 *
 * Stateful, so a differential test has to put both generators on the same seed
 * before it can compare them -- see fs98_util_rand_seed.
 */
static uint32_t fs98_util_rand_state = 12345;

void fs98_util_rand_seed(uint32_t seed)
{
    fs98_util_rand_state = seed;
}

uint32_t fs98_util_rand_get(void)
{
    return fs98_util_rand_state;
}

uint32_t FS98_STDCALL fs98_util_rand(void)
{
    uint64_t v = (uint64_t)fs98_util_rand_state * 0x970u + 0x5b6a9u;
    fs98_util_rand_state = (uint32_t)(v % 0x1b0963u);
    return fs98_util_rand_state;
}

/* ---- slot 0x70: two draws packed into one word ---------------------------
 *
 * Takes the low half of each of two draws.  The retail code builds the result
 * in DX, so only sixteen bits of each survive.
 */
uint32_t FS98_STDCALL fs98_util_rand32(void)
{
    uint32_t hi = fs98_util_rand() & 0xffffu;
    uint32_t lo = fs98_util_rand() & 0xffffu;
    return (hi << 16) | lo;
}

/* ---- slot 0x7c: copy twenty-four bytes ------------------------------------
 *
 * `rep movsd` with a count of six.  Destination first, source second -- the
 * opposite order from memcpy's signature is not in play here, but the argument
 * order is worth stating because the retail code loads EDI from the *first*
 * argument.
 */
void FS98_STDCALL fs98_util_copy24(void *dst, const void *src)
{
    uint32_t *d = (uint32_t *)dst;
    const uint32_t *s = (const uint32_t *)src;
    int i;
    for (i = 0; i < 6; i++)
        d[i] = s[i];
}

/* ---- slot 0xb0: double to int --------------------------------------------
 *
 * `fld` then `fistp`, so it rounds in the x87's current mode rather than
 * truncating the way a C cast would.
 */
int32_t FS98_STDCALL fs98_util_double_to_int(double v)
{
#if FS98_FP_X87
    int32_t r;
    __asm__ __volatile__ ("fldl %1\n\tfistpl %0" : "=m" (r) : "m" (v) : "st");
    return r;
#else
    /* `fistp` to a dword stores the x87's "integer indefinite" when the value
     * will not fit, so the out-of-range answer is 0x80000000, not a clamp. */
    double r = nearbyint(v);
    if (!(r >= -2147483648.0 && r <= 2147483647.0))
        return (int32_t)0x80000000;
    return (int32_t)r;
#endif
}

/* ---- the 48-bit vector ----------------------------------------------------
 *
 * Several services work on a 24-byte group of three 48-bit values, each laid
 * out the usual way -- 16-bit low half at +2, 32-bit high half at +4 -- at
 * strides of 8.  That is the same 24 bytes `copy24` moves, so the type is
 * almost certainly a position or displacement in the three world axes.
 */
#define FS98_V48_STRIDE 8

static void v48_get(const void *p, int i, uint16_t *lo, int32_t *hi)
{
    const unsigned char *q = (const unsigned char *)p + i * FS98_V48_STRIDE;
    *lo = *(const uint16_t *)(q + 2);
    *hi = *(const int32_t  *)(q + 4);
}

static void v48_put(void *p, int i, uint16_t lo, int32_t hi)
{
    unsigned char *q = (unsigned char *)p + i * FS98_V48_STRIDE;
    *(uint16_t *)(q + 2) = lo;
    *(int32_t  *)(q + 4) = hi;
}

/* Slot 0x74.  dst = a + b, component by component, carrying from each 16-bit
 * low half into its 32-bit high half. */
void FS98_STDCALL fs98_util_v48_add(void *dst, const void *a, const void *b)
{
    int i;
    for (i = 0; i < 3; i++) {
        uint16_t alo, blo;
        int32_t  ahi, bhi;
        uint32_t t;
        v48_get(a, i, &alo, &ahi);
        v48_get(b, i, &blo, &bhi);
        t = (uint32_t)alo + blo;
        v48_put(dst, i, (uint16_t)t, ahi + bhi + (int32_t)(t >> 16));
    }
}

/* Slot 0x78.  dst = a - b, borrowing the same way. */
void FS98_STDCALL fs98_util_v48_sub(void *dst, const void *a, const void *b)
{
    int i;
    for (i = 0; i < 3; i++) {
        uint16_t alo, blo;
        int32_t  ahi, bhi;
        uint32_t t;
        v48_get(a, i, &alo, &ahi);
        v48_get(b, i, &blo, &bhi);
        t = (uint32_t)alo - blo;
        v48_put(dst, i, (uint16_t)t, ahi - bhi - (int32_t)((t >> 16) & 1));
    }
}

/* Slot 0x80.  One if every component matches, zero otherwise.  The retail code
 * follows its zero result with a `cmp eax,1` whose flags nothing reads -- dead,
 * and not reproduced because it has no effect a caller can observe. */
int32_t FS98_STDCALL fs98_util_v48_equal(const void *a, const void *b)
{
    int i;
    for (i = 0; i < 3; i++) {
        uint16_t alo, blo;
        int32_t  ahi, bhi;
        v48_get(a, i, &alo, &ahi);
        v48_get(b, i, &blo, &bhi);
        if (alo != blo || ahi != bhi)
            return 0;
    }
    return 1;
}

/* Slot 0x84.  Arithmetic right shift of a single 48-bit value.
 *
 * The shift count reaches `shrd` and `sar` in CL, and x86 masks it to five
 * bits -- so a count of 32 shifts by nothing, not by a word.  Masked here for
 * the same reason, rather than quietly doing the arithmetically sensible thing.
 */
void FS98_STDCALL fs98_util_shr48(void *dst, const void *src, uint32_t count)
{
    uint16_t lo = *(const uint16_t *)((const unsigned char *)src + 2);
    int32_t  hi = *(const int32_t  *)((const unsigned char *)src + 4);
    uint32_t c  = count & 31;
    uint32_t edx = (uint32_t)lo << 16;

    if (c) {
        edx = (edx >> c) | ((uint32_t)hi << (32 - c));
        hi  = hi >> c;
    }
    edx >>= 16;

    *(int32_t  *)((unsigned char *)dst + 4) = hi;
    *(uint16_t *)((unsigned char *)dst + 2) = (uint16_t)edx;
}

/* Slot 0xb4.  Double to integer, keeping the low half of a 64-bit conversion.
 *
 * Unlike slot 0xb0 this stores a quadword and returns only its low dword, so
 * values outside 32-bit range wrap instead of becoming the x87's "integer
 * indefinite".
 */
int32_t FS98_STDCALL fs98_util_double_to_int64_low(double v)
{
#if FS98_FP_X87
    int64_t r;
    __asm__ __volatile__ ("fldl %1\n\tfistpll %0" : "=m" (r) : "m" (v) : "st");
    return (int32_t)r;
#else
    /* Indefinite here is 0x8000000000000000, whose low half is zero. */
    double r = nearbyint(v);
    if (!(r >= -9223372036854775808.0 && r < 9223372036854775808.0))
        return 0;
    return (int32_t)(int64_t)r;
#endif
}

/* ---- slot 0x34: rotate a 2D vector ----------------------------------------
 *
 *     out_x = x*cos - y*sin
 *     out_y = y*cos + x*sin
 *
 * with the sine and cosine in Q15.  Each term is a signed 32x32 multiply,
 * doubled as a full 64-bit quantity and then taken from bit 16 up -- which is
 * the Q15 normalisation, written as `add/adc` followed by `shld`.
 */
static int32_t fs98_util_q15_mul(int32_t v, int32_t k)
{
    uint64_t p = (uint64_t)((int64_t)v * k);
    p <<= 1;
    return (int32_t)(uint32_t)(p >> 16);
}

void FS98_STDCALL fs98_util_rotate2d(int32_t x, int32_t y, int16_t s, int16_t c,
                                     int32_t *out_x, int32_t *out_y)
{
    int32_t rx = fs98_util_q15_mul(x, c) - fs98_util_q15_mul(y, s);
    int32_t ry = fs98_util_q15_mul(y, c) + fs98_util_q15_mul(x, s);
    *out_x = rx;
    *out_y = ry;
}

/* ---- slot 0x30: divide a 48-bit value by an unsigned 16-bit divisor --------
 *
 * Transliterated rather than rewritten.  The routine negates a sign/magnitude
 * pair before dividing and negates the result afterwards, and the pair it
 * negates is the 32-bit high half alongside a *sign-extended 16-bit* low half
 * rather than a proper low 32 bits -- so expressing it as a plain 48-bit
 * division would not reproduce it.
 *
 * Dividing by zero faults, exactly as the retail code does.
 */
void FS98_STDCALL fs98_util_div48_u16(const void *src, uint16_t divisor,
                                      void *dst)
{
    const unsigned char *s = (const unsigned char *)src;
    int32_t  orig_hi = *(const int32_t *)(s + 4);
    uint32_t eax = (uint32_t)orig_hi;
    uint32_t esi = (uint32_t)(int32_t)*(const int16_t *)(s + 2);  /* movsx */
    uint32_t ecx = divisor;
    uint32_t ebx, edx;

    if ((int32_t)eax < 0) {                 /* 64-bit negate of eax:esi */
        uint32_t old = esi;
        esi = (uint32_t)(-(int32_t)esi);
        eax = ~eax + (old != 0 ? 0u : 1u);
    }

    edx = 0;
    ebx = (uint32_t)((((uint64_t)edx << 32) | eax) / ecx);   /* q1 */
    edx = (uint32_t)((((uint64_t)edx << 32) | eax) % ecx);   /* r1 */

    eax = esi << 16;                        /* xor eax,eax / shrd eax,esi,16 */
    {
        uint64_t n = ((uint64_t)edx << 32) | eax;
        eax = (uint32_t)(n / ecx);
    }

    edx = (uint32_t)(int32_t)(int16_t)ebx;  /* movsx edx,bx */
    {                                       /* sar edx,1 / rcr eax,1 */
        uint32_t carry = edx & 1u;
        edx = (uint32_t)((int32_t)edx >> 1);
        eax = (carry << 31) | (eax >> 1);
    }

    if (orig_hi < 0) {                      /* 64-bit negate of edx:eax */
        uint32_t old = eax;
        eax = (uint32_t)(-(int32_t)eax);
        edx = ~edx + (old != 0 ? 0u : 1u);
    }

    *(int32_t  *)((unsigned char *)dst + 4) = (int32_t)((edx << 16) | (eax >> 16));
    *(uint16_t *)((unsigned char *)dst + 2) = (uint16_t)eax;
}

/* ---- slot 0x40: a 32-bit step, through the longitude scaling ---------------
 *
 * Sign-extends a 32-bit value to 48 bits on the stack and hands it to slot
 * 0x44, so it inherits that routine's limits: it faults on a large step and
 * walks off the latitude table outside roughly +/-0x4e.
 */
void FS98_STDCALL fs98_util_metres32_to_longitude(const int32_t *in,
                                                  const void *pos, void *out)
{
    int32_t v = *in;
    unsigned char tmp[8];

    *(uint16_t *)(tmp + 2) = (uint16_t)v;
    *(int32_t  *)(tmp + 4) = v >> 16;       /* cdq / shld edx,eax,16 */
    fs98_util_metres_to_longitude(tmp, pos, out);
}

/* ---- slot 0x08: deinit ----------------------------------------------------
 *
 * A bare `ret`.  Present for completeness; the module has nothing to tear down.
 */
void FS98_STDCALL fs98_util_deinit(void)
{
}

/* Slot 0x04, init, is deliberately absent.
 *
 * It reaches outside the module:
 *
 *     mov  eax, ds:<ImportTable slot for module 1>   ; the host
 *     call DWORD PTR [eax+0x38]                      ; a host service
 *     mov  ds:<seed>, eax                            ; seed the generator
 *     call <internal routine at .text+0x2ab0>
 *
 * So UTIL's random sequence starts from whatever the host's service at +0x38
 * returns -- a clock, on the evidence of what it is used for. That makes init
 * the first service here whose result is not a function of its arguments, and
 * it cannot be checked by the differential harness: with a stubbed host both
 * sides would agree no matter what either did. It needs the host's service
 * table reversed first, and the internal routine at .text+0x2ab0 with it.
 */

/* ---- slot 0x24: 48-bit value times a 16-bit multiplier --------------------
 *
 * The result is a full 64-bit quantity at `dst` -- low dword first -- not the
 * 48-bit layout the input uses, and it is doubled on the way out.
 *
 * Sign is handled the way the retail code handles it: track the sign of the
 * answer by XOR-ing the two operands' sign bits, take magnitudes, multiply
 * unsigned, and negate at the end.  The multiply itself truncates to 64 bits,
 * because the high half of the second partial product is simply dropped.
 */
void FS98_STDCALL fs98_util_mul48_s16(const void *src, int16_t m, void *dst)
{
    const unsigned char *s = (const unsigned char *)src;
    int32_t  hi   = *(const int32_t  *)(s + 4);
    uint16_t lo   = *(const uint16_t *)(s + 2);
    uint32_t v_lo = ((uint32_t)hi << 16) | lo;
    int32_t  v_hi = hi >> 16;
    uint64_t v    = ((uint64_t)(uint32_t)v_hi << 32) | v_lo;
    uint32_t mag  = (m < 0) ? (uint32_t)(-(int32_t)m) : (uint32_t)m;
    int      neg  = ((v_hi ^ (int32_t)m) < 0);
    uint64_t p;

    if (v_hi < 0)
        v = (uint64_t)(-(int64_t)v);
    p = (v * mag) << 1;
    if (neg)
        p = (uint64_t)(-(int64_t)p);

    *(uint32_t *)((unsigned char *)dst + 0) = (uint32_t)p;
    *(uint32_t *)((unsigned char *)dst + 4) = (uint32_t)(p >> 32);
}

/* ---- slot 0x60: unpack a packed position ----------------------------------
 *
 * Expands an eight-byte record into the 24-byte three-component form: 24 bits,
 * 24 bits and 16 bits become the three high halves, and all three low halves
 * are zeroed.  The two 24-bit fields are read as overlapping unaligned dwords
 * at +0 and +3 and shifted into place, which is why the record is eight bytes
 * for seven bytes of payload.
 *
 * The first component is shifted left 8 then arithmetically right 7 -- a sign
 * extension from bit 23 and a doubling in one go.  The second is only shifted
 * left 8, so it is not sign extended the same way.  The asymmetry is retail's.
 */
void FS98_STDCALL fs98_util_unpack_position(const void *src, void *dst)
{
    const unsigned char *s = (const unsigned char *)src;
    unsigned char *d = (unsigned char *)dst;

    *(uint16_t *)(d + 0x02) = 0;
    *(uint16_t *)(d + 0x0a) = 0;
    *(uint16_t *)(d + 0x12) = 0;

    {   /* unaligned little-endian dword loads, as the retail code does */
        uint32_t a = (uint32_t)s[0] | ((uint32_t)s[1] << 8)
                   | ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24);
        uint32_t b = (uint32_t)s[3] | ((uint32_t)s[4] << 8)
                   | ((uint32_t)s[5] << 16) | ((uint32_t)s[6] << 24);
        int16_t  c = (int16_t)((uint16_t)s[6] | ((uint16_t)s[7] << 8));

        *(int32_t *)(d + 0x04) = (int32_t)(a << 8) >> 7;
        *(int32_t *)(d + 0x0c) = (int32_t)(b << 8);
        *(int32_t *)(d + 0x14) = c;
    }
}

/* ---- slot 0x3c: angle of a 2D vector (atan2) ------------------------------
 *
 * Returns a 16-bit angle, the same units everything else here uses: 0x4000 is
 * ninety degrees.  Two tables are involved, and they sit next to each other in
 * .data, which is how the sizes confirm themselves:
 *
 *   +0x3ac  257 words  arctangent over the first eighth-turn, interpolated
 *   +0x5b0    4 bytes  quadrant adjustments, indexed by the two sign bits
 *
 * 257 words is 514 bytes, running from +0x3ac to +0x5ae -- immediately before
 * the quadrant table. The extra entry exists because the interpolation reads
 * index+1.
 *
 * The shape is the usual one: take magnitudes, divide the smaller by the
 * larger to land inside the first eighth-turn, look the ratio up, then put the
 * answer back in the right octant and quadrant. Equal magnitudes short-circuit
 * to 0x2000, which also keeps a zero vector away from the divide.
 */
#define FS98_UTIL_ATANTABLE 0x3ac
#define FS98_UTIL_QUADTABLE 0x5b0

static int16_t fs98_util_atan_word(uint32_t byte_off)
{
    const unsigned char *p = fs98_util_data + FS98_UTIL_ATANTABLE + byte_off;
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* Interpolated lookup.  Transliterated: the `sub ax` and `add ax` are 16-bit
 * and leave the top half of the register alone, and the multiply that follows
 * is unsigned, so the half-width operations have to be reproduced as such. */
static int32_t fs98_util_atan_lookup(uint32_t x)
{
    uint32_t frac = (x & 0xffu) << 8;
    uint32_t idx  = (x >> 7) & 0x1feu;
    int16_t  t0   = fs98_util_atan_word(idx);
    int16_t  t1   = fs98_util_atan_word(idx + 2);
    uint32_t eax  = (uint32_t)(int32_t)t1;          /* movsx */

    eax = (eax & 0xffff0000u) | (uint16_t)((uint16_t)eax - (uint16_t)t0);
    eax = (uint32_t)(((uint64_t)eax * frac) & 0xffffffffu);   /* mul, low half */
    eax = (uint32_t)((int32_t)eax >> 16);                     /* sar eax,16 */
    eax = (eax & 0xffff0000u) | (uint16_t)((uint16_t)eax + (uint16_t)t0);
    return (int32_t)eax;
}

int32_t FS98_STDCALL fs98_util_atan2(int32_t x, int32_t y)
{
    uint32_t q = (((uint32_t)x < 0x80000000u) ? 2u : 0u)
               | (((uint32_t)y < 0x80000000u) ? 1u : 0u);
    uint32_t adjust = (uint32_t)fs98_util_data[FS98_UTIL_QUADTABLE + q];
    uint32_t ax = (uint32_t)((x < 0) ? -x : x);
    uint32_t cx = (uint32_t)((y < 0) ? -y : y);
    int32_t  a;

    if (cx > ax) {
        a = fs98_util_atan_lookup((uint32_t)((((uint64_t)ax << 32) / cx) >> 16));
        a = -a + 0x4000;
    } else if (cx == ax) {
        a = 0x2000;
    } else {
        a = fs98_util_atan_lookup((uint32_t)((((uint64_t)cx << 32) / ax) >> 16));
    }

    if (!(adjust & 0x40))
        a = -a + 0x4000;
    return a - (int32_t)(adjust << 8);
}

/* ---- slot 0x38: angle from a sine, with the quadrant resolved -------------
 *
 * Takes a Q15 sine in the first argument and the sign of the cosine in the
 * second, and returns a full-circle 16-bit angle.
 *
 * The lookup is piecewise over three ranges, each with its own table and its
 * own shift pattern, because the curve steepens towards +-1 and a single table
 * would lose resolution there:
 *
 *   +0x2f8   below 0x7800   60 words, index (x >> 8) & 0x7e
 *   +0x370   below 0x7e00   12 words, index ((x & 0x7ff) >> 6) & 0x1fe
 *   +0x388   at or above    16 words, index ((x & 0x1ff) >> 4) & 0x1e
 *
 * Those three run +0x2f8 to +0x370, +0x370 to +0x388, and +0x388 to +0x3a8,
 * landing immediately before the arctangent table at +0x3ac. Every table in
 * this module abuts the next.
 *
 * Each range is transliterated rather than generalised: the first truncates its
 * interpolation difference back to 16 bits before multiplying and the others do
 * not, the second doubles the product and masks it to a byte, the third masks
 * to eleven bits. Those are not interchangeable.
 */
#define FS98_UTIL_ASIN1 0x2f8
#define FS98_UTIL_ASIN2 0x370
#define FS98_UTIL_ASIN3 0x388

static int16_t fs98_util_word_at(uint32_t off)
{
    const unsigned char *p = fs98_util_data + off;
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int32_t fs98_util_asin_lookup(int32_t v)
{
    uint32_t eax = (uint32_t)v;
    int32_t  esi, ecx;

    if (v < 0x7800) {
        uint32_t idx = ((uint32_t)(v >> 8)) & 0x7eu;
        esi = fs98_util_word_at(FS98_UTIL_ASIN1 + idx);
        ecx = fs98_util_word_at(FS98_UTIL_ASIN1 + idx + 2) - esi;
        ecx = (int16_t)ecx;                      /* movsx cx */
        eax &= 0x1ffu;
        eax = (uint32_t)((int64_t)(int32_t)eax * ecx);
        eax = (uint32_t)((int32_t)eax >> 9);
        return (int32_t)eax + esi;
    }
    if (v < 0x7e00) {
        uint32_t idx;
        eax &= 0x7ffu;
        idx = ((uint32_t)((int32_t)eax >> 6)) & 0x1feu;
        esi = fs98_util_word_at(FS98_UTIL_ASIN2 + idx);
        ecx = fs98_util_word_at(FS98_UTIL_ASIN2 + idx + 2) - esi;
        eax &= 0x7fu;
        eax = (uint32_t)((int64_t)(int32_t)eax * ecx);
        eax += eax;
        eax = (uint32_t)((int32_t)eax >> 8);
        eax &= 0xffu;
        return (int32_t)eax + esi;
    }
    {
        uint32_t idx;
        eax &= 0x1ffu;
        idx = ((uint32_t)((int32_t)eax >> 4)) & 0x1eu;
        esi = fs98_util_word_at(FS98_UTIL_ASIN3 + idx);
        ecx = fs98_util_word_at(FS98_UTIL_ASIN3 + idx + 2) - esi;
        eax &= 0x1fu;
        eax = (uint32_t)((int64_t)(int32_t)eax * ecx);
        eax = (uint32_t)((int32_t)eax >> 5);
        eax &= 0x7ffu;
        return (int32_t)eax + esi;
    }
}

int32_t FS98_STDCALL fs98_util_angle_from_sin(int16_t sine, int16_t cosine)
{
    int32_t a = sine;

    if (a >= 0) {
        a = fs98_util_asin_lookup(a);
        if (cosine < 0)
            a = -a + 0x8000;
        return a;
    }

    /* The retail code follows its `neg` with a `dec` for the case where the
     * negation stays negative.  A value that arrived through `movsx` from 16
     * bits can never do that, so the decrement is unreachable; kept as a note
     * rather than as code. */
    a = fs98_util_asin_lookup(-a);
    if (cosine < 0)
        return a + 0x8000;
    return -a;
}

/* ---- slots 0xbc and 0xc0: the 48-bit type is 32.16 fixed point ------------
 *
 * These two convert between the 48-bit form and a double, and the constants
 * they use say what the format is: 65536.0 one way and -1/65536 the other. So
 * the 32-bit high half is an integer and the 16-bit low half is a fraction in
 * 1/65536ths -- the position fields throughout the sim are 32.16 fixed point.
 *
 * Both go through MSVCRT's `_ftol`, which truncates towards zero rather than
 * rounding, and both treat the integer half as *unsigned* when converting it
 * back to a double.
 *
 * In slot 0xbc that is harmless, for the reason given on fs98_util_ftol below.
 * In slot 0xc0 it is not: a negative position converts to a value near 2^32
 * rather than to itself, so the pair does not round-trip across zero. Both
 * behaviours are reproduced.
 */

/* `_ftol` returns a *64-bit* truncation towards zero, and these two callers
 * keep only part of it.  That matters more than it looks:
 *
 * The integer part is converted back to a double as unsigned -- retail stores
 * it as the low dword of a quadword whose high dword it has just zeroed -- so
 * for a negative value the subtraction that follows is off by exactly 2^32.
 * Multiplied by 65536 that is 2^48, which cannot reach the sixteen bits
 * actually kept, so the fraction still comes out right. Model `_ftol` as
 * 32-bit and it saturates instead, and the fraction comes out zero.
 */
static int64_t fs98_util_ftol(long double v)
{
    if (!(v >= -9223372036854775808.0L && v < 9223372036854775808.0L))
        return (int64_t)0x8000000000000000LL;   /* the x87's indefinite */
    return (int64_t)v;
}

/* Slot 0xc0.  48-bit to double.  Written as retail writes it -- a subtraction
 * of a negative reciprocal -- though the result is identical to adding, since
 * 1/65536 is a power of two and both operations are exact. */
void FS98_STDCALL fs98_util_fixed48_to_double(double *out, const void *in48)
{
    const unsigned char *s = (const unsigned char *)in48;
    uint32_t hi = *(const uint32_t *)(s + 4);
    uint16_t lo = *(const uint16_t *)(s + 2);

    *out = (double)hi - (double)lo * (-1.0 / 65536.0);
}

/* Slot 0xbc.  Double to 48-bit. */
void FS98_STDCALL fs98_util_double_to_fixed48(const double *in, void *out48)
{
    unsigned char *d = (unsigned char *)out48;
    double v = *in;
    int32_t hi = (int32_t)fs98_util_ftol(v);
    /* The whole chain stays on the x87 in retail, at 80-bit extended
     * precision, and with that deliberate 2^32 offset in play the precision
     * is load-bearing: computed in 64-bit double the fraction comes out one
     * or two units different.  `long double` is 80-bit on x86. */
    long double frac = (long double)v - (long double)(uint32_t)hi;
    int64_t lo = fs98_util_ftol(frac * 65536.0L);

    *(int32_t  *)(d + 4) = hi;
    *(uint16_t *)(d + 2) = (uint16_t)lo;
}

/* ---- slot 0xb8: a floating-point generator --------------------------------
 *
 *     state = frac(2531011 - state * -214013)
 *
 * 214013 and 2531011 are the Microsoft C runtime's `rand()` constants, used
 * here in floating point and keeping only the fractional part -- which gives a
 * value in [0,1) without an integer overflow to mask off.
 *
 * The state lives in the last eight bytes of .data and starts at zero, and
 * zero is a fixed point: 2531011 has no fractional part, so an unseeded
 * generator returns zero forever. Something else must seed it; nothing
 * reimplemented so far does, so the seeding service is still among the
 * unfinished ones.
 *
 * Written the way retail writes it -- a subtraction of a negative multiplier --
 * though `a - b*(-c)` and `a + b*c` agree exactly, since negating a double is
 * exact.
 */
static double fs98_util_frand_state = 0.0;

void fs98_util_frand_seed(double seed)
{
    fs98_util_frand_state = seed;
}

double fs98_util_frand_get(void)
{
    return fs98_util_frand_state;
}

/* `modf`'s fractional part, without needing libm: truncation toward zero is
 * exact for anything small enough to have a fractional part at all. */
static double fs98_util_frac(double x)
{
    if (!(x > -9223372036854775808.0 && x < 9223372036854775808.0))
        return 0.0;
    return x - (double)(int64_t)x;
}

double FS98_STDCALL fs98_util_frand(void)
{
    double x = 2531011.0 - fs98_util_frand_state * (-214013.0);
    fs98_util_frand_state = fs98_util_frac(x);
    return fs98_util_frand_state;
}

/* ---- slot 0x9c: the longer of two strings ---------------------------------
 *
 * Measures both through the *host's* service at `+0x2b8`, which is a null-safe
 * `strlen` (a `repnz scasb` returning 0 for a null pointer), and returns
 * whichever is longer. Ties go to the second argument: the test is strictly
 * greater, on the first.
 *
 * This is the first reimplemented service that calls out of the module, so it
 * needs the host pointer the loader puts in the ImportTable. The module
 * supplies it; the differential harness injects a stub host with a real strlen
 * at the same offset.
 */
void *fs98_util_host;      /* the harness's stand-in; see util_imports.h */

void fs98_util_set_host(void *host)
{
    fs98_util_host = host;
}

const char * FS98_STDCALL fs98_util_longer_string(const char *a, const char *b)
{
    /* measured in retail's order, second then first */
    uint32_t lb = FS98_HOST_STRLEN(FS98_UTIL_HOST)(b);
    uint32_t la = FS98_HOST_STRLEN(FS98_UTIL_HOST)(a);
    return (la > lb) ? a : b;
}

/* ---- slot 0x04: init ------------------------------------------------------
 *
 * Seeds both generators from the host's millisecond clock -- service `+0x38`,
 * which is `timeGetTime`:
 *
 *   the integer generator takes the tick directly;
 *   the floating-point one takes 1.0 / tick, retrying while the clock reads
 *   zero so the division cannot fault.
 *
 * The clock is read twice, once for each, and the retry loop applies only to
 * the second. Nothing here is a function of its arguments, so it looked at
 * first like something the differential harness could say nothing about -- but
 * with a host stub returning a known sequence it becomes perfectly testable,
 * because what matters is the state left behind, and that is observable.
 */
void FS98_STDCALL fs98_util_init(void)
{
    uint32_t t;

    fs98_util_rand_state = FS98_HOST_TICKS(FS98_UTIL_HOST)();

    do {
        t = FS98_HOST_TICKS(FS98_UTIL_HOST)();
    } while (t == 0);
    fs98_util_frand_state = 1.0 / (double)t;
}

/* ---- slot 0xc4: double to 32.16, signed ----------------------------------
 *
 * The well-behaved sibling of `+0xbc`. A negative input is first wrapped into
 * the positive range -- the three constants work out to exactly `v + 2^32` --
 * and the integer part is then read back with a *signed* `fild DWORD` rather
 * than the zero-extended quadword `+0xbc` uses. The result is a correct
 * two's-complement 32.16 value: -1.5 becomes hi = -2, fraction = 0.5.
 *
 * It needs 80-bit arithmetic for the same reason `+0xbc` does, which was not
 * obvious and was got wrong here first: the wrapped value is just under 2^32,
 * so in 64-bit double only about twenty bits of fraction survive, and the low
 * sixteen bits of the result come out one short. Retail keeps the whole chain
 * on the x87. So this is the second service that will differ by a unit on ARM,
 * under the decision recorded in the precision note above.
 */
void FS98_STDCALL fs98_util_double_to_fixed48_signed(const double *in, void *out48)
{
    unsigned char *d = (unsigned char *)out48;
    double v = *in;
    long double x = (long double)v;
    int32_t hi;
    int64_t lo;

    if (v < 0.0)                                  /* fcom / test ah,1 */
        x = (281474976710656.0L - (long double)v * -65536.0L)
            * 1.52587890625e-05L;

    hi = (int32_t)fs98_util_ftol(x);              /* signed fild DWORD */
    lo = fs98_util_ftol((x - (long double)hi) * 65536.0L);

    *(int32_t  *)(d + 4) = hi;
    *(uint16_t *)(d + 2) = (uint16_t)lo;
}

/* ---- slot 0xc8: 32.16 to double, signed -----------------------------------
 *
 * The counterpart of `+0xc4`, and the one `+0xc0` should have been. It builds
 * the raw 48-bit integer as `hi * 65536 + lo` with `hi` taken unsigned, and
 * then, if `hi` was actually negative, subtracts 2^48 to wrap it back below
 * zero before scaling. So unlike `+0xc0` it round-trips across zero:
 * hi = -2 with a fraction of 0.5 comes back as -1.5 rather than as something
 * near 2^32.
 *
 * Every step is exact in double: the raw value needs 49 bits and a double
 * carries 53, and both 2^48 and 1/65536 are powers of two. No 80-bit
 * dependency here, unlike the two conversions above -- but that was claimed
 * once before and was wrong, so `make utildiff-portable` is what settles it.
 *
 * Retail writes intermediate values through the output pointer and reads them
 * back; that is not reproduced, since no caller can observe it.
 */
void FS98_STDCALL fs98_util_fixed48_to_double_signed(double *out, const void *in48)
{
    const unsigned char *s = (const unsigned char *)in48;
    uint16_t lo = *(const uint16_t *)(s + 2);
    int32_t  hi = *(const int32_t  *)(s + 4);
    double raw = (double)lo - (double)(uint32_t)hi * -65536.0;

    if (hi < 0)
        raw = (281474976710656.0 - raw) * -1.0;

    *out = raw * 1.52587890625e-05;
}

/* ---- slot 0x48: longitude back to metres ----------------------------------
 *
 * The inverse of `+0x44`: same latitude table, same interpolation, but the
 * 48-bit input is *multiplied* by the circumference instead of divided into
 * it, and the top 64 bits of the 96-bit product are kept.
 *
 * A negative input is negated going in and the result negated coming out,
 * which is the straightforward treatment `+0x44` conspicuously does not get.
 */
void FS98_STDCALL fs98_util_longitude_to_metres(const void *in48,
                                                const void *pos48,
                                                void *out48)
{
    const unsigned char *in = (const unsigned char *)in48;
    unsigned char *out = (unsigned char *)out48;
    int32_t  hi   = *(const int32_t  *)(in + 4);
    uint16_t lo   = *(const uint16_t *)(in + 2);
    uint32_t v_lo = ((uint32_t)hi << 16) | lo;
    int32_t  v_hi = hi >> 16;
    uint32_t scale;
    uint64_t res;
    int negate = 0;

    if (v_hi < 0) {                          /* 64-bit negate of v_hi:v_lo */
        uint32_t old = v_lo;
        v_lo = (uint32_t)(-(int32_t)v_lo);
        v_hi = (int32_t)(~(uint32_t)v_hi + (old != 0 ? 0u : 1u));
        negate = 1;
    }

    scale = fs98_util_lat_scale(pos48);

    /* (value * scale) >> 32, as the high half of the low product plus the
     * whole of the high product */
    res = (uint64_t)(uint32_t)v_hi * scale
        + (((uint64_t)v_lo * scale) >> 32);

    if (negate)
        res = (uint64_t)(-(int64_t)res);

    *(uint16_t *)(out + 2) = (uint16_t)res;
    *(uint32_t *)(out + 4) = (uint32_t)((res >> 16) & 0xffffffffu);
}

/* ---- slot 0x68: distance and bearing between two points -------------------
 *
 * Takes two three-component integer positions and a flags byte, and writes a
 * distance and a 16-bit bearing. The flags select the work:
 *
 *   bit 0   compute the distance; otherwise it is written as zero
 *   bit 1   compute the bearing; otherwise it is written as zero
 *   bit 3   ignore the second component -- a horizontal-only measurement
 *
 * The bearing is `atan2` of the first and third deltas, so the second
 * component is the vertical one. Before that call, if both deltas are small
 * enough that their top halves sit strictly inside +-0x4000, both are doubled
 * -- a free bit of precision for short distances, skipped when it would
 * overflow.
 *
 * The distance is the square root of the sum of squares, rounded and scaled by
 * `(root + 0x80) >> 8` -- a *logical* shift, which matters because the sum can
 * overflow 64 bits for distant points and hand the root its top bit. Note also
 * that this sum carries correctly, low half first: SLEW's equivalent does not,
 * which is a reminder that the bugs are particular rather than house style.
 */
void FS98_STDCALL fs98_util_range_and_bearing(const int32_t *a, const int32_t *b,
                                              int32_t flags, int32_t *out_dist,
                                              int16_t *out_bearing)
{
    uint8_t f = (uint8_t)flags;
    int32_t dx = b[0] - a[0];
    int32_t dy = 0;
    int32_t dz = b[2] - a[2];
    int32_t angle = 0;
    int32_t dist = 0;

    if (!(f & 8))
        dy = b[1] - a[1];

    if (f & 2) {
        int32_t ax = dx, az = dz;
        if ((ax | az) != 0) {
            int16_t hx = (int16_t)((uint32_t)ax >> 16);
            if (hx > (int16_t)0xc000 && hx < 0x4000) {
                int16_t hz = (int16_t)((uint32_t)az >> 16);
                if (hz > (int16_t)0xc000 && hz < 0x4000) {
                    ax += ax;
                    az += az;
                }
            }
        }
        angle = fs98_util_atan2(ax, az);
    }

    if (f & 1) {
        uint32_t sum[2];
        int64_t acc = (int64_t)dx * dx;
        if (!(f & 8))
            acc += (int64_t)dy * dy;
        acc += (int64_t)dz * dz;
        sum[0] = (uint32_t)acc;              /* low half first, as retail */
        sum[1] = (uint32_t)((uint64_t)acc >> 32);
        /* `shr`, not `sar`: the scaling is a logical shift, so a root with
         * its top bit set -- which an overflowing sum of squares can produce
         * -- scales to a large positive rather than staying negative.  An
         * arithmetic shift here is wrong by exactly 2^24. */
        dist = (int32_t)((uint32_t)(fs98_util_sqrt64(sum) + 0x80) >> 8);
    }

    *out_dist    = dist;
    *out_bearing = (int16_t)angle;
}

/* ---- slot 0x4c: rotate a vector through three angles ----------------------
 *
 * Takes three 16-bit angles and a three-component 16-bit vector and writes a
 * three-component 32-bit result. `+0x2c` is called three times, for angles[1],
 * angles[0] and angles[2] in that order, so the rotations are applied about
 * the second axis, then the first, then the third.
 *
 * Every stage doubles its result, which is the Q15 normalisation showing up
 * again, and the later stages keep bits 16..47 of a 64-bit product rather than
 * the low 32.
 *
 * One asymmetry is retail's and is reproduced: the final pair of products is
 * summed with `add edx,ebx` where the matching subtraction uses `sbb`, so the
 * carry out of the low half is discarded in the sum but borrowed correctly in
 * the difference.
 */
static int32_t fs98_util_shr16_64(int64_t p)
{
    return (int32_t)((uint64_t)p >> 16);          /* shld edx,eax,0x10 */
}

void FS98_STDCALL fs98_util_rotate3d(const int16_t *angles, const int16_t *vec,
                                     int32_t *out)
{
    int16_t s, c;
    int32_t X, Y, Z, W, P, R;

    fs98_util_sincos(angles[1], &s, &c);
    X = (int32_t)(((int32_t)vec[0] * c) - ((int32_t)s * vec[1])) * 2;
    Y = (int32_t)(((int32_t)vec[1] * c) + ((int32_t)vec[0] * s)) * 2;

    fs98_util_sincos(angles[0], &s, &c);
    Z = (int32_t)(((int32_t)vec[2] * c) + fs98_util_shr16_64((int64_t)s * Y)) * 2;
    W = (int32_t)(fs98_util_shr16_64((int64_t)c * Y) - ((int32_t)vec[2] * s)) * 2;

    fs98_util_sincos(angles[2], &s, &c);
    {   /* difference, with the borrow propagated (sbb) */
        int64_t a = (int64_t)c * Z;
        int64_t b = (int64_t)s * X;
        uint32_t lo = (uint32_t)a - (uint32_t)b;
        uint32_t bo = ((uint32_t)a < (uint32_t)b) ? 1u : 0u;
        uint32_t hi = (uint32_t)((uint64_t)a >> 32) - (uint32_t)((uint64_t)b >> 32) - bo;
        P = fs98_util_shr16_64((int64_t)((((uint64_t)hi << 32) | lo) << 1));
    }
    {   /* sum, with the carry *not* propagated -- `add edx,ebx`, not `adc` */
        int64_t a = (int64_t)c * X;
        int64_t b = (int64_t)s * Z;
        uint32_t lo = (uint32_t)a + (uint32_t)b;
        uint32_t hi = (uint32_t)((uint64_t)a >> 32) + (uint32_t)((uint64_t)b >> 32);
        R = fs98_util_shr16_64((int64_t)((((uint64_t)hi << 32) | lo) << 1));
    }

    out[0] = R;
    out[1] = W;
    out[2] = P;
}

/* ---- slot 0x64: range and bearing between 48-bit positions ----------------
 *
 * The 48-bit counterpart of `+0x68`. It differences two three-component
 * positions in the 24-byte vector form, converts the second component from
 * longitude to metres through `+0x48`, and reports a distance, a bearing and a
 * *scale*.
 *
 * The scale exists because the differences are 48-bit and the arithmetic that
 * follows is 32-bit. Each component whose top sixteen bits are redundant -- all
 * zeroes or all ones, with the two halves' signs agreeing -- is shifted up by
 * sixteen and the shared counter, which starts at 3, is decremented. So the
 * caller is handed three values on a common exponent plus the exponent, rather
 * than three values it cannot compare.
 *
 * Flags, as in `+0x68`: bit 0 distance, bit 1 bearing, bit 3 ignore the third
 * component. Bit 2 is new and enables the range reduction.
 *
 * The bearing's normalisation loop is worth a note: it tests `jle` against
 * +0x4000 where `+0x68` tests `jge`, so where `+0x68` scales *small* values up,
 * this one scales *large* values up and terminates only when the doubling
 * overflows into the opposite sign. Reproduced as written; the two functions
 * are otherwise so alike that this is unlikely to be deliberate.
 */
static int32_t fs98_util_reduce48(int32_t hi, uint16_t lo, int32_t flags,
                                  uint8_t *scale)
{
    if (flags & 4) {
        uint16_t top = (uint16_t)((uint32_t)hi >> 16);
        if (top == 0 || top == 0xffff) {
            if (!((top ^ (uint16_t)hi) & 0x8000u)) {    /* the `js` test */
                hi = (int32_t)(((uint32_t)hi << 16) | lo);
                (*scale)--;
            }
        }
    }
    return hi;
}

/* one component of the 24-byte vector difference, b - a, as lo16 + hi32 */
static int32_t fs98_util_sub48(const unsigned char *b, const unsigned char *a,
                               int off, uint16_t *lo_out)
{
    uint32_t t = (uint32_t)*(const uint16_t *)(b + off + 2)
               - (uint32_t)*(const uint16_t *)(a + off + 2);
    *lo_out = (uint16_t)t;
    return *(const int32_t *)(b + off + 4) - *(const int32_t *)(a + off + 4)
         - (int32_t)((t >> 16) & 1);
}

void FS98_STDCALL fs98_util_range_and_bearing48(const void *a_, const void *b_,
                                                int32_t flags, int32_t *out_dist,
                                                int16_t *out_bearing,
                                                uint8_t *out_scale)
{
    const unsigned char *a = (const unsigned char *)a_;
    const unsigned char *b = (const unsigned char *)b_;
    uint8_t scale = 3;
    uint16_t lo0, lo1, lo2, lon_lo;
    int32_t raw0, raw2 = 0, D0, D1, D2 = 0, raw_lon;
    int32_t angle = 0, dist = 0;
    unsigned char dif[8], conv[8];

    raw0 = fs98_util_sub48(b, a, 0x00, &lo0);
    D0   = fs98_util_reduce48(raw0, lo0, flags, &scale);

    if (!(flags & 8)) {
        raw2 = fs98_util_sub48(b, a, 0x10, &lo2);
        D2   = fs98_util_reduce48(raw2, lo2, flags, &scale);
    }

    /* the middle component goes through the longitude conversion first */
    {
        int32_t h = fs98_util_sub48(b, a, 0x08, &lo1);
        *(uint16_t *)(dif + 2) = lo1;
        *(int32_t  *)(dif + 4) = h;
        fs98_util_longitude_to_metres(dif, a, conv);
        lon_lo  = *(uint16_t *)(conv + 2);
        raw_lon = *(int32_t  *)(conv + 4);
        D1      = fs98_util_reduce48(raw_lon, lon_lo, flags, &scale);
    }

    if (flags & 1) {
        uint32_t sum[2];
        int64_t acc = (int64_t)D1 * D1;
        if (!(flags & 8))
            acc += (int64_t)D2 * D2;
        acc += (int64_t)D0 * D0;
        sum[0] = (uint32_t)acc;
        sum[1] = (uint32_t)((uint64_t)acc >> 32);
        dist = fs98_util_sqrt64(sum);
    }

    if (flags & 2) {
        int32_t eb = raw0, ed = raw_lon;
        if ((eb | ed) != 0) {
            for (;;) {
                int16_t si = (int16_t)((uint32_t)eb >> 16);
                if (si <= (int16_t)0xc000 || si <= 0x4000)
                    break;
                si = (int16_t)((uint32_t)ed >> 16);
                if (si <= (int16_t)0xc000 || si <= 0x4000)
                    break;
                eb += eb;
                ed += ed;
            }
        }
        angle = fs98_util_atan2(ed, eb);
    }

    *out_dist    = dist;
    *out_bearing = (int16_t)angle;
    *out_scale   = scale;
}

/* ---- slot 0x88: the marshalling engine ------------------------------------
 *
 * Walks an array of 16-byte descriptors, converting values out of one source
 * stream into wherever each descriptor points:
 *
 *   +0x0  repeat count
 *   +0x4  source type, 0..3
 *   +0x8  destination type, 0..7
 *   +0xc  destination pointer
 *
 * The source pointer is set once and advances across every descriptor, so the
 * descriptors describe a packed record being unpacked into separate fields.
 * The destination is reloaded per descriptor.
 *
 * Dispatch is a 4x8 matrix in .data: four sub-tables at +0x700, +0x720, +0x740
 * and +0x760, selected by source type, indexed by destination type. The source
 * types are a byte, a word, a dword and the 48-bit fixed-point value; the
 * destination types are those widths again in zero- and sign-extending
 * variants, plus a null-terminated string copy and a no-op.
 *
 * The no-op advances neither pointer, so a descriptor can be disabled by
 * pointing it at a combination the matrix fills with it -- which is most of
 * the 48-bit row.
 *
 * Retail indexes both tables without bounds checks. The guards here are not
 * reproducing anything; they exist so that a bad descriptor is inert rather
 * than undefined.
 */
enum {
    FS98_MAR_NOP = 0, FS98_MAR_B2B, FS98_MAR_B2W_ZX, FS98_MAR_B2W_SX,
    FS98_MAR_B2D_ZX, FS98_MAR_B2D_SX, FS98_MAR_B2STR,
    FS98_MAR_W2B, FS98_MAR_W2W, FS98_MAR_W2D_ZX, FS98_MAR_W2D_SX,
    FS98_MAR_D2B, FS98_MAR_D2W, FS98_MAR_D2D, FS98_MAR_COPY48
};

/* the dispatch matrix exactly as .data holds it */
static const unsigned char fs98_util_marshal_matrix[4][8] = {
    { FS98_MAR_B2B,   FS98_MAR_B2B,   FS98_MAR_B2W_ZX, FS98_MAR_B2W_SX,
      FS98_MAR_B2D_ZX,FS98_MAR_B2D_SX,FS98_MAR_NOP,    FS98_MAR_B2STR },
    { FS98_MAR_W2B,   FS98_MAR_W2B,   FS98_MAR_W2W,    FS98_MAR_W2W,
      FS98_MAR_W2D_ZX,FS98_MAR_W2D_SX,FS98_MAR_NOP,    FS98_MAR_NOP },
    { FS98_MAR_D2B,   FS98_MAR_D2B,   FS98_MAR_D2W,    FS98_MAR_D2W,
      FS98_MAR_D2D,   FS98_MAR_D2D,   FS98_MAR_NOP,    FS98_MAR_NOP },
    { FS98_MAR_NOP,   FS98_MAR_NOP,   FS98_MAR_NOP,    FS98_MAR_NOP,
      FS98_MAR_NOP,   FS98_MAR_NOP,   FS98_MAR_COPY48, FS98_MAR_NOP }
};

static void fs98_util_marshal_one(unsigned op, unsigned char **dst,
                                  const unsigned char **src)
{
    unsigned char *d = *dst;
    const unsigned char *s = *src;

    switch (op) {
    case FS98_MAR_B2B:    *d = *s; d += 1; s += 1; break;
    case FS98_MAR_B2W_ZX: *(uint16_t *)d = (uint16_t)*s;          d += 2; s += 1; break;
    case FS98_MAR_B2W_SX: *(int16_t  *)d = (int16_t)(int8_t)*s;   d += 2; s += 1; break;
    case FS98_MAR_B2D_ZX: *(uint32_t *)d = (uint32_t)*s;          d += 4; s += 1; break;
    case FS98_MAR_B2D_SX: *(int32_t  *)d = (int32_t)(int8_t)*s;   d += 4; s += 1; break;
    case FS98_MAR_B2STR:  do { *d = *s; d++; s++; } while (d[-1]); break;
    case FS98_MAR_W2B:    *d = (unsigned char)*(const uint16_t *)s; d += 1; s += 2; break;
    case FS98_MAR_W2W:    *(uint16_t *)d = *(const uint16_t *)s;    d += 2; s += 2; break;
    case FS98_MAR_W2D_ZX: *(uint32_t *)d = *(const uint16_t *)s;    d += 4; s += 2; break;
    case FS98_MAR_W2D_SX: *(int32_t  *)d = *(const int16_t  *)s;    d += 4; s += 2; break;
    case FS98_MAR_D2B:    *d = (unsigned char)*(const uint32_t *)s; d += 1; s += 4; break;
    case FS98_MAR_D2W:    *(uint16_t *)d = (uint16_t)*(const uint32_t *)s; d += 2; s += 4; break;
    case FS98_MAR_D2D:    *(uint32_t *)d = *(const uint32_t *)s;    d += 4; s += 4; break;
    case FS98_MAR_COPY48:
        *(uint16_t *)(d + 2) = *(const uint16_t *)s;  s += 2;
        *(uint32_t *)(d + 4) = *(const uint32_t *)s;  s += 4;  d += 8;
        break;
    default: break;                       /* NOP advances nothing */
    }
    *dst = d;
    *src = s;
}

void FS98_STDCALL fs98_util_marshal(const void *source, const void *descriptors,
                                    int32_t count)
{
    const unsigned char *src = (const unsigned char *)source;
    const unsigned char *d   = (const unsigned char *)descriptors;

    while (count != 0) {
        uint32_t reps = *(const uint32_t *)(d + 0);
        uint32_t styp = *(const uint32_t *)(d + 4);
        uint32_t dtyp = *(const uint32_t *)(d + 8);

        if (reps && styp < 4 && dtyp < 8) {
            unsigned char *dst = *(unsigned char *const *)(d + 12);
            unsigned op = fs98_util_marshal_matrix[styp][dtyp];
            do {
                fs98_util_marshal_one(op, &dst, &src);
            } while (--reps);
        }
        d += 16;
        count--;
    }
}
