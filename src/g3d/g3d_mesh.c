/* G3D's mesh compiler (0x38), .text+0x28d0..+0x361a, in C: the portable
 * counterpart of g3d_mesh.S, which carries retail's instructions over as
 * they are.  The C was MSVC's; this follows the instructions, expression
 * by expression, keeping each value where retail keeps it -- on the x87
 * (fx, rounded to the precision control) or stored to a float (TOF).
 *
 * On x86 fx is long double and the arithmetic is the x87's own, under the
 * caller's control word, so this must equal g3d_mesh.S bit for bit.
 * Elsewhere, or with FS98_PORTABLE_FP, fx is a software 80-bit float:
 * add, subtract, multiply, divide and square root correctly rounded (to
 * nearest, ties to even) to g3d_mesh_precision bits -- 24, 53 or 64, the
 * x87's precision control -- with the extended exponent range, as the
 * x87 rounds.  The one transcendental, _CIacos's fpatan, is worked out
 * in the same arithmetic at 64 bits whatever the precision control (as
 * fpatan is): a few units in the 64th bit from the x87's, which a float
 * made from it almost never sees.  (The C library's atan2, good to 53
 * bits, parted from the x87 in one polygon of 10,000 under 24-bit
 * precision.)  tools/difftest/g3dmeshdiff.c checks both against the
 * instructions.
 *
 * Retail's frames are kept where their layout shows: 0x206e28d0 packs its
 * triangles into a list of 150 dwords that runs on into the triangles
 * themselves, which a mesh of many separate triangles reaches. */
#include <stdint.h>

#ifndef G3D_MESH_NAME
#  define G3D_MESH_NAME g3d_mesh_compile
#endif
#ifndef FS98_STDCALL
#  define FS98_STDCALL __attribute__((stdcall))
#endif

#if (defined(__i386__) || defined(__x86_64__)) && !defined(FS98_PORTABLE_FP)
#  define MESH_X87 1
#else
#  define MESH_X87 0
#endif

typedef uint8_t  __attribute__((may_alias)) mesh_u8;
typedef uint16_t __attribute__((may_alias)) mesh_u16;
typedef uint32_t __attribute__((may_alias)) mesh_u32;
typedef int32_t  __attribute__((may_alias)) mesh_i32;

#define B8(a)   (*(mesh_u8  *)(uintptr_t)(uint32_t)(a))
#define W16(a)  (*(mesh_u16 *)(uintptr_t)(uint32_t)(a))
#define D32(a)  (*(mesh_u32 *)(uintptr_t)(uint32_t)(a))
#define I32(a)  (*(mesh_i32 *)(uintptr_t)(uint32_t)(a))

static float float_bits(uint32_t bits)
{
    union { uint32_t u; float f; } v;

    v.u = bits;
    return v.f;
}

/* .rdata 0x206f8000..: the points' scale 2^-16, pi as retail rounded it
 * (0x40490fd0, eleven units short of the float nearest pi), one, a third. */
#define K_SCALE  float_bits(0x37800000u)
#define K_PI     float_bits(0x40490fd0u)
#define K_ONE    float_bits(0x3f800000u)
#define K_THIRD  float_bits(0x3eaaaaabu)

/* ---- the x87's arithmetic ----------------------------------------------------- */

#if MESH_X87

typedef long double fx;

#define F(v)        ((fx)(float)(v))
#define I(v)        ((fx)(int32_t)(v))
#define ADD(a, b)   ((fx)(a) + (fx)(b))
#define SUB(a, b)   ((fx)(a) - (fx)(b))
#define MUL(a, b)   ((fx)(a) * (fx)(b))
#define DIV(a, b)   ((fx)(a) / (fx)(b))
#define CMP(a, b)   (((fx)(a) > (fx)(b)) - ((fx)(a) < (fx)(b)))

static fx SQRT(fx x)
{
    fx r;

    __asm__ ("fsqrt" : "=t" (r) : "0" (x));
    return r;
}

/* fpatan: atan2(y, x), y in st1 and x in st0. */
static fx PATAN(fx y, fx x)
{
    fx r;

    __asm__ ("fpatan" : "=t" (r) : "0" (x), "u" (y) : "st(1)");
    return r;
}

/* fst DWORD PTR: rounded to a float whatever the precision control. */
static float TOF(fx v)
{
    volatile float f = (float)v;

    return f;
}

#else

/* m * 2^(e - 63), m's top bit set; m 0 is a zero of sign s. */
typedef struct {
    uint64_t m;
    int32_t e;
    int s;
} fx;

typedef struct {
    uint64_t hi, lo;
} u128;

int g3d_mesh_precision = 53;        /* control word 0x27f, Windows' own */

static u128 mul64(uint64_t a, uint64_t b)
{
    uint64_t a0 = (uint32_t)a, a1 = a >> 32, b0 = (uint32_t)b, b1 = b >> 32;
    uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
    u128 r;

    r.lo = mid << 32 | (uint32_t)p00;
    r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    return r;
}

/* Right by n (0..127), the bits shifted out or'd into *sticky. */
static u128 shr128(u128 v, int n, int *sticky)
{
    u128 r;

    if (n == 0)
        return v;
    if (n < 64) {
        *sticky |= (v.lo << (64 - n)) != 0;
        r.lo = v.lo >> n | v.hi << (64 - n);
        r.hi = v.hi >> n;
    } else {
        *sticky |= v.lo != 0 || (n > 64 && (v.hi << (128 - n)) != 0);
        r.lo = n == 64 ? v.hi : v.hi >> (n - 64);
        r.hi = 0;
    }
    return r;
}

static u128 shl128(u128 v, int n)
{
    u128 r;

    if (n == 0)
        return v;
    if (n < 64) {
        r.hi = v.hi << n | v.lo >> (64 - n);
        r.lo = v.lo << n;
    } else {
        r.hi = n == 64 ? v.lo : v.lo << (n - 64);
        r.lo = 0;
    }
    return r;
}

static fx fx_zero(int s)
{
    fx r;

    r.m = 0;
    r.e = 0;
    r.s = s;
    return r;
}

/* v (its top bit set; below it all that is known exactly, with sticky
 * for anything nonzero further down) rounded to p bits, to nearest. */
static fx fx_round(int s, int32_t e, u128 v, int sticky, int p)
{
    uint64_t keep, rest, half;
    int up;
    fx r;

    if (p >= 64) {
        keep = v.hi;
        rest = v.lo;
        half = 1ull << 63;
        up = rest > half || (rest == half && (sticky || (keep & 1)));
        if (up && ++keep == 0) {
            keep = 1ull << 63;
            e++;
        }
        r.m = keep;
    } else {
        keep = v.hi >> (64 - p);
        rest = v.hi & ((1ull << (64 - p)) - 1);
        half = 1ull << (63 - p);
        up = rest > half || (rest == half && (v.lo != 0 || sticky || (keep & 1)));
        if (up && ++keep == 1ull << p) {
            keep = 1ull << (p - 1);
            e++;
        }
        r.m = keep << (64 - p);
    }
    r.e = e;
    r.s = s;
    return r;
}

static int fx_magnitude_less(fx a, fx b)
{
    return a.e < b.e || (a.e == b.e && a.m < b.m);
}

static fx fx_add(fx a, fx b)
{
    u128 x, y, r;
    int sticky = 0, d, s;
    int32_t e;

    if (a.m == 0 && b.m == 0)
        return fx_zero(a.s & b.s);
    if (a.m == 0)
        return b;
    if (b.m == 0)
        return a;
    if (fx_magnitude_less(a, b)) {
        fx t = a;

        a = b;
        b = t;
    }
    d = a.e - b.e;
    x.hi = a.m;
    x.lo = 0;
    y.hi = b.m;
    y.lo = 0;
    if (d >= 128) {
        sticky = 1;
        y.hi = y.lo = 0;
    } else {
        y = shr128(y, d, &sticky);
    }
    s = a.s;
    e = a.e;
    if (a.s == b.s) {
        uint64_t lo = x.lo + y.lo, carry_lo = lo < x.lo, hi = x.hi + y.hi + carry_lo;
        int carry = hi < x.hi || (hi == x.hi && carry_lo);

        r.hi = hi;
        r.lo = lo;
        if (carry) {
            r = shr128(r, 1, &sticky);
            r.hi |= 1ull << 63;
            e++;
        }
    } else {
        uint64_t borrow = x.lo < y.lo;

        r.lo = x.lo - y.lo;
        r.hi = x.hi - y.hi - borrow;
        if (sticky) {
            /* what was shifted out lies between 0 and 1 of the lowest
             * place: one less, and the rest a nonzero remainder */
            if (r.lo-- == 0)
                r.hi--;
        }
        if (r.hi == 0 && r.lo == 0 && !sticky)
            return fx_zero(0);
        while (!(r.hi >> 63)) {
            r = shl128(r, 1);
            e--;
        }
    }
    return fx_round(s, e, r, sticky, g3d_mesh_precision);
}

static fx fx_neg(fx a)
{
    a.s ^= 1;
    return a;
}

static fx fx_mul(fx a, fx b)
{
    u128 p;
    int32_t e;

    if (a.m == 0 || b.m == 0)
        return fx_zero(a.s ^ b.s);
    p = mul64(a.m, b.m);
    e = a.e + b.e;
    if (p.hi >> 63)
        e++;
    else
        p = shl128(p, 1);
    return fx_round(a.s ^ b.s, e, p, 0, g3d_mesh_precision);
}

/* a / b, b nonzero: 67 quotient bits by restoring division, the
 * remainder as sticky. */
static fx fx_div(fx a, fx b)
{
    u128 q = { 0, 0 };
    uint64_t r = a.m;
    int rc = 0, i, sticky;
    int32_t e;

    if (a.m == 0)
        return fx_zero(a.s ^ b.s);
    for (i = 0; i < 67; i++) {
        int bit = rc || r >= b.m;

        if (bit)
            r -= b.m;
        q = shl128(q, 1);
        q.lo |= (uint64_t)bit;
        rc = (int)(r >> 63);
        r <<= 1;
    }
    sticky = r != 0 || rc;
    e = a.e - b.e;
    if ((q.hi >> 2) & 1) {          /* bit 66: a's mantissa at least b's */
        q = shl128(q, 61);
    } else {
        q = shl128(q, 62);
        e--;
    }
    return fx_round(a.s ^ b.s, e, q, sticky, g3d_mesh_precision);
}

static int u128_less(u128 a, u128 b)
{
    return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo);
}

static u128 u128_sub(u128 a, u128 b)
{
    u128 r;

    r.lo = a.lo - b.lo;
    r.hi = a.hi - b.hi - (a.lo < b.lo);
    return r;
}

/* sqrt(a), a not negative: the 64-bit integer root of the mantissa placed
 * for an even exponent, then the remainder decides the bit below it (a
 * root of an integer is never a half) and the sticky. */
static fx fx_sqrt(fx a)
{
    u128 v, rem = { 0, 0 }, trial;
    uint64_t root = 0;
    int i, sticky;
    int32_t e;

    if (a.m == 0)
        return a;
    if (a.e & 1) {
        v.hi = a.m;
        v.lo = 0;
    } else {
        v.hi = a.m >> 1;
        v.lo = a.m << 63;
    }
    e = a.e >= 0 ? a.e / 2 : -((1 - a.e) / 2);
    for (i = 0; i < 64; i++) {
        rem = shl128(rem, 2);
        rem.lo |= v.hi >> 62;
        v = shl128(v, 2);
        trial.hi = root >> 62;
        trial.lo = root << 2 | 1;
        if (!u128_less(rem, trial)) {
            rem = u128_sub(rem, trial);
            root = root << 1 | 1;
        } else {
            root <<= 1;
        }
    }
    sticky = rem.hi != 0 || rem.lo != 0;
    v.hi = root;
    v.lo = rem.hi != 0 || rem.lo > root ? 1ull << 63 : 0;
    return fx_round(0, e, v, sticky, g3d_mesh_precision);
}

static fx fx_from_float(float f)
{
    union { float f; uint32_t u; } v;
    uint32_t exp, frac;
    fx r;

    v.f = f;
    exp = v.u >> 23 & 0xff;
    frac = v.u & 0x7fffff;
    r.s = (int)(v.u >> 31);
    if (exp == 0) {
        int top = 22;

        if (frac == 0)
            return fx_zero(r.s);
        while (!(frac >> top))
            top--;
        r.m = (uint64_t)frac << (63 - top);
        r.e = top - 149;
    } else {
        r.m = (uint64_t)(frac | 0x800000) << 40;
        r.e = (int32_t)exp - 127;
    }
    return r;
}

static fx fx_from_int(int32_t i)
{
    uint64_t m;
    int32_t e = 63;
    fx r;

    if (i == 0)
        return fx_zero(0);
    r.s = i < 0;
    m = i < 0 ? (uint64_t)-(int64_t)i : (uint64_t)i;
    while (!(m >> 63)) {
        m <<= 1;
        e--;
    }
    r.m = m;
    r.e = e;
    return r;
}

/* Stored as a float: 24 bits to nearest, fewer below the normal range. */
static float fx_to_float(fx a)
{
    union { float f; uint32_t u; } v;
    u128 m;
    int p;
    fx r;

    if (a.m == 0) {
        v.u = (uint32_t)a.s << 31;
        return v.f;
    }
    m.hi = a.m;
    m.lo = 0;
    p = a.e >= -126 ? 24 : 24 - (-126 - a.e);
    if (p <= 0) {
        /* below half the smallest denormal, or at most just above it */
        int up = p == 0 && a.m > 1ull << 63;

        v.u = (uint32_t)a.s << 31 | (uint32_t)up;
        return v.f;
    }
    r = fx_round(a.s, a.e, m, 0, p);
    if (r.e > 127)
        v.u = (uint32_t)a.s << 31 | 0x7f800000u;
    else if (r.e >= -126)
        v.u = (uint32_t)a.s << 31 | (uint32_t)(r.e + 127) << 23 | (uint32_t)(r.m >> 40 & 0x7fffff);
    else
        v.u = (uint32_t)a.s << 31 | (uint32_t)(r.m >> (40 + (-126 - r.e)));
    return v.f;
}

static int fx_cmp(fx a, fx b)
{
    fx d = fx_add(a, fx_neg(b));

    return d.m == 0 ? 0 : d.s ? -1 : 1;
}

#define F(v)        fx_from_float((float)(v))
#define I(v)        fx_from_int((int32_t)(v))
#define ADD(a, b)   fx_add((a), (b))
#define SUB(a, b)   fx_add((a), fx_neg(b))
#define MUL(a, b)   fx_mul((a), (b))
#define DIV(a, b)   fx_div((a), (b))
#define SQRT(a)     fx_sqrt(a)
#define CMP(a, b)   fx_cmp((a), (b))
#define TOF(a)      fx_to_float(a)

/* fpatan for y > 0, at 64 bits: by the octant to an arctangent of at most
 * 1, halved three times (t / (1 + sqrt(1 + t^2))) to below tan(pi/32),
 * and the odd series to t^25 there.  pi as fldpi has it. */
static fx PATAN(fx y, fx x)
{
    static const fx pi = { 0xc90fdaa22168c235ull, 1, 0 }, half_pi = { 0xc90fdaa22168c235ull, 0, 0 };
    int keep = g3d_mesh_precision, k, steep;
    fx ax = x, t, t2, p, a, one = fx_from_int(1);

    g3d_mesh_precision = 64;
    ax.s = 0;
    steep = fx_cmp(ax, y) <= 0;
    t = steep ? fx_div(ax, y) : fx_div(y, ax);
    for (k = 0; k < 3; k++)
        t = fx_div(t, fx_add(one, fx_sqrt(fx_add(one, fx_mul(t, t)))));
    t2 = fx_mul(t, t);
    p = fx_div(one, fx_from_int(25));
    for (k = 11; k >= 0; k--) {
        fx c = fx_div(one, fx_from_int(2 * k + 1));

        if (k & 1)
            c = fx_neg(c);
        p = fx_add(fx_mul(p, t2), c);
    }
    a = fx_mul(t, p);
    a.e += a.m != 0 ? 3 : 0;                    /* the halvings back */
    if (steep)
        a = x.s ? fx_add(half_pi, a) : fx_add(half_pi, fx_neg(a));
    else if (x.s)
        a = fx_add(pi, fx_neg(a));
    g3d_mesh_precision = keep;
    return a;
}

#endif

/* ---- the compiler ------------------------------------------------------------- */

/* A polygon's vertex as the frames hold it: the table point scaled to
 * floats, and its index. */
struct vtx {
    float x, y, z;
    int32_t idx;
};

/* MSVCRT's _CIacos, the CD's (0x7802e6a3), for |x| < 1: atan2 of
 * sqrt((1 + x) * (1 - x)) and x, rounding to nearest with every exception
 * masked meanwhile and the precision kept. */
static fx acos_cd(fx x)
{
#if MESH_X87
    uint16_t cw, nw;
    volatile fx vx = x, vr;
    fx s;

    __asm__ __volatile__ ("fnstcw %0" : "=m" (cw) : : "memory");
    nw = cw;
    if (cw != 0x27f)
        nw = (uint16_t)((cw & 0x300) | 0x7f);
    __asm__ __volatile__ ("fldcw %0" : : "m" (nw) : "memory");
    x = vx;
    s = SQRT(MUL(ADD(F(K_ONE), x), SUB(F(K_ONE), x)));
    vr = PATAN(s, x);
    __asm__ __volatile__ ("fldcw %0" : : "m" (cw) : "memory");
    return vr;
#else
    return PATAN(SQRT(MUL(ADD(F(K_ONE), x), SUB(F(K_ONE), x))), x);
#endif
}

/* 0x206e2f80 -- the angle between a and b: acos of their dot product over
 * |b| times |a| stored as a float; 0 for a zero length, pi (retail's) at
 * or below -1, 0 at or above 1. */
static fx angle(const struct vtx *a, const struct vtx *b)
{
    fx lb, la, prod, dot;
    float c;

    lb = SQRT(ADD(ADD(MUL(F(b->y), F(b->y)), MUL(F(b->z), F(b->z))), MUL(F(b->x), F(b->x))));
    la = SQRT(ADD(ADD(MUL(F(a->y), F(a->y)), MUL(F(a->z), F(a->z))), MUL(F(a->x), F(a->x))));
    prod = MUL(lb, F(TOF(la)));
    if (CMP(prod, F(0)) == 0)
        return F(0);
    dot = ADD(ADD(MUL(F(b->y), F(a->y)), MUL(F(b->z), F(a->z))), MUL(F(b->x), F(a->x)));
    c = TOF(DIV(dot, prod));
    if (CMP(F(c), F(-1)) <= 0)
        return F(K_PI);
    if (!(CMP(F(c), F(K_ONE)) < 0))
        return F(0);
    return acos_cd(F(c));
}

/* 0x206e3090 -- whether the polygon (n vertices) winds round p: each edge's
 * angle seen from p times the normal of the plane through p and the edge
 * (normalised when it has a length), summed, longer than retail's pi. */
static int winds_round(const struct vtx *poly, int32_t n, const struct vtx *p)
{
    float sx = 0, sy = 0, sz = 0, nxf, nyf, nzf;
    int32_t k;

    for (k = 1; k <= n; k++) {
        const struct vtx *q = poly + (k - 1), *r = poly + k % n;
        struct vtx a, b;
        fx bx, by, bz, nx, ny, nz, l, t;

        a.x = TOF(SUB(F(p->x), F(q->x)));
        a.y = TOF(SUB(F(p->y), F(q->y)));
        a.z = TOF(SUB(F(p->z), F(q->z)));
        bx = SUB(F(p->x), F(r->x));
        by = SUB(F(p->y), F(r->y));
        bz = SUB(F(p->z), F(r->z));
        b.x = TOF(bx);
        b.y = TOF(by);
        b.z = TOF(bz);
        nx = SUB(MUL(bz, F(a.y)), MUL(by, F(a.z)));
        ny = SUB(MUL(bx, F(a.z)), MUL(F(b.z), F(a.x)));
        nz = SUB(MUL(F(b.y), F(a.x)), MUL(F(b.x), F(a.y)));
        nxf = TOF(nx);
        nyf = TOF(ny);
        nzf = TOF(nz);
        l = SQRT(ADD(ADD(MUL(nx, F(nxf)), MUL(ny, F(nyf))), MUL(nz, F(nzf))));
        if (CMP(l, F(0)) > 0) {
            fx inv = DIV(F(K_ONE), l);

            nxf = TOF(MUL(inv, F(nxf)));
            nyf = TOF(MUL(inv, F(nyf)));
            nzf = TOF(MUL(inv, F(nzf)));
        }
        t = angle(&a, &b);
        sx = TOF(ADD(MUL(t, F(nxf)), F(sx)));
        sy = TOF(ADD(MUL(t, F(nyf)), F(sy)));
        sz = TOF(ADD(MUL(t, F(nzf)), F(sz)));
    }
    return CMP(SQRT(ADD(ADD(MUL(F(sx), F(sx)), MUL(F(sy), F(sy))), MUL(F(sz), F(sz)))),
               F(K_PI)) > 0;
}

/* 0x206e3260 -- whether (i, j, k) is an ear: three vertices always are;
 * else the polygon winds round the triangle's centroid and the triangle
 * round none of the other vertices. */
static int is_ear(const struct vtx *v, int32_t n, int32_t i, int32_t j, int32_t k)
{
    struct vtx t[3], g;
    int32_t m;

    if (n < 3)
        return 0;
    if (n == 3)
        return 1;
    t[0] = v[i];
    t[1] = v[j];
    t[2] = v[k];
    g.x = TOF(MUL(ADD(ADD(F(t[0].x), F(t[1].x)), F(t[2].x)), F(K_THIRD)));
    g.y = TOF(MUL(ADD(ADD(F(t[0].y), F(t[1].y)), F(t[2].y)), F(K_THIRD)));
    g.z = TOF(MUL(ADD(ADD(F(t[0].z), F(t[1].z)), F(t[2].z)), F(K_THIRD)));
    g.idx = 0;
    if (!winds_round(v, n, &g))
        return 0;
    for (m = 0; m < n; m++) {
        if (m == i || m == j || m == k)
            continue;
        if (winds_round(t, 3, v + m))
            return 0;
    }
    return 1;
}

/* 0x206e33f0 -- the first ear from the start, its vertices out and the
 * middle one taken from the polygon. */
static int take_ear(struct vtx *v, int32_t *n, struct vtx *o1, struct vtx *o2, struct vtx *o3)
{
    int32_t i, j, k, m;

    for (i = 0; i < *n; i++) {
        j = (i + 1) % *n;
        k = (i + 2) % *n;
        if (is_ear(v, *n, i, j, k)) {
            *o1 = v[i];
            *o2 = v[j];
            *o3 = v[k];
            (*n)--;
            for (m = j; m < *n; m++)
                v[m] = v[m + 1];
            return 1;
        }
    }
    return 0;
}

/* 0x206e2cb0 / 0x206e2db0's start: the polygon's vertices from the table
 * points its indices name, every stride words. */
static void load_polygon(struct vtx *v, uint32_t idx, int32_t stride, int32_t n, uint32_t points)
{
    int32_t k;

    for (k = 0; k < n; k++) {
        uint32_t c = W16(idx), p = points + c * 16;

        idx += (uint32_t)stride * 2;
        v[k].x = TOF(MUL(I(I32(p)), F(K_SCALE)));
        v[k].y = TOF(MUL(I(I32(p + 4)), F(K_SCALE)));
        v[k].z = TOF(MUL(I(I32(p + 8)), F(K_SCALE)));
        v[k].idx = (int32_t)c;
    }
}

/* 0x206e2cb0 -- the polygon cut into triangles by ears, their indices to
 * tris; the count. */
static int32_t ear_triangles(uint32_t idx, int32_t stride, int32_t n, int32_t *tris, uint32_t points)
{
    struct vtx v[150], a, b, c;
    int32_t count = 0;

    load_polygon(v, idx, stride, n, points);
    while (take_ear(v, &n, &a, &b, &c)) {
        tris[count++] = a.idx;
        tris[count++] = b.idx;
        tris[count++] = c.idx;
    }
    return count / 3;
}

/* 0x206e34f0 -- the longest fan of ears from one vertex: its start and
 * count; none, and 0 with a count of 1. */
static int best_fan(const struct vtx *v, int32_t n, int32_t *start, int32_t *count)
{
    int32_t s, b, i;

    if (n < 3)
        return 0;
    if (n == 3) {
        *start = 0;
        *count = 1;
        return 1;
    }
    *count = 0;
    for (s = 0; s < n; s++) {
        b = 0;
        if (n - 2 > 0) {
            for (i = s + 1; is_ear(v, n, s, i % n, (i + 1) % n); i++)
                if (!(++b < n - 2))
                    break;
        }
        if (b > *count) {
            *start = s;
            *count = b;
        }
        if (b == n - 2)
            break;
    }
    if (*count == 0) {
        *start = 0;
        *count = 1;
        return 0;
    }
    return 1;
}

/* 0x206e2db0 -- the polygon cut into fans instead, the longest first, its
 * inner vertices taken out each time. */
static void fan_triangles(uint32_t idx, int32_t stride, int32_t n, int32_t *tris, uint32_t points)
{
    struct vtx v[150];
    int32_t start, count, c, m;

    load_polygon(v, idx, stride, n, points);
    if (!best_fan(v, n, &start, &count))
        return;
    do {
        if (count > 0) {
            int32_t centre = v[start].idx;

            for (c = start + 2; c - 2 - start < count; c++) {
                *tris++ = centre;
                *tris++ = v[(c - 1) % n].idx;
                *tris++ = v[c % n].idx;
            }
        }
        n -= count;
        if (start <= n) {
            for (m = start + 1; m < n; m++)
                v[m] = v[m + count];
        } else {
            for (m = 0; m < n; m++)
                v[m] = v[start - n + 1 + m];
        }
    } while (best_fan(v, n, &start, &count));
}

/* 0x206e28d0 -- the polygon at idx (n indices, every stride words) as a
 * 0x90 mesh written over the instruction at p (len bytes, from a 0x02
 * before it): ears, or fans when the ears do not pack into the room; the
 * triangles as bytes from the lowest index, a triangle sharing its first
 * two points with the last one's first and third as its third alone, the
 * others' first with bit 7.  A spread of 0xff or more, or a byte past
 * 0x7f, and nothing.  The address after the mesh. */
static uint32_t mesh(uint32_t p, uint32_t idx, int32_t stride, int32_t n, int32_t len,
                     uint16_t nx, uint16_t ny, uint16_t nz, uint32_t d, uint32_t points)
{
    int32_t frame[150 + 450], *out = frame, *tris = frame + 150;
    int32_t retried = 0, ntris, lo, hi, i, count;

    len -= 0xf;
    if (W16(p - 2) == 2) {
        p -= 2;
        len += 2;
    }
    ntris = ear_triangles(idx, stride, n, tris, points);
    lo = 0xff;
    hi = 0;
    for (i = 0; i < 3 * ntris; i++) {
        if (lo >= tris[i])
            lo = tris[i];
        if (hi <= tris[i])
            hi = tris[i];
    }
    if (hi - lo >= 0xff)
        return 0;
    for (;;) {
        count = 0;
        for (i = 0; i < ntris; i++) {
            const int32_t *t = tris + 3 * i;

            if (i != 0 && t[0] == t[-3] && t[1] == t[-1]) {
                out[count] = t[2] - lo;
                if (out[count++] > 0x7f)
                    return 0;
                continue;
            }
            out[count] = t[0] - lo;
            if (out[count] > 0x7f)
                return 0;
            out[count] |= 0x80;
            out[count + 1] = t[1] - lo;
            out[count + 2] = t[2] - lo;
            count += 3;
        }
        if (count <= len)
            break;
        if (retried)
            return 0;
        fan_triangles(idx, stride, n, tris, points);
        retried = 1;
    }
    W16(p + 4) = nx;
    W16(p + 6) = ny;
    W16(p + 8) = nz;
    W16(p) = 0x90;
    W16(p + 2) = (uint16_t)count;
    D32(p + 0xa) = d;
    B8(p + 0xe) = (uint8_t)lo;
    for (i = 0; i < count; i++)
        B8(p + 0xf + i) = (uint8_t)out[i];
    if (!(count & 1))
        B8(p + 0xf + (count > 0 ? count : 0)) = 0;
    return p + 2 * (((uint32_t)(uint16_t)count + 0x10) >> 1);
}

/* The rest of the instruction after a mesh written at its start: 0x02s. */
static void pad(uint32_t at, uint32_t end)
{
    uint32_t k = end - at;

    while (k != 0) {
        k -= 2;
        W16(at) = 2;
        at += 2;
    }
}

/* 0x206e2ae0 -- a facet (0x3e: count, normal, distance, indices) of 4 to
 * 150 points. */
static int facet(uint32_t p, uint32_t points)
{
    uint32_t n = W16(p + 2), len = 2 * n + 0xe, r;

    if (n > 0x96)
        return 0;
    if (n < 4)
        return 1;
    r = mesh(p, p + 0xe, 1, (int32_t)n, (int32_t)len, W16(p + 4), W16(p + 6), W16(p + 8),
             D32(p + 0xa), points);
    if (r == 0)
        return 0;
    pad(r, p + len);
    return 1;
}

/* 0x206e2b50 -- a face (0x1d: count, a point, a normal, indices): the
 * distance the dot product of the point's and the normal's words,
 * zero-extended, in 32 bits. */
static int face(uint32_t p, uint32_t points)
{
    uint32_t n = W16(p + 2), len = 2 * n + 0x10, r, d;

    if (n > 0x96)
        return 0;
    if (n < 4)
        return 1;
    d = (uint32_t)W16(p + 4) * W16(p + 0xa) + (uint32_t)W16(p + 6) * W16(p + 0xc) +
        (uint32_t)W16(p + 8) * W16(p + 0xe);
    r = mesh(p, p + 0x10, 1, (int32_t)n, (int32_t)len, W16(p + 0xa), W16(p + 0xc), W16(p + 0xe),
             d, points);
    if (r == 0)
        return 0;
    pad(r, p + len);
    return 1;
}

/* 0x206e2c20 -- a surface of table points: a 0x0f, 0x10s and a 0x08, of 4
 * to 150 points, its normal and distance 0. */
static int surface(uint32_t p, uint32_t points)
{
    uint32_t q = p + 4, r;
    int32_t n = 1;

    if (W16(p) != 0xf)
        return 0;
    while (W16(q) == 0x10) {
        q += 4;
        n++;
    }
    if (W16(p + 4 * (uint32_t)n) != 8 || n <= 3 || n > 0x96)
        return 0;
    r = mesh(p, p + 2, 2, n, 4 * n + 2, 0, 0, 0, 0, points);
    if (r == 0)
        return 0;
    pad(r, p + 4 * (uint32_t)n + 2);
    return 1;
}

/* 0x206e35b0 -- 0x38's pass: itself a 0x02, then past any 0x1e, 0x7d or
 * 0x81 (by next, +0x64) to a surface (its 0x05 a 0x02 too, or after a
 * 0x18's 0x18 bytes), face or facet, which becomes a mesh.  points is
 * .data+0x12a's table.  0, as retail's eax. */
uint32_t G3D_MESH_NAME(uint32_t program, uint32_t points, uint32_t (FS98_STDCALL *next)(uint32_t))
{
    uint32_t a;

    W16(program) = 2;
    a = program + 2;
    while (a != 0) {
        switch (W16(a)) {
        case 0x05:
            W16(a) = 2;
            surface(a + 2, points);
            return 0;
        case 0x18:
            if (W16(a + 0x18) == 0xf)
                surface(a + 0x18, points);
            return 0;
        case 0x1d:
            face(a, points);
            return 0;
        case 0x3e:
            facet(a, points);
            return 0;
        case 0x1e:
        case 0x7d:
        case 0x81:
            a = next(a);
            break;
        default:
            return 0;
        }
    }
    return 0;
}
