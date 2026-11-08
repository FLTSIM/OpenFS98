/* CESSSUC.GAU -- CESSNA_182.SUCTION: the suction needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3260u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x31d0u
#define GAUGE_MOUSE 0x31d8u
#define GAUGE_MOUSE_ARGUMENT 0x31bcu


/* 0x1020: the needle: the value clamped */
static long double __stdcall cesssuc_1020(double value)
{
    return gauge_clamp(value, GR64(0x2000), GR64(0x2008));
}
GAUGE_INSTALL(cesssuc_install)   /* 0x1060 */
GAUGE_INITIALIZE(cesssuc_initialize)   /* 0x10b0 */
GAUGE_UPDATE(cesssuc_update)   /* 0x10c0 */
GAUGE_GENERATE(cesssuc_generate)   /* 0x10d0 */
GAUGE_DRAW(cesssuc_draw)   /* 0x10f0 */
GAUGE_KILL(cesssuc_kill)   /* 0x1100 */
GAUGE_MODULE(cesssuc_module)   /* 0x1130 */
GAUGE_QUERY(cesssuc_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)cesssuc_1020}, {0x1060, (void *)cesssuc_install}, {0x10b0, (void *)cesssuc_initialize}, {0x10c0, (void *)cesssuc_update}, {0x10d0, (void *)cesssuc_generate}, {0x10f0, (void *)cesssuc_draw}, {0x1100, (void *)cesssuc_kill}, {0x1130, (void *)cesssuc_module}, {0x1140, (void *)cesssuc_query})
