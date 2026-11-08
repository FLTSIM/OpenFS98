/* BELLROTR.GAU -- BELL_206B.ROTOR: the rotor and engine needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3378u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x32e8u
#define GAUGE_MOUSE 0x32f0u
#define GAUGE_MOUSE_ARGUMENT 0x32d4u


/* 0x1020: the needle: the value scaled, clamped */
static long double __stdcall bellrotr_1020(double value)
{
    return gauge_clamp((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}

/* 0x1060: the needle: the value scaled, clamped */
static long double __stdcall bellrotr_1060(double value)
{
    return gauge_clamp((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}
GAUGE_INSTALL(bellrotr_install)   /* 0x10a0 */
GAUGE_INITIALIZE(bellrotr_initialize)   /* 0x10f0 */
GAUGE_UPDATE(bellrotr_update)   /* 0x1100 */
GAUGE_GENERATE(bellrotr_generate)   /* 0x1110 */
GAUGE_DRAW(bellrotr_draw)   /* 0x1130 */
GAUGE_KILL(bellrotr_kill)   /* 0x1140 */
GAUGE_MODULE(bellrotr_module)   /* 0x1170 */
GAUGE_QUERY(bellrotr_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)bellrotr_1020}, {0x1060, (void *)bellrotr_1060}, {0x10a0, (void *)bellrotr_install}, {0x10f0, (void *)bellrotr_initialize}, {0x1100, (void *)bellrotr_update}, {0x1110, (void *)bellrotr_generate}, {0x1130, (void *)bellrotr_draw}, {0x1140, (void *)bellrotr_kill}, {0x1170, (void *)bellrotr_module}, {0x1180, (void *)bellrotr_query})
