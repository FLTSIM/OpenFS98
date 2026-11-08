/* SCHVERT.GAU -- SCHWEIZER.VERTICAL-SPEED: the vertical speed needle and the update of its element */
#include "gauge.h"

#define GAUGE_IMPORTS 0x32a8u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3218u
#define GAUGE_MOUSE 0x3220u
#define GAUGE_MOUSE_ARGUMENT 0x3204u


/* 0x1020: the needle: the value scaled, clamped */
static long double __stdcall schvert_1020(double value)
{
    return gauge_clamp((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}
GAUGE_INSTALL(schvert_install)   /* 0x1060 */
GAUGE_INITIALIZE(schvert_initialize)   /* 0x10b0 */

/* 0x10c0, header +0x18: update: PANELS +0x48 with the element at +0x30e0, not the list */
static void __stdcall schvert_10c0(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x30e0));
}
GAUGE_GENERATE(schvert_generate)   /* 0x10d0 */
GAUGE_DRAW(schvert_draw)   /* 0x10f0 */
GAUGE_KILL(schvert_kill)   /* 0x1100 */
GAUGE_MODULE(schvert_module)   /* 0x1130 */
GAUGE_QUERY(schvert_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)schvert_1020}, {0x1060, (void *)schvert_install}, {0x10b0, (void *)schvert_initialize}, {0x10c0, (void *)schvert_10c0}, {0x10d0, (void *)schvert_generate}, {0x10f0, (void *)schvert_draw}, {0x1100, (void *)schvert_kill}, {0x1130, (void *)schvert_module}, {0x1140, (void *)schvert_query})
