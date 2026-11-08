/* EXCMPS.GAU -- EXTRA-300.WISKEY-COMPASS: the compass card's callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3268u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x31d8u
#define GAUGE_MOUSE 0x31e0u
#define GAUGE_MOUSE_ARGUMENT 0x31c4u


/* 0x1020: the card: the first constant less the heading; below the second constant (or
 * unordered) or above the first (ordered), the second constant */
static long double __stdcall excmps_1020(uint8_t *element)
{
    long double card = (long double)GR64(0x2000) - PF64(element, 0xc);
    if (GAUGE_TEST(card, GR64(0x2008), GAUGE_C0) || !GAUGE_TEST(card, GR64(0x2000), GAUGE_C0C3)) return GR64(0x2008);
    return card;
}

/* 0x1060: a constant */
static long double __stdcall excmps_1060(uint8_t *element)
{
    (void)element;
    return GR64(0x2010);
}
GAUGE_INSTALL(excmps_install)   /* 0x1070 */
GAUGE_INITIALIZE(excmps_initialize)   /* 0x10c0 */
GAUGE_UPDATE(excmps_update)   /* 0x10d0 */
GAUGE_GENERATE(excmps_generate)   /* 0x10e0 */
GAUGE_DRAW(excmps_draw)   /* 0x1100 */
GAUGE_KILL(excmps_kill)   /* 0x1110 */
GAUGE_MODULE(excmps_module)   /* 0x1140 */
GAUGE_QUERY(excmps_query)   /* 0x1150 */

GAUGE_ROUTINES({0x1020, (void *)excmps_1020}, {0x1060, (void *)excmps_1060}, {0x1070, (void *)excmps_install}, {0x10c0, (void *)excmps_initialize}, {0x10d0, (void *)excmps_update}, {0x10e0, (void *)excmps_generate}, {0x1100, (void *)excmps_draw}, {0x1110, (void *)excmps_kill}, {0x1140, (void *)excmps_module}, {0x1150, (void *)excmps_query})
