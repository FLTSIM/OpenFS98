/* EXMPFF.GAU -- EXTRA-300.MANIFOLD-PRESSURE-FUEL-FLOW: the manifold pressure and fuel flow needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3528u
#define GAUGE_HEADER 0x3028u
#define GAUGE_ELEMENTS 0x33e8u
#define GAUGE_MOUSE 0x33f0u
#define GAUGE_MOUSE_ARGUMENT 0x33d4u


/* 0x1020: the needle: the variable, below the first constant (or unordered) that constant at
 * once, above the second (ordered) the second */
static long double __stdcall exmpff_1020(uint8_t *element)
{
    long double needle = PF64(element, 0xc);
    if (GAUGE_TEST(needle, GR64(0x2000), GAUGE_C0)) return GR64(0x2000);
    if (!GAUGE_TEST(needle, GR64(0x2008), GAUGE_C0C3)) return GR64(0x2008);
    return needle;
}

/* 0x1060: the needle: the variable through two scales, limited */
static long double __stdcall exmpff_1060(uint8_t *element)
{
    return gauge_limit(((long double)PF64(element, 0xc) * GR64(0x2010)) * GR64(0x2018), GR64(0x2020), GR64(0x2028));
}
GAUGE_INSTALL(exmpff_install)   /* 0x10b0 */
GAUGE_INITIALIZE(exmpff_initialize)   /* 0x1100 */
GAUGE_UPDATE(exmpff_update)   /* 0x1110 */
GAUGE_GENERATE(exmpff_generate)   /* 0x1120 */
GAUGE_DRAW(exmpff_draw)   /* 0x1140 */
GAUGE_KILL(exmpff_kill)   /* 0x1150 */
GAUGE_MODULE(exmpff_module)   /* 0x1180 */
GAUGE_QUERY(exmpff_query)   /* 0x1190 */

GAUGE_ROUTINES({0x1020, (void *)exmpff_1020}, {0x1060, (void *)exmpff_1060}, {0x10b0, (void *)exmpff_install}, {0x1100, (void *)exmpff_initialize}, {0x1110, (void *)exmpff_update}, {0x1120, (void *)exmpff_generate}, {0x1140, (void *)exmpff_draw}, {0x1150, (void *)exmpff_kill}, {0x1180, (void *)exmpff_module}, {0x1190, (void *)exmpff_query})
