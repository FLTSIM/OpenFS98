/* BELLVSPD.GAU -- BELL_206B.VERTICAL_SPEED: the vertical speed needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x33c8u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3338u
#define GAUGE_MOUSE 0x3340u
#define GAUGE_MOUSE_ARGUMENT 0x3324u


/* 0x1020: the needle: the value through three scales, clamped */
static long double __stdcall bellvspd_1020(double value)
{
    return gauge_clamp((((long double)value * GR64(0x2000)) * GR64(0x2008)) * GR64(0x2010), GR64(0x2018), GR64(0x2020));
}
GAUGE_INSTALL(bellvspd_install)   /* 0x1070 */
GAUGE_INITIALIZE(bellvspd_initialize)   /* 0x10c0 */
GAUGE_UPDATE(bellvspd_update)   /* 0x10d0 */
GAUGE_GENERATE(bellvspd_generate)   /* 0x10e0 */
GAUGE_DRAW(bellvspd_draw)   /* 0x1100 */
GAUGE_KILL(bellvspd_kill)   /* 0x1110 */
GAUGE_MODULE(bellvspd_module)   /* 0x1140 */
GAUGE_QUERY(bellvspd_query)   /* 0x1150 */

GAUGE_ROUTINES({0x1020, (void *)bellvspd_1020}, {0x1070, (void *)bellvspd_install}, {0x10c0, (void *)bellvspd_initialize}, {0x10d0, (void *)bellvspd_update}, {0x10e0, (void *)bellvspd_generate}, {0x1100, (void *)bellvspd_draw}, {0x1110, (void *)bellvspd_kill}, {0x1140, (void *)bellvspd_module}, {0x1150, (void *)bellvspd_query})
