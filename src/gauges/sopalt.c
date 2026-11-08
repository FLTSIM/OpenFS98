/* SOPALT.GAU -- SOPWITH.ALTIMETER: the altimeter needle and the update of its element */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3258u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x31c8u
#define GAUGE_MOUSE 0x31d0u
#define GAUGE_MOUSE_ARGUMENT 0x31b4u


/* 0x1020: the needle: above the first constant (ordered) the second; otherwise the value
 * scaled and offset, negated */
static long double __stdcall sopalt_1020(double value)
{
    if (!GAUGE_TEST(value, GR64(0x2000), GAUGE_C0C3)) return GR64(0x2008);
    return gauge_fchs((long double)value * GR64(0x2010) - GR64(0x2018));
}
GAUGE_INSTALL(sopalt_install)   /* 0x1050 */
GAUGE_INITIALIZE(sopalt_initialize)   /* 0x10a0 */

/* 0x10b0, header +0x18: update: PANELS +0x48 with the element at +0x3090, not the list */
static void __stdcall sopalt_10b0(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x3090));
}
GAUGE_GENERATE(sopalt_generate)   /* 0x10c0 */
GAUGE_DRAW(sopalt_draw)   /* 0x10e0 */
GAUGE_KILL(sopalt_kill)   /* 0x10f0 */
GAUGE_MODULE(sopalt_module)   /* 0x1120 */
GAUGE_QUERY(sopalt_query)   /* 0x1130 */

GAUGE_ROUTINES({0x1020, (void *)sopalt_1020}, {0x1050, (void *)sopalt_install}, {0x10a0, (void *)sopalt_initialize}, {0x10b0, (void *)sopalt_10b0}, {0x10c0, (void *)sopalt_generate}, {0x10e0, (void *)sopalt_draw}, {0x10f0, (void *)sopalt_kill}, {0x1120, (void *)sopalt_module}, {0x1130, (void *)sopalt_query})
