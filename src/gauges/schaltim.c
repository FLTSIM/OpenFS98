/* SCHALTIM.GAU -- SCHWEIZER.ALTIMETER: the altimeter's window and needles */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3620u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x34e0u
#define GAUGE_MOUSE 0x34e8u
#define GAUGE_MOUSE_ARGUMENT 0x34ccu


/* 0x1020: the Kollsman window: the variable less a constant, scaled */
static long double __stdcall schaltim_1020(uint8_t *element)
{
    return ((long double)PF64(element, 0xc) - GR64(0x2000)) * GR64(0x2008);
}

/* 0x1040: a needle: the value scaled, offset */
static long double __stdcall schaltim_1040(double value)
{
    return (long double)value * GR64(0x2010) - GR64(0x2018);
}

/* 0x1060: a needle: the value scaled, offset */
static long double __stdcall schaltim_1060(double value)
{
    return (long double)value * GR64(0x2020) - GR64(0x2018);
}

/* 0x1080: a needle: the value scaled, offset */
static long double __stdcall schaltim_1080(double value)
{
    return (long double)value * GR64(0x2028) - GR64(0x2018);
}
GAUGE_INSTALL(schaltim_install)   /* 0x10a0 */
GAUGE_INITIALIZE(schaltim_initialize)   /* 0x10f0 */
GAUGE_UPDATE(schaltim_update)   /* 0x1100 */
GAUGE_GENERATE(schaltim_generate)   /* 0x1110 */
GAUGE_DRAW(schaltim_draw)   /* 0x1130 */
GAUGE_KILL(schaltim_kill)   /* 0x1140 */
GAUGE_MODULE(schaltim_module)   /* 0x1170 */
GAUGE_QUERY(schaltim_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)schaltim_1020}, {0x1040, (void *)schaltim_1040}, {0x1060, (void *)schaltim_1060}, {0x1080, (void *)schaltim_1080}, {0x10a0, (void *)schaltim_install}, {0x10f0, (void *)schaltim_initialize}, {0x1100, (void *)schaltim_update}, {0x1110, (void *)schaltim_generate}, {0x1130, (void *)schaltim_draw}, {0x1140, (void *)schaltim_kill}, {0x1170, (void *)schaltim_module}, {0x1180, (void *)schaltim_query})
