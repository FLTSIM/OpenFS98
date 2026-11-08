/* SOPAS.GAU -- SOPWITH.AIRSPEED: the airspeed needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3378u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x32e8u
#define GAUGE_MOUSE 0x32f0u
#define GAUGE_MOUSE_ARGUMENT 0x32d4u


/* 0x1020: the needle: above the first constant (ordered) that constant; otherwise below the
 * second (or unordered) the second; otherwise the value */
static long double __stdcall sopas_1020(double value)
{
    long double needle = value;
    if (!GAUGE_TEST(needle, GR64(0x2000), GAUGE_C0C3)) return GR64(0x2000);
    if (GAUGE_TEST(needle, GR64(0x2008), GAUGE_C0)) return GR64(0x2008);
    return needle;
}
GAUGE_INSTALL(sopas_install)   /* 0x1060 */
GAUGE_INITIALIZE(sopas_initialize)   /* 0x10b0 */
GAUGE_UPDATE(sopas_update)   /* 0x10c0 */
GAUGE_GENERATE(sopas_generate)   /* 0x10d0 */
GAUGE_DRAW(sopas_draw)   /* 0x10f0 */
GAUGE_KILL(sopas_kill)   /* 0x1100 */
GAUGE_MODULE(sopas_module)   /* 0x1130 */
GAUGE_QUERY(sopas_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)sopas_1020}, {0x1060, (void *)sopas_install}, {0x10b0, (void *)sopas_initialize}, {0x10c0, (void *)sopas_update}, {0x10d0, (void *)sopas_generate}, {0x10f0, (void *)sopas_draw}, {0x1100, (void *)sopas_kill}, {0x1130, (void *)sopas_module}, {0x1140, (void *)sopas_query})
