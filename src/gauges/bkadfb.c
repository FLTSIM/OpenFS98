/* BKADFB.GAU -- BENDIX_KING.ADF.MODEL_B: the ADF card's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3488u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3348u
#define GAUGE_MOUSE 0x3350u
#define GAUGE_MOUSE_ARGUMENT 0x3334u


/* 0x1020: the card: the value offset */
static long double __stdcall bkadfb_1020(double value)
{
    return (long double)value - GR64(0x2000);
}
GAUGE_INSTALL(bkadfb_install)   /* 0x1030 */
GAUGE_INITIALIZE(bkadfb_initialize)   /* 0x1080 */
GAUGE_UPDATE(bkadfb_update)   /* 0x1090 */
GAUGE_GENERATE(bkadfb_generate)   /* 0x10a0 */
GAUGE_DRAW(bkadfb_draw)   /* 0x10c0 */
GAUGE_KILL(bkadfb_kill)   /* 0x10d0 */
GAUGE_MODULE(bkadfb_module)   /* 0x1100 */
GAUGE_QUERY(bkadfb_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)bkadfb_1020}, {0x1030, (void *)bkadfb_install}, {0x1080, (void *)bkadfb_initialize}, {0x1090, (void *)bkadfb_update}, {0x10a0, (void *)bkadfb_generate}, {0x10c0, (void *)bkadfb_draw}, {0x10d0, (void *)bkadfb_kill}, {0x1100, (void *)bkadfb_module}, {0x1110, (void *)bkadfb_query})
