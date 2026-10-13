/* Differential test for individual UTIL.DLL services.
 *
 * UTIL cannot be swapped into the game until all 47 of its slots exist, so a
 * part-finished module cannot be tested by running the sim.  Instead this
 * loads the retail UTIL.DLL, reads the service pointers straight out of its
 * Linkage, and compares each against the reimplementation over a sweep of
 * inputs -- edge cases first, then randomised.
 *
 * The services covered here touch neither GLOBAL nor NUMERIC, so retail's
 * ImportTable is left unbound; they never follow those pointers.  A service
 * that does reach for an import will need the binding that slewdiff.c does.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FS98_STDCALL __attribute__((stdcall))
int32_t FS98_STDCALL fs98_util_sqrt32(uint32_t v);
int32_t FS98_STDCALL fs98_util_sqrt64(const void *p);
int32_t FS98_STDCALL fs98_util_hypot(int16_t a, int16_t b);
void    FS98_STDCALL fs98_util_sincos(int16_t a, int16_t *s, int16_t *c);
void    FS98_STDCALL fs98_util_metres_to_longitude(const void *in, const void *pos, void *out);
int32_t FS98_STDCALL fs98_util_stub_50(int32_t, int32_t);
int32_t FS98_STDCALL fs98_util_stub_54(int32_t, int32_t);
int32_t FS98_STDCALL fs98_util_stub_58(int32_t, int32_t);
void    FS98_STDCALL fs98_util_stub_5c(void);
void    FS98_STDCALL fs98_util_deinit(void);
int32_t  FS98_STDCALL fs98_util_sqrt16(uint16_t v);
uint32_t FS98_STDCALL fs98_util_rand(void);
uint32_t FS98_STDCALL fs98_util_rand32(void);
void     FS98_STDCALL fs98_util_copy24(void *dst, const void *src);
int32_t  FS98_STDCALL fs98_util_double_to_int(double v);
void                  fs98_util_rand_seed(uint32_t seed);
void     FS98_STDCALL fs98_util_v48_add(void *, const void *, const void *);
void     FS98_STDCALL fs98_util_v48_sub(void *, const void *, const void *);
int32_t  FS98_STDCALL fs98_util_v48_equal(const void *, const void *);
void     FS98_STDCALL fs98_util_shr48(void *, const void *, uint32_t);
int32_t  FS98_STDCALL fs98_util_double_to_int64_low(double);
void     FS98_STDCALL fs98_util_rotate2d(int32_t, int32_t, int16_t, int16_t,
                                         int32_t *, int32_t *);
void     FS98_STDCALL fs98_util_div48_u16(const void *, uint16_t, void *);
void     FS98_STDCALL fs98_util_metres32_to_longitude(const int32_t *,
                                                      const void *, void *);
void     FS98_STDCALL fs98_util_mul48_s16(const void *, int16_t, void *);
void     FS98_STDCALL fs98_util_unpack_position(const void *, void *);
int32_t  FS98_STDCALL fs98_util_atan2(int32_t, int32_t);
int32_t  FS98_STDCALL fs98_util_angle_from_sin(int16_t, int16_t);
void     FS98_STDCALL fs98_util_fixed48_to_double(double *, const void *);
void     FS98_STDCALL fs98_util_double_to_fixed48(const double *, void *);
void     FS98_STDCALL fs98_util_double_to_fixed48_signed(const double *, void *);
void     FS98_STDCALL fs98_util_fixed48_to_double_signed(double *, const void *);
double   FS98_STDCALL fs98_util_frand(void);
void                  fs98_util_frand_seed(double);
const char * FS98_STDCALL fs98_util_longer_string(const char *, const char *);
void                  fs98_util_set_host(void *);
uint32_t              fs98_util_rand_get(void);
double                fs98_util_frand_get(void);
void     FS98_STDCALL fs98_util_init(void);
uint32_t FS98_STDCALL fs98_util_rand(void);

typedef int32_t (FS98_STDCALL *fn_sqrt32)(uint32_t);
typedef int32_t (FS98_STDCALL *fn_sqrt64)(const void *);
typedef int32_t (FS98_STDCALL *fn_hypot)(int16_t, int16_t);
typedef void    (FS98_STDCALL *fn_sincos)(int16_t, int16_t *, int16_t *);
typedef void    (FS98_STDCALL *fn_m2lon)(const void *, const void *, void *);
typedef int32_t (FS98_STDCALL *fn_pick2)(int32_t, int32_t);
typedef void    (FS98_STDCALL *fn_void)(void);
typedef int32_t  (FS98_STDCALL *fn_sqrt16)(uint16_t);
typedef uint32_t (FS98_STDCALL *fn_rand)(void);
typedef void     (FS98_STDCALL *fn_copy24)(void *, const void *);
typedef int32_t  (FS98_STDCALL *fn_d2i)(double);
typedef void     (FS98_STDCALL *fn_v48op)(void *, const void *, const void *);
typedef int32_t  (FS98_STDCALL *fn_v48eq)(const void *, const void *);
typedef void     (FS98_STDCALL *fn_shr48)(void *, const void *, uint32_t);
typedef void     (FS98_STDCALL *fn_rot2d)(int32_t, int32_t, int16_t, int16_t,
                                          int32_t *, int32_t *);
typedef void     (FS98_STDCALL *fn_div48)(const void *, uint16_t, void *);
typedef void     (FS98_STDCALL *fn_m32lon)(const int32_t *, const void *, void *);
typedef void     (FS98_STDCALL *fn_mul48)(const void *, int16_t, void *);
typedef void     (FS98_STDCALL *fn_unpack)(const void *, void *);
typedef int32_t  (FS98_STDCALL *fn_atan2)(int32_t, int32_t);
typedef int32_t  (FS98_STDCALL *fn_asin)(int16_t, int16_t);
typedef void     (FS98_STDCALL *fn_f48d)(double *, const void *);
typedef void     (FS98_STDCALL *fn_df48)(const double *, void *);
typedef double   (FS98_STDCALL *fn_frand)(void);
typedef const char * (FS98_STDCALL *fn_longer)(const char *, const char *);
typedef uint32_t (FS98_STDCALL *fn_strlen)(const char *);

/* A stand-in host.  Only one of its 194 services is needed here: +0x2b8, the
 * null-safe strlen that UTIL's string comparison measures through. */
static unsigned char host_blob[0x320];
#define HOST_SLOT(off) (*(void **)(host_blob + (off)))

/* A clock the test controls: a fixed sequence including the zero that the
 * retry loop in init exists to skip. */
static const uint32_t tick_seq[] = { 12345, 0, 0, 777, 1, 0xffffffffu, 2 };
static unsigned tick_pos;

static uint32_t FS98_STDCALL stub_ticks(void)
{
    uint32_t v = tick_seq[tick_pos % (sizeof tick_seq / sizeof tick_seq[0])];
    tick_pos++;
    return v;
}

static uint32_t FS98_STDCALL stub_strlen(const char *s)
{
    uint32_t n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

/* 48-bit value: 16-bit low half at +2, 32-bit high half at +4 */
static void put48(unsigned char *p, uint16_t lo, int32_t hi)
{
    *(uint16_t *)(p + 2) = lo;
    *(int32_t  *)(p + 4) = hi;
}

static void *slot(HMODULE h, unsigned off)
{
    unsigned char *link = (unsigned char *)GetProcAddress(h, "Linkage");
    return link ? *(void **)(link + off) : NULL;
}

static uint32_t rng_state = 0x9e3779b9u;
static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

int main(void)
{
    HMODULE h = LoadLibraryA("UTIL_retail.dll");
    fn_sqrt32 r_sqrt32; fn_sqrt64 r_sqrt64; fn_hypot r_hypot;
    fn_sincos r_sincos; fn_m2lon r_m2lon;
    unsigned i, bad = 0, checked = 0;

    if (!h) { printf("cannot load UTIL_retail.dll (%lu)\n", GetLastError()); return 2; }
    r_sqrt32 = (fn_sqrt32)slot(h, 0x1c);
    r_sqrt64 = (fn_sqrt64)slot(h, 0x18);
    r_hypot  = (fn_hypot) slot(h, 0x28);
    r_sincos = (fn_sincos)slot(h, 0x2c);
    r_m2lon  = (fn_m2lon) slot(h, 0x44);
    if (!r_sqrt32 || !r_sqrt64 || !r_hypot || !r_sincos || !r_m2lon) {
        printf("missing service slots\n"); return 2;
    }

    /* slot 0x1c -- sqrt32 */
    for (i = 0; i < 300000; i++) {
        uint32_t v = (i < 300) ? i : rng();
        int32_t a = r_sqrt32(v), b = fs98_util_sqrt32(v);
        checked++;
        if (a != b && bad++ < 8)
            printf("sqrt32(%u): retail %d, openfs98 %d\n", v, a, b);
    }

    /* slot 0x18 -- sqrt64 */
    for (i = 0; i < 300000; i++) {
        uint32_t q[2];
        int32_t a, b;
        if (i < 4) { q[0] = (uint32_t)i; q[1] = 0; }
        else       { q[0] = rng(); q[1] = rng(); }
        a = r_sqrt64(q); b = fs98_util_sqrt64(q);
        checked++;
        if (a != b && bad++ < 8)
            printf("sqrt64(%08x%08x): retail %d, openfs98 %d\n", q[1], q[0], a, b);
    }

    /* slot 0x28 -- hypot.  16 bit inputs, so sweep a slice exhaustively. */
    for (i = 0; i < 300000; i++) {
        int16_t x, y;
        int32_t a, b;
        if (i < 65536) { x = (int16_t)(i - 32768); y = (int16_t)(i & 0x7f); }
        else           { x = (int16_t)rng(); y = (int16_t)rng(); }
        a = r_hypot(x, y); b = fs98_util_hypot(x, y);
        checked++;
        if (a != b && bad++ < 8)
            printf("hypot(%d,%d): retail %d, openfs98 %d\n", x, y, a, b);
    }

    /* slot 0x2c -- sincos.  The whole 16 bit input domain, exhaustively. */
    for (i = 0; i < 65536; i++) {
        int16_t ang = (int16_t)i;
        int16_t rs = 0x5a5a, rc = 0x5a5a, os = 0x5a5a, oc = 0x5a5a;
        r_sincos(ang, &rs, &rc);
        fs98_util_sincos(ang, &os, &oc);
        checked++;
        if ((rs != os || rc != oc) && bad++ < 8)
            printf("sincos(%d): retail sin=%d cos=%d, openfs98 sin=%d cos=%d\n",
                   ang, rs, rc, os, oc);
    }
    /* slot 0x44 -- metres to longitude.
     *
     * Two constraints on the inputs, both real properties of the retail
     * routine rather than conveniences:
     *
     *  - Latitudes stay inside the table's range.  The index is the top bits
     *    of the latitude, and past roughly +/-0x4e retail walks off the end of
     *    the table into neighbouring data.  Matching it there would mean
     *    locking in whatever happens to sit in .data.
     *
     *  - The numerator stays small enough that the quotient fits in 32 bits.
     *    The routine divides a 64-bit numerator by a divisor of about 2^25
     *    using a single `div`, which raises #DE when the quotient overflows --
     *    retail crashes outright on a large step, so there is nothing to
     *    match.  Keeping the high half under 2^24 stays well clear. */
    for (i = 0; i < 400000; i++) {
        unsigned char in[8], pos[8], ra[8], ob[8];
        int32_t lat_hi = (int32_t)(rng() % 0x9c) - 0x4e;   /* table range */
        uint16_t lat_lo = (uint16_t)rng();
        int32_t n_hi = (int32_t)(rng() & 0x01ffffffu) - 0x01000000;
        uint16_t n_lo = (uint16_t)rng();

        if (i < 64) { lat_hi = (int32_t)i - 16; lat_lo = 0; n_hi = (int32_t)i - 32; n_lo = 0; }
        if (i % 7 == 0) n_hi >>= 12;    /* exercise small steps too */

        memset(in, 0, 8); memset(pos, 0, 8);
        memset(ra, 0xcd, 8); memset(ob, 0xcd, 8);
        put48(in, n_lo, n_hi);
        put48(pos, lat_lo, lat_hi);

        r_m2lon(in, pos, ra);
        fs98_util_metres_to_longitude(in, pos, ob);
        checked++;
        if (memcmp(ra, ob, 8) && bad++ < 8)
            printf("m2lon(in=%04x:%08x lat=%04x:%08x): retail %04x:%08x, "
                   "openfs98 %04x:%08x\n",
                   n_lo, n_hi, lat_lo, lat_hi,
                   *(uint16_t *)(ra + 2), *(uint32_t *)(ra + 4),
                   *(uint16_t *)(ob + 2), *(uint32_t *)(ob + 4));
    }

    /* slots 0x50, 0x54, 0x58 -- unfinished in retail: ignore the first
     * argument, return the second.  0x5c leaves EAX undefined, so there is no
     * return value to compare; it is called only to confirm it returns and
     * does not disturb the stack. */
    {
        fn_pick2 r50 = (fn_pick2)slot(h, 0x50);
        fn_pick2 r54 = (fn_pick2)slot(h, 0x54);
        fn_pick2 r58 = (fn_pick2)slot(h, 0x58);
        fn_void  r5c = (fn_void) slot(h, 0x5c);
        for (i = 0; i < 20000; i++) {
            int32_t a = (int32_t)rng(), bb = (int32_t)rng();
            struct { fn_pick2 r; int32_t (FS98_STDCALL *o)(int32_t, int32_t);
                     const char *n; } t[3] = {
                { r50, fs98_util_stub_50, "0x50" },
                { r54, fs98_util_stub_54, "0x54" },
                { r58, fs98_util_stub_58, "0x58" },
            };
            unsigned k;
            for (k = 0; k < 3; k++) {
                int32_t x = t[k].r(a, bb), y = t[k].o(a, bb);
                checked++;
                if (x != y && bad++ < 8)
                    printf("slot %s(%d,%d): retail %d, openfs98 %d\n",
                           t[k].n, a, bb, x, y);
            }
        }
        r5c();
        fs98_util_stub_5c();
        checked++;

        /* slot 0x08 -- deinit, a bare ret.  Called only to confirm it returns
         * and leaves the stack where it found it. */
        {
            fn_void r08 = (fn_void)slot(h, 0x08);
            r08();
            fs98_util_deinit();
            checked++;
        }
    }

    /* slot 0x20 -- sqrt of a 16 bit value.  Exhaustive: the domain is 65536. */
    {
        fn_sqrt16 r20 = (fn_sqrt16)slot(h, 0x20);
        for (i = 0; i < 65536; i++) {
            int32_t x = r20((uint16_t)i), y = fs98_util_sqrt16((uint16_t)i);
            checked++;
            if (x != y && bad++ < 8)
                printf("sqrt16(%u): retail %d, openfs98 %d\n", i, x, y);
        }
    }

    /* slots 0x6c and 0x70 -- the generator.  Stateful, so both are put on the
     * same seed first; retail keeps its seed in its own .data at RVA 0x50f0. */
    {
        fn_rand r6c = (fn_rand)slot(h, 0x6c);
        fn_rand r70 = (fn_rand)slot(h, 0x70);
        uint32_t *retail_seed = (uint32_t *)((unsigned char *)h + 0x50f0);

        *retail_seed = 12345; fs98_util_rand_seed(12345);
        for (i = 0; i < 200000; i++) {
            uint32_t x = r6c(), y = fs98_util_rand();
            checked++;
            if (x != y && bad++ < 8)
                printf("rand #%u: retail %u, openfs98 %u\n", i, x, y);
        }
        *retail_seed = 1; fs98_util_rand_seed(1);
        for (i = 0; i < 100000; i++) {
            uint32_t x = r70(), y = fs98_util_rand32();
            checked++;
            if (x != y && bad++ < 8)
                printf("rand32 #%u: retail %08x, openfs98 %08x\n", i, x, y);
        }
    }

    /* slot 0x7c -- copy twenty-four bytes */
    {
        fn_copy24 r7c = (fn_copy24)slot(h, 0x7c);
        for (i = 0; i < 20000; i++) {
            uint32_t src[6], da[6], db[6];
            unsigned k;
            for (k = 0; k < 6; k++) src[k] = rng();
            memset(da, 0xa5, sizeof da); memset(db, 0xa5, sizeof db);
            r7c(da, src); fs98_util_copy24(db, src);
            checked++;
            if (memcmp(da, db, sizeof da) && bad++ < 8)
                printf("copy24 #%u differs\n", i);
        }
    }

    /* slot 0xb0 -- double to int, in the x87's rounding mode */
    {
        fn_d2i rb0 = (fn_d2i)slot(h, 0xb0);
        static const double fixed[] = { 0.0, 0.5, 1.5, 2.5, -0.5, -1.5, -2.5,
                                        3.49, 3.51, -3.49, -3.51, 1e9, -1e9 };
        unsigned k;
        for (k = 0; k < sizeof fixed / sizeof fixed[0]; k++) {
            int32_t x = rb0(fixed[k]), y = fs98_util_double_to_int(fixed[k]);
            checked++;
            if (x != y && bad++ < 8)
                printf("d2i(%f): retail %d, openfs98 %d\n", fixed[k], x, y);
        }
        for (i = 0; i < 50000; i++) {
            double d = (double)(int32_t)rng() / (double)(1 + (rng() % 1000));
            int32_t x = rb0(d), y = fs98_util_double_to_int(d);
            checked++;
            if (x != y && bad++ < 8)
                printf("d2i(%f): retail %d, openfs98 %d\n", d, x, y);
        }
    }

    /* slots 0x74, 0x78, 0x80 -- the 24-byte three-component 48-bit vector */
    {
        fn_v48op r74 = (fn_v48op)slot(h, 0x74);
        fn_v48op r78 = (fn_v48op)slot(h, 0x78);
        fn_v48eq r80 = (fn_v48eq)slot(h, 0x80);
        for (i = 0; i < 120000; i++) {
            unsigned char a[24], b[24], da[24], db[24];
            unsigned k;
            int32_t ea, eb;
            for (k = 0; k < 24; k++) { a[k] = (unsigned char)rng(); b[k] = (unsigned char)rng(); }
            /* every so often make them equal, so 0x80's true branch is reached */
            if (i % 11 == 0) memcpy(b, a, 24);

            memset(da, 0x5a, 24); memset(db, 0x5a, 24);
            r74(da, a, b); fs98_util_v48_add(db, a, b);
            checked++;
            if (memcmp(da, db, 24) && bad++ < 8) printf("v48_add #%u differs\n", i);

            memset(da, 0x5a, 24); memset(db, 0x5a, 24);
            r78(da, a, b); fs98_util_v48_sub(db, a, b);
            checked++;
            if (memcmp(da, db, 24) && bad++ < 8) printf("v48_sub #%u differs\n", i);

            ea = r80(a, b); eb = fs98_util_v48_equal(a, b);
            checked++;
            if (ea != eb && bad++ < 8)
                printf("v48_equal #%u: retail %d, openfs98 %d\n", i, ea, eb);
        }
    }

    /* slot 0x84 -- 48-bit arithmetic shift.  Counts past 31 are included on
     * purpose: x86 masks them to five bits and the result is not what the
     * arithmetic would suggest. */
    {
        fn_shr48 r84 = (fn_shr48)slot(h, 0x84);
        for (i = 0; i < 120000; i++) {
            unsigned char src[8], da[8], db[8];
            uint32_t n = (i < 70) ? i : (rng() % 70);
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            memset(da, 0x5a, 8); memset(db, 0x5a, 8);
            r84(da, src, n); fs98_util_shr48(db, src, n);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("shr48 by %u differs\n", n);
        }
    }

    /* slot 0xb4 -- double to the low half of a 64-bit conversion */
    {
        fn_d2i rb4 = (fn_d2i)slot(h, 0xb4);
        static const double fixed[] = { 0.0, 0.5, 1.5, 2.5, -0.5, -1.5, -2.5,
                                        2147483647.0, 2147483648.0, -2147483649.0,
                                        1e12, -1e12 };
        unsigned k;
        for (k = 0; k < sizeof fixed / sizeof fixed[0]; k++) {
            int32_t x = rb4(fixed[k]), y = fs98_util_double_to_int64_low(fixed[k]);
            checked++;
            if (x != y && bad++ < 8)
                printf("d2i64(%f): retail %d, openfs98 %d\n", fixed[k], x, y);
        }
        for (i = 0; i < 50000; i++) {
            double d = (double)(int32_t)rng() * (double)(1 + (rng() % 1000));
            int32_t x = rb4(d), y = fs98_util_double_to_int64_low(d);
            checked++;
            if (x != y && bad++ < 8)
                printf("d2i64(%f): retail %d, openfs98 %d\n", d, x, y);
        }
    }

    /* slot 0x34 -- 2D rotation in Q15 */
    {
        fn_rot2d r34 = (fn_rot2d)slot(h, 0x34);
        for (i = 0; i < 200000; i++) {
            int32_t x = (int32_t)rng(), y = (int32_t)rng();
            int16_t sn = (int16_t)rng(), cs = (int16_t)rng();
            int32_t ax = 0, ay = 0, bx = 0, by = 0;
            if (i < 16) { x = (int32_t)i - 8; y = 8 - (int32_t)i; sn = 0; cs = 32767; }
            r34(x, y, sn, cs, &ax, &ay);
            fs98_util_rotate2d(x, y, sn, cs, &bx, &by);
            checked++;
            if ((ax != bx || ay != by) && bad++ < 8)
                printf("rot2d(%d,%d,s=%d,c=%d): retail %d,%d openfs98 %d,%d\n",
                       x, y, sn, cs, ax, ay, bx, by);
        }
    }

    /* slot 0x30 -- 48-bit divided by an unsigned 16-bit value.  Divisor is
     * never zero: retail faults on that exactly as a `div` must. */
    {
        fn_div48 r30 = (fn_div48)slot(h, 0x30);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8], da[8], db[8];
            uint16_t d = (uint16_t)(1 + (rng() % 65535));
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 40) { memset(src, 0, 8); *(int32_t *)(src + 4) = (int32_t)i - 20; d = 1 + (uint16_t)(i % 7); }
            memset(da, 0x3c, 8); memset(db, 0x3c, 8);
            r30(src, d, da); fs98_util_div48_u16(src, d, db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("div48(%04x:%08x / %u): retail %04x:%08x openfs98 %04x:%08x\n",
                       *(uint16_t *)(src + 2), *(uint32_t *)(src + 4), d,
                       *(uint16_t *)(da + 2), *(uint32_t *)(da + 4),
                       *(uint16_t *)(db + 2), *(uint32_t *)(db + 4));
        }
    }

    /* slot 0x40 -- a 32-bit step through the longitude scaling.  Same domain
     * limits as slot 0x44, which it calls. */
    {
        fn_m32lon r40 = (fn_m32lon)slot(h, 0x40);
        for (i = 0; i < 150000; i++) {
            unsigned char pos[8], ra[8], ob[8];
            int32_t v = (int32_t)(rng() & 0x00ffffffu) - 0x00800000;
            int32_t lat_hi = (int32_t)(rng() % 0x9c) - 0x4e;
            memset(pos, 0, 8);
            put48(pos, (uint16_t)rng(), lat_hi);
            memset(ra, 0xcd, 8); memset(ob, 0xcd, 8);
            r40(&v, pos, ra);
            fs98_util_metres32_to_longitude(&v, pos, ob);
            checked++;
            if (memcmp(ra, ob, 8) && bad++ < 8)
                printf("m32lon(%d, lat_hi=%d): retail %04x:%08x openfs98 %04x:%08x\n",
                       v, lat_hi,
                       *(uint16_t *)(ra + 2), *(uint32_t *)(ra + 4),
                       *(uint16_t *)(ob + 2), *(uint32_t *)(ob + 4));
        }
    }

    /* slot 0x24 -- 48-bit times 16-bit, into a full 64-bit result */
    {
        fn_mul48 r24 = (fn_mul48)slot(h, 0x24);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8], da[8], db[8];
            int16_t m = (int16_t)rng();
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 40) { memset(src, 0, 8); *(int32_t *)(src + 4) = (int32_t)i - 20;
                          m = (int16_t)((int)i - 20); }
            memset(da, 0x77, 8); memset(db, 0x77, 8);
            r24(src, m, da); fs98_util_mul48_s16(src, m, db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("mul48(%04x:%08x * %d): retail %08x%08x openfs98 %08x%08x\n",
                       *(uint16_t *)(src + 2), *(uint32_t *)(src + 4), m,
                       *(uint32_t *)(da + 4), *(uint32_t *)da,
                       *(uint32_t *)(db + 4), *(uint32_t *)db);
        }
    }

    /* slot 0x60 -- unpack an eight-byte record into the 24-byte vector */
    {
        fn_unpack r60 = (fn_unpack)slot(h, 0x60);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8], da[24], db[24];
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 32) { memset(src, 0, 8); src[i % 8] = (unsigned char)(1u << (i / 8)); }
            memset(da, 0x11, 24); memset(db, 0x11, 24);
            r60(src, da); fs98_util_unpack_position(src, db);
            checked++;
            if (memcmp(da, db, 24) && bad++ < 8)
                printf("unpack #%u differs\n", i);
        }
    }

    /* slot 0x3c -- atan2.  Includes the axes, the diagonals and both zeros,
     * since the equal-magnitude short-circuit is what keeps a zero vector away
     * from the divide. */
    {
        fn_atan2 r3c = (fn_atan2)slot(h, 0x3c);
        static const int32_t edge[] = { 0, 1, -1, 2, -2, 1000, -1000,
                                        0x7fffffff, -0x7fffffff, 0x40000000 };
        unsigned a, b;
        for (a = 0; a < sizeof edge / sizeof edge[0]; a++)
            for (b = 0; b < sizeof edge / sizeof edge[0]; b++) {
                int32_t x = r3c(edge[a], edge[b]);
                int32_t y = fs98_util_atan2(edge[a], edge[b]);
                checked++;
                if (x != y && bad++ < 8)
                    printf("atan2(%d,%d): retail %d, openfs98 %d\n",
                           edge[a], edge[b], x, y);
            }
        for (i = 0; i < 300000; i++) {
            int32_t px = (int32_t)rng() >> (rng() % 20);
            int32_t py = (int32_t)rng() >> (rng() % 20);
            int32_t x, y;
            if (i % 5 == 0) py = px;            /* the equal-magnitude path */
            if (i % 11 == 0) py = -px;
            x = r3c(px, py); y = fs98_util_atan2(px, py);
            checked++;
            if (x != y && bad++ < 8)
                printf("atan2(%d,%d): retail %d, openfs98 %d\n", px, py, x, y);
        }
    }

    /* slot 0x38 -- angle from a sine.  The sine argument is swept exhaustively
     * over all 65536 values against both signs of cosine, so every one of the
     * three piecewise ranges and both range boundaries are covered. */
    {
        fn_asin r38 = (fn_asin)slot(h, 0x38);
        for (i = 0; i < 65536; i++) {
            int16_t sn = (int16_t)i;
            int16_t cs;
            for (cs = -1; cs <= 1; cs += 2) {
                int32_t x = r38(sn, cs), y = fs98_util_angle_from_sin(sn, cs);
                checked++;
                if (x != y && bad++ < 8)
                    printf("asin(%d, cos %d): retail %d, openfs98 %d\n",
                           sn, cs, x, y);
            }
        }
    }

    /* slots 0xc0 and 0xbc -- 32.16 fixed point to double and back */
    {
        fn_f48d rc0 = (fn_f48d)slot(h, 0xc0);
        fn_df48 rbc = (fn_df48)slot(h, 0xbc);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8], da[8], db[8];
            double x = 0, y = 0, v;
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 40) { memset(src, 0, 8); *(int32_t *)(src + 4) = (int32_t)i - 20; }
            rc0(&x, src); fs98_util_fixed48_to_double(&y, src);
            checked++;
            if (memcmp(&x, &y, sizeof x) && bad++ < 8)
                printf("fixed48->double(%04x:%08x): retail %.17g openfs98 %.17g\n",
                       *(uint16_t *)(src + 2), *(uint32_t *)(src + 4), x, y);

            /* and back, over a spread that includes negatives, fractions and
             * values past the 32-bit boundary where _ftol gives up */
            v = (double)(int32_t)rng() / (double)(1 + (rng() % 100000));
            if (i % 9 == 0) v = -v;
            if (i % 23 == 0) v = (double)(int32_t)rng() * 1000.0;
            memset(da, 0x2b, 8); memset(db, 0x2b, 8);
            rbc(&v, da); fs98_util_double_to_fixed48(&v, db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("double->fixed48(%.17g): retail %04x:%08x openfs98 %04x:%08x\n",
                       v, *(uint16_t *)(da + 2), *(uint32_t *)(da + 4),
                       *(uint16_t *)(db + 2), *(uint32_t *)(db + 4));
        }
    }

    /* slot 0xb8 -- the floating-point generator.  Stateful, and its state is
     * the last eight bytes of retail's .data, so both are seeded alike first.
     * Zero is included deliberately: it is the initial state and a fixed
     * point, so an unseeded generator must return zero forever. */
    {
        fn_frand rb8 = (fn_frand)slot(h, 0xb8);
        double *retail_state = (double *)((unsigned char *)h + 0x57f8);
        static const double seeds[] = { 0.0, 0.5, 0.125, 0.9999, 1.0 / 3.0 };
        unsigned k;
        for (k = 0; k < sizeof seeds / sizeof seeds[0]; k++) {
            *retail_state = seeds[k];
            fs98_util_frand_seed(seeds[k]);
            for (i = 0; i < 40000; i++) {
                double x = rb8(), y = fs98_util_frand();
                checked++;
                if (memcmp(&x, &y, sizeof x) && bad++ < 8)
                    printf("frand seed %.17g step %u: retail %.17g openfs98 %.17g\n",
                           seeds[k], i, x, y);
            }
        }
    }

    /* slot 0x9c -- the longer of two strings, measured through the host.
     * Retail's ImportTable entry for module 1 is pointed at the stub host, so
     * both sides measure with the same strlen and any difference is theirs. */
    {
        fn_longer r9c = (fn_longer)slot(h, 0x9c);
        struct import { uint32_t id; void *link; } *im;
        static const char *s[] = { "", "a", "ab", "abc", "abcd", "hello world",
                                   "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" };
        unsigned x, y;

        memset(host_blob, 0, sizeof host_blob);
        *(uint32_t *)host_blob = 1;
        HOST_SLOT(0x2b8) = (void *)stub_strlen;
        HOST_SLOT(0x038) = (void *)stub_ticks;
        fs98_util_set_host(host_blob);
        for (im = (struct import *)GetProcAddress(h, "ImportTable"); im && im->id; im++)
            if (im->id == 1) im->link = host_blob;

        for (x = 0; x < sizeof s / sizeof s[0]; x++)
            for (y = 0; y < sizeof s / sizeof s[0]; y++) {
                const char *p = r9c(s[x], s[y]);
                const char *q = fs98_util_longer_string(s[x], s[y]);
                checked++;
                if (p != q && bad++ < 8)
                    printf("longer(%s,%s): retail %s, openfs98 %s\n",
                           s[x], s[y], p ? p : "(null)", q ? q : "(null)");
            }
        /* and the null-pointer case the host service goes out of its way to
         * tolerate */
        {
            const char *p = r9c(NULL, "abc");
            const char *q = fs98_util_longer_string(NULL, "abc");
            checked++;
            if (p != q && bad++ < 8) printf("longer(NULL,abc) differs\n");
        }
    }

    /* slot 0x04 -- init.  Not a function of its arguments, but the state it
     * leaves behind is, and that state is readable on both sides: retail keeps
     * its integer seed at .data+0x0f0 and its floating-point seed at
     * .data+0x7f8.  Driving both through the same controlled clock -- which
     * includes the zeros the retry loop exists to skip -- makes init testable
     * after all. */
    {
        fn_void r04 = (fn_void)slot(h, 0x04);
        uint32_t *retail_seed  = (uint32_t *)((unsigned char *)h + 0x50f0);
        double   *retail_fseed = (double   *)((unsigned char *)h + 0x57f8);
        unsigned start;

        for (start = 0; start < sizeof tick_seq / sizeof tick_seq[0]; start++) {
            uint32_t rs, os;
            double rf, of;

            tick_pos = start;
            *retail_seed = 0xdeadbeef; *retail_fseed = -1.0;
            r04();
            rs = *retail_seed; rf = *retail_fseed;

            tick_pos = start;
            fs98_util_rand_seed(0xdeadbeef); fs98_util_frand_seed(-1.0);
            fs98_util_init();
            os = fs98_util_rand_get(); of = fs98_util_frand_get();

            checked++;
            if (os != rs && bad++ < 8)
                printf("init from tick %u: integer seed retail %u, openfs98 %u\n",
                       start, rs, os);
            checked++;
            if (memcmp(&of, &rf, sizeof of) && bad++ < 8)
                printf("init from tick %u: float seed retail %.17g, openfs98 %.17g\n",
                       start, rf, of);
        }
    }

    /* slot 0xc4 -- the signed double to 32.16 conversion.  Negatives matter
     * most here, since the wrap into the positive range is the whole point. */
    {
        fn_df48 rc4 = (fn_df48)slot(h, 0xc4);
        static const double fixed[] = { 0.0, 1.5, -1.5, 0.25, -0.25, -0.0,
                                        1e6, -1e6, 32767.75, -32768.25 };
        unsigned k;
        for (k = 0; k < sizeof fixed / sizeof fixed[0]; k++) {
            unsigned char da[8], db[8];
            memset(da, 0x6e, 8); memset(db, 0x6e, 8);
            rc4(&fixed[k], da); fs98_util_double_to_fixed48_signed(&fixed[k], db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("to_fixed48s(%.17g): retail %04x:%08x openfs98 %04x:%08x\n",
                       fixed[k], *(uint16_t *)(da + 2), *(uint32_t *)(da + 4),
                       *(uint16_t *)(db + 2), *(uint32_t *)(db + 4));
        }
        for (i = 0; i < 200000; i++) {
            unsigned char da[8], db[8];
            double v = (double)(int32_t)rng() / (double)(1 + (rng() % 100000));
            if (i % 3 == 0) v = -v;
            memset(da, 0x6e, 8); memset(db, 0x6e, 8);
            rc4(&v, da); fs98_util_double_to_fixed48_signed(&v, db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("to_fixed48s(%.17g): retail %04x:%08x openfs98 %04x:%08x\n",
                       v, *(uint16_t *)(da + 2), *(uint32_t *)(da + 4),
                       *(uint16_t *)(db + 2), *(uint32_t *)(db + 4));
        }
    }

    /* slot 0xc8 -- signed 32.16 to double.  Checked both on its own and as a
     * round trip with +0xc4, which is the pair that should be inverse. */
    {
        fn_f48d rc8 = (fn_f48d)slot(h, 0xc8);
        fn_df48 rc4 = (fn_df48)slot(h, 0xc4);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8];
            double x = 0, y = 0;
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 40) { memset(src, 0, 8); *(int32_t *)(src + 4) = (int32_t)i - 20; }
            rc8(&x, src); fs98_util_fixed48_to_double_signed(&y, src);
            checked++;
            if (memcmp(&x, &y, sizeof x) && bad++ < 8)
                printf("fixed48s->double(%04x:%08x): retail %.17g openfs98 %.17g\n",
                       *(uint16_t *)(src + 2), *(uint32_t *)(src + 4), x, y);

            /* round trip: a double through +0xc4 and back through +0xc8 */
            if (i % 4 == 0) {
                unsigned char mid[8];
                double v = (double)(int32_t)rng() / 4096.0, back = 0;
                rc4(&v, mid); rc8(&back, mid);
                checked++;
                if (back != v && bad++ < 8)
                    printf("round trip %.17g -> %.17g\n", v, back);
            }
        }
    }

    /* and that null outputs are tolerated the way retail tolerates them */
    r_sincos(1234, NULL, NULL);
    fs98_util_sincos(1234, NULL, NULL);

    printf("\n%u comparisons, %u mismatches\n", checked, bad);
    if (!bad) printf("IDENTICAL\n");
    return bad != 0;
}
