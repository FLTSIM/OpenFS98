/* CRGFUEL.GAU -- CESSNA_182_RG.FUEL: the fuel needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3538u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x33f8u
#define GAUGE_MOUSE 0x3400u
#define GAUGE_MOUSE_ARGUMENT 0x33e4u


/* 0x1020: the needle: the value limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall crgfuel_1020(double value)
{
    return gauge_limit(value, GR64(0x2000), GR64(0x2008));
}

/* 0x1060: the needle: the value limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall crgfuel_1060(double value)
{
    return gauge_limit(value, GR64(0x2000), GR64(0x2008));
}
GAUGE_INSTALL(crgfuel_install)   /* 0x10a0 */
GAUGE_INITIALIZE(crgfuel_initialize)   /* 0x10f0 */
GAUGE_UPDATE(crgfuel_update)   /* 0x1100 */
GAUGE_GENERATE(crgfuel_generate)   /* 0x1110 */
GAUGE_DRAW(crgfuel_draw)   /* 0x1130 */
GAUGE_KILL(crgfuel_kill)   /* 0x1140 */
GAUGE_MODULE(crgfuel_module)   /* 0x1170 */
GAUGE_QUERY(crgfuel_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)crgfuel_1020}, {0x1060, (void *)crgfuel_1060}, {0x10a0, (void *)crgfuel_install}, {0x10f0, (void *)crgfuel_initialize}, {0x1100, (void *)crgfuel_update}, {0x1110, (void *)crgfuel_generate}, {0x1130, (void *)crgfuel_draw}, {0x1140, (void *)crgfuel_kill}, {0x1170, (void *)crgfuel_module}, {0x1180, (void *)crgfuel_query})
