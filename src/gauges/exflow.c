/* EXFLOW.GAU -- EXTRA-300.FUEL-FLOW-GPH: the fuel flow as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x32b0u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3218u
#define GAUGE_MOUSE 0x3220u
#define GAUGE_MOUSE_ARGUMENT 0x3204u


/* 0x1020: the fuel flow (+0x30d0) kept to its range (below the first constant, or unordered,
 * that constant; above the second, ordered, the second), printed as whole and tenths (the
 * fraction times the third constant, each through _ftol) into the element's buffer (+0x3140)
 * with the format at +0x32a4; returns the flow as retail parks it, a double */
static long double __stdcall exflow_1020(double value)
{
    long double flow = GD64(0x30d0);
    double shown;
    uint32_t whole, tenths;
    (void)value;
    if (GAUGE_TEST(flow, GR64(0x2000), GAUGE_C0)) flow = GR64(0x2000);
    else if (!GAUGE_TEST(flow, GR64(0x2008), GAUGE_C0C3)) flow = GR64(0x2008);
    shown = (double)flow;
    whole = (uint32_t)gauge_ftol(flow);
    tenths = (uint32_t)gauge_ftol((flow - (long double)(int32_t)whole) * GR64(0x2010));
    wsprintfA((char *)(uintptr_t)GD32(0x3140), (const char *)GAUGE_D(0x32a4), whole, tenths);
    return shown;
}
GAUGE_INSTALL(exflow_install)   /* 0x10a0 */
GAUGE_INITIALIZE(exflow_initialize)   /* 0x10f0 */
GAUGE_UPDATE(exflow_update)   /* 0x1100 */
GAUGE_GENERATE(exflow_generate)   /* 0x1110 */
GAUGE_DRAW(exflow_draw)   /* 0x1130 */
GAUGE_KILL(exflow_kill)   /* 0x1140 */
GAUGE_MODULE(exflow_module)   /* 0x1170 */

GAUGE_QUERY(exflow_1180)   /* 0x1180 (the _ftol import thunk at 0x11a0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)exflow_1020}, {0x10a0, (void *)exflow_install}, {0x10f0, (void *)exflow_initialize}, {0x1100, (void *)exflow_update}, {0x1110, (void *)exflow_generate}, {0x1130, (void *)exflow_draw}, {0x1140, (void *)exflow_kill}, {0x1170, (void *)exflow_module}, {0x1180, (void *)exflow_1180})
