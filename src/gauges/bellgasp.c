/* BELLGASP.GAU -- BELL_206B.GAS_PRODUCER: the gas producer needles */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3348u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x32b8u
#define GAUGE_MOUSE 0x32c0u
#define GAUGE_MOUSE_ARGUMENT 0x32a4u


/* 0x1020: the needle: the value scaled, clamped */
static long double __stdcall bellgasp_1020(double value)
{
    return gauge_clamp((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}

/* 0x1060: the vernier: the scaled value's units digit (through _ftol, unsigned modulo 10),
 * scaled */
static long double __stdcall bellgasp_1060(double value)
{
    uint32_t whole = (uint32_t)gauge_ftol((long double)value * GR64(0x2000));
    return (long double)(uint64_t)(whole % 10) * GR64(0x2018);
}
GAUGE_INSTALL(bellgasp_install)   /* 0x10a0 */
GAUGE_INITIALIZE(bellgasp_initialize)   /* 0x10f0 */
GAUGE_UPDATE(bellgasp_update)   /* 0x1100 */
GAUGE_GENERATE(bellgasp_generate)   /* 0x1110 */
GAUGE_DRAW(bellgasp_draw)   /* 0x1130 */
GAUGE_KILL(bellgasp_kill)   /* 0x1140 */
GAUGE_MODULE(bellgasp_module)   /* 0x1170 */

GAUGE_QUERY(bellgasp_1180)   /* 0x1180 (the _ftol import thunk at 0x11a0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)bellgasp_1020}, {0x1060, (void *)bellgasp_1060}, {0x10a0, (void *)bellgasp_install}, {0x10f0, (void *)bellgasp_initialize}, {0x1100, (void *)bellgasp_update}, {0x1110, (void *)bellgasp_generate}, {0x1130, (void *)bellgasp_draw}, {0x1140, (void *)bellgasp_kill}, {0x1170, (void *)bellgasp_module}, {0x1180, (void *)bellgasp_1180})
