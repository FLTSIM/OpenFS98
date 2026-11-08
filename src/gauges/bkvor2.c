/* BKVOR2.GAU -- BENDIX_KING.VOR2: the VOR flag's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3580u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3440u
#define GAUGE_MOUSE 0x3448u
#define GAUGE_MOUSE_ARGUMENT 0x342cu


/* 0x1020: the flag: 1 the first constant, 2 the second, anything else the third */
static long double __stdcall bkvor2_1020(uint8_t *element)
{
    uint32_t state = P32(element, 0xc);
    if (state == 1) return GR64(0x2000);
    if (state == 2) return GR64(0x2008);
    return GR64(0x2010);
}
GAUGE_INSTALL(bkvor2_install)   /* 0x1050 */
GAUGE_INITIALIZE(bkvor2_initialize)   /* 0x10a0 */
GAUGE_UPDATE(bkvor2_update)   /* 0x10b0 */
GAUGE_GENERATE(bkvor2_generate)   /* 0x10c0 */
GAUGE_DRAW(bkvor2_draw)   /* 0x10e0 */
GAUGE_KILL(bkvor2_kill)   /* 0x10f0 */
GAUGE_MODULE(bkvor2_module)   /* 0x1120 */
GAUGE_QUERY(bkvor2_query)   /* 0x1130 */

GAUGE_ROUTINES({0x1020, (void *)bkvor2_1020}, {0x1050, (void *)bkvor2_install}, {0x10a0, (void *)bkvor2_initialize}, {0x10b0, (void *)bkvor2_update}, {0x10c0, (void *)bkvor2_generate}, {0x10e0, (void *)bkvor2_draw}, {0x10f0, (void *)bkvor2_kill}, {0x1120, (void *)bkvor2_module}, {0x1130, (void *)bkvor2_query})
