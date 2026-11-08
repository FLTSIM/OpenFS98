/* BELLFUEL.GAU -- BELL_206B.FUEL_QUANTITY: the fuel needle reads its own variable */
#include "gauge.h"

#define GAUGE_IMPORTS 0x32e0u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3250u
#define GAUGE_MOUSE 0x3258u
#define GAUGE_MOUSE_ARGUMENT 0x323cu


/* 0x1020: the needle: the variable at +0x30d0 looked up (PANELS +0xa0), its value (+0x30dc)
 * clamped: above the first constant (ordered) that constant, otherwise below the second (or
 * unordered) the second */
static long double __stdcall bellfuel_1020(double value)
{
    long double needle;
    (void)value;
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x30d0));
    needle = GD64(0x30dc);
    if (!GAUGE_TEST(needle, GR64(0x2000), GAUGE_C0C3)) return GR64(0x2000);
    if (GAUGE_TEST(needle, GR64(0x2008), GAUGE_C0)) return GR64(0x2008);
    return needle;
}
GAUGE_INSTALL(bellfuel_install)   /* 0x1070 */

/* 0x10c0, header +0x14: initialise: the elements, then the variable (PANELS +0x98) */
static void __stdcall bellfuel_10c0(void)
{
    GAUGE_PANELS_CALL(0x68, 1)(GD32(GAUGE_ELEMENTS));
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x30d0));
}
GAUGE_UPDATE(bellfuel_update)   /* 0x10f0 */
GAUGE_GENERATE(bellfuel_generate)   /* 0x1100 */
GAUGE_DRAW(bellfuel_draw)   /* 0x1120 */
GAUGE_KILL(bellfuel_kill)   /* 0x1130 */
GAUGE_MODULE(bellfuel_module)   /* 0x1160 */
GAUGE_QUERY(bellfuel_query)   /* 0x1170 */

GAUGE_ROUTINES({0x1020, (void *)bellfuel_1020}, {0x1070, (void *)bellfuel_install}, {0x10c0, (void *)bellfuel_10c0}, {0x10f0, (void *)bellfuel_update}, {0x1100, (void *)bellfuel_generate}, {0x1120, (void *)bellfuel_draw}, {0x1130, (void *)bellfuel_kill}, {0x1160, (void *)bellfuel_module}, {0x1170, (void *)bellfuel_query})
