/* BELLOIL.GAU -- BELL_206B.OIL: the oil needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x34d8u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3398u
#define GAUGE_MOUSE 0x33a0u
#define GAUGE_MOUSE_ARGUMENT 0x3384u


/* 0x1020: the needle: the value scaled */
static long double __stdcall belloil_1020(double value)
{
    return (long double)value * GR64(0x2000);
}

/* 0x1030: the needle: the value scaled */
static long double __stdcall belloil_1030(double value)
{
    return (long double)value * GR64(0x2008);
}
GAUGE_INSTALL(belloil_install)   /* 0x1040 */
GAUGE_INITIALIZE(belloil_initialize)   /* 0x1090 */
GAUGE_UPDATE(belloil_update)   /* 0x10a0 */
GAUGE_GENERATE(belloil_generate)   /* 0x10b0 */
GAUGE_DRAW(belloil_draw)   /* 0x10d0 */
GAUGE_KILL(belloil_kill)   /* 0x10e0 */
GAUGE_MODULE(belloil_module)   /* 0x1110 */
GAUGE_QUERY(belloil_query)   /* 0x1120 */

GAUGE_ROUTINES({0x1020, (void *)belloil_1020}, {0x1030, (void *)belloil_1030}, {0x1040, (void *)belloil_install}, {0x1090, (void *)belloil_initialize}, {0x10a0, (void *)belloil_update}, {0x10b0, (void *)belloil_generate}, {0x10d0, (void *)belloil_draw}, {0x10e0, (void *)belloil_kill}, {0x1110, (void *)belloil_module}, {0x1120, (void *)belloil_query})
