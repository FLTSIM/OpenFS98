/* CESSDG.GAU -- CESSNA_182.DIRECTIONAL_GYRO: the directional gyro card's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3510u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3320u
#define GAUGE_MOUSE 0x3328u
#define GAUGE_MOUSE_ARGUMENT 0x330cu


/* 0x1020: the card: the heading less the bug's offset (+0x3220) and a constant */
static long double __stdcall cessdg_1020(double value)
{
    return ((long double)value - GD64(0x3220)) - GR64(0x2000);
}
GAUGE_INSTALL(cessdg_install)   /* 0x1040 */
GAUGE_INITIALIZE(cessdg_initialize)   /* 0x1090 */
GAUGE_UPDATE(cessdg_update)   /* 0x10a0 */
GAUGE_GENERATE(cessdg_generate)   /* 0x10b0 */
GAUGE_DRAW(cessdg_draw)   /* 0x10d0 */
GAUGE_KILL(cessdg_kill)   /* 0x10e0 */
GAUGE_MODULE(cessdg_module)   /* 0x1110 */
GAUGE_QUERY(cessdg_query)   /* 0x1120 */

GAUGE_ROUTINES({0x1020, (void *)cessdg_1020}, {0x1040, (void *)cessdg_install}, {0x1090, (void *)cessdg_initialize}, {0x10a0, (void *)cessdg_update}, {0x10b0, (void *)cessdg_generate}, {0x10d0, (void *)cessdg_draw}, {0x10e0, (void *)cessdg_kill}, {0x1110, (void *)cessdg_module}, {0x1120, (void *)cessdg_query})
