/* BELLASPD.GAU -- BELL_206B.AIRSPEED: the airspeed needle and the update of its card */
#include "gauge.h"

#define GAUGE_IMPORTS 0x33f0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3360u
#define GAUGE_MOUSE 0x3368u
#define GAUGE_MOUSE_ARGUMENT 0x334cu


/* 0x1020: the needle: the variable kept to [0, 160] in place (above the first constant,
 * ordered, 160; below the second or unordered, 0), then returned */
static long double __stdcall bellaspd_1020(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C0C3)) PF64(element, 0xc) = 160.0;
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2008), GAUGE_C0)) PF64(element, 0xc) = 0.0;
    return PF64(element, 0xc);
}
GAUGE_INSTALL(bellaspd_install)   /* 0x1060 */
GAUGE_INITIALIZE(bellaspd_initialize)   /* 0x10b0 */

/* 0x10c0, header +0x18: update: PANELS +0x48 with the card element (+0x3228) */
static void __stdcall bellaspd_10c0(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x3228));
}
GAUGE_GENERATE(bellaspd_generate)   /* 0x10d0 */
GAUGE_DRAW(bellaspd_draw)   /* 0x10f0 */
GAUGE_KILL(bellaspd_kill)   /* 0x1100 */
GAUGE_MODULE(bellaspd_module)   /* 0x1130 */
GAUGE_QUERY(bellaspd_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)bellaspd_1020}, {0x1060, (void *)bellaspd_install}, {0x10b0, (void *)bellaspd_initialize}, {0x10c0, (void *)bellaspd_10c0}, {0x10d0, (void *)bellaspd_generate}, {0x10f0, (void *)bellaspd_draw}, {0x1100, (void *)bellaspd_kill}, {0x1130, (void *)bellaspd_module}, {0x1140, (void *)bellaspd_query})
