/* LRFLAPS.GAU -- LEAR_45.FLAPS: the flap indicator, which installs its mouse rectangles when initialised */
#include "gauge.h"

#define GAUGE_IMPORTS 0x32a0u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3160u
#define GAUGE_MOUSE 0x0u
#define GAUGE_MOUSE_ARGUMENT 0x0u


/* 0x1020: the indicator: the variable scaled (and stored, then replaced in place): up to the
 * second constant (or unordered) times the third; up to the fourth (or unordered) less the
 * second, scaled and offset; beyond, less the fourth, scaled and offset; returns it */
static long double __stdcall lrflaps_1020(uint8_t *element)
{
    long double flaps = (long double)PF64(element, 0xc) * GR64(0x2000);
    uint16_t first = gauge_fcom(flaps, GR64(0x2008));
    gauge_fst(element + 0xc, flaps);     /* the fst (its rounding's flags) */
    if (first & GAUGE_C0C3) PF64(element, 0xc) = (double)(flaps * GR64(0x2010));
    else if (GAUGE_TEST(flaps, GR64(0x2018), GAUGE_C0C3)) PF64(element, 0xc) = (double)((flaps - GR64(0x2008)) * GR64(0x2020) - GR64(0x2028));
    else PF64(element, 0xc) = (double)((flaps - GR64(0x2018)) * GR64(0x2030) - GR64(0x2038));
    return PF64(element, 0xc);
}

/* 0x1090, header +0x10: install: the elements only */
static void __stdcall lrflaps_1090(uint32_t context)
{
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
}

/* 0x10b0, header +0x14: initialise: the elements, then the mouse rectangles (+0x3168, with
 * +4 of the pointer at +0x314c) */
static void __stdcall lrflaps_10b0(void)
{
    GAUGE_PANELS_CALL(0x68, 1)(GD32(GAUGE_ELEMENTS));
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(0x3168), GAUGE_ADDRESS(GAUGE_HEADER), GD32(0x314c) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(0x3168), GAUGE_ADDRESS(GAUGE_HEADER));
}
GAUGE_UPDATE(lrflaps_update)   /* 0x1100 */
GAUGE_GENERATE(lrflaps_generate)   /* 0x1110 */
GAUGE_DRAW(lrflaps_draw)   /* 0x1130 */

/* 0x1140, header +0x24: kill: the mouse rectangles (+0x3168), then the elements */
static void __stdcall lrflaps_1140(void)
{
    GAUGE_PANELS_CALL(0x88, 2)(GAUGE_ADDRESS(0x3168), GAUGE_ADDRESS(GAUGE_HEADER));
    GAUGE_PANELS_CALL(0x7c, 1)(GD32(GAUGE_ELEMENTS));
}
GAUGE_MODULE(lrflaps_module)   /* 0x1170 */
GAUGE_QUERY(lrflaps_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)lrflaps_1020}, {0x1090, (void *)lrflaps_1090}, {0x10b0, (void *)lrflaps_10b0}, {0x1100, (void *)lrflaps_update}, {0x1110, (void *)lrflaps_generate}, {0x1130, (void *)lrflaps_draw}, {0x1140, (void *)lrflaps_1140}, {0x1170, (void *)lrflaps_module}, {0x1180, (void *)lrflaps_query})
