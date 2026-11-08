/* BKVOR1.GAU -- BENDIX_KING.VOR1: the VOR flag's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x36e8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x35a8u
#define GAUGE_MOUSE 0x35b0u
#define GAUGE_MOUSE_ARGUMENT 0x3594u


/* 0x1020: the flag: 1 the first constant, 2 the second, anything else the third */
static long double __stdcall bkvor1_1020(uint8_t *element)
{
    uint32_t state = P32(element, 0xc);
    if (state == 1) return GR64(0x2000);
    if (state == 2) return GR64(0x2008);
    return GR64(0x2010);
}
GAUGE_INSTALL(bkvor1_install)   /* 0x1050 */
GAUGE_INITIALIZE(bkvor1_initialize)   /* 0x10a0 */
GAUGE_UPDATE(bkvor1_update)   /* 0x10b0 */
GAUGE_GENERATE(bkvor1_generate)   /* 0x10c0 */
GAUGE_DRAW(bkvor1_draw)   /* 0x10e0 */
GAUGE_KILL(bkvor1_kill)   /* 0x10f0 */
GAUGE_MODULE(bkvor1_module)   /* 0x1120 */
GAUGE_QUERY(bkvor1_query)   /* 0x1130 */

GAUGE_ROUTINES({0x1020, (void *)bkvor1_1020}, {0x1050, (void *)bkvor1_install}, {0x10a0, (void *)bkvor1_initialize}, {0x10b0, (void *)bkvor1_update}, {0x10c0, (void *)bkvor1_generate}, {0x10e0, (void *)bkvor1_draw}, {0x10f0, (void *)bkvor1_kill}, {0x1120, (void *)bkvor1_module}, {0x1130, (void *)bkvor1_query})
