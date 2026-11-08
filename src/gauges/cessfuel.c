/* CESSFUEL.GAU -- CESSNA_182.FUEL_QUANTITY: the fuel needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x35c8u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3488u
#define GAUGE_MOUSE 0x3490u
#define GAUGE_MOUSE_ARGUMENT 0x3474u


/* 0x1020: the needle: the value limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall cessfuel_1020(double value)
{
    return gauge_limit(value, GR64(0x2000), GR64(0x2008));
}

/* 0x1060: the needle: the value limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall cessfuel_1060(double value)
{
    return gauge_limit(value, GR64(0x2000), GR64(0x2008));
}
GAUGE_INSTALL(cessfuel_install)   /* 0x10a0 */
GAUGE_INITIALIZE(cessfuel_initialize)   /* 0x10f0 */
GAUGE_UPDATE(cessfuel_update)   /* 0x1100 */
GAUGE_GENERATE(cessfuel_generate)   /* 0x1110 */
GAUGE_DRAW(cessfuel_draw)   /* 0x1130 */
GAUGE_KILL(cessfuel_kill)   /* 0x1140 */
GAUGE_MODULE(cessfuel_module)   /* 0x1170 */
GAUGE_QUERY(cessfuel_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)cessfuel_1020}, {0x1060, (void *)cessfuel_1060}, {0x10a0, (void *)cessfuel_install}, {0x10f0, (void *)cessfuel_initialize}, {0x1100, (void *)cessfuel_update}, {0x1110, (void *)cessfuel_generate}, {0x1130, (void *)cessfuel_draw}, {0x1140, (void *)cessfuel_kill}, {0x1170, (void *)cessfuel_module}, {0x1180, (void *)cessfuel_query})
