/* SOPRPM.GAU -- SOPWITH.RPM-INDICATOR: the tachometer, reading its variable */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3368u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x32d8u
#define GAUGE_MOUSE 0x32e0u
#define GAUGE_MOUSE_ARGUMENT 0x32c4u


/* 0x1020: the needle: the variable (+0x3060) looked up (PANELS +0xa0); the element's value
 * times its value (+0x306c) and a scale, above the second constant (ordered) that constant */
static long double __stdcall soprpm_1020(uint8_t *element)
{
    long double needle;
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3060));
    needle = ((long double)PF64(element, 0xc) * GD64(0x306c)) * GR64(0x2000);
    if (!GAUGE_TEST(needle, GR64(0x2008), GAUGE_C0C3)) needle = GR64(0x2008);
    return needle;
}

/* 0x1060, header +0x10: install: the variables (+0x3060, +0x3060) initialised
 * (PANELS +0x98), then the standard install */
static void __stdcall soprpm_1060(uint32_t context)
{
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3060));
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3060));
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
}
GAUGE_INITIALIZE(soprpm_initialize)   /* 0x10d0 */

/* 0x10e0, header +0x18: update: PANELS +0x48 with the element at +0x31a0, not the list */
static void __stdcall soprpm_10e0(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x31a0));
}
GAUGE_GENERATE(soprpm_generate)   /* 0x10f0 */
GAUGE_DRAW(soprpm_draw)   /* 0x1110 */
GAUGE_KILL(soprpm_kill)   /* 0x1120 */
GAUGE_MODULE(soprpm_module)   /* 0x1150 */
GAUGE_QUERY(soprpm_query)   /* 0x1160 */

GAUGE_ROUTINES({0x1020, (void *)soprpm_1020}, {0x1060, (void *)soprpm_1060}, {0x10d0, (void *)soprpm_initialize}, {0x10e0, (void *)soprpm_10e0}, {0x10f0, (void *)soprpm_generate}, {0x1110, (void *)soprpm_draw}, {0x1120, (void *)soprpm_kill}, {0x1150, (void *)soprpm_module}, {0x1160, (void *)soprpm_query})
