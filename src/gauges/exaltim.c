/* EXALTIM.GAU -- EXTRA-300.ALTIMETER: the altimeter's needles and window */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3620u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x34e0u
#define GAUGE_MOUSE 0x34e8u
#define GAUGE_MOUSE_ARGUMENT 0x34ccu


/* 0x1020: a needle: the value scaled, offset */
static long double __stdcall exaltim_1020(double value)
{
    return (long double)value * GR64(0x2000) - GR64(0x2008);
}

/* 0x1040: a needle: the value scaled, offset */
static long double __stdcall exaltim_1040(double value)
{
    return (long double)value * GR64(0x2010) - GR64(0x2008);
}

/* 0x1060: a needle: the value scaled, offset */
static long double __stdcall exaltim_1060(double value)
{
    return (long double)value * GR64(0x2018) - GR64(0x2008);
}

/* 0x1080: the Kollsman window: the variable less a constant, scaled */
static long double __stdcall exaltim_1080(uint8_t *element)
{
    return ((long double)PF64(element, 0xc) - GR64(0x2020)) * GR64(0x2028);
}
GAUGE_INSTALL(exaltim_install)   /* 0x10a0 */
GAUGE_INITIALIZE(exaltim_initialize)   /* 0x10f0 */
GAUGE_UPDATE(exaltim_update)   /* 0x1100 */
GAUGE_GENERATE(exaltim_generate)   /* 0x1110 */
GAUGE_DRAW(exaltim_draw)   /* 0x1130 */
GAUGE_KILL(exaltim_kill)   /* 0x1140 */
GAUGE_MODULE(exaltim_module)   /* 0x1170 */
GAUGE_QUERY(exaltim_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)exaltim_1020}, {0x1040, (void *)exaltim_1040}, {0x1060, (void *)exaltim_1060}, {0x1080, (void *)exaltim_1080}, {0x10a0, (void *)exaltim_install}, {0x10f0, (void *)exaltim_initialize}, {0x1100, (void *)exaltim_update}, {0x1110, (void *)exaltim_generate}, {0x1130, (void *)exaltim_draw}, {0x1140, (void *)exaltim_kill}, {0x1170, (void *)exaltim_module}, {0x1180, (void *)exaltim_query})
