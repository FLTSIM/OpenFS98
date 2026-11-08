/* CRGOIL.GAU -- CESSNA_182_RG.OIL: the oil needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x34b0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3370u
#define GAUGE_MOUSE 0x3378u
#define GAUGE_MOUSE_ARGUMENT 0x335cu


/* 0x1020: the needle: the value scaled and offset, limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall crgoil_1020(double value)
{
    return gauge_limit(((long double)value * GR64(0x2000)) - GR64(0x2008), GR64(0x2010), GR64(0x2018));
}

/* 0x1060: the needle: the value scaled, limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall crgoil_1060(double value)
{
    return gauge_limit((long double)value * GR64(0x2020), GR64(0x2028), GR64(0x2030));
}
GAUGE_INSTALL(crgoil_install)   /* 0x10a0 */
GAUGE_INITIALIZE(crgoil_initialize)   /* 0x10f0 */
GAUGE_UPDATE(crgoil_update)   /* 0x1100 */
GAUGE_GENERATE(crgoil_generate)   /* 0x1110 */
GAUGE_DRAW(crgoil_draw)   /* 0x1130 */
GAUGE_KILL(crgoil_kill)   /* 0x1140 */
GAUGE_MODULE(crgoil_module)   /* 0x1170 */
GAUGE_QUERY(crgoil_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)crgoil_1020}, {0x1060, (void *)crgoil_1060}, {0x10a0, (void *)crgoil_install}, {0x10f0, (void *)crgoil_initialize}, {0x1100, (void *)crgoil_update}, {0x1110, (void *)crgoil_generate}, {0x1130, (void *)crgoil_draw}, {0x1140, (void *)crgoil_kill}, {0x1170, (void *)crgoil_module}, {0x1180, (void *)crgoil_query})
