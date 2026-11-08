/* SCHAIRSP.GAU -- SCHWEIZER.AIRSPEED: the airspeed needle and the airspeed as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x34b0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3418u
#define GAUGE_MOUSE 0x3420u
#define GAUGE_MOUSE_ARGUMENT 0x3404u


/* 0x1020: the needle: the value scaled, limited */
static long double __stdcall schairsp_1020(double value)
{
    return gauge_limit((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}

/* 0x1060: the airspeed as text: the value scaled (parked as a double; the unrounded product
 * below the third constant, or unordered, parks 0), printed through _ftol into the element's
 * buffer (+0x3340) with the format at +0x34a4; returns it */
static long double __stdcall schairsp_1060(double value)
{
    long double speed = (long double)value * GR64(0x2000);
    volatile double shown = (double)speed;     /* stored before the test, as retail stores it (its rounding's flags) */
    if (GAUGE_TEST(speed, GR64(0x2010), GAUGE_C0)) shown = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3340), (const char *)GAUGE_D(0x34a4), (uint32_t)gauge_ftol(shown));
    return shown;
}
GAUGE_INSTALL(schairsp_install)   /* 0x10b0 */
GAUGE_INITIALIZE(schairsp_initialize)   /* 0x1100 */
GAUGE_UPDATE(schairsp_update)   /* 0x1110 */
GAUGE_GENERATE(schairsp_generate)   /* 0x1120 */
GAUGE_DRAW(schairsp_draw)   /* 0x1140 */
GAUGE_KILL(schairsp_kill)   /* 0x1150 */
GAUGE_MODULE(schairsp_module)   /* 0x1180 */

GAUGE_QUERY(schairsp_1190)   /* 0x1190 (the _ftol import thunk at 0x11b0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)schairsp_1020}, {0x1060, (void *)schairsp_1060}, {0x10b0, (void *)schairsp_install}, {0x1100, (void *)schairsp_initialize}, {0x1110, (void *)schairsp_update}, {0x1120, (void *)schairsp_generate}, {0x1140, (void *)schairsp_draw}, {0x1150, (void *)schairsp_kill}, {0x1180, (void *)schairsp_module}, {0x1190, (void *)schairsp_1190})
