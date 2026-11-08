/* CESSALT.GAU -- CESSNA_182.ALTIMETER: the altimeter's window and needles */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3620u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x34e0u
#define GAUGE_MOUSE 0x34e8u
#define GAUGE_MOUSE_ARGUMENT 0x34ccu


/* 0x1020: the Kollsman window: the variable less a constant, scaled */
static long double __stdcall cessalt_1020(uint8_t *element)
{
    return ((long double)PF64(element, 0xc) - GR64(0x2000)) * GR64(0x2008);
}

/* 0x1040: a needle: the value scaled, offset */
static long double __stdcall cessalt_1040(double value)
{
    return (long double)value * GR64(0x2010) - GR64(0x2018);
}

/* 0x1060: a needle: the value scaled, offset */
static long double __stdcall cessalt_1060(double value)
{
    return (long double)value * GR64(0x2020) - GR64(0x2018);
}

/* 0x1080: a needle: the value scaled, offset */
static long double __stdcall cessalt_1080(double value)
{
    return (long double)value * GR64(0x2028) - GR64(0x2018);
}
GAUGE_INSTALL(cessalt_install)   /* 0x10a0 */
GAUGE_INITIALIZE(cessalt_initialize)   /* 0x10f0 */
GAUGE_UPDATE(cessalt_update)   /* 0x1100 */
GAUGE_GENERATE(cessalt_generate)   /* 0x1110 */
GAUGE_DRAW(cessalt_draw)   /* 0x1130 */
GAUGE_KILL(cessalt_kill)   /* 0x1140 */
GAUGE_MODULE(cessalt_module)   /* 0x1170 */
GAUGE_QUERY(cessalt_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)cessalt_1020}, {0x1040, (void *)cessalt_1040}, {0x1060, (void *)cessalt_1060}, {0x1080, (void *)cessalt_1080}, {0x10a0, (void *)cessalt_install}, {0x10f0, (void *)cessalt_initialize}, {0x1100, (void *)cessalt_update}, {0x1110, (void *)cessalt_generate}, {0x1130, (void *)cessalt_draw}, {0x1140, (void *)cessalt_kill}, {0x1170, (void *)cessalt_module}, {0x1180, (void *)cessalt_query})
