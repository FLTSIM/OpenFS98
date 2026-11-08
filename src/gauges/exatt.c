/* EXATT.GAU -- EXTRA-300.ATTITUDE: the attitude limit's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x33f0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3360u
#define GAUGE_MOUSE 0x3368u
#define GAUGE_MOUSE_ARGUMENT 0x334cu


/* 0x1020: the attitude's limit: the variable above the first constant (ordered) gives that
 * constant, below the second (or unordered) the second, otherwise the variable */
static long double __stdcall exatt_1020(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C0C3)) return GR64(0x2000);
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2008), GAUGE_C0)) return GR64(0x2008);
    return PF64(element, 0xc);
}
GAUGE_INSTALL(exatt_install)   /* 0x1060 */
GAUGE_INITIALIZE(exatt_initialize)   /* 0x10b0 */
GAUGE_UPDATE(exatt_update)   /* 0x10c0 */
GAUGE_GENERATE(exatt_generate)   /* 0x10d0 */
GAUGE_DRAW(exatt_draw)   /* 0x10f0 */
GAUGE_KILL(exatt_kill)   /* 0x1100 */
GAUGE_MODULE(exatt_module)   /* 0x1130 */
GAUGE_QUERY(exatt_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)exatt_1020}, {0x1060, (void *)exatt_install}, {0x10b0, (void *)exatt_initialize}, {0x10c0, (void *)exatt_update}, {0x10d0, (void *)exatt_generate}, {0x10f0, (void *)exatt_draw}, {0x1100, (void *)exatt_kill}, {0x1130, (void *)exatt_module}, {0x1140, (void *)exatt_query})
