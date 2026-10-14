/* WEATHERM.DLL -- module id 27: the weather.
 *
 * Each frame MAIN asks for the weather at the aircraft (+0x1c), and ATC asks
 * for it at an airport (+0x18).  WEATHERM blends GLOBAL's global weather block
 * (+0xf1c..+0xf8e) with whichever of the two weather areas (+0xf90, +0x1078)
 * the position is deepest inside, into the working weather GLOBAL publishes at
 * +0xe8a..+0xf0e: cloud layers, temperature layers, wind layers, visibility,
 * and what the position sees of them -- the temperature and pressure at its
 * altitude, the wind, the cloud it is in.  A CHAIN list 3 callback moves the
 * areas with their own wind and lets the pressure wander.  +0x28 takes the
 * weather effects G3D finds in scenery.
 *
 * Retail is hand-written assembly to .text+0x2a3f and compiled C after it.
 * Transliterated from all of it; the comments carry retail's addresses and
 * docs/modules/weatherm.md has the walk through.  Before calling into it,
 * know that:
 *
 *   - Everything lives in one .data image laid out exactly as retail's,
 *     ImportTable at +0x00 and Linkage at +0x38, and the assembly half
 *     addresses its variables by retail's offsets.  The image holds real
 *     pointers (the position, the three pressure blocks), so it is a 32-bit
 *     layout, as are the C half's records.
 *   - The assembly passes values between routines in registers, and several
 *     routines hand back three or four at once.  Those are written as C
 *     functions with out-parameters named after the registers, so each can be
 *     read against the listing.
 *   - Retail divides in three places where a quotient can overflow or a
 *     divisor be zero, and faults.  So does this, with the exception the
 *     processor would raise; see wx_fault().
 *   - The C half (+0x2c..+0x5c) allocates without asking the host to zero
 *     memory, then walks list pointers it never set.  It only works if the
 *     allocator happens to return zeroes.  Nothing in the retail set calls it.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define WX_DATA_VA    0x208f5000u   /* retail .data */
#define WX_IMAGE_SIZE 0x200         /* .data's 0x1c5, rounded up */

typedef uint16_t __attribute__((may_alias)) wx_u16;
typedef uint32_t __attribute__((may_alias)) wx_u32;

typedef void (FS98_STDCALL *wx_tick_fn)(uint32_t a, uint32_t b);

struct wx_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;
    void    *dialogs;                                               /* +0x14: none */
    void     (FS98_STDCALL *weather_at)(void *position);            /* +0x18 */
    void     (*weather_here)(void);                                 /* +0x1c */
    void     (FS98_STDCALL *temperature)(uint32_t base, uint16_t *temp,
                                         uint16_t *swing);          /* +0x20 */
    void     (*reset)(void);                                        /* +0x24 */
    void     (FS98_STDCALL *effect)(const uint8_t *record);         /* +0x28 */
    int32_t  (FS98_STDCALL *list_create)(void **cell);              /* +0x2c */
    int32_t  (FS98_STDCALL *list_destroy)(void **cell);             /* +0x30 */
    void    *(FS98_STDCALL *area_new)(void *cell);                  /* +0x34 */
    int32_t  (FS98_STDCALL *area_remove)(void *cell, void *area);   /* +0x38 */
    uint32_t (FS98_STDCALL *area_copy)(void *src, void *dst);       /* +0x3c */
    void    *(FS98_STDCALL *area_next)(void *cell, void *area);     /* +0x40 */
    uint32_t (FS98_STDCALL *area_append)(void *cell, void *area);   /* +0x44 */
    uint32_t (FS98_STDCALL *area_count)(void *cell);                /* +0x48 */
    void    *(FS98_STDCALL *record_new)(void *area, uint32_t type); /* +0x4c */
    uint32_t (FS98_STDCALL *record_free)(void *area, void *record,
                                         uint32_t type);            /* +0x50 */
    uint32_t (FS98_STDCALL *records_copy)(void *src, void *dst, void *record,
                                          uint32_t type, uint32_t keep); /* +0x54 */
    void    *(FS98_STDCALL *record_next)(void *area, void *record,
                                         uint32_t type);            /* +0x58 */
    uint32_t (FS98_STDCALL *record_append)(void *area, void *record,
                                           uint32_t type);          /* +0x5c */
};

struct wx_image {
    struct fs98_import import[7];       /* +0x000 ImportTable */
    struct wx_linkage  linkage;         /* +0x038 Linkage */
    uint8_t            data[WX_IMAGE_SIZE - 0x98]; /* +0x098 on */
};

extern struct wx_image wx_image;

typedef char (*wx_layout_check)[sizeof(struct wx_image) == WX_IMAGE_SIZE &&
                                sizeof(struct wx_linkage) == 0x60 ? 1 : -1];

/* ---- the image and its imports ------------------------------------------------ */

#define D           ((uint8_t *)&wx_image)
#define D8(o)       (*(uint8_t *)(D + (o)))
#define D16(o)      (*(wx_u16 *)(D + (o)))
#define D32(o)      (*(wx_u32 *)(D + (o)))

#define HOST        ((void *)wx_image.import[0].link)
#define G           ((uint8_t *)wx_image.import[1].link)
#define G8(o)       (*(uint8_t *)(G + (o)))
#define G16(o)      (*(wx_u16 *)(G + (o)))
#define G32(o)      (*(wx_u32 *)(G + (o)))

/* any other pointer: a record, the position */
#define P8(p, o)    (*(uint8_t *)((uint8_t *)(p) + (o)))
#define P16(p, o)   (*(wx_u16 *)((uint8_t *)(p) + (o)))
#define P32(p, o)   (*(wx_u32 *)((uint8_t *)(p) + (o)))
#define PPTR(p, o)  (*(void **)((uint8_t *)(p) + (o)))

#define SLOT(i, off, type) (*(type *)((uint8_t *)wx_image.import[i].link + (off)))

/* Every prototype read from the callee's `ret N`. */
typedef int32_t  (FS98_STDCALL *chain_add_fn)(uint32_t list, wx_tick_fn cb);      /* ret 8 */
typedef void     (FS98_STDCALL *g3d_release_fn)(void *object);                    /* ret 4 */
typedef void     (*main_weather_fn)(void);                                        /* ret */
typedef void     (FS98_STDCALL *util_mul48_fn)(const void *src, int16_t m,
                                               void *dst);                        /* ret 0xc */
typedef void     (FS98_STDCALL *util_sincos_fn)(int16_t angle, int16_t *s,
                                                int16_t *c);                      /* ret 0xc */
typedef void     (FS98_STDCALL *util_rotate2d_fn)(int32_t x, int32_t y, int16_t s,
                                                  int16_t c, int32_t *ox,
                                                  int32_t *oy);                   /* ret 0x18 */
typedef int32_t  (FS98_STDCALL *util_atan2_fn)(int32_t x, int32_t y);            /* ret 8 */
typedef void     (FS98_STDCALL *util_convert_fn)(const void *in48, const void *pos48,
                                                 void *out48);                    /* ret 0xc */
typedef uint32_t (FS98_STDCALL *util_rand_fn)(void);                             /* ret */

#define CHAIN_ADD        SLOT(2, 0x18, chain_add_fn)
#define CHAIN_REMOVE     SLOT(2, 0x1c, chain_add_fn)
#define G3D_RELEASE      SLOT(3, 0x20, g3d_release_fn)
#define MAIN_WEATHER     SLOT(4, 0x94, main_weather_fn)
#define UTIL_MUL48       SLOT(5, 0x24, util_mul48_fn)
#define UTIL_SINCOS      SLOT(5, 0x2c, util_sincos_fn)
#define UTIL_ROTATE2D    SLOT(5, 0x34, util_rotate2d_fn)
#define UTIL_ATAN2       SLOT(5, 0x3c, util_atan2_fn)
#define UTIL_TO_LONGITUDE SLOT(5, 0x44, util_convert_fn)
#define UTIL_TO_METRES   SLOT(5, 0x48, util_convert_fn)
#define UTIL_RAND        SLOT(5, 0x6c, util_rand_fn)

/* Variables, at retail's offsets. */
#define WX_X            D32(0xa8)    /* rotated offset, along the area's axis */
#define WX_Y            D32(0xac)
#define WX_WEIGHT       D16(0xb0)    /* blend weight, and its complement */
#define WX_WEIGHT_NOT   D16(0xb2)
#define WX_SLOPE        D32(0xb4)    /* temperature: change across the band */
#define WX_DX           D32(0xb8)    /* offset from an area's centre, metres */
#define WX_DY           D32(0xbc)
#define WX_BAND         D16(0xc0)    /* temperature: the band's depth */
#define WX_TEMP         D16(0xc2)    /* temperature at the band's base */
#define WX_PRESSURE     D16(0xc4)
#define WX_ALT          D16(0xc6)
#define WX_POSITION     (*(uint8_t **)(D + 0xf6))
#define WX_DIVSTEP      D8(0xf2)     /* the long division's step counter */
#define WX_SURFACE      D8(0x102)    /* set while the surface wind is used */
#define WX_PASSES       D16(0x1c3)   /* the cloud-cover blend's pass counter */

/* .data+0xc8..0xe3: five three-word arrays, per pressure block -- indexed with
 * the block number unscaled, so blocks 1 and 2 overlap.  See wx_wander(). */
#define WX_P_BASE(i)    D16(0xc8 + (i))
#define WX_P_LAST(i)    D16(0xce + (i))
#define WX_P_TARGET(i)  D16(0xd4 + (i))
#define WX_P_OFFSET(i)  D16(0xda + (i))
#define WX_P_CLOCK(i)   D16(0xe0 + (i))
#define WX_P_BLOCK(i)   (*(uint8_t **)(D + 0xe6 + 4 * (i)))

#define WX_NONE         0xd8f1       /* -9999: an unused temperature layer */

/* Retail divides with `div`/`idiv` and so faults on a zero divisor or a
 * quotient that does not fit.  So does this: the exceptions are the ones
 * Windows raises for the processor's divide error. */
static void wx_fault(DWORD code)
{
    RaiseException(code, EXCEPTION_NONCONTINUABLE, 0, NULL);
}

/* ---- the long division (.text+0x2803) ------------------------------------------- */

/* .text+0x2845 -- 32 steps of restoring division: edx:eax shifts left through
 * edx, quotient bits into ecx.  The step counter is a .data byte. */
static void wx_divsteps(uint32_t *eax, uint32_t *edx, uint32_t *ecx, uint32_t ebx)
{
    WX_DIVSTEP = 0x20;
    do {
        if (*edx >= ebx) {                  /* sub edx,ebx; jae */
            *edx -= ebx;
            *ecx = (*ecx << 1) | 1;         /* stc; adc ecx,ecx */
        } else {
            *ecx <<= 1;
        }
        *edx = (*edx << 1) | (*eax >> 31);  /* add eax,eax; adc edx,edx */
        *eax <<= 1;
    } while (--WX_DIVSTEP != 0);
}

static void wx_neg64(uint32_t *eax, uint32_t *edx)
{
    uint32_t borrow = *eax != 0;            /* neg eax; not edx; sbb edx,-1 */
    *eax = 0u - *eax;
    *edx = ~*edx + 1u - borrow;
}

/* .text+0x2803 -- signed edx:eax / ebx, as a 64-bit quotient in 16.48 pieces:
 * bx gets bits 0-15, cx 16-31, ax 32-47 and edx 48-63. */
static void wx_divide(uint32_t *eax, uint32_t *edx, uint32_t *ebx, uint32_t *ecx)
{
    uint32_t sign = *edx ^ *ebx, high;

    *ecx = sign;
    if ((int32_t)*edx < 0)
        wx_neg64(eax, edx);
    if ((int32_t)*ebx < 0)
        *ebx = 0u - *ebx;
    *edx = (*edx << 1) | (*eax >> 31);
    *eax <<= 1;
    wx_divsteps(eax, edx, ecx, *ebx);
    high = *ecx;
    wx_divsteps(eax, edx, ecx, *ebx);
    *eax = *ecx;
    *edx = high;
    if ((int32_t)sign < 0)
        wx_neg64(eax, edx);
    *ebx = (*ebx & 0xffff0000u) | (*eax & 0xffffu);
    *ecx = (sign << 16) | (*eax >> 16);     /* shld ecx,eax,16 */
    *eax = (*eax & 0xffff0000u) | (*edx & 0xffffu);
    *edx >>= 16;
}

/* ---- table interpolation ---------------------------------------------------------- */

/* .text+0x2871 -- 32-step table at .data+`table`, by ax's top bits,
 * interpolated with the rest.  (.text+0x2892, a 16-step variant, has no
 * callers.) */
static uint16_t wx_lookup(uint16_t ax, uint32_t table)
{
    uint32_t bx = ((uint32_t)(ax >> 8) >> 2) & 0x3e;
    uint16_t cx = D16(table + bx);
    uint16_t dx = (uint16_t)(D16(table + bx + 2) - cx);
    uint32_t p;

    ax = (uint16_t)((ax & 0x7ff) << 4);
    p = (uint32_t)((int32_t)(int16_t)ax * (int16_t)dx) << 1;
    return (uint16_t)(cx + (p >> 16));
}

/* .text+0x2743 -- dx:ax a 16.16 ratio, clamped to [0, 2), through the table at
 * +0x103; the result comes back sign-extended and times 32 in dx:ax. */
static uint32_t wx_log_ratio(uint16_t dx, uint16_t ax)
{
    if ((int16_t)dx >= 2) {
        dx = 1;
        ax = 0xffff;
    } else if ((int16_t)dx < 0) {
        dx = 0;
        ax = 0;
    }
    ax = (uint16_t)((ax >> 1) | ((dx & 1) << 15));  /* shr dx,1; rcr ax,1 */
    ax = wx_lookup(ax, 0x103);
    return (uint32_t)(int32_t)(int16_t)ax << 5;     /* cwd; five shl/rcl */
}

/* .text+0x279a -- the table at +0x181, then scaled by one of the 32-bit
 * multipliers at +0x145, chosen by dx: dx:ax times a 16-bit value, 16.16. */
static uint32_t wx_exp_scale(uint16_t ax, uint16_t dx)
{
    uint16_t cx = wx_lookup(ax, 0x181), hi, lo, di, rx;
    uint32_t i, p, q;

    ax = (uint16_t)(dx - 2);
    ax = (uint16_t)((uint16_t)(0u - ax) << 2);      /* sub ax,2; neg ax; shl ax,1 twice */
    if (ax != 0) {
        if ((int16_t)ax > 0) {
            if ((int16_t)ax > 0x38)
                ax = 0x38;
        } else {
            ax = 0;
        }
    }
    i = ax;
    hi = D16(0x145 + i);
    lo = D16(0x147 + i);
    p = (uint32_t)lo * cx;
    di = (uint16_t)(p >> 16);
    q = (uint32_t)hi * cx;
    rx = (uint16_t)q;
    ax = (uint16_t)(rx + di);
    dx = (uint16_t)((q >> 16) + (ax < rx));         /* add ax,di; adc dx,0 */
    return ((uint32_t)dx << 16) | ax;
}

/* ---- temperature and pressure ------------------------------------------------------- */

/* `imul dx` by a constant, then al=ah; ah=dl; dl=dh: the product's middle
 * sixteen bits, and dx left as dh twice. */
static uint16_t wx_mul_mid(uint16_t ax, uint16_t k, uint16_t *dx)
{
    uint32_t p = (uint32_t)((int32_t)(int16_t)ax * (int16_t)k);
    uint8_t dh = (uint8_t)(p >> 24);

    *dx = (uint16_t)((dh << 8) | dh);
    return (uint16_t)(p >> 8);
}

/* .text+0x2567 -- a temperature at the altitude in WX_ALT between two layers
 * (cx above, dx below, ax and bx their temperatures).  Leaves the band's
 * depth and the temperature change across it for wx_pressure().  The `idiv`
 * faults when WX_ALT is the upper layer's altitude and the band is an even
 * number of metres deep. */
static uint16_t wx_interpolate(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx)
{
    int32_t a = (int16_t)ax, b = (int16_t)bx, c = (int16_t)cx, d = (int16_t)dx;
    int32_t depth = c - d, up;
    int64_t q, p;

    WX_BAND = (uint16_t)depth;
    up = ((int32_t)(int16_t)WX_ALT - d) >> 1;
    if (depth == 0)
        wx_fault(EXCEPTION_INT_DIVIDE_BY_ZERO);
    q = (int64_t)((uint64_t)(uint32_t)up << 32) / depth;
    if (q > INT32_MAX || q < INT32_MIN)
        wx_fault(EXCEPTION_INT_OVERFLOW);
    WX_SLOPE = (uint32_t)(a - b);
    p = (int64_t)(int32_t)WX_SLOPE * (int32_t)q;
    return (uint16_t)((uint32_t)((uint64_t)p << 1 >> 32) + (uint32_t)b);
}

/* .text+0x25a8 -- the pressure at the top of a band, from the pressure at its
 * base (WX_PRESSURE), its base temperature (WX_TEMP), and either the lapse
 * across it (WX_SLOPE, WX_BAND, and bx the temperature at the top) or, with
 * no lapse, the isothermal form (dx the altitude at the top). */
static uint32_t wx_pressure(uint16_t bx, uint16_t dx)
{
    uint8_t in[8] = { 0 }, out[8];
    uint32_t eax, ebx, ecx = 0, edx, r;

    if (WX_SLOPE != 0) {
        eax = (uint32_t)(int32_t)(int16_t)WX_BAND;
        edx = (int32_t)eax < 0 ? 0xffffffffu : 0;   /* cdq, then only eax shifts */
        eax <<= 16;
        ebx = 0u - WX_SLOPE;
        wx_divide(&eax, &edx, &ebx, &ecx);
        P16(in, 2) = (uint16_t)ecx;
        P32(in, 4) = (edx << 16) | (eax & 0xffffu);
        UTIL_MUL48(in, 0x23, out);
        eax = (P32(out, 4) << 16) | (P32(out, 0) >> 16);
        D32(0xfa) = (uint32_t)((int32_t)eax >> 3);

        ebx = (uint32_t)((int32_t)(int16_t)WX_TEMP + 0x11126);
        eax = (uint32_t)((int32_t)(int16_t)bx + 0x11126);
        edx = 0;
        wx_divide(&eax, &edx, &ebx, &ecx);
        r = wx_log_ratio((uint16_t)eax, (uint16_t)ecx);
        D16(0xfe) = (uint16_t)r;
        D16(0x100) = (uint16_t)(r >> 16);
        {
            int64_t p = (int64_t)(int32_t)D32(0xfa) * (int32_t)r;
            eax = (uint32_t)p >> 16;
            edx = (uint32_t)((uint64_t)p >> 32);
        }
        r = wx_exp_scale((uint16_t)eax, (uint16_t)edx);
    } else {
        eax = (uint32_t)((int32_t)(int16_t)WX_TEMP + 0x11126);
        ebx = (uint32_t)((uint64_t)eax * 0x75) >> 2;
        eax = (uint32_t)(int32_t)(int16_t)(uint16_t)(dx - WX_ALT);
        edx = (int32_t)eax < 0 ? 0xffffffffu : 0;
        eax <<= 8;
        wx_divide(&eax, &edx, &ebx, &ecx);
        r = wx_exp_scale((uint16_t)ecx, (uint16_t)eax);
    }
    P16(in, 2) = 0;
    P32(in, 4) = r;
    UTIL_MUL48(in, (int16_t)WX_PRESSURE, out);
    return (uint32_t)((int32_t)P32(out, 4) >> 1);
}

/* .text+0x1d70 -- Linkage +0x20: the day's temperature swing.  `base` plus the
 * swing, by the time of day (GLOBAL+0x238 hours, +0x239 minutes), when
 * +0xec2 asks for it. */
static void FS98_STDCALL wx_temperature_now(uint32_t base, uint16_t *temp, uint16_t *swing)
{
    int16_t s, c;
    uint16_t bx, ax, dx = 0;

    if (G16(0xec2) != 0) {
        bx = (uint16_t)(G8(0x238) * D16(0xf4));
        ax = (uint16_t)(G8(0x239) * D8(0xf3));
        bx = (uint16_t)(bx + ax - 0x5550);
        UTIL_SINCOS((int16_t)bx, &s, &c);
        ax = (uint16_t)((uint16_t)(s >> 1) - 0x3fff);
        dx = (uint16_t)(((uint32_t)((int32_t)(int16_t)ax * (int16_t)G16(0xec4)) << 1) >> 16);
    }
    *swing = dx;
    *temp = (uint16_t)(dx + (uint16_t)base);
}

/* .text+0x188e -- the temperature and pressure at the position's altitude.
 * GLOBAL+0xeb2..+0xec0 holds four layers, (altitude, temperature) from the
 * top down; the surface pressure is +0xec6. */
static void wx_temperature(void)
{
    static const uint16_t layer[4][2] = {   /* altitude, temperature */
        { 0xebe, 0xec0 }, { 0xeba, 0xebc }, { 0xeb6, 0xeb8 }, { 0xeb2, 0xeb4 }
    };
    uint16_t ax, bx, cx, dx, alt, saved = 0, temp, swing;
    int k, pushed = 0;

    if (G16(0xebe) == WX_NONE) {
        G16(0xebe) = 0;
        G16(0xec0) = 0xf00;
    }
    /* sea level, from the lowest layer */
    ax = (uint16_t)(wx_mul_mid(G16(0xebe), 0x1aa, &dx) + G16(0xec0));
    WX_TEMP = ax;
    WX_SLOPE = (uint32_t)(int32_t)(int16_t)(uint16_t)-ax;
    bx = G16(0xec0);
    WX_SLOPE += (uint32_t)(int32_t)(int16_t)bx;
    WX_ALT = G16(0xebe);
    WX_BAND = G16(0xebe);
    WX_PRESSURE = G16(0xec6);
    cx = (uint16_t)wx_pressure(bx, 0);
    WX_PRESSURE = cx;
    bx = G16(0xec0);
    WX_TEMP = bx;
    alt = ax = P16(WX_POSITION, 0x14);
    WX_ALT = ax;

    /* below the lowest layer */
    if (G16(0xebe) != WX_NONE && !((int16_t)ax > (int16_t)G16(0xebe))) {
        ax = wx_mul_mid((uint16_t)(G16(0xebe) - WX_ALT), 0x1aa, &dx);
        WX_SLOPE = (uint32_t)(int32_t)(int16_t)ax;
        bx = (uint16_t)(ax + G16(0xec0));
        WX_BAND = (uint16_t)(WX_ALT - G16(0xebe));
        cx = (uint16_t)wx_pressure(bx, G16(0xebe));
        goto done;
    }

    /* up through the layers above it */
    dx = G16(layer[0][0]);
    for (k = 1; k < 4; k++) {
        uint16_t top = G16(layer[k][0]), below = G16(layer[k - 1][0]);

        cx = top;
        dx = below;
        if (top == WX_NONE)
            goto above;
        if (!((int16_t)ax > (int16_t)top)) {
            bx = wx_interpolate(G16(layer[k][1]), bx, cx, dx);
            cx = (uint16_t)wx_pressure(bx, below);
            goto done;
        }
        WX_ALT = cx;
        wx_interpolate(G16(layer[k][1]), bx, cx, dx);  /* for the band's slope */
        bx = G16(layer[k][1]);
        cx = (uint16_t)wx_pressure(bx, below);
        WX_PRESSURE = cx;
        WX_TEMP = bx;
        ax = alt;
        WX_ALT = ax;
        dx = top;
    }

above:
    /* above the top layer, a fixed lapse to 11000 m, isothermal beyond */
    if (!((int16_t)dx >= 0x2af8)) {
        if ((int16_t)WX_ALT >= 0x2af8) {
            saved = WX_ALT;
            pushed = 1;
            ax = 0x2af8;
            WX_ALT = ax;
        }
        ax = (uint16_t)(ax - dx);
        WX_BAND = ax;
        ax = wx_mul_mid(ax, 0xfe56, &dx);
        WX_SLOPE = (uint32_t)(int32_t)(int16_t)ax;
        bx = (uint16_t)(ax + WX_TEMP);
        cx = (uint16_t)wx_pressure(bx, dx);
        WX_PRESSURE = cx;
        WX_TEMP = bx;
        if ((int16_t)WX_ALT < 0x2af8)
            goto done;
        if (pushed)
            WX_ALT = saved;
        dx = 0x2af8;
    }
    WX_SLOPE = 0;
    cx = (uint16_t)wx_pressure(bx, dx);
    bx = WX_TEMP;

done:
    G16(0xe8e) = cx;
    wx_temperature_now(bx, &temp, &swing);
    G16(0xe8c) = temp;
}

/* ---- blending the global weather with an area's ---------------------------------- */

/* Three cursors the blend routines walk together: the area's value, the
 * global value, and where the result goes. */
struct wx_cursor { uint8_t *a, *b, *d; };

/* .text+0x1348 -- `count` words blended by WX_WEIGHT (0 all area, 0x4000 all
 * global).  flags 1: unsigned, no special case for zero; 3: signed; 4: as a
 * signed difference from the global value.  Without 1 or 4 a zero on either
 * side takes the other's value -- no caller asks for that. */
static void wx_blend(struct wx_cursor *k, uint32_t count, uint32_t flags, uint16_t w)
{
    WX_WEIGHT = w;
    WX_WEIGHT_NOT = (uint16_t)(0x4000 - w);
    do {
        uint16_t a = P16(k->a, 0), b = P16(k->b, 0), r;
        uint32_t s;

        if (flags & 4) {
            s = (uint32_t)((int32_t)(int16_t)(uint16_t)(a - b) * (int16_t)WX_WEIGHT_NOT) << 2;
            r = (uint16_t)(b + (s >> 16));
        } else if (!(flags & 1) && a == 0) {
            r = b;
        } else if (!(flags & 1) && b == 0) {
            r = a;
        } else {
            if (flags & 2)
                s = (uint32_t)((int32_t)(int16_t)a * (int16_t)WX_WEIGHT_NOT)
                  + (uint32_t)((int32_t)(int16_t)b * (int16_t)WX_WEIGHT);
            else
                s = (uint32_t)a * WX_WEIGHT_NOT + (uint32_t)b * WX_WEIGHT;
            r = (uint16_t)((s << 2) >> 16);
        }
        P16(k->d, 0) = r;
        k->a += 2;
        k->b += 2;
        k->d += 2;
    } while (--count != 0);
}

/* .text+0x1419 -- one cloud layer's top, base and cover.  A layer missing on
 * one side (top 0) takes the other side's top and base, and only the cover is
 * blended -- against zero. */
static void wx_blend_layer(struct wx_cursor *k, uint16_t w)
{
    uint16_t saved;

    if (P16(k->a, 0) == 0) {
        P32(k->d, 0) = P32(k->b, 0);
        k->d += 4; k->b += 4; k->a += 4;
        saved = P16(k->a, 0);
        P16(k->a, 0) = 0;
        wx_blend(k, 1, 1, w);
        P16(k->a, -2) = saved;
    } else if (P16(k->b, 0) == 0) {
        P32(k->d, 0) = P32(k->a, 0);
        k->d += 4; k->b += 4; k->a += 4;
        saved = P16(k->b, 0);
        P16(k->b, 0) = 0;
        wx_blend(k, 1, 1, w);
        P16(k->b, -2) = saved;
    } else {
        wx_blend(k, 2, 3, w);
        wx_blend(k, 1, 1, w);
    }
}

/* .text+0x148c -- the temperature layers.  Between the extremes, the eight
 * words of altitudes are taken whole from whichever side has more weight;
 * then the surface pressure is clamped to 0x34f1..0x4a0f. */
static void wx_blend_temperatures(struct wx_cursor *k, uint16_t w)
{
    if (w == 0 || w == 0x4000) {
        wx_blend(k, 0xc, 3, w);
    } else {
        uint16_t pick = (int16_t)w >= 0x2000 ? 0x4000 : 0;
        WX_PASSES = 3;
        do
            wx_blend(k, 2, 3, pick);
        while ((int16_t)--WX_PASSES >= 0);
        wx_blend(k, 4, 3, pick);
    }
    if (G16(0xec6) <= 0x34f0 || G16(0xec6) >= 0x4a10)
        G16(0xec6) = 0x3f80;
}

/* .text+0x1304 -- one wind layer: altitudes, direction (as a difference), and
 * speed and turbulence. */
static void wx_blend_wind(struct wx_cursor *k, uint32_t first, uint16_t w)
{
    wx_blend(k, first, 1, w);
    wx_blend(k, 1, 4, w);
    wx_blend(k, 2, 1, w);
}

/* .text+0x1329 -- one word at `offset` into both sources.  The destination is
 * not offset -- so every call writes the same word. */
static void wx_blend_at(const struct wx_cursor *k, uint32_t offset, uint16_t w)
{
    struct wx_cursor c = { k->a + offset, k->b + offset, k->d };
    wx_blend(&c, 1, 1, w);
}

/* .text+0x11ec -- blend the global weather with the chosen area into the
 * working copy. */
static void wx_blend_area(const uint8_t *area, uint16_t w)
{
    struct wx_cursor k;
    uint8_t *tail;
    const uint8_t *src;
    int i;

    if (area == G + 0xf90) {
        k.a = G + 0x1004;
        tail = G + 0x1062;
    } else {
        k.a = G + 0x10ec;
        tail = G + 0x114a;
    }
    k.b = G + 0xf1c;
    k.d = G + 0xe9a;
    for (i = 0; i < 3; i++) {               /* cloud layers */
        wx_blend_layer(&k, w);
        k.b += 2; k.a += 2; k.d += 2;
    }
    wx_blend_temperatures(&k, w);
    for (i = 0; i < 3; i++)                 /* wind layers, and the surface's */
        wx_blend_wind(&k, 3, w);
    wx_blend_wind(&k, 2, w);

    /* the surface wind's three blends all land on GLOBAL+0xef8 and are then
     * overwritten; the copy below leaves +0xefc, +0xf02 and +0xf08 as they were */
    k.a = tail;
    k.b = G + 0xf7a;
    k.d = G + 0xef8;
    wx_blend_at(&k, 4, w);
    wx_blend_at(&k, 0xa, w);
    wx_blend_at(&k, 0x10, w);
    src = w >= 0x2000 ? G + 0xf7a : tail;
    P16(k.d, 0x0) = P16(src, 0x0);
    P16(k.d, 0x2) = P16(src, 0x2);
    P16(k.d, 0x6) = P16(src, 0x6);
    P16(k.d, 0x8) = P16(src, 0x8);
    P16(k.d, 0xc) = P16(src, 0xc);
    P16(k.d, 0xe) = P16(src, 0xe);

    k.a = tail + 0x12;                      /* visibility */
    k.b = G + 0xf8c;
    k.d = G + 0xe8a;
    wx_blend_at(&k, 0, w);
}

/* rep movsb */
static void wx_copy(uint8_t *dst, const uint8_t *src, uint32_t n)
{
    while (n--)
        *dst++ = *src++;
}

/* .text+0x2382 -- no area in force: the global weather as it stands. */
static void wx_copy_global(void)
{
    wx_copy(G + 0xe9a, G + 0xf1c, 0x18);
    wx_copy(G + 0xeb2, G + 0xf34, 0x18);
    wx_copy(G + 0xeca, G + 0xf4c, 0x2e);
    wx_copy(G + 0xef8, G + 0xf7a, 0x12);
    wx_copy(G + 0xe8a, G + 0xf8c, 2);
}

/* ---- where the position is, relative to an area ---------------------------------- */

/* .text+0x163d -- (lat, lon) relative to the area's centre, the longitude
 * converted to metres at the centre's latitude, into WX_DX/WX_DY.  Returns
 * WX_DX. */
static uint32_t wx_offset(uint32_t lat, uint32_t lon, uint8_t *area)
{
    uint8_t in[8] = { 0 }, out[8];

    WX_DY = lat - P32(area, 0x3c);
    P32(in, 4) = lon - P32(area, 0x44);
    P16(in, 2) = 0;
    UTIL_TO_METRES(in, area + 0x38, out);
    WX_DX = P32(out, 4);
    return WX_DX;
}

/* .text+0x1697 -- WX_DX/WX_DY rotated by `angle` into WX_X/WX_Y. */
static void wx_rotate(uint16_t angle)
{
    int16_t s, c;

    UTIL_SINCOS((int16_t)angle, &s, &c);
    UTIL_ROTATE2D((int32_t)WX_DX, (int32_t)WX_DY, s, c,
                  (int32_t *)(D + 0xa8), (int32_t *)(D + 0xac));
}

/* .text+0x150b -- an area's centre, axis angle and half-width, from its two
 * corners (+0x06 and +0x16, 48-bit lat/lon pairs). */
static void wx_shape(uint8_t *area)
{
    uint32_t eax, ecx;
    uint16_t angle;

    if (P16(area, 0) == 0)
        return;
    P32(area, 0x3c) = (uint32_t)(((uint64_t)P32(area, 0x08) + P32(area, 0x18)) >> 1);
    P32(area, 0x44) = (uint32_t)(((uint64_t)P32(area, 0x10) + P32(area, 0x20)) >> 1);
    eax = wx_offset(P32(area, 0x08), P32(area, 0x10), area);
    ecx = WX_DY;
    if ((eax | ecx) != 0) {
        /* scale both up together while both stay in range */
        for (;;) {
            if ((int32_t)eax > INT32_MAX / 2 || (int32_t)eax < INT32_MIN / 2)
                break;                      /* eax would overflow: leave both */
            eax <<= 1;
            if ((int32_t)ecx > INT32_MAX / 2 || (int32_t)ecx < INT32_MIN / 2) {
                eax = (uint32_t)((int32_t)eax >> 1);
                break;
            }
            ecx <<= 1;
        }
    }
    angle = (uint16_t)UTIL_ATAN2((int32_t)eax, (int32_t)ecx);
    wx_rotate(angle);
    P16(area, 0x48) = angle;
    P32(area, 0x4c) = WX_Y << 1;
}

/* .text+0x160b -- how far in from one edge, 0..0x4000: the distance past half
 * the extent at `off`, over the fade width at +0x28. */
static uint16_t wx_edge(uint32_t eax, uint8_t *area, uint32_t off)
{
    uint32_t ebx;

    eax -= (uint32_t)((int32_t)P32(area, off) >> 1);
    if ((int32_t)eax < 0)
        return 0;
    ebx = P32(area, 0x28);
    if (eax >= ebx)
        return 0x4000;
    eax = (eax << 14) >> 1;
    ebx >>= 1;
    if (ebx == 0)
        wx_fault(EXCEPTION_INT_DIVIDE_BY_ZERO);
    return (uint16_t)(eax / ebx);
}

/* .text+0x1596 and +0x15ce -- the position's weight against an area: 0x4000
 * outside it or when it is off, less the deeper in. */
static void wx_weigh(uint8_t *area)
{
    uint32_t x, y;
    uint16_t a, b;

    wx_offset(P32(WX_POSITION, 0x04), P32(WX_POSITION, 0x0c), area);
    wx_rotate(P16(area, 0x48));
    x = WX_X;
    y = WX_Y;
    if ((int32_t)x < 0)
        x = 0u - x;
    if ((int32_t)y < 0)
        y = 0u - y;

    if (P16(area, 0) == 0) {
        P16(area, 0x50) = 0x4000;
        return;
    }
    a = wx_edge(x, area, 0x24);
    b = wx_edge(y, area, 0x4c);
    P16(area, 0x50) = (int16_t)a < (int16_t)b ? b : a;
}

/* ---- what the position sees ---------------------------------------------------------- */

/* .text+0x171d -- which cloud layer the position is in, and the layers as
 * the renderer wants them.  Layer bases below the ground (GLOBAL+0x20 >> 8)
 * read as 0.  If the second layer's middle is above ground + 200, the ground
 * line stays raised by 200 for the comparisons after it. */
static void wx_clouds(void)
{
    uint8_t *L = G + 0xe9a, *T = G + 0xef8;
    uint16_t alt = P16(WX_POSITION, 0x14), ax, bx, cx, dx;
    uint32_t ground;

    bx = P16(T, 0x10); cx = P16(T, 0xe); dx = P16(T, 0xc);
    if (alt > P16(L, 0x10) || alt < P16(L, 0x12)) {
        bx = P16(T, 4); cx = P16(T, 2); dx = P16(T, 0);
        if (alt > P16(L, 0) || alt < P16(L, 2)) {
            bx = P16(T, 0xa); cx = P16(T, 8); dx = P16(T, 6);
            if (alt > P16(L, 8) || alt < P16(L, 0xa))
                bx = cx = dx = 0;
        }
    }
    G16(0xf0a) = bx;
    G16(0xf0e) = cx;
    G16(0xf0c) = dx;

    G16(0xe72) = (uint16_t)(((uint32_t)P16(L, 0) + P16(L, 2)) >> 1);
    ground = G32(0x20) >> 8;
    ax = P16(L, 0);
    if (ax < (uint16_t)ground)
        ax = 0;
    G16(0xe78) = ax;
    G16(0xe74) = (uint16_t)(ax - P16(L, 2));
    G16(0xe74) += (uint16_t)(((uint32_t)P16(L, 0xe) * G16(0x1162)) >> 16);
    G16(0xe76) = P16(L, 4);

    ax = (uint16_t)(((uint32_t)P16(L, 8) + P16(L, 0xa)) >> 1);
    ground += 0xc8;
    if (!((int16_t)(uint16_t)ground < (int16_t)ax)) {
        ax = (uint16_t)ground;
        ground -= 0xc8;
    }
    G16(0xe6a) = ax;
    ax = P16(L, 8);
    if (ax < (uint16_t)ground)
        ax = 0;
    G16(0xe70) = ax;
    G16(0xe6c) = (uint16_t)(ax - P16(L, 0xa));
    G16(0xe6c) += (uint16_t)(((uint32_t)P16(L, 0xe) * G16(0x1162)) >> 16);
    G16(0xe6e) = P16(L, 0xc);

    ax = P16(L, 0x10);
    if (ax < (uint16_t)ground)
        ax = 0;
    G16(0xe7e) = ax;
    G16(0xe7a) = ax;
    G16(0xe7c) = P16(L, 0x14);
    G16(0xe80) = (uint16_t)(P16(L, 0x10) - P16(L, 0x12));
}

/* Speed (a word, of which the layer sets only the low byte) times one Q15
 * component, then times -264, keeping the middle sixteen bits each time. */
static uint16_t wx_wind_component(int16_t v)
{
    uint32_t p = (uint32_t)((int32_t)v * (int16_t)G16(0xe90));
    uint16_t ax = (uint16_t)(p >> 8);

    p = (uint32_t)((int32_t)(int16_t)ax * (int16_t)0xfef8);
    return (uint16_t)(p >> 8);
}

/* .text+0x1e18 / +0x1e5c -- the wind at the position: the surface wind below
 * GLOBAL+0xeee above ground (with the gust, +0x2a0, added to its direction),
 * else the layer the altitude falls in. */
static void wx_wind(void)
{
    uint16_t alt = (uint16_t)(P16(WX_POSITION, 0x14) - G16(0x21)), dir;
    uint8_t speed, extra, carry;
    int16_t s, c;

    if (alt < G16(0xeee)) {
        dir = G16(0xef2); speed = G8(0xef0); extra = G8(0xef4);
        WX_SURFACE++;
    } else {
        alt = (uint16_t)(alt + G16(0x21));
        if (alt >= G16(0xee4) && alt < G16(0xee2)) {
            dir = G16(0xee8); speed = G8(0xee6); extra = G8(0xeea);
        } else if (alt >= G16(0xed8) && alt < G16(0xed6)) {
            dir = G16(0xedc); speed = G8(0xeda); extra = G8(0xede);
        } else if (alt >= G16(0xecc) && alt < G16(0xeca)) {
            dir = G16(0xed0); speed = G8(0xece); extra = G8(0xed2);
        } else {
            dir = 0; speed = 0; extra = 0;
        }
    }
    G16(0xe92) = dir;
    G8(0xe90) = speed;
    carry = WX_SURFACE & 1;
    WX_SURFACE >>= 1;
    if (carry)
        dir = (uint16_t)(dir + G16(0x2a0));
    UTIL_SINCOS((int16_t)dir, &s, &c);
    G16(0xe94) = wx_wind_component(s);
    G16(0xe98) = (uint16_t)(G8(0x116a) << 8);
    G16(0xe96) = wx_wind_component(c);
    G8(0x1168) = extra;
}

/* .text+0x1128 -- the update, at WX_POSITION. */
static void wx_update(void)
{
    uint8_t *area;
    uint16_t w;

    if ((G16(0xf90) | G16(0x1078)) == 0) {
        G16(0xfe0) = 0x4000;
        G16(0x10c8) = 0x4000;
        wx_copy_global();
    } else {
        if (G16(0x1074) == 0)
            G16(0x1074) = 0x7fff;
        if (G16(0x115c) == 0)
            G16(0x115c) = 0x7fff;
        wx_shape(G + 0xf90);
        wx_shape(G + 0x1078);
        wx_weigh(G + 0xf90);
        wx_weigh(G + 0x1078);
        area = G + 0xf90;
        w = P16(area, 0x50);
        if ((int16_t)w > (int16_t)P16(G + 0x1078, 0x50)) {
            area = G + 0x1078;
            w = P16(area, 0x50);
        }
        wx_blend_area(area, w);
    }
    wx_wind();
    wx_clouds();
    wx_temperature();
}

/* ---- the frame callback ------------------------------------------------------------ */

/* 48-bit add of edx:ax into the value whose low word is at p. */
static void wx_add48(uint8_t *p, uint16_t lo, uint32_t hi)
{
    uint32_t s = (uint32_t)P16(p, 0) + lo;

    P16(p, 0) = (uint16_t)s;
    P32(p, 2) += hi + (s >> 16);
}

/* .text+0x2969 -- move an area's corners with its wind (+0x2c direction,
 * +0x2e speed), scaled by the sim rate (GLOBAL+0xc1a). */
static void wx_drift(uint8_t *area)
{
    uint8_t in[8] = { 0 }, out[8];
    uint32_t speed = P16(area, 0x2e);
    int64_t p;
    int16_t s, c;

    if (speed == 0)
        return;
    speed *= 0x1ff;
    speed = (uint32_t)(((uint64_t)speed * G16(0xc1a)) >> 8);
    UTIL_SINCOS((int16_t)P16(area, 0x2c), &s, &c);

    p = (int64_t)(int32_t)c * (int32_t)speed;
    wx_add48(area + 0x06, (uint16_t)((uint32_t)p >> 16), (uint32_t)((uint64_t)p >> 32));
    wx_add48(area + 0x16, (uint16_t)((uint32_t)p >> 16), (uint32_t)((uint64_t)p >> 32));

    p = (int64_t)(int32_t)s * (int32_t)speed;
    P16(in, 2) = (uint16_t)((uint32_t)p >> 16);
    P32(in, 4) = (uint32_t)((uint64_t)p >> 32);
    UTIL_TO_LONGITUDE(in, G + 0x560, out);
    wx_add48(area + 0x0e, P16(out, 2), P32(out, 4));
    wx_add48(area + 0x1e, P16(out, 2), P32(out, 4));
}

/* .text+0x1c51 -- let pressure block `i` wander: towards a random target
 * offset, one unit at a time, at a rate set by a random clock.  Indexed with
 * `i` as a byte offset into word arrays, so blocks 1 and 2 share bytes with
 * their neighbours; reproduced. */
static void FS98_STDCALL wx_wander(uint32_t i)
{
    uint8_t *block;
    uint16_t ax;
    uint32_t sum;

    if (i >= 3)
        return;
    block = WX_P_BLOCK(i);
    if (P16(block, 0x16) == 0)
        return;
    ax = P16(block, 0x14);
    if (ax != WX_P_LAST(i)) {
        WX_P_BASE(i) = ax;
        WX_P_TARGET(i) = 0;
        WX_P_OFFSET(i) = 0;
    }
    if (WX_P_OFFSET(i) == WX_P_TARGET(i)) {
        WX_P_TARGET(i) = (uint16_t)((int16_t)(uint16_t)UTIL_RAND() >> 8);
        WX_P_CLOCK(i) = 0;
    }
    ax = (uint16_t)(UTIL_RAND() & 0x1ff);
    ax = (uint16_t)(((uint32_t)ax * G16(0xc1a)) >> 8);
    sum = (uint32_t)WX_P_CLOCK(i) + ax;
    WX_P_CLOCK(i) = (uint16_t)sum;
    if (sum > 0xffff) {
        WX_P_CLOCK(i) = 0;
        ax = (uint16_t)(WX_P_TARGET(i) - WX_P_OFFSET(i));
        ax = (int16_t)ax < 0 ? 0xffff : 1;          /* sar ax,16; shl ax,1; add ax,1 */
        WX_P_OFFSET(i) = (uint16_t)(WX_P_OFFSET(i) + ax);
    }
    ax = (uint16_t)(WX_P_OFFSET(i) + WX_P_BASE(i));
    P16(block, 0x14) = ax;
    WX_P_LAST(i) = ax;
}

/* .text+0x28c9 -- CHAIN list 3. */
static void FS98_STDCALL wx_tick(uint32_t a, uint32_t b)
{
    void *object;

    (void)a;
    (void)b;
    G8(0x116a) = 0;
    object = PPTR(G, 0x1164);
    if (object != NULL)
        G3D_RELEASE(object);
    PPTR(G, 0x1164) = NULL;
    G8(0x1169) = 0;
    wx_drift(G + 0xf90);
    wx_drift(G + 0x1078);
    wx_wander(0);
    wx_wander(1);
    wx_wander(2);
}

/* ---- Linkage, the assembly half ------------------------------------------------------ */

/* .text+0x1020 */
static void wx_init(void)
{
    CHAIN_ADD(3, wx_tick);
    WX_P_BLOCK(0) = G + 0xf34;
    WX_P_BLOCK(1) = G + 0x101c;
    WX_P_BLOCK(2) = G + 0x1104;
    G16(0xf90) = 0;
    G16(0x1078) = 0;
}

/* .text+0x1089 */
static void wx_deinit(void)
{
    CHAIN_REMOVE(3, wx_tick);
}

/* .text+0x10b9 -- +0x18: the weather at another position, then back to the
 * aircraft. */
static void FS98_STDCALL wx_weather_at(void *position)
{
    WX_POSITION = position;
    wx_update();
    WX_POSITION = G + 0x560;
}

/* .text+0x10e5 -- +0x1c: the weather at the aircraft. */
static void wx_weather_here(void)
{
    WX_POSITION = G + 0x560;
    wx_update();
    MAIN_WEATHER();
}

/* .text+0x1fb3 -- +0x24: reset the global weather's blocks that ask for it --
 * +0xf12 the clouds, +0xf13 the winds, either the temperatures -- and each
 * area's when its +0x30/+0x32 flags are set. */
static void wx_reset(void)
{
    static const uint16_t winds[] = {
        0x2710, 0x7d0, 0xa, 0x4000, 0, 0, 0x7d0, 0x3e8, 0xf, 0x4000, 0, 0,
        0x3e8, 0x32, 0x5, 0x4000, 0, 0, 0x32, 0, 0, 0
    };
    static const uint16_t temps[] = {
        0x2ee0, 0xc900, 0xbb8, 0xfc00, 0x1f4, 0xc00, 0xb4, 0x1100, 0, 0, 0x3f50, 0
    };
    uint16_t ax;
    unsigned i;

    /* the winds' tables skip one word in three, which stays as it was */
#define WX_WINDS(base) \
    for (i = 0; i < sizeof winds / sizeof *winds; i++) \
        if (i % 6 != 5) G16((base) + 2 * i) = winds[i];

    if (G8(0xf12)) {
        G16(0xf20) = 0;
        G16(0xf24) = 0x7d0;
        G16(0xf26) = 0x5dc;
        ax = (uint16_t)UTIL_RAND();
        if (ax >= 0xd000)
            ax = 0xd000;
        G16(0xf28) = ax;
        G16(0xf30) = 0;
    }
    if (G8(0xf13)) {
        WX_WINDS(0xf4c)
    }
    if (G8(0xf12) | G8(0xf13))
        for (i = 0; i < sizeof temps / sizeof *temps; i++)
            G16(0xf34 + 2 * i) = temps[i];
    if (G16(0xfc0)) {
        G16(0x1008) = 0;
        G16(0x100c) = 0x7d0;
        G16(0x100e) = 0x5dc;
        G16(0x1010) = (uint16_t)((UTIL_RAND() & 0xff) << 8);
        G16(0x1018) = 0;
    }
    if (G16(0xfc2)) {
        WX_WINDS(0x1034)
    }
    if (G16(0x10a8)) {
        G16(0x10f0) = 0;
        G16(0x10f4) = 0x7d0;
        G16(0x10f6) = 0x5dc;
        G16(0x10f8) = (uint16_t)((UTIL_RAND() & 0xff) << 8);
        G16(0x1100) = 0;
    }
    if (G16(0x10aa)) {
        WX_WINDS(0x111c)
    }
#undef WX_WINDS
}

/* .text+0x249f -- a scenery effect's strength along the surface wind: the
 * record's strength, times the cosine of the angle between its direction (cx)
 * and the wind's, times the wind speed.  Zero when they are more than 90
 * degrees apart -- unless `type` is 3, which turns round and tries again.
 * After a sincos `type` has been overwritten by the strength's low byte, so a
 * second try asks that byte instead. */
static uint16_t wx_effect_strength(uint8_t type, uint16_t cx, uint16_t dx)
{
    uint16_t bx = (uint16_t)(G16(0xef2) - cx), ax;
    uint32_t p;
    int16_t s, c;

    if ((int16_t)bx > 0x3fff || (int16_t)bx < (int16_t)0xc001)
        goto opposed;
    for (;;) {
        UTIL_SINCOS((int16_t)bx, &s, &c);
        ax = dx;
        if (c >= 0)
            break;
        type = (uint8_t)ax;
opposed:
        if (type != 3)
            return 0;
        bx = (uint16_t)(bx + 0x8000);
    }
    p = (uint32_t)((int32_t)(int16_t)ax * c) << 1;
    p = (uint32_t)((int32_t)G8(0xef0) * (int16_t)(p >> 16)) << 1;
    return (uint16_t)(p >> 16);
}

/* .text+0x2406 -- +0x28: one 8-byte weather effect from scenery: type at +1,
 * direction at +2, strength at +4.  1 sets the thermal lift (GLOBAL+0x116a),
 * 2 and 3 the turbulence (+0x1168), 4 lift by the hour of the day. */
static void FS98_STDCALL wx_effect(const uint8_t *record)
{
    uint8_t type = record[1], dl, al;
    uint16_t cx = P16(record, 2), dx = P16(record, 4);

    switch (type) {
    case 1:
        G8(0x116a) = (uint8_t)wx_effect_strength(type, cx, dx);
        break;
    case 2:
    case 3:
        dl = (uint8_t)wx_effect_strength(type, cx, dx);
        if ((int8_t)dl < 0)
            dl = 0;
        if ((int8_t)dl > 0xa)
            dl = 0xa;
        G8(0x1168) = (uint8_t)(0x19 * dl);
        break;
    case 4:
        al = (uint8_t)(G8(0x238) - 6);
        if ((int8_t)al < 0)
            al = 0;
        else if (al > 6) {
            al = (uint8_t)(0x12 - G8(0x238));
            if ((int8_t)al < 0)
                al = 0;
        }
        G8(0x116a) = al;
        break;
    }
}

/* ---- Linkage, the C half (.text+0x2a40 on) -------------------------------------------
 *
 * A list of 0xc0-byte areas, each with four lists of effect records hanging
 * off +0xb0..+0xbc.  The first dword of everything is its `next`.  Records'
 * type is a word at +4 but is compared as the dword at +4, and types 4, 5 and
 * 6 are created with a non-zero word at +6 -- so the type-filtered walks never
 * find them, and nothing that frees or copies by type touches them. */

static int32_t wx_alloc(uint32_t size, void **out)
{
    return FS98_HOST_ALLOC(HOST)(1, size, out, 0);
}

static int32_t wx_free(void **pp)
{
    return FS98_HOST_FREE(HOST)(pp);
}

/* .text+0x31d0 -- the list a record type lives in. */
static void **wx_records(void *area, uint32_t type)
{
    switch (type) {
    case 1: return (void **)((uint8_t *)area + 0xb0);
    case 2: return (void **)((uint8_t *)area + 0xb4);
    case 3: return (void **)((uint8_t *)area + 0xb8);
    case 4: case 5: case 6: return (void **)((uint8_t *)area + 0xbc);
    default: return NULL;
    }
}

/* .text+0x2c30 -- +0x40: the first area, or the one after `area`. */
static void *FS98_STDCALL wx_area_next(void *cell, void *area)
{
    if (cell == NULL)
        return NULL;
    if (area != NULL)
        return PPTR(area, 0);
    return PPTR(cell, 0);
}

/* .text+0x2c50 -- +0x44: append an area.  Returns `cell` when the list was
 * empty or `area` null, else 0. */
static uint32_t FS98_STDCALL wx_area_append(void *cell, void *area)
{
    void *last;

    if (cell == NULL)
        return 0;
    if (area == NULL)
        return (uint32_t)(uintptr_t)cell;
    last = PPTR(cell, 0);
    if (last == NULL) {
        PPTR(cell, 0) = area;
        return (uint32_t)(uintptr_t)cell;
    }
    while (PPTR(last, 0) != NULL)
        last = PPTR(last, 0);
    PPTR(last, 0) = area;
    return 0;
}

/* .text+0x2c80 -- +0x48: how many areas, as a word. */
static uint32_t FS98_STDCALL wx_area_count(void *cell)
{
    uint16_t n = 0;
    void *a = wx_area_next(cell, NULL);

    while (a != NULL) {
        n++;
        a = wx_area_next(cell, a);
    }
    return n;
}

/* .text+0x3130 -- +0x58: the first record of `type`, or the next after
 * `record` of `type` (type 0: any). */
static void *FS98_STDCALL wx_record_next(void *area, void *record, uint32_t type)
{
    void **head, *e;

    if (area == NULL)
        return NULL;
    if (record != NULL) {
        for (e = PPTR(record, 0); e != NULL; e = PPTR(e, 0))
            if (type == 0 || type == P32(e, 4))
                return e;
        return NULL;
    }
    head = wx_records(area, type);
    if (head == NULL)
        return NULL;
    for (e = *head; e != NULL; e = PPTR(e, 0))
        if (type == P32(e, 4))
            return e;
    return NULL;
}

/* .text+0x3190 -- +0x5c: append a record to its type's list.  Returns the
 * list head's address when it was empty, `area` when `record` is null, else 0. */
static uint32_t FS98_STDCALL wx_record_append(void *area, void *record, uint32_t type)
{
    void **head, *last;

    if (area == NULL)
        return 0;
    if (record == NULL)
        return (uint32_t)(uintptr_t)area;
    head = wx_records(area, type);
    if (head == NULL)
        return 0;
    last = *head;
    if (last == NULL) {
        *head = record;
        return (uint32_t)(uintptr_t)head;
    }
    while (PPTR(last, 0) != NULL)
        last = PPTR(last, 0);
    PPTR(last, 0) = record;
    return 0;
}

/* .text+0x2cb0 -- +0x4c: a new record of `type` with its defaults, appended. */
static void *FS98_STDCALL wx_record_new(void *area, uint32_t type)
{
    void *r = NULL;

    if (area == NULL)
        return NULL;
    switch (type) {
    case 1:
        wx_alloc(0x14, &r);
        if (r == NULL)
            break;
        P16(r, 0x4) = 1; P16(r, 0x6) = 0; P16(r, 0x8) = 0xbea;
        P16(r, 0xa) = 0xd1b; P16(r, 0xc) = 3; P16(r, 0xe) = 0;
        break;
    case 2:
        wx_alloc(0x14, &r);
        if (r == NULL)
            break;
        P16(r, 0x4) = 2; P16(r, 0x6) = 0; P16(r, 0x8) = 0x1c9;
        P16(r, 0xa) = 0x2fa; P16(r, 0x10) = 0;
        break;
    case 3:
        wx_alloc(0xc, &r);
        if (r == NULL)
            break;
        P16(r, 0x4) = 3; P16(r, 0x6) = 0; P16(r, 0x8) = 0x2dc; P16(r, 0xa) = 0xfc1d;
        break;
    case 4:
        wx_alloc(0xc, &r);
        if (r == NULL)
            break;
        P16(r, 0x4) = 4; P16(r, 0x6) = 0x3f53; P16(r, 0x8) = 0;
        break;
    case 5:
        wx_alloc(0xc, &r);
        if (r == NULL)
            break;
        P16(r, 0x4) = 5; P16(r, 0x6) = 1; P16(r, 0x8) = 0xf3c7;
        break;
    case 6:
        wx_alloc(8, &r);
        if (r == NULL)
            break;
        P16(r, 0x4) = 6; P16(r, 0x6) = 0x7d00;
        break;
    }
    wx_record_append(area, r, type);
    return r;
}

/* .text+0x30c0 -- copy a record's body over another's, by the source's type,
 * keeping the destination's `next`. */
static void wx_record_copy(const void *src, void *dst)
{
    uint32_t size, i;
    void *next = PPTR(dst, 0);

    switch (P32(src, 4)) {
    case 1: case 2: size = 0x14; break;
    case 3: case 4: case 5: size = 0xc; break;
    case 6: size = 8; break;
    default: size = 0; break;
    }
    for (i = 0; i < size; i++)
        P8(dst, i) = P8(src, i);
    PPTR(dst, 0) = next;
}

/* .text+0x2e60 -- +0x50: free `record` from its type's list; or with no
 * record, every record of `type`; or with neither, every type.  A record
 * that is not in the list is freed anyway.  When freeing by type, the
 * previous pointer is lost after the first free that is not at the head, and
 * a second such free writes through null. */
static uint32_t FS98_STDCALL wx_record_free(void *area, void *record, uint32_t type)
{
    void **head, *first, *cur, *prev, *next;

    if (record != NULL) {
        head = wx_records(area, type);
        first = *head;
        if (record == first) {
            *wx_records(area, type) = wx_record_next(area, first, 0);
        } else if (first != NULL) {
            prev = cur = first;
            for (;;) {
                cur = wx_record_next(area, cur, 0);
                if (cur == record || cur == NULL)
                    break;
                prev = cur;
            }
            if (cur != NULL)
                PPTR(prev, 0) = wx_record_next(area, cur, 0);
        }
        wx_free(&record);
        return 0;
    }
    if (type != 0) {
        head = wx_records(area, type);
        if (head == NULL)
            return 0;
        cur = *head;
        prev = cur;
        while (cur != NULL) {
            next = wx_record_next(area, cur, 0);
            if (P32(cur, 4) == type) {
                if (cur == *head)
                    *head = next;
                else
                    PPTR(prev, 0) = next;
                wx_free(&cur);          /* nulls cur, so prev becomes null */
            }
            prev = cur;
            cur = next;
        }
        return 0;
    }
    for (type = 1; type <= 6; type++)
        wx_record_free(area, NULL, type);
    return 0;
}

/* .text+0x2fc0 -- +0x54: copy `record`, or every record of `type` in `src`,
 * or every type, into `dst`, first emptying the type in `dst` unless `keep`.
 * Returns 1 when the last allocation failed (or, copying by type, when there
 * was nothing to copy and `src` is null). */
static uint32_t FS98_STDCALL wx_records_copy(void *src, void *dst, void *record,
                                             uint32_t type, uint32_t keep)
{
    void *r = NULL, *cur;

    if (keep == 0)
        wx_record_free(dst, NULL, type);
    if (record != NULL) {
        r = wx_record_new(dst, type);
        if (r != NULL)
            wx_record_copy(record, r);
        return r == NULL;
    }
    if (type != 0) {
        cur = wx_record_next(src, NULL, type);
        if (cur == NULL)
            return src == NULL;
        do {
            r = wx_record_new(dst, type);
            if (r != NULL)
                wx_record_copy(cur, r);
            cur = wx_record_next(src, cur, type);
        } while (cur != NULL);
        return r == NULL;
    }
    for (type = 1; type <= 6; type++)
        wx_records_copy(src, dst, NULL, type, keep);
    return src == NULL;
}

/* .text+0x2a40 -- +0x2c: a new, empty list -- a four-byte head, unzeroed. */
static int32_t FS98_STDCALL wx_list_create(void **cell)
{
    return wx_alloc(4, cell);
}

/* .text+0x2a80 -- +0x34: a new area with its three default type 4-6 records,
 * appended. */
static void *FS98_STDCALL wx_area_new(void *cell)
{
    void *area = NULL;

    if (cell == NULL)
        return NULL;
    wx_alloc(0xc0, &area);
    if (area != NULL) {
        P16(area, 4) = 1;
        wx_record_new(area, 4);
        wx_record_new(area, 5);
        wx_record_new(area, 6);
    }
    wx_area_append(cell, area);
    return area;
}

/* .text+0x2b00 -- +0x38: unlink and free `area` (freed even when it is not in
 * the list), or with none, every area. */
static int32_t FS98_STDCALL wx_area_remove(void *cell, void *area)
{
    void *first = wx_area_next(cell, NULL), *cur, *prev;

    if (area != NULL) {
        if (area == first) {
            PPTR(cell, 0) = wx_area_next(cell, first);
        } else if (first != NULL) {
            prev = cur = first;
            for (;;) {
                cur = wx_area_next(cell, cur);
                if (area == cur || cur == NULL)
                    break;
                prev = cur;
            }
            if (cur != NULL)
                PPTR(prev, 0) = wx_area_next(cell, cur);
        }
        wx_record_free(area, NULL, 0);
        return wx_free(&area);
    }
    cur = first;
    while (cur != NULL) {
        void *next = wx_area_next(cell, cur);
        wx_record_free(cur, NULL, 0);
        wx_free(&cur);
        cur = next;
    }
    PPTR(cell, 0) = NULL;
    return 0;
}

/* .text+0x2a60 -- +0x30: free every area and the list head. */
static int32_t FS98_STDCALL wx_list_destroy(void **cell)
{
    wx_area_remove(*cell, NULL);
    return wx_free(cell);
}

/* .text+0x2bd0 -- +0x3c: make `dst` a copy of `src`, records and all, keeping
 * its place in the list. */
static uint32_t FS98_STDCALL wx_area_copy(void *src, void *dst)
{
    void *next = PPTR(dst, 0);
    uint32_t i;

    wx_record_free(dst, NULL, 0);
    for (i = 0; i < 0xc0; i++)
        P8(dst, i) = P8(src, i);
    PPTR(dst, 0) = next;
    P32(dst, 0xb0) = 0;
    P32(dst, 0xb4) = 0;
    P32(dst, 0xb8) = 0;
    P32(dst, 0xbc) = 0;
    return wx_records_copy(src, dst, NULL, 0, 1);
}

/* ---- the module ------------------------------------------------------------------- */

struct wx_image wx_image = {
    {
        { FS98_MOD_HOST,   0 },
        { FS98_MOD_GLOBAL, 0 },
        { FS98_MOD_CHAIN,  0 },
        { FS98_MOD_G3D,    0 },
        { FS98_MOD_MAIN,   0 },
        { FS98_MOD_UTIL,   0 },
        { 0, 0 }
    },
    {
        FS98_MOD_WEATHERM, wx_init, wx_deinit, 0, 0, 0,
        wx_weather_at, wx_weather_here, wx_temperature_now, wx_reset, wx_effect,
        wx_list_create, wx_list_destroy, wx_area_new, wx_area_remove, wx_area_copy,
        wx_area_next, wx_area_append, wx_area_count, wx_record_new, wx_record_free,
        wx_records_copy, wx_record_next, wx_record_append
    },
    {
#include "weatherm_data.inc"
    }
};

/* The two exports are the image's first 0x38 bytes and the 0x60 at +0x38,
 * where retail has them. */
__asm__(".globl _ImportTable\n\t.set _ImportTable, _wx_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _wx_image + 0x38");

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
