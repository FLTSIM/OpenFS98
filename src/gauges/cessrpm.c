/* CESSRPM.GAU -- CESSNA_182.RPM: the tachometer, whose variable is installed and scaled */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3308u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3278u
#define GAUGE_MOUSE 0x3280u
#define GAUGE_MOUSE_ARGUMENT 0x3264u


/* 0x1020: the needle: the value times the variable (+0x312c), clamped */
static long double __stdcall cessrpm_1020(double value)
{
    return gauge_clamp((long double)value * GD64(0x312c), GR64(0x2000), GR64(0x2008));
}

/* 0x1060, header +0x10: install: the elements, the variable at +0x3120 initialised (PANELS
 * +0x98), then the mouse rectangles */
static void __stdcall cessrpm_1060(uint32_t context)
{
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3120));
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
}

/* 0x10c0, header +0x14: initialise: the elements, the variable looked up (PANELS +0xa0), and
 * its value (+0x312c) scaled in place */
static void __stdcall cessrpm_10c0(void)
{
    GAUGE_PANELS_CALL(0x68, 1)(GD32(GAUGE_ELEMENTS));
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3120));
    GD64(0x312c) = (double)((long double)GD64(0x312c) * GR64(0x2010));
}
GAUGE_UPDATE(cessrpm_update)   /* 0x1100 */
GAUGE_GENERATE(cessrpm_generate)   /* 0x1110 */
GAUGE_DRAW(cessrpm_draw)   /* 0x1130 */
GAUGE_KILL(cessrpm_kill)   /* 0x1140 */
GAUGE_MODULE(cessrpm_module)   /* 0x1170 */
GAUGE_QUERY(cessrpm_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)cessrpm_1020}, {0x1060, (void *)cessrpm_1060}, {0x10c0, (void *)cessrpm_10c0}, {0x1100, (void *)cessrpm_update}, {0x1110, (void *)cessrpm_generate}, {0x1130, (void *)cessrpm_draw}, {0x1140, (void *)cessrpm_kill}, {0x1170, (void *)cessrpm_module}, {0x1180, (void *)cessrpm_query})
