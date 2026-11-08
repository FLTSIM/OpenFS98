/* CESSVAC.GAU -- CESSNA_182.VACUUM_AMPS: the vacuum and ammeter needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3520u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x33e0u
#define GAUGE_MOUSE 0x33e8u
#define GAUGE_MOUSE_ARGUMENT 0x33ccu


/* 0x1020: the needle: the value scaled, the low bound first, then the high */
static long double __stdcall cessvac_1020(double value)
{
    return gauge_clamp_low_first((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}

/* 0x1060: the needle: the value limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall cessvac_1060(double value)
{
    return gauge_limit(value, GR64(0x2018), GR64(0x2020));
}
GAUGE_INSTALL(cessvac_install)   /* 0x10a0 */
GAUGE_INITIALIZE(cessvac_initialize)   /* 0x10f0 */
GAUGE_UPDATE(cessvac_update)   /* 0x1100 */
GAUGE_GENERATE(cessvac_generate)   /* 0x1110 */
GAUGE_DRAW(cessvac_draw)   /* 0x1130 */
GAUGE_KILL(cessvac_kill)   /* 0x1140 */
GAUGE_MODULE(cessvac_module)   /* 0x1170 */
GAUGE_QUERY(cessvac_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)cessvac_1020}, {0x1060, (void *)cessvac_1060}, {0x10a0, (void *)cessvac_install}, {0x10f0, (void *)cessvac_initialize}, {0x1100, (void *)cessvac_update}, {0x1110, (void *)cessvac_generate}, {0x1130, (void *)cessvac_draw}, {0x1140, (void *)cessvac_kill}, {0x1170, (void *)cessvac_module}, {0x1180, (void *)cessvac_query})
