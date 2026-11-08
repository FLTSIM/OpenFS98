/* EXAMPS.GAU -- EXTRA-300.AMPS: the ammeter needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3270u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x31e0u
#define GAUGE_MOUSE 0x31e8u
#define GAUGE_MOUSE_ARGUMENT 0x31ccu


/* 0x1020: the needle: the variable scaled, then the low limit first (below or unordered) and
 * the high one (above, ordered) */
static long double __stdcall examps_1020(uint8_t *element)
{
    long double value = (long double)PF64(element, 0xc) * GR64(0x2000);
    if (GAUGE_TEST(value, GR64(0x2008), GAUGE_C0)) value = GR64(0x2008);
    if (!GAUGE_TEST(value, GR64(0x2010), GAUGE_C0C3)) value = GR64(0x2010);
    return value;
}
GAUGE_INSTALL(examps_install)   /* 0x1060 */
GAUGE_INITIALIZE(examps_initialize)   /* 0x10b0 */
GAUGE_UPDATE(examps_update)   /* 0x10c0 */
GAUGE_GENERATE(examps_generate)   /* 0x10d0 */
GAUGE_DRAW(examps_draw)   /* 0x10f0 */
GAUGE_KILL(examps_kill)   /* 0x1100 */
GAUGE_MODULE(examps_module)   /* 0x1130 */
GAUGE_QUERY(examps_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)examps_1020}, {0x1060, (void *)examps_install}, {0x10b0, (void *)examps_initialize}, {0x10c0, (void *)examps_update}, {0x10d0, (void *)examps_generate}, {0x10f0, (void *)examps_draw}, {0x1100, (void *)examps_kill}, {0x1130, (void *)examps_module}, {0x1140, (void *)examps_query})
