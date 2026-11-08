/* CESSATT.GAU -- CESSNA_182.ATTITUDE: the attitude limit's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x33f0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3360u
#define GAUGE_MOUSE 0x3368u
#define GAUGE_MOUSE_ARGUMENT 0x334cu


/* 0x1020: the attitude's limit: the variable above the first constant (ordered) gives that
 * constant, below the second (or unordered) the second, otherwise the variable */
static long double __stdcall cessatt_1020(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C0C3)) return GR64(0x2000);
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2008), GAUGE_C0)) return GR64(0x2008);
    return PF64(element, 0xc);
}
GAUGE_INSTALL(cessatt_install)   /* 0x1060 */
GAUGE_INITIALIZE(cessatt_initialize)   /* 0x10b0 */
GAUGE_UPDATE(cessatt_update)   /* 0x10c0 */
GAUGE_GENERATE(cessatt_generate)   /* 0x10d0 */
GAUGE_DRAW(cessatt_draw)   /* 0x10f0 */
GAUGE_KILL(cessatt_kill)   /* 0x1100 */
GAUGE_MODULE(cessatt_module)   /* 0x1130 */
GAUGE_QUERY(cessatt_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)cessatt_1020}, {0x1060, (void *)cessatt_install}, {0x10b0, (void *)cessatt_initialize}, {0x10c0, (void *)cessatt_update}, {0x10d0, (void *)cessatt_generate}, {0x10f0, (void *)cessatt_draw}, {0x1100, (void *)cessatt_kill}, {0x1130, (void *)cessatt_module}, {0x1140, (void *)cessatt_query})
