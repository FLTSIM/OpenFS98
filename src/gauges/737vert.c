/* 737VERT.GAU -- 737-400.VERTICAL-SPEED: the vertical speed needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3330u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x32a0u
#define GAUGE_MOUSE 0x32a8u
#define GAUGE_MOUSE_ARGUMENT 0x328cu


/* 0x1020: the needle: the value scaled, clamped */
static long double __stdcall g737vert_1020(double value)
{
    return gauge_clamp((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}
GAUGE_INSTALL(g737vert_install)   /* 0x1060 */
GAUGE_INITIALIZE(g737vert_initialize)   /* 0x10b0 */
GAUGE_UPDATE(g737vert_update)   /* 0x10c0 */
GAUGE_GENERATE(g737vert_generate)   /* 0x10d0 */
GAUGE_DRAW(g737vert_draw)   /* 0x10f0 */
GAUGE_KILL(g737vert_kill)   /* 0x1100 */
GAUGE_MODULE(g737vert_module)   /* 0x1130 */
GAUGE_QUERY(g737vert_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)g737vert_1020}, {0x1060, (void *)g737vert_install}, {0x10b0, (void *)g737vert_initialize}, {0x10c0, (void *)g737vert_update}, {0x10d0, (void *)g737vert_generate}, {0x10f0, (void *)g737vert_draw}, {0x1100, (void *)g737vert_kill}, {0x1130, (void *)g737vert_module}, {0x1140, (void *)g737vert_query})
