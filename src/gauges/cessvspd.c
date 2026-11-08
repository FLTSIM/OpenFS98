/* CESSVSPD.GAU -- CESSNA_182.VERTICAL_SPEED: the vertical speed needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3338u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x32a8u
#define GAUGE_MOUSE 0x32b0u
#define GAUGE_MOUSE_ARGUMENT 0x3294u


/* 0x1020: the needle: the value scaled, clamped */
static long double __stdcall cessvspd_1020(double value)
{
    return gauge_clamp((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}
GAUGE_INSTALL(cessvspd_install)   /* 0x1060 */
GAUGE_INITIALIZE(cessvspd_initialize)   /* 0x10b0 */
GAUGE_UPDATE(cessvspd_update)   /* 0x10c0 */
GAUGE_GENERATE(cessvspd_generate)   /* 0x10d0 */
GAUGE_DRAW(cessvspd_draw)   /* 0x10f0 */
GAUGE_KILL(cessvspd_kill)   /* 0x1100 */
GAUGE_MODULE(cessvspd_module)   /* 0x1130 */
GAUGE_QUERY(cessvspd_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)cessvspd_1020}, {0x1060, (void *)cessvspd_install}, {0x10b0, (void *)cessvspd_initialize}, {0x10c0, (void *)cessvspd_update}, {0x10d0, (void *)cessvspd_generate}, {0x10f0, (void *)cessvspd_draw}, {0x1100, (void *)cessvspd_kill}, {0x1130, (void *)cessvspd_module}, {0x1140, (void *)cessvspd_query})
