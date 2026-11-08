/* 737GEAR.GAU -- 737-400.LANDING-GEAR: the gear lights, which install and update their variable */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3408u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3350u
#define GAUGE_MOUSE 0x3358u
#define GAUGE_MOUSE_ARGUMENT 0x333cu


/* 0x1020: the gear lights: unless the variable equals the gear's last state (0x308c), the
 * first constant; equal to it, the second; otherwise the third when the variable is exactly
 * 0x40cfff80:00000000 (16383), else the first */
static long double __stdcall g737gear_1020(uint8_t *element)
{
    if (!GAUGE_TEST(GD64(0x308c), PF64(element, 0xc), GAUGE_C3)) return GR64(0x2000);
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C3)) return GR64(0x2008);
    if (P32(element, 0xc) == 0 && P32(element, 0x10) == 0x40cfff80u) return GR64(0x2010);
    return GR64(0x2000);
}

/* 0x1070, header +0x10: install: the standard install, then the variable at +0x3080
 * initialised (PANELS +0x98) */
static void __stdcall g737gear_1070(uint32_t context)
{
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3080));
}
GAUGE_INITIALIZE(g737gear_initialize)   /* 0x10d0 */

/* 0x10e0, header +0x18: update: the variable looked up (PANELS +0xa0), then the elements */
static void __stdcall g737gear_10e0(void)
{
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3080));
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
}
GAUGE_GENERATE(g737gear_generate)   /* 0x1110 */
GAUGE_DRAW(g737gear_draw)   /* 0x1130 */
GAUGE_KILL(g737gear_kill)   /* 0x1140 */
GAUGE_MODULE(g737gear_module)   /* 0x1170 */
GAUGE_QUERY(g737gear_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)g737gear_1020}, {0x1070, (void *)g737gear_1070}, {0x10d0, (void *)g737gear_initialize}, {0x10e0, (void *)g737gear_10e0}, {0x1110, (void *)g737gear_generate}, {0x1130, (void *)g737gear_draw}, {0x1140, (void *)g737gear_kill}, {0x1170, (void *)g737gear_module}, {0x1180, (void *)g737gear_query})
