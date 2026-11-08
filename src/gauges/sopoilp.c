/* SOPOILP.GAU -- SOPWITH.OIL-PRESSURE: the oil pressure needle's steps */
#include "gauge.h"

#define GAUGE_IMPORTS 0x31d0u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3140u
#define GAUGE_MOUSE 0x3148u
#define GAUGE_MOUSE_ARGUMENT 0x312cu


/* 0x1020: the needle in steps: the variable through _ftol, up to 0x10 the first constant, to
 * 0xc00 the second, to 0x1d00 the third, to 0x2800 the fourth, beyond the fifth */
static long double __stdcall sopoilp_1020(uint8_t *element)
{
    int32_t pressure = (int32_t)(uint32_t)gauge_ftol(PF64(element, 0xc));
    if (pressure <= 0x10) return GR64(0x2000);
    if (pressure <= 0xc00) return GR64(0x2008);
    if (pressure <= 0x1d00) return GR64(0x2010);
    if (pressure <= 0x2800) return GR64(0x2018);
    return GR64(0x2020);
}
GAUGE_INSTALL(sopoilp_install)   /* 0x1080 */
GAUGE_INITIALIZE(sopoilp_initialize)   /* 0x10d0 */

/* 0x10e0, header +0x18: update: PANELS +0x48 with the element at +0x3060, not the list */
static void __stdcall sopoilp_10e0(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x3060));
}
GAUGE_GENERATE(sopoilp_generate)   /* 0x10f0 */
GAUGE_DRAW(sopoilp_draw)   /* 0x1110 */
GAUGE_KILL(sopoilp_kill)   /* 0x1120 */
GAUGE_MODULE(sopoilp_module)   /* 0x1150 */

GAUGE_QUERY(sopoilp_1160)   /* 0x1160 (the _ftol import thunk at 0x1180 follows it) */

GAUGE_ROUTINES({0x1020, (void *)sopoilp_1020}, {0x1080, (void *)sopoilp_install}, {0x10d0, (void *)sopoilp_initialize}, {0x10e0, (void *)sopoilp_10e0}, {0x10f0, (void *)sopoilp_generate}, {0x1110, (void *)sopoilp_draw}, {0x1120, (void *)sopoilp_kill}, {0x1150, (void *)sopoilp_module}, {0x1160, (void *)sopoilp_1160})
