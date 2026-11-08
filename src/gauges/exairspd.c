/* EXAIRSPD.GAU -- EXTRA-300.AIRSPEED: the airspeed needle and the airspeed as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3448u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x33b0u
#define GAUGE_MOUSE 0x33b8u
#define GAUGE_MOUSE_ARGUMENT 0x339cu


/* 0x1020: the needle: the value limited */
static long double __stdcall exairspd_1020(double value)
{
    return gauge_limit(value, GR64(0x2000), GR64(0x2008));
}

/* 0x1060: the airspeed as text: above the third constant (ordered) 999, otherwise below the
 * second (or unordered) 0; printed through _ftol into the element's buffer (+0x32d8) with the
 * format at +0x343c; returns it */
static long double __stdcall exairspd_1060(double value)
{
    if (!GAUGE_TEST(value, GR64(0x2010), GAUGE_C0C3)) value = 999.0;
    else if (GAUGE_TEST(value, GR64(0x2008), GAUGE_C0)) value = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x32d8), (const char *)GAUGE_D(0x343c), (uint32_t)gauge_ftol(value));
    return value;
}
GAUGE_INSTALL(exairspd_install)   /* 0x10d0 */
GAUGE_INITIALIZE(exairspd_initialize)   /* 0x1120 */
GAUGE_UPDATE(exairspd_update)   /* 0x1130 */
GAUGE_GENERATE(exairspd_generate)   /* 0x1140 */
GAUGE_DRAW(exairspd_draw)   /* 0x1160 */
GAUGE_KILL(exairspd_kill)   /* 0x1170 */
GAUGE_MODULE(exairspd_module)   /* 0x11a0 */

GAUGE_QUERY(exairspd_11b0)   /* 0x11b0 (the _ftol import thunk at 0x11d0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)exairspd_1020}, {0x1060, (void *)exairspd_1060}, {0x10d0, (void *)exairspd_install}, {0x1120, (void *)exairspd_initialize}, {0x1130, (void *)exairspd_update}, {0x1140, (void *)exairspd_generate}, {0x1160, (void *)exairspd_draw}, {0x1170, (void *)exairspd_kill}, {0x11a0, (void *)exairspd_module}, {0x11b0, (void *)exairspd_11b0})
