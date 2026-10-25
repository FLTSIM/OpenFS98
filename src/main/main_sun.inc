/* MAIN.DLL slice 12: +0xa0, the sun direction and its intensity.
 *
 * Retail .text+0x830d with its helper .text+0x9097, assembly.  It builds a
 * unit sun vector into GLOBAL (+0xb2a x, +0xb2e y, +0xb32 z) from the time of
 * day and the latitude, through four of UTIL +0x2c's sine/cosine pairs, then
 * the helper turns the vertical component into a light intensity (+0xcba) and
 * a sky-colour band (+0x116e, +0x12a0).  The two flags +0xb34 and +0x116e are
 * primed to 1 on entry and cleared or raised as the geometry decides.
 *
 * The arithmetic is 16-bit signed fixed point: each product is formed with
 * `imul`, doubled (`add ax,ax; adc dx,dx`) and the high word kept -- a Q15
 * multiply, `q15` below.  Several divides run into overflow and lean on the
 * host's divide filter (see repair_divide in maindiff.c and the note on it):
 * the minutes-of-day and day-of-year `div`s saturate to all ones, and the
 * helper's `idiv bx` overflows for a mid-range sun and saturates to 0x7fff.
 * ours computes each repaired result in C so it never traps.
 *
 * UTIL +0x2c is verified in UTIL; the harness drives this slice against a
 * deterministic sine/cosine model of it, identical on both sides, so only
 * MAIN's own fixed-point arithmetic and glue are under test here.  The call
 * takes (angle, &first, &second) and writes a word to each; the hand-rolled
 * push sequence's stack alignment has no observable effect and is left out.
 */

#define SUN_V        V(0x20802086)   /* the vertical component, kept for the helper */
#define SUN_GATE     V(0x20801a76)   /* under 0x1000 forces the vector straight down */
/* UTIL +0x2c's sine/cosine scratch words, a pair written per call */
#define SUN_T1D      V(0x2080201d)
#define SUN_T1F      V(0x2080201f)
#define SUN_T21      V(0x20802021)
#define SUN_T23      V(0x20802023)
#define SUN_T25      V(0x20802025)
#define SUN_T27      V(0x20802027)
#define SUN_T29      V(0x20802029)
#define SUN_T2B      V(0x2080202b)

#define SUN_2C       CALL(UTIL, 0x2c, f3)

/* imul m16; add ax,ax; adc dx,dx; keep dx -- a signed Q15 multiply. */
static int16_t q15(int16_t a, int16_t b)
{
    uint32_t p = (uint32_t)((int32_t)a * (int32_t)b);

    return (int16_t)((p << 1) >> 16);
}

/* an unsigned `div r16` as the host leaves ax: the quotient, or all ones when
 * it will not fit (the filter's div repair).  The divisor is never zero here.
 * Only the quotient (ax) is consumed by any caller, so the remainder is not
 * modelled. */
static uint16_t div16u(uint32_t num, uint16_t den)
{
    uint32_t q = num / den;

    return q > 0xffff ? 0xffff : (uint16_t)q;
}

/* a signed `idiv r16` as the host leaves ax: the quotient, or -- on overflow
 * -- the largest value, or the most negative but one when the divisor and the
 * dividend's top half differ in sign (the filter's idiv repair, windowdiff.c's
 * div_fixup and docs/modules/host.md).  Divisor never zero; remainder unused. */
static int16_t idiv16(int32_t num, int16_t den)
{
    int32_t q = num / den;

    if (q > 0x7fff || q < -0x8000) {
        int neg = (den < 0) != ((int16_t)(num >> 16) < 0);
        return neg ? (int16_t)0x8001 : (int16_t)0x7fff;
    }
    return (int16_t)q;
}

/* .text+0x9097: the vertical sun into an intensity (+0xcba) and a colour band
 * (+0x116e, +0x12a0).  Reaches GLOBAL through edi, which its one caller holds
 * live, so it takes the pointer. */
static void sun_light(uint32_t gp)
{
    uint16_t ax, bx;
    int16_t dx;

    ax = 0xffff;
    dx = (int16_t)M16(SUN_V);
    if (dx > (int16_t)0xe4a8) {
        dx = (int16_t)(dx - 0x1b58);
        if (dx > 0)
            dx = 0;
        dx = (int16_t)(-dx);
        ax = div16u((uint32_t)(uint16_t)dx << 16, 0x36b0);
        bx = M16(gp + 0xb54) != 0 ? 0x1c00 : 0x1000;
        if (ax < bx)
            ax = bx;
    }
    M16(gp + 0xcba) = ax;

    dx = (int16_t)M16(SUN_V);
    if (dx > (int16_t)0xe4a8) {
        M16(gp + 0x116e) = 2;
        if (dx > (int16_t)0xfe80)
            M16(gp + 0x116e) = 4;
    }
    dx = (int16_t)(-dx);
    if (dx > (int16_t)0xfc18) {
        dx = (int16_t)(dx - 0x1770);
        if (dx >= 0) { M16(gp + 0x12a0) = 0; return; }
        dx = (int16_t)(-dx);
        bx = 0x36b0;
    } else {
        dx = (int16_t)(dx - (int16_t)0xf060);
        if (dx <= 0) { M16(gp + 0x12a0) = 0; return; }
        bx = 0x1770;
    }
    ax = (uint16_t)idiv16((int32_t)((uint32_t)(uint16_t)dx << 16), (int16_t)bx);
    M16(gp + 0x12a0) = (uint16_t)(ax >> 5);
}

/* .text+0x830d == Linkage +0xa0. */
static uint32_t FS98_STDCALL nav_sun(void)
{
    uint32_t gp = G;
    uint16_t ax, bx, dx;
    int16_t x, y, z, t;

    M16(gp + 0xb34) = 1;
    M16(gp + 0x116e) = 1;

    /* the fraction of the day, plus a fixed offset, as the first angle */
    ax = (uint16_t)(M8(gp + 0x23b) * 0x3cu);
    dx = (uint16_t)(M8(gp + 0x23c) + ax);
    ax = div16u((uint32_t)dx << 16, 0x5a0);
    bx = (uint16_t)(ax + M16(gp + 0x56e));
    SUN_2C(bx, SUN_T2B, SUN_T29);

    /* the day of the year, as the second angle */
    dx = M16(gp + 0x23e);
    ax = div16u((uint32_t)dx << 16, 0x16d);
    ax = (uint16_t)(ax + 0x650);
    bx = (uint16_t)(ax - 0x4000);
    SUN_2C(bx, SUN_T1D, SUN_T1F);

    /* a tilt scaled from the day angle's sine */
    bx = (uint16_t)q15((int16_t)(-(int16_t)M16(SUN_T1D)), 0x1004);
    SUN_2C(bx, SUN_T23, SUN_T21);

    /* the latitude, as the fourth angle */
    ax = (uint16_t)idiv16((int32_t)(((uint32_t)M16(gp + 0x566) << 16) | M16(gp + 0x564)), 0x131);
    ax = (uint16_t)((int16_t)ax >> 1);
    bx = (uint16_t)(ax + 0xc000);
    SUN_2C(bx, SUN_T27, SUN_T25);

    /* the vector from the four pairs */
    t = q15((int16_t)M16(SUN_T21), (int16_t)M16(SUN_T29));
    t = q15(t, (int16_t)M16(SUN_T25));
    z = (int16_t)(q15((int16_t)M16(SUN_T23), (int16_t)M16(SUN_T27)) + t);
    M16(gp + 0xb32) = (uint16_t)(-z);

    t = q15((int16_t)M16(SUN_T21), (int16_t)M16(SUN_T29));
    t = q15(t, (int16_t)M16(SUN_T27));
    y = (int16_t)(q15((int16_t)M16(SUN_T23), (int16_t)M16(SUN_T25)) - t);
    M16(gp + 0xb2e) = (uint16_t)y;

    x = q15((int16_t)M16(SUN_T21), (int16_t)M16(SUN_T2B));
    M16(gp + 0xb2a) = (uint16_t)(-x);

    M16(SUN_V) = M16(gp + 0xb2e);
    if ((int16_t)M16(gp + 0xb2e) >= 0) {
        M16(gp + 0xb2a) = 0;
        M16(gp + 0xb2e) = 0;
        M16(gp + 0xb32) = 0;
    }
    if ((int16_t)M16(gp + 0xb2e) >= (int16_t)0xff00)
        M16(gp + 0xb34) = 0;

    sun_light(gp);

    ax = 0xffff;
    if (M16(SUN_GATE) < 0x1000) {
        M16(gp + 0xb2a) = 0;
        M16(gp + 0xb2e) = 0xc000;
        M16(gp + 0xb32) = 0;
    }
    bx = (uint16_t)((uint16_t)(((uint32_t)ax * M16(gp + 0xcba)) >> 16) >> 1);
    M16(gp + 0xb2a) = (uint16_t)q15((int16_t)M16(gp + 0xb2a), (int16_t)bx);
    M16(gp + 0xb2e) = (uint16_t)q15((int16_t)M16(gp + 0xb2e), (int16_t)bx);
    M16(gp + 0xb32) = (uint16_t)q15((int16_t)M16(gp + 0xb32), (int16_t)bx);
    return 0;
}
