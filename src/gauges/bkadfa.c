/* BKADFA.GAU -- BENDIX_KING.ADF.MODEL_A: the ADF card's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3510u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x33d0u
#define GAUGE_MOUSE 0x33d8u
#define GAUGE_MOUSE_ARGUMENT 0x33bcu


/* 0x1020: the card: the value offset */
static long double __stdcall bkadfa_1020(double value)
{
    return (long double)value - GR64(0x2000);
}
GAUGE_INSTALL(bkadfa_install)   /* 0x1030 */
GAUGE_INITIALIZE(bkadfa_initialize)   /* 0x1080 */
GAUGE_UPDATE(bkadfa_update)   /* 0x1090 */
GAUGE_GENERATE(bkadfa_generate)   /* 0x10a0 */
GAUGE_DRAW(bkadfa_draw)   /* 0x10c0 */
GAUGE_KILL(bkadfa_kill)   /* 0x10d0 */
GAUGE_MODULE(bkadfa_module)   /* 0x1100 */
GAUGE_QUERY(bkadfa_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)bkadfa_1020}, {0x1030, (void *)bkadfa_install}, {0x1080, (void *)bkadfa_initialize}, {0x1090, (void *)bkadfa_update}, {0x10a0, (void *)bkadfa_generate}, {0x10c0, (void *)bkadfa_draw}, {0x10d0, (void *)bkadfa_kill}, {0x1100, (void *)bkadfa_module}, {0x1110, (void *)bkadfa_query})
