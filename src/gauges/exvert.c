/* EXVERT.GAU -- EXTRA-300.VERTICAL-SPEED: the vertical speed needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3338u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x32a8u
#define GAUGE_MOUSE 0x32b0u
#define GAUGE_MOUSE_ARGUMENT 0x3294u


/* 0x1020: the needle: the value scaled, clamped */
static long double __stdcall exvert_1020(double value)
{
    return gauge_clamp((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}
GAUGE_INSTALL(exvert_install)   /* 0x1060 */
GAUGE_INITIALIZE(exvert_initialize)   /* 0x10b0 */
GAUGE_UPDATE(exvert_update)   /* 0x10c0 */
GAUGE_GENERATE(exvert_generate)   /* 0x10d0 */
GAUGE_DRAW(exvert_draw)   /* 0x10f0 */
GAUGE_KILL(exvert_kill)   /* 0x1100 */
GAUGE_MODULE(exvert_module)   /* 0x1130 */
GAUGE_QUERY(exvert_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)exvert_1020}, {0x1060, (void *)exvert_install}, {0x10b0, (void *)exvert_initialize}, {0x10c0, (void *)exvert_update}, {0x10d0, (void *)exvert_generate}, {0x10f0, (void *)exvert_draw}, {0x1100, (void *)exvert_kill}, {0x1130, (void *)exvert_module}, {0x1140, (void *)exvert_query})
