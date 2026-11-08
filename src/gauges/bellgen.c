/* BELLGEN.GAU -- BELL_206B.GENERATOR_FUEL_PSI: the generator needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x34d0u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3390u
#define GAUGE_MOUSE 0x3398u
#define GAUGE_MOUSE_ARGUMENT 0x337cu


/* 0x1020: the needle: the value scaled */
static long double __stdcall bellgen_1020(double value)
{
    return (long double)value * GR64(0x2000);
}

/* 0x1030: the needle: the value scaled */
static long double __stdcall bellgen_1030(double value)
{
    return (long double)value * GR64(0x2000);
}
GAUGE_INSTALL(bellgen_install)   /* 0x1040 */
GAUGE_INITIALIZE(bellgen_initialize)   /* 0x1090 */
GAUGE_UPDATE(bellgen_update)   /* 0x10a0 */
GAUGE_GENERATE(bellgen_generate)   /* 0x10b0 */
GAUGE_DRAW(bellgen_draw)   /* 0x10d0 */
GAUGE_KILL(bellgen_kill)   /* 0x10e0 */
GAUGE_MODULE(bellgen_module)   /* 0x1110 */
GAUGE_QUERY(bellgen_query)   /* 0x1120 */

GAUGE_ROUTINES({0x1020, (void *)bellgen_1020}, {0x1030, (void *)bellgen_1030}, {0x1040, (void *)bellgen_install}, {0x1090, (void *)bellgen_initialize}, {0x10a0, (void *)bellgen_update}, {0x10b0, (void *)bellgen_generate}, {0x10d0, (void *)bellgen_draw}, {0x10e0, (void *)bellgen_kill}, {0x1110, (void *)bellgen_module}, {0x1120, (void *)bellgen_query})
