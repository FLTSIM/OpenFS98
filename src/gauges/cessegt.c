/* CESSEGT.GAU -- CESSNA_182.EGT_CHT: the exhaust gas temperature and fuel flow needles */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3668u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3528u
#define GAUGE_MOUSE 0x3530u
#define GAUGE_MOUSE_ARGUMENT 0x3514u


/* 0x1020: the needle: the value limited */
static long double __stdcall cessegt_1020(double value)
{
    return gauge_limit(value, GR64(0x2000), GR64(0x2008));
}

/* 0x1060: the needle: the value limited */
static long double __stdcall cessegt_1060(double value)
{
    return gauge_limit(value, GR64(0x2010), GR64(0x2008));
}

/* 0x10a0: the needle: a constant less the value through two scales, limited */
static long double __stdcall cessegt_10a0(double value)
{
    return gauge_limit((long double)GR64(0x2028) - ((long double)value * GR64(0x2018)) * GR64(0x2020), GR64(0x2000), GR64(0x2030));
}
GAUGE_INSTALL(cessegt_install)   /* 0x10f0 */
GAUGE_INITIALIZE(cessegt_initialize)   /* 0x1140 */
GAUGE_UPDATE(cessegt_update)   /* 0x1150 */
GAUGE_GENERATE(cessegt_generate)   /* 0x1160 */
GAUGE_DRAW(cessegt_draw)   /* 0x1180 */
GAUGE_KILL(cessegt_kill)   /* 0x1190 */
GAUGE_MODULE(cessegt_module)   /* 0x11c0 */
GAUGE_QUERY(cessegt_query)   /* 0x11d0 */

GAUGE_ROUTINES({0x1020, (void *)cessegt_1020}, {0x1060, (void *)cessegt_1060}, {0x10a0, (void *)cessegt_10a0}, {0x10f0, (void *)cessegt_install}, {0x1140, (void *)cessegt_initialize}, {0x1150, (void *)cessegt_update}, {0x1160, (void *)cessegt_generate}, {0x1180, (void *)cessegt_draw}, {0x1190, (void *)cessegt_kill}, {0x11c0, (void *)cessegt_module}, {0x11d0, (void *)cessegt_query})
