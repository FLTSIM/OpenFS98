/* EXEGT.GAU -- EXTRA-300.EGT: the exhaust temperature needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3270u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x31e0u
#define GAUGE_MOUSE 0x31e8u
#define GAUGE_MOUSE_ARGUMENT 0x31ccu


/* 0x1020: the needle: the value clamped */
static long double __stdcall exegt_1020(double value)
{
    return gauge_clamp(value, GR64(0x2000), GR64(0x2008));
}
GAUGE_INSTALL(exegt_install)   /* 0x1060 */
GAUGE_INITIALIZE(exegt_initialize)   /* 0x10b0 */
GAUGE_UPDATE(exegt_update)   /* 0x10c0 */
GAUGE_GENERATE(exegt_generate)   /* 0x10d0 */
GAUGE_DRAW(exegt_draw)   /* 0x10f0 */
GAUGE_KILL(exegt_kill)   /* 0x1100 */
GAUGE_MODULE(exegt_module)   /* 0x1130 */
GAUGE_QUERY(exegt_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)exegt_1020}, {0x1060, (void *)exegt_install}, {0x10b0, (void *)exegt_initialize}, {0x10c0, (void *)exegt_update}, {0x10d0, (void *)exegt_generate}, {0x10f0, (void *)exegt_draw}, {0x1100, (void *)exegt_kill}, {0x1130, (void *)exegt_module}, {0x1140, (void *)exegt_query})
