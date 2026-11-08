/* CESSMPFF.GAU -- CESSNA_182.MANIFOLD_PRESSURE_FUEL_FLOW: the manifold pressure and fuel flow needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3590u
#define GAUGE_HEADER 0x3028u
#define GAUGE_ELEMENTS 0x3450u
#define GAUGE_MOUSE 0x3458u
#define GAUGE_MOUSE_ARGUMENT 0x343cu


/* 0x1020: the needle: the value scaled, limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall cessmpff_1020(double value)
{
    return gauge_limit((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}

/* 0x1060: the needle: the value limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall cessmpff_1060(double value)
{
    return gauge_limit(value, GR64(0x2018), GR64(0x2020));
}
GAUGE_INSTALL(cessmpff_install)   /* 0x10a0 */
GAUGE_INITIALIZE(cessmpff_initialize)   /* 0x10f0 */
GAUGE_UPDATE(cessmpff_update)   /* 0x1100 */
GAUGE_GENERATE(cessmpff_generate)   /* 0x1110 */
GAUGE_DRAW(cessmpff_draw)   /* 0x1130 */
GAUGE_KILL(cessmpff_kill)   /* 0x1140 */
GAUGE_MODULE(cessmpff_module)   /* 0x1170 */
GAUGE_QUERY(cessmpff_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)cessmpff_1020}, {0x1060, (void *)cessmpff_1060}, {0x10a0, (void *)cessmpff_install}, {0x10f0, (void *)cessmpff_initialize}, {0x1100, (void *)cessmpff_update}, {0x1110, (void *)cessmpff_generate}, {0x1130, (void *)cessmpff_draw}, {0x1140, (void *)cessmpff_kill}, {0x1170, (void *)cessmpff_module}, {0x1180, (void *)cessmpff_query})
