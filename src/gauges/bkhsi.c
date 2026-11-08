/* BKHSI.GAU -- BENDIX_KING.HSI: the HSI: its card, flags and deviation */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3c48u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3a58u
#define GAUGE_MOUSE 0x3a60u
#define GAUGE_MOUSE_ARGUMENT 0x3a44u


/* 0x1020: the card: the variable less the heading bug (+0x3950) and a constant */
static long double __stdcall bkhsi_1020(uint8_t *element)
{
    return ((long double)PF64(element, 0xc) - GD64(0x3950)) - GR64(0x2000);
}

/* 0x1040: a flag: bit 3 of the variable's low byte set the third constant, clear the second */
static long double __stdcall bkhsi_1040(uint8_t *element)
{
    return (P8(element, 0xc) & 8) ? GR64(0x2010) : GR64(0x2008);
}

/* 0x1060: the to/from flags: the variable at the fourth constant (equal, or unordered) clears bit
 * 30 of +0x31b0 and +0x3308, otherwise sets it; returns the third constant either way */
static long double __stdcall bkhsi_1060(uint8_t *element)
{
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2018), GAUGE_C3)) {
        GD32(0x31b0) &= 0xbfffffffu;
        GD32(0x3308) &= 0xbfffffffu;
    } else {
        GD32(0x31b0) |= 0x40000000u;
        GD32(0x3308) |= 0x40000000u;
    }
    return GR64(0x2010);
}

/* 0x10d0: the course pointer by the source (+0x3074): 1 the variable less the bug, 2 less the
 * bug and the fifth constant (each with bit 30 of +0x3718 cleared); otherwise hidden (the bit
 * set) at the third constant */
static long double __stdcall bkhsi_10d0(uint8_t *element)
{
    uint32_t source = GD32(0x3074);
    if (source == 1) {
        GD32(0x3718) &= 0xbfffffffu;
        return (long double)PF64(element, 0xc) - GD64(0x3950);
    }
    if (source == 2) {
        GD32(0x3718) &= 0xbfffffffu;
        return ((long double)PF64(element, 0xc) - GD64(0x3950)) - GR64(0x2020);
    }
    GD32(0x3718) |= 0x40000000u;
    return GR64(0x2010);
}

/* 0x1140, header +0x10: install: the variable at +0x3068 initialised (PANELS +0x98), then the
 * standard install */
static void __stdcall bkhsi_1140(uint32_t context)
{
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3068));
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
}
GAUGE_INITIALIZE(bkhsi_initialize)   /* 0x11a0 */

/* 0x11b0, header +0x18: update: the variable looked up (PANELS +0xa0), then the elements */
static void __stdcall bkhsi_11b0(void)
{
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3068));
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
}
GAUGE_GENERATE(bkhsi_generate)   /* 0x11e0 */
GAUGE_DRAW(bkhsi_draw)   /* 0x1200 */
GAUGE_KILL(bkhsi_kill)   /* 0x1210 */
GAUGE_MODULE(bkhsi_module)   /* 0x1240 */
GAUGE_QUERY(bkhsi_query)   /* 0x1250 */

GAUGE_ROUTINES({0x1020, (void *)bkhsi_1020}, {0x1040, (void *)bkhsi_1040}, {0x1060, (void *)bkhsi_1060}, {0x10d0, (void *)bkhsi_10d0}, {0x1140, (void *)bkhsi_1140}, {0x11a0, (void *)bkhsi_initialize}, {0x11b0, (void *)bkhsi_11b0}, {0x11e0, (void *)bkhsi_generate}, {0x1200, (void *)bkhsi_draw}, {0x1210, (void *)bkhsi_kill}, {0x1240, (void *)bkhsi_module}, {0x1250, (void *)bkhsi_query})
