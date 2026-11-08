/* 737ALTIM.GAU -- 737-400.ALTIMETER: the altimeter: its drum and the altitude and setting as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3790u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3590u
#define GAUGE_MOUSE 0x3598u
#define GAUGE_MOUSE_ARGUMENT 0x357cu


/* 0x1020: the drum: the variable scaled, offset */
static long double __stdcall g737altim_1020(uint8_t *element)
{
    return (long double)PF64(element, 0xc) * GR64(0x2000) - GR64(0x2008);
}

/* 0x1040: the altitude as text: the altitude (+0x3308) kept below 99999 in place (above the
 * third constant, ordered), printed through _ftol into the element's buffer (+0x3378) with the
 * format at +0x377c; returns the value */
static long double __stdcall g737altim_1040(double value)
{
    if (!GAUGE_TEST(GD64(0x3308), GR64(0x2010), GAUGE_C0C3)) GD64(0x3308) = 99999.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3378), (const char *)GAUGE_D(0x377c), (uint32_t)gauge_ftol(GD64(0x3308)));
    return value;
}

/* 0x1090: the setting as text: the value as whole and hundredths (the fraction times the
 * fourth constant, each through _ftol) into the element's buffer (+0x34b8) with the format at
 * +0x3784; returns the value */
static long double __stdcall g737altim_1090(double value)
{
    uint32_t whole = (uint32_t)gauge_ftol(value);
    uint32_t part = (uint32_t)gauge_ftol(((long double)value - (long double)(int32_t)whole) * GR64(0x2018));
    wsprintfA((char *)(uintptr_t)GD32(0x34b8), (const char *)GAUGE_D(0x3784), whole, part);
    return value;
}
GAUGE_INSTALL(g737altim_install)   /* 0x10e0 */
GAUGE_INITIALIZE(g737altim_initialize)   /* 0x1130 */
GAUGE_UPDATE(g737altim_update)   /* 0x1140 */
GAUGE_GENERATE(g737altim_generate)   /* 0x1150 */
GAUGE_DRAW(g737altim_draw)   /* 0x1170 */
GAUGE_KILL(g737altim_kill)   /* 0x1180 */
GAUGE_MODULE(g737altim_module)   /* 0x11b0 */

GAUGE_QUERY(g737altim_11c0)   /* 0x11c0 (the _ftol import thunk at 0x11e0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737altim_1020}, {0x1040, (void *)g737altim_1040}, {0x1090, (void *)g737altim_1090}, {0x10e0, (void *)g737altim_install}, {0x1130, (void *)g737altim_initialize}, {0x1140, (void *)g737altim_update}, {0x1150, (void *)g737altim_generate}, {0x1170, (void *)g737altim_draw}, {0x1180, (void *)g737altim_kill}, {0x11b0, (void *)g737altim_module}, {0x11c0, (void *)g737altim_11c0})
